// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcclient.h"

#include "rpcprotocol.h"
#include "util.h"

#include <set>
#include <stdint.h>

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include "univalue/univalue.h"

using namespace std;

class crpcconvertparam
{
public:
    std::string methodname;            //! method whose params want conversion
    int paramidx;                      //! 0-based idx of param to convert
};

static const crpcconvertparam vrpcconvertparams[] =
{
    { "stop", 0 },
    { "setmocktime", 0 },
    { "getaddednodeinfo", 0 },
    { "setgenerate", 0 },
    { "setgenerate", 1 },
    { "generate", 0 },
    { "getnetworkhashps", 0 },
    { "getnetworkhashps", 1 },
    { "sendtoaddress", 1 },
    { "sendtoaddress", 4 },
    { "settxfee", 0 },
    { "getreceivedbyaddress", 1 },
    { "getreceivedbyaccount", 1 },
    { "listreceivedbyaddress", 0 },
    { "listreceivedbyaddress", 1 },
    { "listreceivedbyaddress", 2 },
    { "listreceivedbyaccount", 0 },
    { "listreceivedbyaccount", 1 },
    { "listreceivedbyaccount", 2 },
    { "getbalance", 1 },
    { "getbalance", 2 },
    { "getblockhash", 0 },
    { "move", 2 },
    { "move", 3 },
    { "sendfrom", 2 },
    { "sendfrom", 3 },
    { "listtransactions", 1 },
    { "listtransactions", 2 },
    { "listtransactions", 3 },
    { "listaccounts", 0 },
    { "listaccounts", 1 },
    { "walletpassphrase", 1 },
    { "getblocktemplate", 0 },
    { "listsinceblock", 1 },
    { "listsinceblock", 2 },
    { "sendmany", 1 },
    { "sendmany", 2 },
    { "sendmany", 4 },
    { "addmultisigaddress", 0 },
    { "addmultisigaddress", 1 },
    { "createmultisig", 0 },
    { "createmultisig", 1 },
    { "listunspent", 0 },
    { "listunspent", 1 },
    { "listunspent", 2 },
    { "getblock", 1 },
    { "gettransaction", 1 },
    { "getrawtransaction", 1 },
    { "createrawtransaction", 0 },
    { "createrawtransaction", 1 },
    { "signrawtransaction", 1 },
    { "signrawtransaction", 2 },
    { "sendrawtransaction", 1 },
    { "gettxout", 1 },
    { "gettxout", 2 },
    { "gettxoutproof", 0 },
    { "lockunspent", 0 },
    { "lockunspent", 1 },
    { "importprivkey", 2 },
    { "importaddress", 2 },
    { "verifychain", 0 },
    { "verifychain", 1 },
    { "keypoolrefill", 0 },
    { "getrawmempool", 0 },
    { "estimatefee", 0 },
    { "estimatepriority", 0 },
    { "prioritisetransaction", 1 },
    { "prioritisetransaction", 2 },
    { "setban", 2 },
    { "setban", 3 },
};

class crpcconverttable
{
private:
    std::set<std::pair<std::string, int> > members;

public:
    crpcconverttable();

    bool convert(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
};

crpcconverttable::crpcconverttable()
{
    const unsigned int n_elem =
        (sizeof(vrpcconvertparams) / sizeof(vrpcconvertparams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vrpcconvertparams[i].methodname,
                                      vrpcconvertparams[i].paramidx));
    }
}

static crpcconverttable rpccvttable;

/** non-rfc4627 json parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
univalue parsenonrfcjsonvalue(const std::string& strval)
{
    univalue jval;
    if (!jval.read(std::string("[")+strval+std::string("]")) ||
        !jval.isarray() || jval.size()!=1)
        throw runtime_error(string("error parsing json:")+strval);
    return jval[0];
}

/** convert strings to command-specific rpc representation */
univalue rpcconvertvalues(const std::string &strmethod, const std::vector<std::string> &strparams)
{
    univalue params(univalue::varr);

    for (unsigned int idx = 0; idx < strparams.size(); idx++) {
        const std::string& strval = strparams[idx];

        if (!rpccvttable.convert(strmethod, idx)) {
            // insert string value directly
            params.push_back(strval);
        } else {
            // parse string as json, insert bool/number/object/etc. value
            params.push_back(parsenonrfcjsonvalue(strval));
        }
    }

    return params;
}

