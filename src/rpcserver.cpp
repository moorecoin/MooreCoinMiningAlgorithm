// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"

#include "base58.h"
#include "init.h"
#include "random.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#ifdef enable_wallet
#include "wallet/wallet.h"
#endif

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread.hpp>

#include "univalue/univalue.h"

using namespace boost::asio;
using namespace rpcserver;
using namespace std;

static std::string strrpcusercolonpass;

static bool frpcrunning = false;
static bool frpcinwarmup = true;
static std::string rpcwarmupstatus("rpc server started");
static ccriticalsection cs_rpcwarmup;

//! these are created by startrpcthreads, destroyed in stoprpcthreads
static boost::asio::io_service* rpc_io_service = null;
static map<string, boost::shared_ptr<deadline_timer> > deadlinetimers;
static ssl::context* rpc_ssl_context = null;
static boost::thread_group* rpc_worker_group = null;
static boost::asio::io_service::work *rpc_dummy_work = null;
static std::vector<csubnet> rpc_allow_subnets; //!< list of subnets to allow rpc connections from
static std::vector< boost::shared_ptr<ip::tcp::acceptor> > rpc_acceptors;

static struct crpcsignals
{
    boost::signals2::signal<void ()> started;
    boost::signals2::signal<void ()> stopped;
    boost::signals2::signal<void (const crpccommand&)> precommand;
    boost::signals2::signal<void (const crpccommand&)> postcommand;
} g_rpcsignals;

void rpcserver::onstarted(boost::function<void ()> slot)
{
    g_rpcsignals.started.connect(slot);
}

void rpcserver::onstopped(boost::function<void ()> slot)
{
    g_rpcsignals.stopped.connect(slot);
}

void rpcserver::onprecommand(boost::function<void (const crpccommand&)> slot)
{
    g_rpcsignals.precommand.connect(boost::bind(slot, _1));
}

void rpcserver::onpostcommand(boost::function<void (const crpccommand&)> slot)
{
    g_rpcsignals.postcommand.connect(boost::bind(slot, _1));
}

void rpctypecheck(const univalue& params,
                  const list<univalue::vtype>& typesexpected,
                  bool fallownull)
{
    unsigned int i = 0;
    boost_foreach(univalue::vtype t, typesexpected)
    {
        if (params.size() <= i)
            break;

        const univalue& v = params[i];
        if (!((v.type() == t) || (fallownull && (v.isnull()))))
        {
            string err = strprintf("expected type %s, got %s",
                                   uvtypename(t), uvtypename(v.type()));
            throw jsonrpcerror(rpc_type_error, err);
        }
        i++;
    }
}

void rpctypecheckobj(const univalue& o,
                  const map<string, univalue::vtype>& typesexpected,
                  bool fallownull)
{
    boost_foreach(const pairtype(string, univalue::vtype)& t, typesexpected)
    {
        const univalue& v = find_value(o, t.first);
        if (!fallownull && v.isnull())
            throw jsonrpcerror(rpc_type_error, strprintf("missing %s", t.first));

        if (!((v.type() == t.second) || (fallownull && (v.isnull()))))
        {
            string err = strprintf("expected type %s for %s, got %s",
                                   uvtypename(t.second), t.first, uvtypename(v.type()));
            throw jsonrpcerror(rpc_type_error, err);
        }
    }
}

camount amountfromvalue(const univalue& value)
{
    if (!value.isreal() && !value.isnum())
        throw jsonrpcerror(rpc_type_error, "amount is not a number");
    camount amount;
    if (!parsemoney(value.getvalstr(), amount))
        throw jsonrpcerror(rpc_type_error, "invalid amount");
    if (!moneyrange(amount))
        throw jsonrpcerror(rpc_type_error, "amount out of range");
    return amount;
}

univalue valuefromamount(const camount& amount)
{
    return univalue(univalue::vreal, formatmoney(amount));
}

uint256 parsehashv(const univalue& v, string strname)
{
    string strhex;
    if (v.isstr())
        strhex = v.get_str();
    if (!ishex(strhex)) // note: ishex("") is false
        throw jsonrpcerror(rpc_invalid_parameter, strname+" must be hexadecimal string (not '"+strhex+"')");
    uint256 result;
    result.sethex(strhex);
    return result;
}
uint256 parsehasho(const univalue& o, string strkey)
{
    return parsehashv(find_value(o, strkey), strkey);
}
vector<unsigned char> parsehexv(const univalue& v, string strname)
{
    string strhex;
    if (v.isstr())
        strhex = v.get_str();
    if (!ishex(strhex))
        throw jsonrpcerror(rpc_invalid_parameter, strname+" must be hexadecimal string (not '"+strhex+"')");
    return parsehex(strhex);
}
vector<unsigned char> parsehexo(const univalue& o, string strkey)
{
    return parsehexv(find_value(o, strkey), strkey);
}


/**
 * note: this interface may still be subject to change.
 */

std::string crpctable::help(const std::string& strcommand) const
{
    string strret;
    string category;
    set<rpcfn_type> setdone;
    vector<pair<string, const crpccommand*> > vcommands;

    for (map<string, const crpccommand*>::const_iterator mi = mapcommands.begin(); mi != mapcommands.end(); ++mi)
        vcommands.push_back(make_pair(mi->second->category + mi->first, mi->second));
    sort(vcommands.begin(), vcommands.end());

    boost_foreach(const pairtype(string, const crpccommand*)& command, vcommands)
    {
        const crpccommand *pcmd = command.second;
        string strmethod = pcmd->name;
        // we already filter duplicates, but these deprecated screw up the sort order
        if (strmethod.find("label") != string::npos)
            continue;
        if ((strcommand != "" || pcmd->category == "hidden") && strmethod != strcommand)
            continue;
        try
        {
            univalue params;
            rpcfn_type pfn = pcmd->actor;
            if (setdone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (const std::exception& e)
        {
            // help text is returned in an exception
            string strhelp = string(e.what());
            if (strcommand == "")
            {
                if (strhelp.find('\n') != string::npos)
                    strhelp = strhelp.substr(0, strhelp.find('\n'));

                if (category != pcmd->category)
                {
                    if (!category.empty())
                        strret += "\n";
                    category = pcmd->category;
                    string firstletter = category.substr(0,1);
                    boost::to_upper(firstletter);
                    strret += "== " + firstletter + category.substr(1) + " ==\n";
                }
            }
            strret += strhelp + "\n";
        }
    }
    if (strret == "")
        strret = strprintf("help: unknown command: %s\n", strcommand);
    strret = strret.substr(0,strret.size()-1);
    return strret;
}

univalue help(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "help ( \"command\" )\n"
            "\nlist all commands, or get help for a specified command.\n"
            "\narguments:\n"
            "1. \"command\"     (string, optional) the command to get help on\n"
            "\nresult:\n"
            "\"text\"     (string) the help text\n"
        );

    string strcommand;
    if (params.size() > 0)
        strcommand = params[0].get_str();

    return tablerpc.help(strcommand);
}


univalue stop(const univalue& params, bool fhelp)
{
    // accept the deprecated and ignored 'detach' boolean argument
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "stop\n"
            "\nstop moorecoin server.");
    // shutdown will take long enough that the response should get back
    startshutdown();
    return "moorecoin server stopping";
}



/**
 * call table
 */
static const crpccommand vrpccommands[] =
{ //  category              name                      actor (function)         oksafemode
  //  --------------------- ------------------------  -----------------------  ----------
    /* overall control/query calls */
    { "control",            "getinfo",                &getinfo,                true  }, /* uses wallet if enabled */
    { "control",            "help",                   &help,                   true  },
    { "control",            "stop",                   &stop,                   true  },

    /* p2p networking */
    { "network",            "getnetworkinfo",         &getnetworkinfo,         true  },
    { "network",            "addnode",                &addnode,                true  },
    { "network",            "disconnectnode",         &disconnectnode,         true  },
    { "network",            "getaddednodeinfo",       &getaddednodeinfo,       true  },
    { "network",            "getconnectioncount",     &getconnectioncount,     true  },
    { "network",            "getnettotals",           &getnettotals,           true  },
    { "network",            "getpeerinfo",            &getpeerinfo,            true  },
    { "network",            "ping",                   &ping,                   true  },
    { "network",            "setban",                 &setban,                 true  },
    { "network",            "listbanned",             &listbanned,             true  },
    { "network",            "clearbanned",            &clearbanned,            true  },

    /* block chain and utxo */
    { "blockchain",         "getblockchaininfo",      &getblockchaininfo,      true  },
    { "blockchain",         "getbestblockhash",       &getbestblockhash,       true  },
    { "blockchain",         "getblockcount",          &getblockcount,          true  },
    { "blockchain",         "getblock",               &getblock,               true  },
    { "blockchain",         "getblockhash",           &getblockhash,           true  },
    { "blockchain",         "getchaintips",           &getchaintips,           true  },
    { "blockchain",         "getdifficulty",          &getdifficulty,          true  },
    { "blockchain",         "getmempoolinfo",         &getmempoolinfo,         true  },
    { "blockchain",         "getrawmempool",          &getrawmempool,          true  },
    { "blockchain",         "gettxout",               &gettxout,               true  },
    { "blockchain",         "gettxoutproof",          &gettxoutproof,          true  },
    { "blockchain",         "verifytxoutproof",       &verifytxoutproof,       true  },
    { "blockchain",         "gettxoutsetinfo",        &gettxoutsetinfo,        true  },
    { "blockchain",         "verifychain",            &verifychain,            true  },

    /* mining */
    { "mining",             "getblocktemplate",       &getblocktemplate,       true  },
    { "mining",             "getmininginfo",          &getmininginfo,          true  },
    { "mining",             "getnetworkhashps",       &getnetworkhashps,       true  },
    { "mining",             "prioritisetransaction",  &prioritisetransaction,  true  },
    { "mining",             "submitblock",            &submitblock,            true  },

#ifdef enable_wallet
    /* coin generation */
    { "generating",         "getgenerate",            &getgenerate,            true  },
    { "generating",         "setgenerate",            &setgenerate,            true  },
    { "generating",         "generate",               &generate,               true  },
#endif

    /* raw transactions */
    { "rawtransactions",    "createrawtransaction",   &createrawtransaction,   true  },
    { "rawtransactions",    "decoderawtransaction",   &decoderawtransaction,   true  },
    { "rawtransactions",    "decodescript",           &decodescript,           true  },
    { "rawtransactions",    "getrawtransaction",      &getrawtransaction,      true  },
    { "rawtransactions",    "sendrawtransaction",     &sendrawtransaction,     false },
    { "rawtransactions",    "signrawtransaction",     &signrawtransaction,     false }, /* uses wallet if enabled */

    /* utility functions */
    { "util",               "createmultisig",         &createmultisig,         true  },
    { "util",               "validateaddress",        &validateaddress,        true  }, /* uses wallet if enabled */
    { "util",               "verifymessage",          &verifymessage,          true  },
    { "util",               "estimatefee",            &estimatefee,            true  },
    { "util",               "estimatepriority",       &estimatepriority,       true  },

    /* not shown in help */
    { "hidden",             "invalidateblock",        &invalidateblock,        true  },
    { "hidden",             "reconsiderblock",        &reconsiderblock,        true  },
    { "hidden",             "setmocktime",            &setmocktime,            true  },
#ifdef enable_wallet
    { "hidden",             "resendwallettransactions", &resendwallettransactions, true},
#endif

#ifdef enable_wallet
    /* wallet */
    { "wallet",             "addmultisigaddress",     &addmultisigaddress,     true  },
    { "wallet",             "backupwallet",           &backupwallet,           true  },
    { "wallet",             "dumpprivkey",            &dumpprivkey,            true  },
    { "wallet",             "dumpwallet",             &dumpwallet,             true  },
    { "wallet",             "encryptwallet",          &encryptwallet,          true  },
    { "wallet",             "getaccountaddress",      &getaccountaddress,      true  },
    { "wallet",             "getaccount",             &getaccount,             true  },
    { "wallet",             "getaddressesbyaccount",  &getaddressesbyaccount,  true  },
    { "wallet",             "getbalance",             &getbalance,             false },
    { "wallet",             "getnewaddress",          &getnewaddress,          true  },
    { "wallet",             "getrawchangeaddress",    &getrawchangeaddress,    true  },
    { "wallet",             "getreceivedbyaccount",   &getreceivedbyaccount,   false },
    { "wallet",             "getreceivedbyaddress",   &getreceivedbyaddress,   false },
    { "wallet",             "gettransaction",         &gettransaction,         false },
    { "wallet",             "getunconfirmedbalance",  &getunconfirmedbalance,  false },
    { "wallet",             "getwalletinfo",          &getwalletinfo,          false },
    { "wallet",             "importprivkey",          &importprivkey,          true  },
    { "wallet",             "importwallet",           &importwallet,           true  },
    { "wallet",             "importaddress",          &importaddress,          true  },
    { "wallet",             "keypoolrefill",          &keypoolrefill,          true  },
    { "wallet",             "listaccounts",           &listaccounts,           false },
    { "wallet",             "listaddressgroupings",   &listaddressgroupings,   false },
    { "wallet",             "listlockunspent",        &listlockunspent,        false },
    { "wallet",             "listreceivedbyaccount",  &listreceivedbyaccount,  false },
    { "wallet",             "listreceivedbyaddress",  &listreceivedbyaddress,  false },
    { "wallet",             "listsinceblock",         &listsinceblock,         false },
    { "wallet",             "listtransactions",       &listtransactions,       false },
    { "wallet",             "listunspent",            &listunspent,            false },
    { "wallet",             "lockunspent",            &lockunspent,            true  },
    { "wallet",             "move",                   &movecmd,                false },
    { "wallet",             "sendfrom",               &sendfrom,               false },
    { "wallet",             "sendmany",               &sendmany,               false },
    { "wallet",             "sendtoaddress",          &sendtoaddress,          false },
    { "wallet",             "setaccount",             &setaccount,             true  },
    { "wallet",             "settxfee",               &settxfee,               true  },
    { "wallet",             "signmessage",            &signmessage,            true  },
    { "wallet",             "walletlock",             &walletlock,             true  },
    { "wallet",             "walletpassphrasechange", &walletpassphrasechange, true  },
    { "wallet",             "walletpassphrase",       &walletpassphrase,       true  },
#endif // enable_wallet
};

crpctable::crpctable()
{
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vrpccommands) / sizeof(vrpccommands[0])); vcidx++)
    {
        const crpccommand *pcmd;

        pcmd = &vrpccommands[vcidx];
        mapcommands[pcmd->name] = pcmd;
    }
}

const crpccommand *crpctable::operator[](const std::string& name) const
{
    map<string, const crpccommand*>::const_iterator it = mapcommands.find(name);
    if (it == mapcommands.end())
        return null;
    return (*it).second;
}


bool httpauthorized(map<string, string>& mapheaders)
{
    string strauth = mapheaders["authorization"];
    if (strauth.substr(0,6) != "basic ")
        return false;
    string struserpass64 = strauth.substr(6); boost::trim(struserpass64);
    string struserpass = decodebase64(struserpass64);
    return timingresistantequal(struserpass, strrpcusercolonpass);
}

void errorreply(std::ostream& stream, const univalue& objerror, const univalue& id)
{
    // send error reply from json-rpc error object
    int nstatus = http_internal_server_error;
    int code = find_value(objerror, "code").get_int();
    if (code == rpc_invalid_request) nstatus = http_bad_request;
    else if (code == rpc_method_not_found) nstatus = http_not_found;
    string strreply = jsonrpcreply(nullunivalue, objerror, id);
    stream << httpreply(nstatus, strreply, false) << std::flush;
}

cnetaddr boostasiotocnetaddr(boost::asio::ip::address address)
{
    cnetaddr netaddr;
    // make sure that ipv4-compatible and ipv4-mapped ipv6 addresses are treated as ipv4 addresses
    if (address.is_v6()
     && (address.to_v6().is_v4_compatible()
      || address.to_v6().is_v4_mapped()))
        address = address.to_v6().to_v4();

    if(address.is_v4())
    {
        boost::asio::ip::address_v4::bytes_type bytes = address.to_v4().to_bytes();
        netaddr.setraw(net_ipv4, &bytes[0]);
    }
    else
    {
        boost::asio::ip::address_v6::bytes_type bytes = address.to_v6().to_bytes();
        netaddr.setraw(net_ipv6, &bytes[0]);
    }
    return netaddr;
}

bool clientallowed(const boost::asio::ip::address& address)
{
    cnetaddr netaddr = boostasiotocnetaddr(address);
    boost_foreach(const csubnet &subnet, rpc_allow_subnets)
        if (subnet.match(netaddr))
            return true;
    return false;
}

template <typename protocol>
class acceptedconnectionimpl : public acceptedconnection
{
public:
    acceptedconnectionimpl(
            boost::asio::io_service& io_service,
            ssl::context &context,
            bool fusessl) :
        sslstream(io_service, context),
        _d(sslstream, fusessl),
        _stream(_d)
    {
    }

    virtual std::iostream& stream()
    {
        return _stream;
    }

    virtual std::string peer_address_to_string() const
    {
        return peer.address().to_string();
    }

    virtual void close()
    {
        _stream.close();
    }

    typename protocol::endpoint peer;
    boost::asio::ssl::stream<typename protocol::socket> sslstream;

private:
    ssliostreamdevice<protocol> _d;
    boost::iostreams::stream< ssliostreamdevice<protocol> > _stream;
};

void serviceconnection(acceptedconnection *conn);

//! forward declaration required for rpclisten
template <typename protocol, typename socketacceptorservice>
static void rpcaccepthandler(boost::shared_ptr< basic_socket_acceptor<protocol, socketacceptorservice> > acceptor,
                             ssl::context& context,
                             bool fusessl,
                             boost::shared_ptr< acceptedconnection > conn,
                             const boost::system::error_code& error);

/**
 * sets up i/o resources to accept and handle a new connection.
 */
template <typename protocol, typename socketacceptorservice>
static void rpclisten(boost::shared_ptr< basic_socket_acceptor<protocol, socketacceptorservice> > acceptor,
                   ssl::context& context,
                   const bool fusessl)
{
    // accept connection
    boost::shared_ptr< acceptedconnectionimpl<protocol> > conn(new acceptedconnectionimpl<protocol>(acceptor->get_io_service(), context, fusessl));

    acceptor->async_accept(
            conn->sslstream.lowest_layer(),
            conn->peer,
            boost::bind(&rpcaccepthandler<protocol, socketacceptorservice>,
                acceptor,
                boost::ref(context),
                fusessl,
                conn,
                _1));
}


/**
 * accept and handle incoming connection.
 */
template <typename protocol, typename socketacceptorservice>
static void rpcaccepthandler(boost::shared_ptr< basic_socket_acceptor<protocol, socketacceptorservice> > acceptor,
                             ssl::context& context,
                             const bool fusessl,
                             boost::shared_ptr< acceptedconnection > conn,
                             const boost::system::error_code& error)
{
    // immediately start accepting new connections, except when we're cancelled or our socket is closed.
    if (error != boost::asio::error::operation_aborted && acceptor->is_open())
        rpclisten(acceptor, context, fusessl);

    acceptedconnectionimpl<ip::tcp>* tcp_conn = dynamic_cast< acceptedconnectionimpl<ip::tcp>* >(conn.get());

    if (error)
    {
        // todo: actually handle errors
        logprintf("%s: error: %s\n", __func__, error.message());
    }
    // restrict callers by ip.  it is important to
    // do this before starting client thread, to filter out
    // certain dos and misbehaving clients.
    else if (tcp_conn && !clientallowed(tcp_conn->peer.address()))
    {
        // only send a 403 if we're not using ssl to prevent a dos during the ssl handshake.
        if (!fusessl)
            conn->stream() << httperror(http_forbidden, false) << std::flush;
        conn->close();
    }
    else {
        serviceconnection(conn.get());
        conn->close();
    }
}

static ip::tcp::endpoint parseendpoint(const std::string &strendpoint, int defaultport)
{
    std::string addr;
    int port = defaultport;
    splithostport(strendpoint, port, addr);
    return ip::tcp::endpoint(boost::asio::ip::address::from_string(addr), port);
}

void startrpcthreads()
{
    rpc_allow_subnets.clear();
    rpc_allow_subnets.push_back(csubnet("127.0.0.0/8")); // always allow ipv4 local subnet
    rpc_allow_subnets.push_back(csubnet("::1")); // always allow ipv6 localhost
    if (mapmultiargs.count("-rpcallowip"))
    {
        const vector<string>& vallow = mapmultiargs["-rpcallowip"];
        boost_foreach(string strallow, vallow)
        {
            csubnet subnet(strallow);
            if(!subnet.isvalid())
            {
                uiinterface.threadsafemessagebox(
                    strprintf("invalid -rpcallowip subnet specification: %s. valid are a single ip (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/cidr (e.g. 1.2.3.4/24).", strallow),
                    "", cclientuiinterface::msg_error);
                startshutdown();
                return;
            }
            rpc_allow_subnets.push_back(subnet);
        }
    }
    std::string strallowed;
    boost_foreach(const csubnet &subnet, rpc_allow_subnets)
        strallowed += subnet.tostring() + " ";
    logprint("rpc", "allowing rpc connections from: %s\n", strallowed);

    strrpcusercolonpass = mapargs["-rpcuser"] + ":" + mapargs["-rpcpassword"];
    if (((mapargs["-rpcpassword"] == "") ||
         (mapargs["-rpcuser"] == mapargs["-rpcpassword"])) && params().requirerpcpassword())
    {
        unsigned char rand_pwd[32];
        getrandbytes(rand_pwd, 32);
        uiinterface.threadsafemessagebox(strprintf(
            _("to use moorecoind, or the -server option to moorecoin-qt, you must set an rpcpassword in the configuration file:\n"
              "%s\n"
              "it is recommended you use the following random password:\n"
              "rpcuser=moorecoinrpc\n"
              "rpcpassword=%s\n"
              "(you do not need to remember this password)\n"
              "the username and password must not be the same.\n"
              "if the file does not exist, create it with owner-readable-only file permissions.\n"
              "it is also recommended to set alertnotify so you are notified of problems;\n"
              "for example: alertnotify=echo %%s | mail -s \"moorecoin alert\" admin@foo.com\n"),
                getconfigfile().string(),
                encodebase58(&rand_pwd[0],&rand_pwd[0]+32)),
                "", cclientuiinterface::msg_error | cclientuiinterface::secure);
        startshutdown();
        return;
    }

    assert(rpc_io_service == null);
    rpc_io_service = new boost::asio::io_service();
    rpc_ssl_context = new ssl::context(*rpc_io_service, ssl::context::sslv23);

    const bool fusessl = getboolarg("-rpcssl", false);

    if (fusessl)
    {
        rpc_ssl_context->set_options(ssl::context::no_sslv2 | ssl::context::no_sslv3);

        boost::filesystem::path pathcertfile(getarg("-rpcsslcertificatechainfile", "server.cert"));
        if (!pathcertfile.is_complete()) pathcertfile = boost::filesystem::path(getdatadir()) / pathcertfile;
        if (boost::filesystem::exists(pathcertfile)) rpc_ssl_context->use_certificate_chain_file(pathcertfile.string());
        else logprintf("threadrpcserver error: missing server certificate file %s\n", pathcertfile.string());

        boost::filesystem::path pathpkfile(getarg("-rpcsslprivatekeyfile", "server.pem"));
        if (!pathpkfile.is_complete()) pathpkfile = boost::filesystem::path(getdatadir()) / pathpkfile;
        if (boost::filesystem::exists(pathpkfile)) rpc_ssl_context->use_private_key_file(pathpkfile.string(), ssl::context::pem);
        else logprintf("threadrpcserver error: missing server private key file %s\n", pathpkfile.string());

        string strciphers = getarg("-rpcsslciphers", "tlsv1.2+high:tlsv1+high:!sslv2:!anull:!enull:!3des:@strength");
        ssl_ctx_set_cipher_list(rpc_ssl_context->impl(), strciphers.c_str());
    }

    std::vector<ip::tcp::endpoint> vendpoints;
    bool bbindany = false;
    int defaultport = getarg("-rpcport", baseparams().rpcport());
    if (!mapargs.count("-rpcallowip")) // default to loopback if not allowing external ips
    {
        vendpoints.push_back(ip::tcp::endpoint(boost::asio::ip::address_v6::loopback(), defaultport));
        vendpoints.push_back(ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), defaultport));
        if (mapargs.count("-rpcbind"))
        {
            logprintf("warning: option -rpcbind was ignored because -rpcallowip was not specified, refusing to allow everyone to connect\n");
        }
    } else if (mapargs.count("-rpcbind")) // specific bind address
    {
        boost_foreach(const std::string &addr, mapmultiargs["-rpcbind"])
        {
            try {
                vendpoints.push_back(parseendpoint(addr, defaultport));
            }
            catch (const boost::system::system_error&)
            {
                uiinterface.threadsafemessagebox(
                    strprintf(_("could not parse -rpcbind value %s as network address"), addr),
                    "", cclientuiinterface::msg_error);
                startshutdown();
                return;
            }
        }
    } else { // no specific bind address specified, bind to any
        vendpoints.push_back(ip::tcp::endpoint(boost::asio::ip::address_v6::any(), defaultport));
        vendpoints.push_back(ip::tcp::endpoint(boost::asio::ip::address_v4::any(), defaultport));
        // prefer making the socket dual ipv6/ipv4 instead of binding
        // to both addresses seperately.
        bbindany = true;
    }

    bool flistening = false;
    std::string strerr;
    std::string straddress;
    boost_foreach(const ip::tcp::endpoint &endpoint, vendpoints)
    {
        try {
            boost::asio::ip::address bindaddress = endpoint.address();
            straddress = bindaddress.to_string();
            logprintf("binding rpc on address %s port %i (ipv4+ipv6 bind any: %i)\n", straddress, endpoint.port(), bbindany);
            boost::system::error_code v6_only_error;
            boost::shared_ptr<ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(*rpc_io_service));

            acceptor->open(endpoint.protocol());
            acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

            // try making the socket dual ipv6/ipv4 when listening on the ipv6 "any" address
            acceptor->set_option(boost::asio::ip::v6_only(
                !bbindany || bindaddress != boost::asio::ip::address_v6::any()), v6_only_error);

            acceptor->bind(endpoint);
            acceptor->listen(socket_base::max_connections);

            rpclisten(acceptor, *rpc_ssl_context, fusessl);

            flistening = true;
            rpc_acceptors.push_back(acceptor);
            // if dual ipv6/ipv4 bind successful, skip binding to ipv4 separately
            if(bbindany && bindaddress == boost::asio::ip::address_v6::any() && !v6_only_error)
                break;
        }
        catch (const boost::system::system_error& e)
        {
            logprintf("error: binding rpc on address %s port %i failed: %s\n", straddress, endpoint.port(), e.what());
            strerr = strprintf(_("an error occurred while setting up the rpc address %s port %u for listening: %s"), straddress, endpoint.port(), e.what());
        }
    }

    if (!flistening) {
        uiinterface.threadsafemessagebox(strerr, "", cclientuiinterface::msg_error);
        startshutdown();
        return;
    }

    rpc_worker_group = new boost::thread_group();
    for (int i = 0; i < getarg("-rpcthreads", 4); i++)
        rpc_worker_group->create_thread(boost::bind(&boost::asio::io_service::run, rpc_io_service));
    frpcrunning = true;
    g_rpcsignals.started();
}

void startdummyrpcthread()
{
    if(rpc_io_service == null)
    {
        rpc_io_service = new boost::asio::io_service();
        /* create dummy "work" to keep the thread from exiting when no timeouts active,
         * see http://www.boost.org/doc/libs/1_51_0/doc/html/boost_asio/reference/io_service.html#boost_asio.reference.io_service.stopping_the_io_service_from_running_out_of_work */
        rpc_dummy_work = new boost::asio::io_service::work(*rpc_io_service);
        rpc_worker_group = new boost::thread_group();
        rpc_worker_group->create_thread(boost::bind(&boost::asio::io_service::run, rpc_io_service));
        frpcrunning = true;
    }
}

void stoprpcthreads()
{
    if (rpc_io_service == null) return;
    // set this to false first, so that longpolling loops will exit when woken up
    frpcrunning = false;

    // first, cancel all timers and acceptors
    // this is not done automatically by ->stop(), and in some cases the destructor of
    // boost::asio::io_service can hang if this is skipped.
    boost::system::error_code ec;
    boost_foreach(const boost::shared_ptr<ip::tcp::acceptor> &acceptor, rpc_acceptors)
    {
        acceptor->cancel(ec);
        if (ec)
            logprintf("%s: warning: %s when cancelling acceptor", __func__, ec.message());
    }
    rpc_acceptors.clear();
    boost_foreach(const pairtype(std::string, boost::shared_ptr<deadline_timer>) &timer, deadlinetimers)
    {
        timer.second->cancel(ec);
        if (ec)
            logprintf("%s: warning: %s when cancelling timer", __func__, ec.message());
    }
    deadlinetimers.clear();

    rpc_io_service->stop();
    g_rpcsignals.stopped();
    if (rpc_worker_group != null)
        rpc_worker_group->join_all();
    delete rpc_dummy_work; rpc_dummy_work = null;
    delete rpc_worker_group; rpc_worker_group = null;
    delete rpc_ssl_context; rpc_ssl_context = null;
    delete rpc_io_service; rpc_io_service = null;
}

bool isrpcrunning()
{
    return frpcrunning;
}

void setrpcwarmupstatus(const std::string& newstatus)
{
    lock(cs_rpcwarmup);
    rpcwarmupstatus = newstatus;
}

void setrpcwarmupfinished()
{
    lock(cs_rpcwarmup);
    assert(frpcinwarmup);
    frpcinwarmup = false;
}

bool rpcisinwarmup(std::string *outstatus)
{
    lock(cs_rpcwarmup);
    if (outstatus)
        *outstatus = rpcwarmupstatus;
    return frpcinwarmup;
}

void rpcrunhandler(const boost::system::error_code& err, boost::function<void(void)> func)
{
    if (!err)
        func();
}

void rpcrunlater(const std::string& name, boost::function<void(void)> func, int64_t nseconds)
{
    assert(rpc_io_service != null);

    if (deadlinetimers.count(name) == 0)
    {
        deadlinetimers.insert(make_pair(name,
                                        boost::shared_ptr<deadline_timer>(new deadline_timer(*rpc_io_service))));
    }
    deadlinetimers[name]->expires_from_now(boost::posix_time::seconds(nseconds));
    deadlinetimers[name]->async_wait(boost::bind(rpcrunhandler, _1, func));
}

class jsonrequest
{
public:
    univalue id;
    string strmethod;
    univalue params;

    jsonrequest() { id = nullunivalue; }
    void parse(const univalue& valrequest);
};

void jsonrequest::parse(const univalue& valrequest)
{
    // parse request
    if (!valrequest.isobject())
        throw jsonrpcerror(rpc_invalid_request, "invalid request object");
    const univalue& request = valrequest.get_obj();

    // parse id now so errors from here on will have the id
    id = find_value(request, "id");

    // parse method
    univalue valmethod = find_value(request, "method");
    if (valmethod.isnull())
        throw jsonrpcerror(rpc_invalid_request, "missing method");
    if (!valmethod.isstr())
        throw jsonrpcerror(rpc_invalid_request, "method must be a string");
    strmethod = valmethod.get_str();
    if (strmethod != "getblocktemplate")
        logprint("rpc", "threadrpcserver method=%s\n", sanitizestring(strmethod));

    // parse params
    univalue valparams = find_value(request, "params");
    if (valparams.isarray())
        params = valparams.get_array();
    else if (valparams.isnull())
        params = univalue(univalue::varr);
    else
        throw jsonrpcerror(rpc_invalid_request, "params must be an array");
}


static univalue jsonrpcexecone(const univalue& req)
{
    univalue rpc_result(univalue::vobj);

    jsonrequest jreq;
    try {
        jreq.parse(req);

        univalue result = tablerpc.execute(jreq.strmethod, jreq.params);
        rpc_result = jsonrpcreplyobj(result, nullunivalue, jreq.id);
    }
    catch (const univalue& objerror)
    {
        rpc_result = jsonrpcreplyobj(nullunivalue, objerror, jreq.id);
    }
    catch (const std::exception& e)
    {
        rpc_result = jsonrpcreplyobj(nullunivalue,
                                     jsonrpcerror(rpc_parse_error, e.what()), jreq.id);
    }

    return rpc_result;
}

static string jsonrpcexecbatch(const univalue& vreq)
{
    univalue ret(univalue::varr);
    for (unsigned int reqidx = 0; reqidx < vreq.size(); reqidx++)
        ret.push_back(jsonrpcexecone(vreq[reqidx]));

    return ret.write() + "\n";
}

static bool httpreq_jsonrpc(acceptedconnection *conn,
                            string& strrequest,
                            map<string, string>& mapheaders,
                            bool frun)
{
    // check authorization
    if (mapheaders.count("authorization") == 0)
    {
        conn->stream() << httperror(http_unauthorized, false) << std::flush;
        return false;
    }

    if (!httpauthorized(mapheaders))
    {
        logprintf("threadrpcserver incorrect password attempt from %s\n", conn->peer_address_to_string());
        /* deter brute-forcing
           we don't support exposing the rpc port, so this shouldn't result
           in a dos. */
        millisleep(250);

        conn->stream() << httperror(http_unauthorized, false) << std::flush;
        return false;
    }

    jsonrequest jreq;
    try
    {
        // parse request
        univalue valrequest;
        if (!valrequest.read(strrequest))
            throw jsonrpcerror(rpc_parse_error, "parse error");

        // return immediately if in warmup
        {
            lock(cs_rpcwarmup);
            if (frpcinwarmup)
                throw jsonrpcerror(rpc_in_warmup, rpcwarmupstatus);
        }

        string strreply;

        // singleton request
        if (valrequest.isobject()) {
            jreq.parse(valrequest);

            univalue result = tablerpc.execute(jreq.strmethod, jreq.params);

            // send reply
            strreply = jsonrpcreply(result, nullunivalue, jreq.id);

        // array of requests
        } else if (valrequest.isarray())
            strreply = jsonrpcexecbatch(valrequest.get_array());
        else
            throw jsonrpcerror(rpc_parse_error, "top-level object parse error");

        conn->stream() << httpreplyheader(http_ok, frun, strreply.size()) << strreply << std::flush;
    }
    catch (const univalue& objerror)
    {
        errorreply(conn->stream(), objerror, jreq.id);
        return false;
    }
    catch (const std::exception& e)
    {
        errorreply(conn->stream(), jsonrpcerror(rpc_parse_error, e.what()), jreq.id);
        return false;
    }
    return true;
}

void serviceconnection(acceptedconnection *conn)
{
    bool frun = true;
    while (frun && !shutdownrequested())
    {
        int nproto = 0;
        map<string, string> mapheaders;
        string strrequest, strmethod, struri;

        // read http request line
        if (!readhttprequestline(conn->stream(), nproto, strmethod, struri))
            break;

        // read http message headers and body
        readhttpmessage(conn->stream(), mapheaders, strrequest, nproto, max_size);

        // http keep-alive is false; close connection immediately
        if ((mapheaders["connection"] == "close") || (!getboolarg("-rpckeepalive", true)))
            frun = false;

        // process via json-rpc api
        if (struri == "/") {
            if (!httpreq_jsonrpc(conn, strrequest, mapheaders, frun))
                break;

        // process via http rest api
        } else if (struri.substr(0, 6) == "/rest/" && getboolarg("-rest", false)) {
            if (!httpreq_rest(conn, struri, strrequest, mapheaders, frun))
                break;

        } else {
            conn->stream() << httperror(http_not_found, false) << std::flush;
            break;
        }
    }
}

univalue crpctable::execute(const std::string &strmethod, const univalue &params) const
{
    // find method
    const crpccommand *pcmd = tablerpc[strmethod];
    if (!pcmd)
        throw jsonrpcerror(rpc_method_not_found, "method not found");

    g_rpcsignals.precommand(*pcmd);

    try
    {
        // execute
        return pcmd->actor(params, false);
    }
    catch (const std::exception& e)
    {
        throw jsonrpcerror(rpc_misc_error, e.what());
    }

    g_rpcsignals.postcommand(*pcmd);
}

std::string helpexamplecli(const std::string& methodname, const std::string& args)
{
    return "> moorecoin-cli " + methodname + " " + args + "\n";
}

std::string helpexamplerpc(const std::string& methodname, const std::string& args)
{
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", "
        "\"method\": \"" + methodname + "\", \"params\": [" + args + "] }' -h 'content-type: text/plain;' http://127.0.0.1:8332/\n";
}

const crpctable tablerpc;
