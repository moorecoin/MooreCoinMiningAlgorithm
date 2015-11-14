// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
#include "rpcserver.h"
#include "util.h"
#include "validationinterface.h"
#ifdef enable_wallet
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "univalue/univalue.h"

using namespace std;

/**
 * return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive.
 * if 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
univalue getnetworkhashps(int lookup, int height) {
    cblockindex *pb = chainactive.tip();

    if (height >= 0 && height < chainactive.height())
        pb = chainactive[height];

    if (pb == null || !pb->nheight)
        return 0;

    // if lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0)
        lookup = pb->nheight % params().getconsensus().difficultyadjustmentinterval() + 1;

    // if lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nheight)
        lookup = pb->nheight;

    cblockindex *pb0 = pb;
    int64_t mintime = pb0->getblocktime();
    int64_t maxtime = mintime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->getblocktime();
        mintime = std::min(time, mintime);
        maxtime = std::max(time, maxtime);
    }

    // in case there's a situation where mintime == maxtime, we don't want a divide by zero exception.
    if (mintime == maxtime)
        return 0;

    arith_uint256 workdiff = pb->nchainwork - pb0->nchainwork;
    int64_t timediff = maxtime - mintime;

    return (int64_t)(workdiff.getdouble() / timediff);
}

univalue getnetworkhashps(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() > 2)
        throw runtime_error(
            "getnetworkhashps ( blocks height )\n"
            "\nreturns the estimated network hashes per second based on the last n blocks.\n"
            "pass in [blocks] to override # of blocks, -1 specifies since last difficulty change.\n"
            "pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\narguments:\n"
            "1. blocks     (numeric, optional, default=120) the number of blocks, or -1 for blocks since last difficulty change.\n"
            "2. height     (numeric, optional, default=-1) to estimate at the time of the given height.\n"
            "\nresult:\n"
            "x             (numeric) hashes per second estimated\n"
            "\nexamples:\n"
            + helpexamplecli("getnetworkhashps", "")
            + helpexamplerpc("getnetworkhashps", "")
       );

    lock(cs_main);
    return getnetworkhashps(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

#ifdef enable_wallet
univalue getgenerate(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\nreturn if the server is set to generate coins or not. the default is false.\n"
            "it is set with the command line argument -gen (or moorecoin.conf setting gen)\n"
            "it can also be set with the setgenerate call.\n"
            "\nresult\n"
            "true|false      (boolean) if the server is set to generate coins or not\n"
            "\nexamples:\n"
            + helpexamplecli("getgenerate", "")
            + helpexamplerpc("getgenerate", "")
        );

    lock(cs_main);
    return getboolarg("-gen", false);
}

univalue generate(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "generate numblocks\n"
            "\nmine blocks immediately (before the rpc call returns)\n"
            "\nnote: this function can only be used on the regtest network\n"
            "1. numblocks    (numeric) how many blocks are generated immediately.\n"
            "\nresult\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nexamples:\n"
            "\ngenerate 11 blocks\n"
            + helpexamplecli("generate", "11")
        );

    if (pwalletmain == null)
        throw jsonrpcerror(rpc_method_not_found, "method not found (disabled)");
    if (!params().mineblocksondemand())
        throw jsonrpcerror(rpc_method_not_found, "this method can only be used on regtest");

    int nheightstart = 0;
    int nheightend = 0;
    int nheight = 0;
    int ngenerate = params[0].get_int();
    creservekey reservekey(pwalletmain);

    {   // don't keep cs_main locked
        lock(cs_main);
        nheightstart = chainactive.height();
        nheight = nheightstart;
        nheightend = nheightstart+ngenerate;
    }
    unsigned int nextranonce = 0;
    univalue blockhashes(univalue::varr);
    while (nheight < nheightend)
    {
        auto_ptr<cblocktemplate> pblocktemplate(createnewblockwithkey(reservekey));
        if (!pblocktemplate.get())
            throw jsonrpcerror(rpc_internal_error, "wallet keypool empty");
        cblock *pblock = &pblocktemplate->block;
        {
            lock(cs_main);
            incrementextranonce(pblock, chainactive.tip(), nextranonce);
        }
        while (!checkproofofwork(pblock->gethash(), pblock->nbits, params().getconsensus())) {
            // yes, there is a chance every nonce could fail to satisfy the -regtest
            // target -- 1 in 2^(2^32). that ain't gonna happen.
            ++pblock->nnonce;
        }
        cvalidationstate state;
        if (!processnewblock(state, null, pblock, true, null))
            throw jsonrpcerror(rpc_internal_error, "processnewblock, block not accepted");
        ++nheight;
        blockhashes.push_back(pblock->gethash().gethex());
    }
    return blockhashes;
}


univalue setgenerate(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate generate ( genproclimit )\n"
            "\nset 'generate' true or false to turn generation on or off.\n"
            "generation is limited to 'genproclimit' processors, -1 is unlimited.\n"
            "see the getgenerate call for the current setting.\n"
            "\narguments:\n"
            "1. generate         (boolean, required) set to true to turn on generation, off to turn off.\n"
            "2. genproclimit     (numeric, optional) set the processor limit for when generation is on. can be -1 for unlimited.\n"
            "\nexamples:\n"
            "\nset the generation on with a limit of one processor\n"
            + helpexamplecli("setgenerate", "true 1") +
            "\ncheck the setting\n"
            + helpexamplecli("getgenerate", "") +
            "\nturn off generation\n"
            + helpexamplecli("setgenerate", "false") +
            "\nusing json rpc\n"
            + helpexamplerpc("setgenerate", "true, 1")
        );

    if (pwalletmain == null)
        throw jsonrpcerror(rpc_method_not_found, "method not found (disabled)");
    if (params().mineblocksondemand())
        throw jsonrpcerror(rpc_method_not_found, "use the generate method instead of setgenerate on this network");

    bool fgenerate = true;
    if (params.size() > 0)
        fgenerate = params[0].get_bool();

    int ngenproclimit = -1;
    if (params.size() > 1)
    {
        ngenproclimit = params[1].get_int();
        if (ngenproclimit == 0)
            fgenerate = false;
    }

    mapargs["-gen"] = (fgenerate ? "1" : "0");
    mapargs ["-genproclimit"] = itostr(ngenproclimit);
    generatemoorecoins(fgenerate, pwalletmain, ngenproclimit);

    return nullunivalue;
}
#endif


univalue getmininginfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\nreturns a json object containing mining-related information."
            "\nresult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) the current block\n"
            "  \"currentblocksize\": nnn,   (numeric) the last block size\n"
            "  \"currentblocktx\": nnn,     (numeric) the last block transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) the current difficulty\n"
            "  \"errors\": \"...\"          (string) current errors\n"
            "  \"generate\": true|false     (boolean) if the generation is on or off (see getgenerate or setgenerate calls)\n"
            "  \"genproclimit\": n          (numeric) the processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"pooledtx\": n              (numeric) the size of the mem pool\n"
            "  \"testnet\": true|false      (boolean) if using testnet or not\n"
            "  \"chain\": \"xxxx\",         (string) current network name as defined in bip70 (main, test, regtest)\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getmininginfo", "")
            + helpexamplerpc("getmininginfo", "")
        );


    lock(cs_main);

    univalue obj(univalue::vobj);
    obj.push_back(pair("blocks",           (int)chainactive.height()));
    obj.push_back(pair("currentblocksize", (uint64_t)nlastblocksize));
    obj.push_back(pair("currentblocktx",   (uint64_t)nlastblocktx));
    obj.push_back(pair("difficulty",       (double)getdifficulty()));
    obj.push_back(pair("errors",           getwarnings("statusbar")));
    obj.push_back(pair("genproclimit",     (int)getarg("-genproclimit", -1)));
    obj.push_back(pair("networkhashps",    getnetworkhashps(params, false)));
    obj.push_back(pair("pooledtx",         (uint64_t)mempool.size()));
    obj.push_back(pair("testnet",          params().testnettobedeprecatedfieldrpc()));
    obj.push_back(pair("chain",            params().networkidstring()));
#ifdef enable_wallet
    obj.push_back(pair("generate",         getgenerate(params, false)));
#endif
    return obj;
}


// note: unlike wallet rpc (which use btc values), mining rpcs follow gbt (bip 22) in using satoshi amounts
univalue prioritisetransaction(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 3)
        throw runtime_error(
            "prioritisetransaction <txid> <priority delta> <fee delta>\n"
            "accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\narguments:\n"
            "1. \"txid\"       (string, required) the transaction id.\n"
            "2. priority delta (numeric, required) the priority to add or subtract.\n"
            "                  the transaction selection algorithm considers the tx as it would have a higher priority.\n"
            "                  (priority of a transaction is calculated: coinage * value_in_satoshis / txsize) \n"
            "3. fee delta      (numeric, required) the fee value (in satoshis) to add (or subtract, if negative).\n"
            "                  the fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nresult\n"
            "true              (boolean) returns true\n"
            "\nexamples:\n"
            + helpexamplecli("prioritisetransaction", "\"txid\" 0.0 10000")
            + helpexamplerpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        );

    lock(cs_main);

    uint256 hash = parsehashstr(params[0].get_str(), "txid");
    camount namount = params[2].get_int64();

    mempool.prioritisetransaction(hash, params[0].get_str(), params[1].get_real(), namount);
    return true;
}


// note: assumes a conclusive result; if result is inconclusive, it must be handled by caller
static univalue bip22validationresult(const cvalidationstate& state)
{
    if (state.isvalid())
        return nullunivalue;

    std::string strrejectreason = state.getrejectreason();
    if (state.iserror())
        throw jsonrpcerror(rpc_verify_error, strrejectreason);
    if (state.isinvalid())
    {
        if (strrejectreason.empty())
            return "rejected";
        return strrejectreason;
    }
    // should be impossible
    return "valid?";
}

univalue getblocktemplate(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "getblocktemplate ( \"jsonrequestobject\" )\n"
            "\nif the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            "it returns data needed to construct a block to work on.\n"
            "see https://en.moorecoin.it/wiki/bip_0022 for full specification.\n"

            "\narguments:\n"
            "1. \"jsonrequestobject\"       (string, optional) a json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"    (string, optional) this must be set to \"template\" or omitted\n"
            "       \"capabilities\":[       (array, optional) a list of strings\n"
            "           \"support\"           (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "         ]\n"
            "     }\n"
            "\n"

            "\nresult:\n"
            "{\n"
            "  \"version\" : n,                    (numeric) the block version\n"
            "  \"previousblockhash\" : \"xxxx\",    (string) the hash of current highest block\n"
            "  \"transactions\" : [                (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",          (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"hash\" : \"xxxx\",          (string) hash/id encoded in little-endian hexadecimal\n"
            "         \"depends\" : [              (array) array of numbers \n"
            "             n                        (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                   (numeric) difference in value between transaction inputs and outputs (in satoshis); for coinbase transactions, this is a negative number of the total collected block fees (ie, not including the block subsidy); if key is not present, fee is unknown and clients must not assume there isn't one\n"
            "         \"sigops\" : n,               (numeric) total number of sigops, as counted for purposes of block limits; if key is not present, sigop count is unknown and clients must not assume there aren't any\n"
            "         \"required\" : true|false     (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
            "  \"coinbaseaux\" : {                  (json object) data that should be included in the coinbase's scriptsig content\n"
            "      \"flags\" : \"flags\"            (string) \n"
            "  },\n"
            "  \"coinbasevalue\" : n,               (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in satoshis)\n"
            "  \"coinbasetxn\" : { ... },           (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",               (string) the hash target\n"
            "  \"mintime\" : xxx,                   (numeric) the minimum timestamp appropriate for next block time in seconds since epoch (jan 1 1970 gmt)\n"
            "  \"mutable\" : [                      (array of string) list of ways the block template may be changed \n"
            "     \"value\"                         (string) a way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",   (string) a range of valid nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block size\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp in seconds since epoch (jan 1 1970 gmt)\n"
            "  \"bits\" : \"xxx\",                 (string) compressed target of next block\n"
            "  \"height\" : n                      (numeric) the height of the next block\n"
            "}\n"

            "\nexamples:\n"
            + helpexamplecli("getblocktemplate", "")
            + helpexamplerpc("getblocktemplate", "")
         );

    lock(cs_main);

    std::string strmode = "template";
    univalue lpval = nullunivalue;
    if (params.size() > 0)
    {
        const univalue& oparam = params[0].get_obj();
        const univalue& modeval = find_value(oparam, "mode");
        if (modeval.isstr())
            strmode = modeval.get_str();
        else if (modeval.isnull())
        {
            /* do nothing */
        }
        else
            throw jsonrpcerror(rpc_invalid_parameter, "invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strmode == "proposal")
        {
            const univalue& dataval = find_value(oparam, "data");
            if (!dataval.isstr())
                throw jsonrpcerror(rpc_type_error, "missing data string key for proposal");

            cblock block;
            if (!decodehexblk(block, dataval.get_str()))
                throw jsonrpcerror(rpc_deserialization_error, "block decode failed");

            uint256 hash = block.gethash();
            blockmap::iterator mi = mapblockindex.find(hash);
            if (mi != mapblockindex.end()) {
                cblockindex *pindex = mi->second;
                if (pindex->isvalid(block_valid_scripts))
                    return "duplicate";
                if (pindex->nstatus & block_failed_mask)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            cblockindex* const pindexprev = chainactive.tip();
            // testblockvalidity only supports blocks built on the current tip
            if (block.hashprevblock != pindexprev->getblockhash())
                return "inconclusive-not-best-prevblk";
            cvalidationstate state;
            testblockvalidity(state, block, pindexprev, false, true);
            return bip22validationresult(state);
        }
    }

    if (strmode != "template")
        throw jsonrpcerror(rpc_invalid_parameter, "invalid mode");

    if (vnodes.empty())
        throw jsonrpcerror(rpc_client_not_connected, "moorecoin is not connected!");

    if (isinitialblockdownload())
        throw jsonrpcerror(rpc_client_in_initial_download, "moorecoin is downloading blocks...");

    static unsigned int ntransactionsupdatedlast;

    if (!lpval.isnull())
    {
        // wait to respond until either the best block changes, or a minute has passed and there are more transactions
        uint256 hashwatchedchain;
        boost::system_time checktxtime;
        unsigned int ntransactionsupdatedlastlp;

        if (lpval.isstr())
        {
            // format: <hashbestchain><ntransactionsupdatedlast>
            std::string lpstr = lpval.get_str();

            hashwatchedchain.sethex(lpstr.substr(0, 64));
            ntransactionsupdatedlastlp = atoi64(lpstr.substr(64));
        }
        else
        {
            // note: spec does not specify behaviour for non-string longpollid, but this makes testing easier
            hashwatchedchain = chainactive.tip()->getblockhash();
            ntransactionsupdatedlastlp = ntransactionsupdatedlast;
        }

        // release the wallet and main lock while waiting
        leave_critical_section(cs_main);
        {
            checktxtime = boost::get_system_time() + boost::posix_time::minutes(1);

            boost::unique_lock<boost::mutex> lock(csbestblock);
            while (chainactive.tip()->getblockhash() == hashwatchedchain && isrpcrunning())
            {
                if (!cvblockchange.timed_wait(lock, checktxtime))
                {
                    // timeout: check transactions for update
                    if (mempool.gettransactionsupdated() != ntransactionsupdatedlastlp)
                        break;
                    checktxtime += boost::posix_time::seconds(10);
                }
            }
        }
        enter_critical_section(cs_main);

        if (!isrpcrunning())
            throw jsonrpcerror(rpc_client_not_connected, "shutting down");
        // todo: maybe recheck connections/ibd and (if something wrong) send an expires-immediately template to stop miners?
    }

    // update block
    static cblockindex* pindexprev;
    static int64_t nstart;
    static cblocktemplate* pblocktemplate;
    if (pindexprev != chainactive.tip() ||
        (mempool.gettransactionsupdated() != ntransactionsupdatedlast && gettime() - nstart > 5))
    {
        // clear pindexprev so future calls make a new block, despite any failures from here on
        pindexprev = null;

        // store the pindexbest used before createnewblock, to avoid races
        ntransactionsupdatedlast = mempool.gettransactionsupdated();
        cblockindex* pindexprevnew = chainactive.tip();
        nstart = gettime();

        // create new block
        if(pblocktemplate)
        {
            delete pblocktemplate;
            pblocktemplate = null;
        }
        cscript scriptdummy = cscript() << op_true;
        pblocktemplate = createnewblock(scriptdummy);
        if (!pblocktemplate)
            throw jsonrpcerror(rpc_out_of_memory, "out of memory");

        // need to update only after we know createnewblock succeeded
        pindexprev = pindexprevnew;
    }
    cblock* pblock = &pblocktemplate->block; // pointer for convenience

    // update ntime
    updatetime(pblock, params().getconsensus(), pindexprev);
    pblock->nnonce = 0;

    univalue acaps(univalue::varr); acaps.push_back("proposal");

    univalue transactions(univalue::varr);
    map<uint256, int64_t> settxindex;
    int i = 0;
    boost_foreach (const ctransaction& tx, pblock->vtx) {
        uint256 txhash = tx.gethash();
        settxindex[txhash] = i++;

        if (tx.iscoinbase())
            continue;

        univalue entry(univalue::vobj);

        entry.push_back(pair("data", encodehextx(tx)));

        entry.push_back(pair("hash", txhash.gethex()));

        univalue deps(univalue::varr);
        boost_foreach (const ctxin &in, tx.vin)
        {
            if (settxindex.count(in.prevout.hash))
                deps.push_back(settxindex[in.prevout.hash]);
        }
        entry.push_back(pair("depends", deps));

        int index_in_template = i - 1;
        entry.push_back(pair("fee", pblocktemplate->vtxfees[index_in_template]));
        entry.push_back(pair("sigops", pblocktemplate->vtxsigops[index_in_template]));

        transactions.push_back(entry);
    }

    univalue aux(univalue::vobj);
    aux.push_back(pair("flags", hexstr(coinbase_flags.begin(), coinbase_flags.end())));

    arith_uint256 hashtarget = arith_uint256().setcompact(pblock->nbits);

    static univalue amutable(univalue::varr);
    if (amutable.empty())
    {
        amutable.push_back("time");
        amutable.push_back("transactions");
        amutable.push_back("prevblock");
    }

    univalue result(univalue::vobj);
    result.push_back(pair("capabilities", acaps));
    result.push_back(pair("version", pblock->nversion));
    result.push_back(pair("previousblockhash", pblock->hashprevblock.gethex()));
    result.push_back(pair("transactions", transactions));
    result.push_back(pair("coinbaseaux", aux));
    result.push_back(pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nvalue));
    result.push_back(pair("longpollid", chainactive.tip()->getblockhash().gethex() + i64tostr(ntransactionsupdatedlast)));
    result.push_back(pair("target", hashtarget.gethex()));
    result.push_back(pair("mintime", (int64_t)pindexprev->getmediantimepast()+1));
    result.push_back(pair("mutable", amutable));
    result.push_back(pair("noncerange", "00000000ffffffff"));
    result.push_back(pair("sigoplimit", (int64_t)max_block_sigops));
    result.push_back(pair("sizelimit", (int64_t)max_block_size));
    result.push_back(pair("curtime", pblock->getblocktime()));
    result.push_back(pair("bits", strprintf("%08x", pblock->nbits)));
    result.push_back(pair("height", (int64_t)(pindexprev->nheight+1)));

    return result;
}

class submitblock_statecatcher : public cvalidationinterface
{
public:
    uint256 hash;
    bool found;
    cvalidationstate state;

    submitblock_statecatcher(const uint256 &hashin) : hash(hashin), found(false), state() {};

protected:
    virtual void blockchecked(const cblock& block, const cvalidationstate& statein) {
        if (block.gethash() != hash)
            return;
        found = true;
        state = statein;
    };
};

univalue submitblock(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "submitblock \"hexdata\" ( \"jsonparametersobject\" )\n"
            "\nattempts to submit new block to network.\n"
            "the 'jsonparametersobject' parameter is currently ignored.\n"
            "see https://en.moorecoin.it/wiki/bip_0022 for full specification.\n"

            "\narguments\n"
            "1. \"hexdata\"    (string, required) the hex-encoded block data to submit\n"
            "2. \"jsonparametersobject\"     (string, optional) object of optional parameters\n"
            "    {\n"
            "      \"workid\" : \"id\"    (string, optional) if the server provided a workid, it must be included with submissions\n"
            "    }\n"
            "\nresult:\n"
            "\nexamples:\n"
            + helpexamplecli("submitblock", "\"mydata\"")
            + helpexamplerpc("submitblock", "\"mydata\"")
        );

    cblock block;
    if (!decodehexblk(block, params[0].get_str()))
        throw jsonrpcerror(rpc_deserialization_error, "block decode failed");

    uint256 hash = block.gethash();
    bool fblockpresent = false;
    {
        lock(cs_main);
        blockmap::iterator mi = mapblockindex.find(hash);
        if (mi != mapblockindex.end()) {
            cblockindex *pindex = mi->second;
            if (pindex->isvalid(block_valid_scripts))
                return "duplicate";
            if (pindex->nstatus & block_failed_mask)
                return "duplicate-invalid";
            // otherwise, we might only have the header - process the block before returning
            fblockpresent = true;
        }
    }

    cvalidationstate state;
    submitblock_statecatcher sc(block.gethash());
    registervalidationinterface(&sc);
    bool faccepted = processnewblock(state, null, &block, true, null);
    unregistervalidationinterface(&sc);
    if (fblockpresent)
    {
        if (faccepted && !sc.found)
            return "duplicate-inconclusive";
        return "duplicate";
    }
    if (faccepted)
    {
        if (!sc.found)
            return "inconclusive";
        state = sc.state;
    }
    return bip22validationresult(state);
}

univalue estimatefee(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "estimatefee nblocks\n"
            "\nestimates the approximate fee per kilobyte\n"
            "needed for a transaction to begin confirmation\n"
            "within nblocks blocks.\n"
            "\narguments:\n"
            "1. nblocks     (numeric)\n"
            "\nresult:\n"
            "n :    (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nexample:\n"
            + helpexamplecli("estimatefee", "6")
            );

    rpctypecheck(params, boost::assign::list_of(univalue::vnum));

    int nblocks = params[0].get_int();
    if (nblocks < 1)
        nblocks = 1;

    cfeerate feerate = mempool.estimatefee(nblocks);
    if (feerate == cfeerate(0))
        return -1.0;

    return valuefromamount(feerate.getfeeperk());
}

univalue estimatepriority(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "estimatepriority nblocks\n"
            "\nestimates the approximate priority\n"
            "a zero-fee transaction needs to begin confirmation\n"
            "within nblocks blocks.\n"
            "\narguments:\n"
            "1. nblocks     (numeric)\n"
            "\nresult:\n"
            "n :    (numeric) estimated priority\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nexample:\n"
            + helpexamplecli("estimatepriority", "6")
            );

    rpctypecheck(params, boost::assign::list_of(univalue::vnum));

    int nblocks = params[0].get_int();
    if (nblocks < 1)
        nblocks = 1;

    return mempool.estimatepriority(nblocks);
}
