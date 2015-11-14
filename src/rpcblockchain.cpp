// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"
#include "consensus/validation.h"
#include "main.h"
#include "primitives/transaction.h"
#include "rpcserver.h"
#include "sync.h"
#include "util.h"

#include <stdint.h>

#include "univalue/univalue.h"

using namespace std;

extern void txtojson(const ctransaction& tx, const uint256 hashblock, univalue& entry);
void scriptpubkeytojson(const cscript& scriptpubkey, univalue& out, bool fincludehex);

double getdifficulty(const cblockindex* blockindex)
{
    // floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.
    if (blockindex == null)
    {
        if (chainactive.tip() == null)
            return 1.0;
        else
            blockindex = chainactive.tip();
    }

    int nshift = (blockindex->nbits >> 24) & 0xff;

    double ddiff =
        (double)0x0000ffff / (double)(blockindex->nbits & 0x00ffffff);

    while (nshift < 29)
    {
        ddiff *= 256.0;
        nshift++;
    }
    while (nshift > 29)
    {
        ddiff /= 256.0;
        nshift--;
    }

    return ddiff;
}


univalue blocktojson(const cblock& block, const cblockindex* blockindex, bool txdetails = false)
{
    univalue result(univalue::vobj);
    result.push_back(pair("hash", block.gethash().gethex()));
    int confirmations = -1;
    // only report confirmations if the block is on the main chain
    if (chainactive.contains(blockindex))
        confirmations = chainactive.height() - blockindex->nheight + 1;
    result.push_back(pair("confirmations", confirmations));
    result.push_back(pair("size", (int)::getserializesize(block, ser_network, protocol_version)));
    result.push_back(pair("height", blockindex->nheight));
    result.push_back(pair("version", block.nversion));
    result.push_back(pair("merkleroot", block.hashmerkleroot.gethex()));
    univalue txs(univalue::varr);
    boost_foreach(const ctransaction&tx, block.vtx)
    {
        if(txdetails)
        {
            univalue objtx(univalue::vobj);
            txtojson(tx, uint256(), objtx);
            txs.push_back(objtx);
        }
        else
            txs.push_back(tx.gethash().gethex());
    }
    result.push_back(pair("tx", txs));
    result.push_back(pair("time", block.getblocktime()));
    result.push_back(pair("nonce", (uint64_t)block.nnonce));
    result.push_back(pair("bits", strprintf("%08x", block.nbits)));
    result.push_back(pair("difficulty", getdifficulty(blockindex)));
    result.push_back(pair("chainwork", blockindex->nchainwork.gethex()));

    if (blockindex->pprev)
        result.push_back(pair("previousblockhash", blockindex->pprev->getblockhash().gethex()));
    cblockindex *pnext = chainactive.next(blockindex);
    if (pnext)
        result.push_back(pair("nextblockhash", pnext->getblockhash().gethex()));
    return result;
}


univalue getblockcount(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "\nreturns the number of blocks in the longest block chain.\n"
            "\nresult:\n"
            "n    (numeric) the current block count\n"
            "\nexamples:\n"
            + helpexamplecli("getblockcount", "")
            + helpexamplerpc("getblockcount", "")
        );

    lock(cs_main);
    return chainactive.height();
}

univalue getbestblockhash(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getbestblockhash\n"
            "\nreturns the hash of the best (tip) block in the longest block chain.\n"
            "\nresult\n"
            "\"hex\"      (string) the block hash hex encoded\n"
            "\nexamples\n"
            + helpexamplecli("getbestblockhash", "")
            + helpexamplerpc("getbestblockhash", "")
        );

    lock(cs_main);
    return chainactive.tip()->getblockhash().gethex();
}

univalue getdifficulty(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "\nreturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nresult:\n"
            "n.nnn       (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nexamples:\n"
            + helpexamplecli("getdifficulty", "")
            + helpexamplerpc("getdifficulty", "")
        );

    lock(cs_main);
    return getdifficulty();
}


univalue getrawmempool(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "getrawmempool ( verbose )\n"
            "\nreturns all transaction ids in memory pool as a json array of string transaction ids.\n"
            "\narguments:\n"
            "1. verbose           (boolean, optional, default=false) true for a json object, false for array of transaction ids\n"
            "\nresult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) the transaction id\n"
            "  ,...\n"
            "]\n"
            "\nresult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            "    \"size\" : n,             (numeric) transaction size in bytes\n"
            "    \"fee\" : n,              (numeric) transaction fee in moorecoins\n"
            "    \"time\" : n,             (numeric) local time transaction entered pool in seconds since 1 jan 1970 gmt\n"
            "    \"height\" : n,           (numeric) block height when transaction entered pool\n"
            "    \"startingpriority\" : n, (numeric) priority when transaction entered pool\n"
            "    \"currentpriority\" : n,  (numeric) transaction priority now\n"
            "    \"depends\" : [           (array) unconfirmed transactions used as inputs for this transaction\n"
            "        \"transactionid\",    (string) parent transaction id\n"
            "       ... ]\n"
            "  }, ...\n"
            "}\n"
            "\nexamples\n"
            + helpexamplecli("getrawmempool", "true")
            + helpexamplerpc("getrawmempool", "true")
        );

    lock(cs_main);

    bool fverbose = false;
    if (params.size() > 0)
        fverbose = params[0].get_bool();

    if (fverbose)
    {
        lock(mempool.cs);
        univalue o(univalue::vobj);
        boost_foreach(const pairtype(uint256, ctxmempoolentry)& entry, mempool.maptx)
        {
            const uint256& hash = entry.first;
            const ctxmempoolentry& e = entry.second;
            univalue info(univalue::vobj);
            info.push_back(pair("size", (int)e.gettxsize()));
            info.push_back(pair("fee", valuefromamount(e.getfee())));
            info.push_back(pair("time", e.gettime()));
            info.push_back(pair("height", (int)e.getheight()));
            info.push_back(pair("startingpriority", e.getpriority(e.getheight())));
            info.push_back(pair("currentpriority", e.getpriority(chainactive.height())));
            const ctransaction& tx = e.gettx();
            set<string> setdepends;
            boost_foreach(const ctxin& txin, tx.vin)
            {
                if (mempool.exists(txin.prevout.hash))
                    setdepends.insert(txin.prevout.hash.tostring());
            }

            univalue depends(univalue::varr);
            boost_foreach(const string& dep, setdepends)
            {
                depends.push_back(dep);
            }

            info.push_back(pair("depends", depends));
            o.push_back(pair(hash.tostring(), info));
        }
        return o;
    }
    else
    {
        vector<uint256> vtxid;
        mempool.queryhashes(vtxid);

        univalue a(univalue::varr);
        boost_foreach(const uint256& hash, vtxid)
            a.push_back(hash.tostring());

        return a;
    }
}

univalue getblockhash(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "getblockhash index\n"
            "\nreturns hash of block in best-block-chain at index provided.\n"
            "\narguments:\n"
            "1. index         (numeric, required) the block index\n"
            "\nresult:\n"
            "\"hash\"         (string) the block hash\n"
            "\nexamples:\n"
            + helpexamplecli("getblockhash", "1000")
            + helpexamplerpc("getblockhash", "1000")
        );

    lock(cs_main);

    int nheight = params[0].get_int();
    if (nheight < 0 || nheight > chainactive.height())
        throw jsonrpcerror(rpc_invalid_parameter, "block height out of range");

    cblockindex* pblockindex = chainactive[nheight];
    return pblockindex->getblockhash().gethex();
}

univalue getblock(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getblock \"hash\" ( verbose )\n"
            "\nif verbose is false, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
            "if verbose is true, returns an object with information about block <hash>.\n"
            "\narguments:\n"
            "1. \"hash\"          (string, required) the block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"
            "\nresult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) the number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) the block size\n"
            "  \"height\" : n,          (numeric) the block height or index\n"
            "  \"version\" : n,         (numeric) the block version\n"
            "  \"merkleroot\" : \"xxxx\", (string) the merkle root\n"
            "  \"tx\" : [               (array of string) the transaction ids\n"
            "     \"transactionid\"     (string) the transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) the block time in seconds since epoch (jan 1 1970 gmt)\n"
            "  \"nonce\" : n,           (numeric) the nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) the bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) the difficulty\n"
            "  \"previousblockhash\" : \"hash\",  (string) the hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) the hash of the next block\n"
            "}\n"
            "\nresult (for verbose=false):\n"
            "\"data\"             (string) a string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nexamples:\n"
            + helpexamplecli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + helpexamplerpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        );

    lock(cs_main);

    std::string strhash = params[0].get_str();
    uint256 hash(uint256s(strhash));

    bool fverbose = true;
    if (params.size() > 1)
        fverbose = params[1].get_bool();

    if (mapblockindex.count(hash) == 0)
        throw jsonrpcerror(rpc_invalid_address_or_key, "block not found");

    cblock block;
    cblockindex* pblockindex = mapblockindex[hash];

    if (fhavepruned && !(pblockindex->nstatus & block_have_data) && pblockindex->ntx > 0)
        throw jsonrpcerror(rpc_internal_error, "block not available (pruned data)");

    if(!readblockfromdisk(block, pblockindex))
        throw jsonrpcerror(rpc_internal_error, "can't read block from disk");

    if (!fverbose)
    {
        cdatastream ssblock(ser_network, protocol_version);
        ssblock << block;
        std::string strhex = hexstr(ssblock.begin(), ssblock.end());
        return strhex;
    }

    return blocktojson(block, pblockindex);
}

univalue gettxoutsetinfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "gettxoutsetinfo\n"
            "\nreturns statistics about the unspent transaction output set.\n"
            "note this call may take some time.\n"
            "\nresult:\n"
            "{\n"
            "  \"height\":n,     (numeric) the current block height (index)\n"
            "  \"bestblock\": \"hex\",   (string) the best block hash hex\n"
            "  \"transactions\": n,      (numeric) the number of transactions\n"
            "  \"txouts\": n,            (numeric) the number of output transactions\n"
            "  \"bytes_serialized\": n,  (numeric) the serialized size\n"
            "  \"hash_serialized\": \"hash\",   (string) the serialized hash\n"
            "  \"total_amount\": x.xxx          (numeric) the total amount\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("gettxoutsetinfo", "")
            + helpexamplerpc("gettxoutsetinfo", "")
        );

    univalue ret(univalue::vobj);

    ccoinsstats stats;
    flushstatetodisk();
    if (pcoinstip->getstats(stats)) {
        ret.push_back(pair("height", (int64_t)stats.nheight));
        ret.push_back(pair("bestblock", stats.hashblock.gethex()));
        ret.push_back(pair("transactions", (int64_t)stats.ntransactions));
        ret.push_back(pair("txouts", (int64_t)stats.ntransactionoutputs));
        ret.push_back(pair("bytes_serialized", (int64_t)stats.nserializedsize));
        ret.push_back(pair("hash_serialized", stats.hashserialized.gethex()));
        ret.push_back(pair("total_amount", valuefromamount(stats.ntotalamount)));
    }
    return ret;
}

univalue gettxout(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            "gettxout \"txid\" n ( includemempool )\n"
            "\nreturns details about an unspent transaction output.\n"
            "\narguments:\n"
            "1. \"txid\"       (string, required) the transaction id\n"
            "2. n              (numeric, required) vout value\n"
            "3. includemempool  (boolean, optional) whether to included the mem pool\n"
            "\nresult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",    (string) the block hash\n"
            "  \"confirmations\" : n,       (numeric) the number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) the transaction value in btc\n"
            "  \"scriptpubkey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqsigs\" : n,          (numeric) number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) the type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of moorecoin addresses\n"
            "        \"moorecoinaddress\"     (string) moorecoin address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"version\" : n,            (numeric) the version\n"
            "  \"coinbase\" : true|false   (boolean) coinbase or not\n"
            "}\n"

            "\nexamples:\n"
            "\nget unspent transactions\n"
            + helpexamplecli("listunspent", "") +
            "\nview the details\n"
            + helpexamplecli("gettxout", "\"txid\" 1") +
            "\nas a json rpc call\n"
            + helpexamplerpc("gettxout", "\"txid\", 1")
        );

    lock(cs_main);

    univalue ret(univalue::vobj);

    std::string strhash = params[0].get_str();
    uint256 hash(uint256s(strhash));
    int n = params[1].get_int();
    bool fmempool = true;
    if (params.size() > 2)
        fmempool = params[2].get_bool();

    ccoins coins;
    if (fmempool) {
        lock(mempool.cs);
        ccoinsviewmempool view(pcoinstip, mempool);
        if (!view.getcoins(hash, coins))
            return nullunivalue;
        mempool.prunespent(hash, coins); // todo: this should be done by the ccoinsviewmempool
    } else {
        if (!pcoinstip->getcoins(hash, coins))
            return nullunivalue;
    }
    if (n<0 || (unsigned int)n>=coins.vout.size() || coins.vout[n].isnull())
        return nullunivalue;

    blockmap::iterator it = mapblockindex.find(pcoinstip->getbestblock());
    cblockindex *pindex = it->second;
    ret.push_back(pair("bestblock", pindex->getblockhash().gethex()));
    if ((unsigned int)coins.nheight == mempool_height)
        ret.push_back(pair("confirmations", 0));
    else
        ret.push_back(pair("confirmations", pindex->nheight - coins.nheight + 1));
    ret.push_back(pair("value", valuefromamount(coins.vout[n].nvalue)));
    univalue o(univalue::vobj);
    scriptpubkeytojson(coins.vout[n].scriptpubkey, o, true);
    ret.push_back(pair("scriptpubkey", o));
    ret.push_back(pair("version", coins.nversion));
    ret.push_back(pair("coinbase", coins.fcoinbase));

    return ret;
}

univalue verifychain(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() > 2)
        throw runtime_error(
            "verifychain ( checklevel numblocks )\n"
            "\nverifies blockchain database.\n"
            "\narguments:\n"
            "1. checklevel   (numeric, optional, 0-4, default=3) how thorough the block verification is.\n"
            "2. numblocks    (numeric, optional, default=288, 0=all) the number of blocks to check.\n"
            "\nresult:\n"
            "true|false       (boolean) verified or not\n"
            "\nexamples:\n"
            + helpexamplecli("verifychain", "")
            + helpexamplerpc("verifychain", "")
        );

    lock(cs_main);

    int nchecklevel = getarg("-checklevel", 3);
    int ncheckdepth = getarg("-checkblocks", 288);
    if (params.size() > 0)
        nchecklevel = params[0].get_int();
    if (params.size() > 1)
        ncheckdepth = params[1].get_int();

    return cverifydb().verifydb(pcoinstip, nchecklevel, ncheckdepth);
}

univalue getblockchaininfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getblockchaininfo\n"
            "returns an object containing various state info regarding block chain processing.\n"
            "\nresult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",        (string) current network name as defined in bip70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,         (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,        (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\", (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,     (numeric) the current difficulty\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1]\n"
            "  \"chainwork\": \"xxxx\"     (string) total amount of work in active chain, in hexadecimal\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getblockchaininfo", "")
            + helpexamplerpc("getblockchaininfo", "")
        );

    lock(cs_main);

    univalue obj(univalue::vobj);
    obj.push_back(pair("chain",                 params().networkidstring()));
    obj.push_back(pair("blocks",                (int)chainactive.height()));
    obj.push_back(pair("headers",               pindexbestheader ? pindexbestheader->nheight : -1));
    obj.push_back(pair("bestblockhash",         chainactive.tip()->getblockhash().gethex()));
    obj.push_back(pair("difficulty",            (double)getdifficulty()));
    obj.push_back(pair("verificationprogress",  checkpoints::guessverificationprogress(params().checkpoints(), chainactive.tip())));
    obj.push_back(pair("chainwork",             chainactive.tip()->nchainwork.gethex()));
    obj.push_back(pair("pruned",                fprunemode));
    if (fprunemode)
    {
        cblockindex *block = chainactive.tip();
        while (block && block->pprev && (block->pprev->nstatus & block_have_data))
            block = block->pprev;

        obj.push_back(pair("pruneheight",        block->nheight));
    }
    return obj;
}

/** comparison function for sorting the getchaintips heads.  */
struct compareblocksbyheight
{
    bool operator()(const cblockindex* a, const cblockindex* b) const
    {
        /* make sure that unequal blocks with the same height do not compare
           equal. use the pointers themselves to make a distinction. */

        if (a->nheight != b->nheight)
          return (a->nheight > b->nheight);

        return a < b;
    }
};

univalue getchaintips(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getchaintips\n"
            "return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"
            "\nresult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "possible values for status:\n"
            "1.  \"invalid\"               this branch contains at least one invalid block\n"
            "2.  \"headers-only\"          not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         all blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            this branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                this is the tip of the active main chain, which is certainly valid\n"
            "\nexamples:\n"
            + helpexamplecli("getchaintips", "")
            + helpexamplerpc("getchaintips", "")
        );

    lock(cs_main);

    /* build up a list of chain tips.  we start with the list of all
       known blocks, and successively remove blocks that appear as pprev
       of another block.  */
    std::set<const cblockindex*, compareblocksbyheight> settips;
    boost_foreach(const pairtype(const uint256, cblockindex*)& item, mapblockindex)
        settips.insert(item.second);
    boost_foreach(const pairtype(const uint256, cblockindex*)& item, mapblockindex)
    {
        const cblockindex* pprev = item.second->pprev;
        if (pprev)
            settips.erase(pprev);
    }

    // always report the currently active tip.
    settips.insert(chainactive.tip());

    /* construct the output array.  */
    univalue res(univalue::varr);
    boost_foreach(const cblockindex* block, settips)
    {
        univalue obj(univalue::vobj);
        obj.push_back(pair("height", block->nheight));
        obj.push_back(pair("hash", block->phashblock->gethex()));

        const int branchlen = block->nheight - chainactive.findfork(block)->nheight;
        obj.push_back(pair("branchlen", branchlen));

        string status;
        if (chainactive.contains(block)) {
            // this block is part of the currently active chain.
            status = "active";
        } else if (block->nstatus & block_failed_mask) {
            // this block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nchaintx == 0) {
            // this block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->isvalid(block_valid_scripts)) {
            // this block is fully validated, but no longer part of the active chain. it was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->isvalid(block_valid_tree)) {
            // the headers for this block are valid, but it has not been validated. it was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // no clue.
            status = "unknown";
        }
        obj.push_back(pair("status", status));

        res.push_back(obj);
    }

    return res;
}

univalue getmempoolinfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getmempoolinfo\n"
            "\nreturns details on the active state of the tx memory pool.\n"
            "\nresult:\n"
            "{\n"
            "  \"size\": xxxxx                (numeric) current tx count\n"
            "  \"bytes\": xxxxx               (numeric) sum of all tx sizes\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getmempoolinfo", "")
            + helpexamplerpc("getmempoolinfo", "")
        );

    univalue ret(univalue::vobj);
    ret.push_back(pair("size", (int64_t) mempool.size()));
    ret.push_back(pair("bytes", (int64_t) mempool.gettotaltxsize()));

    return ret;
}

univalue invalidateblock(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "invalidateblock \"hash\"\n"
            "\npermanently marks a block as invalid, as if it violated a consensus rule.\n"
            "\narguments:\n"
            "1. hash   (string, required) the hash of the block to mark as invalid\n"
            "\nresult:\n"
            "\nexamples:\n"
            + helpexamplecli("invalidateblock", "\"blockhash\"")
            + helpexamplerpc("invalidateblock", "\"blockhash\"")
        );

    std::string strhash = params[0].get_str();
    uint256 hash(uint256s(strhash));
    cvalidationstate state;

    {
        lock(cs_main);
        if (mapblockindex.count(hash) == 0)
            throw jsonrpcerror(rpc_invalid_address_or_key, "block not found");

        cblockindex* pblockindex = mapblockindex[hash];
        invalidateblock(state, pblockindex);
    }

    if (state.isvalid()) {
        activatebestchain(state);
    }

    if (!state.isvalid()) {
        throw jsonrpcerror(rpc_database_error, state.getrejectreason());
    }

    return nullunivalue;
}

univalue reconsiderblock(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "reconsiderblock \"hash\"\n"
            "\nremoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "this can be used to undo the effects of invalidateblock.\n"
            "\narguments:\n"
            "1. hash   (string, required) the hash of the block to reconsider\n"
            "\nresult:\n"
            "\nexamples:\n"
            + helpexamplecli("reconsiderblock", "\"blockhash\"")
            + helpexamplerpc("reconsiderblock", "\"blockhash\"")
        );

    std::string strhash = params[0].get_str();
    uint256 hash(uint256s(strhash));
    cvalidationstate state;

    {
        lock(cs_main);
        if (mapblockindex.count(hash) == 0)
            throw jsonrpcerror(rpc_invalid_address_or_key, "block not found");

        cblockindex* pblockindex = mapblockindex[hash];
        reconsiderblock(state, pblockindex);
    }

    if (state.isvalid()) {
        activatebestchain(state);
    }

    if (!state.isvalid()) {
        throw jsonrpcerror(rpc_database_error, state.getrejectreason());
    }

    return nullunivalue;
}
