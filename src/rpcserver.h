// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_rpcserver_h
#define moorecoin_rpcserver_h

#include "amount.h"
#include "rpcprotocol.h"
#include "uint256.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <boost/function.hpp>

#include "univalue/univalue.h"

class crpccommand;

namespace rpcserver
{
    void onstarted(boost::function<void ()> slot);
    void onstopped(boost::function<void ()> slot);
    void onprecommand(boost::function<void (const crpccommand&)> slot);
    void onpostcommand(boost::function<void (const crpccommand&)> slot);
}

class cblockindex;
class cnetaddr;

class acceptedconnection
{
public:
    virtual ~acceptedconnection() {}

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
};

/** start rpc threads */
void startrpcthreads();
/**
 * alternative to startrpcthreads for the gui, when no server is
 * used. the rpc thread in this case is only used to handle timeouts.
 * if real rpc threads have already been started this is a no-op.
 */
void startdummyrpcthread();
/** stop rpc threads */
void stoprpcthreads();
/** query whether rpc is running */
bool isrpcrunning();

/**
 * set the rpc warmup status.  when this is done, all rpc calls will error out
 * immediately with rpc_in_warmup.
 */
void setrpcwarmupstatus(const std::string& newstatus);
/* mark warmup as done.  rpc calls will be processed from now on.  */
void setrpcwarmupfinished();

/* returns the current warmup state.  */
bool rpcisinwarmup(std::string *statusout);

/**
 * type-check arguments; throws jsonrpcerror if wrong type given. does not check that
 * the right number of arguments are passed, just that any passed are the correct type.
 * use like:  rpctypecheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
 */
void rpctypecheck(const univalue& params,
                  const std::list<univalue::vtype>& typesexpected, bool fallownull=false);

/*
  check for expected keys/value types in an object.
  use like: rpctypecheckobj(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void rpctypecheckobj(const univalue& o,
                  const std::map<std::string, univalue::vtype>& typesexpected, bool fallownull=false);

/**
 * run func nseconds from now. uses boost deadline timers.
 * overrides previous timer <name> (if any).
 */
void rpcrunlater(const std::string& name, boost::function<void(void)> func, int64_t nseconds);

//! convert boost::asio address to cnetaddr
extern cnetaddr boostasiotocnetaddr(boost::asio::ip::address address);

typedef univalue(*rpcfn_type)(const univalue& params, bool fhelp);

class crpccommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    bool oksafemode;
};

/**
 * moorecoin rpc command dispatcher.
 */
class crpctable
{
private:
    std::map<std::string, const crpccommand*> mapcommands;
public:
    crpctable();
    const crpccommand* operator[](const std::string& name) const;
    std::string help(const std::string& name) const;

    /**
     * execute a method.
     * @param method   method to execute
     * @param params   univalue array of arguments (json objects)
     * @returns result of the call.
     * @throws an exception (univalue) when an error happens.
     */
    univalue execute(const std::string &method, const univalue &params) const;
};

extern const crpctable tablerpc;

/**
 * utilities: convert hex-encoded values
 * (throws error if not hex).
 */
extern uint256 parsehashv(const univalue& v, std::string strname);
extern uint256 parsehasho(const univalue& o, std::string strkey);
extern std::vector<unsigned char> parsehexv(const univalue& v, std::string strname);
extern std::vector<unsigned char> parsehexo(const univalue& o, std::string strkey);

extern void initrpcmining();
extern void shutdownrpcmining();

extern int64_t nwalletunlocktime;
extern camount amountfromvalue(const univalue& value);
extern univalue valuefromamount(const camount& amount);
extern double getdifficulty(const cblockindex* blockindex = null);
extern std::string helprequiringpassphrase();
extern std::string helpexamplecli(const std::string& methodname, const std::string& args);
extern std::string helpexamplerpc(const std::string& methodname, const std::string& args);

extern void ensurewalletisunlocked();

extern univalue getconnectioncount(const univalue& params, bool fhelp); // in rpcnet.cpp
extern univalue getpeerinfo(const univalue& params, bool fhelp);
extern univalue ping(const univalue& params, bool fhelp);
extern univalue addnode(const univalue& params, bool fhelp);
extern univalue disconnectnode(const univalue& params, bool fhelp);
extern univalue getaddednodeinfo(const univalue& params, bool fhelp);
extern univalue getnettotals(const univalue& params, bool fhelp);
extern univalue setban(const univalue& params, bool fhelp);
extern univalue listbanned(const univalue& params, bool fhelp);
extern univalue clearbanned(const univalue& params, bool fhelp);

extern univalue dumpprivkey(const univalue& params, bool fhelp); // in rpcdump.cpp
extern univalue importprivkey(const univalue& params, bool fhelp);
extern univalue importaddress(const univalue& params, bool fhelp);
extern univalue dumpwallet(const univalue& params, bool fhelp);
extern univalue importwallet(const univalue& params, bool fhelp);

extern univalue getgenerate(const univalue& params, bool fhelp); // in rpcmining.cpp
extern univalue setgenerate(const univalue& params, bool fhelp);
extern univalue generate(const univalue& params, bool fhelp);
extern univalue getnetworkhashps(const univalue& params, bool fhelp);
extern univalue getmininginfo(const univalue& params, bool fhelp);
extern univalue prioritisetransaction(const univalue& params, bool fhelp);
extern univalue getblocktemplate(const univalue& params, bool fhelp);
extern univalue submitblock(const univalue& params, bool fhelp);
extern univalue estimatefee(const univalue& params, bool fhelp);
extern univalue estimatepriority(const univalue& params, bool fhelp);

extern univalue getnewaddress(const univalue& params, bool fhelp); // in rpcwallet.cpp
extern univalue getaccountaddress(const univalue& params, bool fhelp);
extern univalue getrawchangeaddress(const univalue& params, bool fhelp);
extern univalue setaccount(const univalue& params, bool fhelp);
extern univalue getaccount(const univalue& params, bool fhelp);
extern univalue getaddressesbyaccount(const univalue& params, bool fhelp);
extern univalue sendtoaddress(const univalue& params, bool fhelp);
extern univalue signmessage(const univalue& params, bool fhelp);
extern univalue verifymessage(const univalue& params, bool fhelp);
extern univalue getreceivedbyaddress(const univalue& params, bool fhelp);
extern univalue getreceivedbyaccount(const univalue& params, bool fhelp);
extern univalue getbalance(const univalue& params, bool fhelp);
extern univalue getunconfirmedbalance(const univalue& params, bool fhelp);
extern univalue movecmd(const univalue& params, bool fhelp);
extern univalue sendfrom(const univalue& params, bool fhelp);
extern univalue sendmany(const univalue& params, bool fhelp);
extern univalue addmultisigaddress(const univalue& params, bool fhelp);
extern univalue createmultisig(const univalue& params, bool fhelp);
extern univalue listreceivedbyaddress(const univalue& params, bool fhelp);
extern univalue listreceivedbyaccount(const univalue& params, bool fhelp);
extern univalue listtransactions(const univalue& params, bool fhelp);
extern univalue listaddressgroupings(const univalue& params, bool fhelp);
extern univalue listaccounts(const univalue& params, bool fhelp);
extern univalue listsinceblock(const univalue& params, bool fhelp);
extern univalue gettransaction(const univalue& params, bool fhelp);
extern univalue backupwallet(const univalue& params, bool fhelp);
extern univalue keypoolrefill(const univalue& params, bool fhelp);
extern univalue walletpassphrase(const univalue& params, bool fhelp);
extern univalue walletpassphrasechange(const univalue& params, bool fhelp);
extern univalue walletlock(const univalue& params, bool fhelp);
extern univalue encryptwallet(const univalue& params, bool fhelp);
extern univalue validateaddress(const univalue& params, bool fhelp);
extern univalue getinfo(const univalue& params, bool fhelp);
extern univalue getwalletinfo(const univalue& params, bool fhelp);
extern univalue getblockchaininfo(const univalue& params, bool fhelp);
extern univalue getnetworkinfo(const univalue& params, bool fhelp);
extern univalue setmocktime(const univalue& params, bool fhelp);
extern univalue resendwallettransactions(const univalue& params, bool fhelp);

extern univalue getrawtransaction(const univalue& params, bool fhelp); // in rcprawtransaction.cpp
extern univalue listunspent(const univalue& params, bool fhelp);
extern univalue lockunspent(const univalue& params, bool fhelp);
extern univalue listlockunspent(const univalue& params, bool fhelp);
extern univalue createrawtransaction(const univalue& params, bool fhelp);
extern univalue decoderawtransaction(const univalue& params, bool fhelp);
extern univalue decodescript(const univalue& params, bool fhelp);
extern univalue signrawtransaction(const univalue& params, bool fhelp);
extern univalue sendrawtransaction(const univalue& params, bool fhelp);
extern univalue gettxoutproof(const univalue& params, bool fhelp);
extern univalue verifytxoutproof(const univalue& params, bool fhelp);

extern univalue getblockcount(const univalue& params, bool fhelp); // in rpcblockchain.cpp
extern univalue getbestblockhash(const univalue& params, bool fhelp);
extern univalue getdifficulty(const univalue& params, bool fhelp);
extern univalue settxfee(const univalue& params, bool fhelp);
extern univalue getmempoolinfo(const univalue& params, bool fhelp);
extern univalue getrawmempool(const univalue& params, bool fhelp);
extern univalue getblockhash(const univalue& params, bool fhelp);
extern univalue getblock(const univalue& params, bool fhelp);
extern univalue gettxoutsetinfo(const univalue& params, bool fhelp);
extern univalue gettxout(const univalue& params, bool fhelp);
extern univalue verifychain(const univalue& params, bool fhelp);
extern univalue getchaintips(const univalue& params, bool fhelp);
extern univalue invalidateblock(const univalue& params, bool fhelp);
extern univalue reconsiderblock(const univalue& params, bool fhelp);

// in rest.cpp
extern bool httpreq_rest(acceptedconnection *conn,
                  const std::string& struri,
                  const std::string& strrequest,
                  const std::map<std::string, std::string>& mapheaders,
                  bool frun);

#endif // moorecoin_rpcserver_h
