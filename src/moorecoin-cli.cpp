// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"
#include "clientversion.h"
#include "rpcclient.h"
#include "rpcprotocol.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/filesystem/operations.hpp>

#include "univalue/univalue.h"

using namespace std;

std::string helpmessagecli()
{
    string strusage;
    strusage += helpmessagegroup(_("options:"));
    strusage += helpmessageopt("-?", _("this help message"));
    strusage += helpmessageopt("-conf=<file>", strprintf(_("specify configuration file (default: %s)"), "moorecoin.conf"));
    strusage += helpmessageopt("-datadir=<dir>", _("specify data directory"));
    strusage += helpmessageopt("-testnet", _("use the test network"));
    strusage += helpmessageopt("-regtest", _("enter regression test mode, which uses a special chain in which blocks can be "
                                             "solved instantly. this is intended for regression testing tools and app development."));
    strusage += helpmessageopt("-rpcconnect=<ip>", strprintf(_("send commands to node running on <ip> (default: %s)"), "127.0.0.1"));
    strusage += helpmessageopt("-rpcport=<port>", strprintf(_("connect to json-rpc on <port> (default: %u or testnet: %u)"), 8332, 18332));
    strusage += helpmessageopt("-rpcwait", _("wait for rpc server to start"));
    strusage += helpmessageopt("-rpcuser=<user>", _("username for json-rpc connections"));
    strusage += helpmessageopt("-rpcpassword=<pw>", _("password for json-rpc connections"));

    strusage += helpmessagegroup(_("ssl options: (see the moorecoin wiki for ssl setup instructions)"));
    strusage += helpmessageopt("-rpcssl", _("use openssl (https) for json-rpc connections"));

    return strusage;
}

//////////////////////////////////////////////////////////////////////////////
//
// start
//

//
// exception thrown on connection error.  this error is used to determine
// when to wait if -rpcwait is given.
//
class cconnectionfailed : public std::runtime_error
{
public:

    explicit inline cconnectionfailed(const std::string& msg) :
        std::runtime_error(msg)
    {}

};

static bool appinitrpc(int argc, char* argv[])
{
    //
    // parameters
    //
    parseparameters(argc, argv);
    if (argc<2 || mapargs.count("-?") || mapargs.count("-help") || mapargs.count("-version")) {
        std::string strusage = _("moorecoin core rpc client version") + " " + formatfullversion() + "\n";
        if (!mapargs.count("-version")) {
            strusage += "\n" + _("usage:") + "\n" +
                  "  moorecoin-cli [options] <command> [params]  " + _("send command to moorecoin core") + "\n" +
                  "  moorecoin-cli [options] help                " + _("list commands") + "\n" +
                  "  moorecoin-cli [options] help <command>      " + _("get help for a command") + "\n";

            strusage += "\n" + helpmessagecli();
        }

        fprintf(stdout, "%s", strusage.c_str());
        return false;
    }
    if (!boost::filesystem::is_directory(getdatadir(false))) {
        fprintf(stderr, "error: specified data directory \"%s\" does not exist.\n", mapargs["-datadir"].c_str());
        return false;
    }
    try {
        readconfigfile(mapargs, mapmultiargs);
    } catch (const std::exception& e) {
        fprintf(stderr,"error reading configuration file: %s\n", e.what());
        return false;
    }
    // check for -testnet or -regtest parameter (baseparams() calls are only valid after this clause)
    if (!selectbaseparamsfromcommandline()) {
        fprintf(stderr, "error: invalid combination of -regtest and -testnet.\n");
        return false;
    }
    return true;
}

univalue callrpc(const string& strmethod, const univalue& params)
{
    if (mapargs["-rpcuser"] == "" && mapargs["-rpcpassword"] == "")
        throw runtime_error(strprintf(
            _("you must set rpcpassword=<password> in the configuration file:\n%s\n"
              "if the file does not exist, create it with owner-readable-only file permissions."),
                getconfigfile().string().c_str()));

    // connect to localhost
    bool fusessl = getboolarg("-rpcssl", false);
    boost::asio::io_service io_service;
    boost::asio::ssl::context context(io_service, boost::asio::ssl::context::sslv23);
    context.set_options(boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3);
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> sslstream(io_service, context);
    ssliostreamdevice<boost::asio::ip::tcp> d(sslstream, fusessl);
    boost::iostreams::stream< ssliostreamdevice<boost::asio::ip::tcp> > stream(d);

    const bool fconnected = d.connect(getarg("-rpcconnect", "127.0.0.1"), getarg("-rpcport", itostr(baseparams().rpcport())));
    if (!fconnected)
        throw cconnectionfailed("couldn't connect to server");

    // http basic authentication
    string struserpass64 = encodebase64(mapargs["-rpcuser"] + ":" + mapargs["-rpcpassword"]);
    map<string, string> maprequestheaders;
    maprequestheaders["authorization"] = string("basic ") + struserpass64;

    // send request
    string strrequest = jsonrpcrequest(strmethod, params, 1);
    string strpost = httppost(strrequest, maprequestheaders);
    stream << strpost << std::flush;

    // receive http reply status
    int nproto = 0;
    int nstatus = readhttpstatus(stream, nproto);

    // receive http reply message headers and body
    map<string, string> mapheaders;
    string strreply;
    readhttpmessage(stream, mapheaders, strreply, nproto, std::numeric_limits<size_t>::max());

    if (nstatus == http_unauthorized)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nstatus >= 400 && nstatus != http_bad_request && nstatus != http_not_found && nstatus != http_internal_server_error)
        throw runtime_error(strprintf("server returned http error %d", nstatus));
    else if (strreply.empty())
        throw runtime_error("no response from server");

    // parse reply
    univalue valreply(univalue::vstr);
    if (!valreply.read(strreply))
        throw runtime_error("couldn't parse reply from server");
    const univalue& reply = valreply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}

int commandlinerpc(int argc, char *argv[])
{
    string strprint;
    int nret = 0;
    try {
        // skip switches
        while (argc > 1 && isswitchchar(argv[1][0])) {
            argc--;
            argv++;
        }

        // method
        if (argc < 2)
            throw runtime_error("too few parameters");
        string strmethod = argv[1];

        // parameters default to strings
        std::vector<std::string> strparams(&argv[2], &argv[argc]);
        univalue params = rpcconvertvalues(strmethod, strparams);

        // execute and handle connection failures with -rpcwait
        const bool fwait = getboolarg("-rpcwait", false);
        do {
            try {
                const univalue reply = callrpc(strmethod, params);

                // parse reply
                const univalue& result = find_value(reply, "result");
                const univalue& error  = find_value(reply, "error");

                if (!error.isnull()) {
                    // error
                    int code = error["code"].get_int();
                    if (fwait && code == rpc_in_warmup)
                        throw cconnectionfailed("server in warmup");
                    strprint = "error: " + error.write();
                    nret = abs(code);
                } else {
                    // result
                    if (result.isnull())
                        strprint = "";
                    else if (result.isstr())
                        strprint = result.get_str();
                    else
                        strprint = result.write(2);
                }
                // connection succeeded, no need to retry.
                break;
            }
            catch (const cconnectionfailed&) {
                if (fwait)
                    millisleep(1000);
                else
                    throw;
            }
        } while (fwait);
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (const std::exception& e) {
        strprint = string("error: ") + e.what();
        nret = exit_failure;
    }
    catch (...) {
        printexceptioncontinue(null, "commandlinerpc()");
        throw;
    }

    if (strprint != "") {
        fprintf((nret == 0 ? stdout : stderr), "%s\n", strprint.c_str());
    }
    return nret;
}

int main(int argc, char* argv[])
{
    setupenvironment();

    try {
        if(!appinitrpc(argc, argv))
            return exit_failure;
    }
    catch (const std::exception& e) {
        printexceptioncontinue(&e, "appinitrpc()");
        return exit_failure;
    } catch (...) {
        printexceptioncontinue(null, "appinitrpc()");
        return exit_failure;
    }

    int ret = exit_failure;
    try {
        ret = commandlinerpc(argc, argv);
    }
    catch (const std::exception& e) {
        printexceptioncontinue(&e, "commandlinerpc()");
    } catch (...) {
        printexceptioncontinue(null, "commandlinerpc()");
    }
    return ret;
}
