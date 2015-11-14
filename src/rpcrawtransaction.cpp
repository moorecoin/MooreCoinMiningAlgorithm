// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2015 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "init.h"
#include "keystore.h"
#include "main.h"
#include "merkleblock.h"
#include "net.h"
#include "primitives/transaction.h"
#include "rpcserver.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/standard.h"
#include "uint256.h"
#ifdef enable_wallet
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "univalue/univalue.h"

using namespace std;

void scriptpubkeytojson(const cscript& scriptpubkey, univalue& out, bool fincludehex)
{
    txnouttype type;
    vector<ctxdestination> addresses;
    int nrequired;

    out.push_back(pair("asm", scriptpubkey.tostring()));
    if (fincludehex)
        out.push_back(pair("hex", hexstr(scriptpubkey.begin(), scriptpubkey.end())));

    if (!extractdestinations(scriptpubkey, type, addresses, nrequired)) {
        out.push_back(pair("type", gettxnoutputtype(type)));
        return;
    }

    out.push_back(pair("reqsigs", nrequired));
    out.push_back(pair("type", gettxnoutputtype(type)));

    univalue a(univalue::varr);
    boost_foreach(const ctxdestination& addr, addresses)
        a.push_back(cmoorecoinaddress(addr).tostring());
    out.push_back(pair("addresses", a));
}

void txtojson(const ctransaction& tx, const uint256 hashblock, univalue& entry)
{
    entry.push_back(pair("txid", tx.gethash().gethex()));
    entry.push_back(pair("version", tx.nversion));
    entry.push_back(pair("locktime", (int64_t)tx.nlocktime));
    univalue vin(univalue::varr);
    boost_foreach(const ctxin& txin, tx.vin) {
        univalue in(univalue::vobj);
        if (tx.iscoinbase())
            in.push_back(pair("coinbase", hexstr(txin.scriptsig.begin(), txin.scriptsig.end())));
        else {
            in.push_back(pair("txid", txin.prevout.hash.gethex()));
            in.push_back(pair("vout", (int64_t)txin.prevout.n));
            univalue o(univalue::vobj);
            o.push_back(pair("asm", txin.scriptsig.tostring()));
            o.push_back(pair("hex", hexstr(txin.scriptsig.begin(), txin.scriptsig.end())));
            in.push_back(pair("scriptsig", o));
        }
        in.push_back(pair("sequence", (int64_t)txin.nsequence));
        vin.push_back(in);
    }
    entry.push_back(pair("vin", vin));
    univalue vout(univalue::varr);
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const ctxout& txout = tx.vout[i];
        univalue out(univalue::vobj);
        out.push_back(pair("value", valuefromamount(txout.nvalue)));
        out.push_back(pair("n", (int64_t)i));
        univalue o(univalue::vobj);
        scriptpubkeytojson(txout.scriptpubkey, o, true);
        out.push_back(pair("scriptpubkey", o));
        vout.push_back(out);
    }
    entry.push_back(pair("vout", vout));

    if (!hashblock.isnull()) {
        entry.push_back(pair("blockhash", hashblock.gethex()));
        blockmap::iterator mi = mapblockindex.find(hashblock);
        if (mi != mapblockindex.end() && (*mi).second) {
            cblockindex* pindex = (*mi).second;
            if (chainactive.contains(pindex)) {
                entry.push_back(pair("confirmations", 1 + chainactive.height() - pindex->nheight));
                entry.push_back(pair("time", pindex->getblocktime()));
                entry.push_back(pair("blocktime", pindex->getblocktime()));
            }
            else
                entry.push_back(pair("confirmations", 0));
        }
    }
}

univalue getrawtransaction(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getrawtransaction \"txid\" ( verbose )\n"
            "\nnote: by default this function only works sometimes. this is when the tx is in the mempool\n"
            "or there is an unspent output in the utxo for this transaction. to make it always work,\n"
            "you need to maintain a transaction index, using the -txindex command line option.\n"
            "\nreturn the raw transaction data.\n"
            "\nif verbose=0, returns a string that is serialized, hex-encoded data for 'txid'.\n"
            "if verbose is non-zero, returns an object with information about 'txid'.\n"

            "\narguments:\n"
            "1. \"txid\"      (string, required) the transaction id\n"
            "2. verbose       (numeric, optional, default=0) if 0, return a string, other return a json object\n"

            "\nresult (if verbose is not set or set to 0):\n"
            "\"data\"      (string) the serialized, hex-encoded data for 'txid'\n"

            "\nresult (if verbose > 0):\n"
            "{\n"
            "  \"hex\" : \"data\",       (string) the serialized, hex-encoded data for 'txid'\n"
            "  \"txid\" : \"id\",        (string) the transaction id (same as provided)\n"
            "  \"version\" : n,          (numeric) the version\n"
            "  \"locktime\" : ttt,       (numeric) the lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) the transaction id\n"
            "       \"vout\": n,         (numeric) \n"
            "       \"scriptsig\": {     (json object) the script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n      (numeric) the script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [              (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) the value in btc\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptpubkey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqsigs\" : n,            (numeric) the required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) the type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"moorecoinaddress\"        (string) moorecoin address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"blockhash\" : \"hash\",   (string) the block hash\n"
            "  \"confirmations\" : n,      (numeric) the confirmations\n"
            "  \"time\" : ttt,             (numeric) the transaction time in seconds since epoch (jan 1 1970 gmt)\n"
            "  \"blocktime\" : ttt         (numeric) the block time in seconds since epoch (jan 1 1970 gmt)\n"
            "}\n"

            "\nexamples:\n"
            + helpexamplecli("getrawtransaction", "\"mytxid\"")
            + helpexamplecli("getrawtransaction", "\"mytxid\" 1")
            + helpexamplerpc("getrawtransaction", "\"mytxid\", 1")
        );

    lock(cs_main);

    uint256 hash = parsehashv(params[0], "parameter 1");

    bool fverbose = false;
    if (params.size() > 1)
        fverbose = (params[1].get_int() != 0);

    ctransaction tx;
    uint256 hashblock;
    if (!gettransaction(hash, tx, hashblock, true))
        throw jsonrpcerror(rpc_invalid_address_or_key, "no information available about transaction");

    string strhex = encodehextx(tx);

    if (!fverbose)
        return strhex;

    univalue result(univalue::vobj);
    result.push_back(pair("hex", strhex));
    txtojson(tx, hashblock, result);
    return result;
}

univalue gettxoutproof(const univalue& params, bool fhelp)
{
    if (fhelp || (params.size() != 1 && params.size() != 2))
        throw runtime_error(
            "gettxoutproof [\"txid\",...] ( blockhash )\n"
            "\nreturns a hex-encoded proof that \"txid\" was included in a block.\n"
            "\nnote: by default this function only works sometimes. this is when there is an\n"
            "unspent output in the utxo for this transaction. to make it always work,\n"
            "you need to maintain a transaction index, using the -txindex command line option or\n"
            "specify the block in which the transaction is included in manually (by blockhash).\n"
            "\nreturn the raw transaction data.\n"
            "\narguments:\n"
            "1. \"txids\"       (string) a json array of txids to filter\n"
            "    [\n"
            "      \"txid\"     (string) a transaction hash\n"
            "      ,...\n"
            "    ]\n"
            "2. \"block hash\"  (string, optional) if specified, looks for txid in the block with this hash\n"
            "\nresult:\n"
            "\"data\"           (string) a string that is a serialized, hex-encoded data for the proof.\n"
        );

    set<uint256> settxids;
    uint256 onetxid;
    univalue txids = params[0].get_array();
    for (unsigned int idx = 0; idx < txids.size(); idx++) {
        const univalue& txid = txids[idx];
        if (txid.get_str().length() != 64 || !ishex(txid.get_str()))
            throw jsonrpcerror(rpc_invalid_parameter, string("invalid txid ")+txid.get_str());
        uint256 hash(uint256s(txid.get_str()));
        if (settxids.count(hash))
            throw jsonrpcerror(rpc_invalid_parameter, string("invalid parameter, duplicated txid: ")+txid.get_str());
       settxids.insert(hash);
       onetxid = hash;
    }

    lock(cs_main);

    cblockindex* pblockindex = null;

    uint256 hashblock;
    if (params.size() > 1)
    {
        hashblock = uint256s(params[1].get_str());
        if (!mapblockindex.count(hashblock))
            throw jsonrpcerror(rpc_invalid_address_or_key, "block not found");
        pblockindex = mapblockindex[hashblock];
    } else {
        ccoins coins;
        if (pcoinstip->getcoins(onetxid, coins) && coins.nheight > 0 && coins.nheight <= chainactive.height())
            pblockindex = chainactive[coins.nheight];
    }

    if (pblockindex == null)
    {
        ctransaction tx;
        if (!gettransaction(onetxid, tx, hashblock, false) || hashblock.isnull())
            throw jsonrpcerror(rpc_invalid_address_or_key, "transaction not yet in block");
        if (!mapblockindex.count(hashblock))
            throw jsonrpcerror(rpc_internal_error, "transaction index corrupt");
        pblockindex = mapblockindex[hashblock];
    }

    cblock block;
    if(!readblockfromdisk(block, pblockindex))
        throw jsonrpcerror(rpc_internal_error, "can't read block from disk");

    unsigned int ntxfound = 0;
    boost_foreach(const ctransaction&tx, block.vtx)
        if (settxids.count(tx.gethash()))
            ntxfound++;
    if (ntxfound != settxids.size())
        throw jsonrpcerror(rpc_invalid_address_or_key, "(not all) transactions not found in specified block");

    cdatastream ssmb(ser_network, protocol_version);
    cmerkleblock mb(block, settxids);
    ssmb << mb;
    std::string strhex = hexstr(ssmb.begin(), ssmb.end());
    return strhex;
}

univalue verifytxoutproof(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "verifytxoutproof \"proof\"\n"
            "\nverifies that a proof points to a transaction in a block, returning the transaction it commits to\n"
            "and throwing an rpc error if the block is not in our best chain\n"
            "\narguments:\n"
            "1. \"proof\"    (string, required) the hex-encoded proof generated by gettxoutproof\n"
            "\nresult:\n"
            "[\"txid\"]      (array, strings) the txid(s) which the proof commits to, or empty array if the proof is invalid\n"
        );

    cdatastream ssmb(parsehexv(params[0], "proof"), ser_network, protocol_version);
    cmerkleblock merkleblock;
    ssmb >> merkleblock;

    univalue res(univalue::varr);

    vector<uint256> vmatch;
    if (merkleblock.txn.extractmatches(vmatch) != merkleblock.header.hashmerkleroot)
        return res;

    lock(cs_main);

    if (!mapblockindex.count(merkleblock.header.gethash()) || !chainactive.contains(mapblockindex[merkleblock.header.gethash()]))
        throw jsonrpcerror(rpc_invalid_address_or_key, "block not found in chain");

    boost_foreach(const uint256& hash, vmatch)
        res.push_back(hash.gethex());
    return res;
}

univalue createrawtransaction(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 2)
        throw runtime_error(
            "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,...}\n"
            "\ncreate a transaction spending the given inputs and sending to the given addresses.\n"
            "returns hex-encoded raw transaction.\n"
            "note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.\n"

            "\narguments:\n"
            "1. \"transactions\"        (string, required) a json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",  (string, required) the transaction id\n"
            "         \"vout\":n        (numeric, required) the output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "2. \"addresses\"           (string, required) a json object with addresses as keys and amounts as values\n"
            "    {\n"
            "      \"address\": x.xxx   (numeric, required) the key is the moorecoin address, the value is the btc amount\n"
            "      ,...\n"
            "    }\n"

            "\nresult:\n"
            "\"transaction\"            (string) hex string of the transaction\n"

            "\nexamples\n"
            + helpexamplecli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"")
            + helpexamplerpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01}\"")
        );

    lock(cs_main);
    rpctypecheck(params, boost::assign::list_of(univalue::varr)(univalue::vobj));

    univalue inputs = params[0].get_array();
    univalue sendto = params[1].get_obj();

    cmutabletransaction rawtx;

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const univalue& input = inputs[idx];
        const univalue& o = input.get_obj();

        uint256 txid = parsehasho(o, "txid");

        const univalue& vout_v = find_value(o, "vout");
        if (!vout_v.isnum())
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter, missing vout key");
        int noutput = vout_v.get_int();
        if (noutput < 0)
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter, vout must be positive");

        ctxin in(coutpoint(txid, noutput));
        rawtx.vin.push_back(in);
    }

    set<cmoorecoinaddress> setaddress;
    vector<string> addrlist = sendto.getkeys();
    boost_foreach(const string& name_, addrlist) {
        cmoorecoinaddress address(name_);
        if (!address.isvalid())
            throw jsonrpcerror(rpc_invalid_address_or_key, string("invalid moorecoin address: ")+name_);

        if (setaddress.count(address))
            throw jsonrpcerror(rpc_invalid_parameter, string("invalid parameter, duplicated address: ")+name_);
        setaddress.insert(address);

        cscript scriptpubkey = getscriptfordestination(address.get());
        camount namount = amountfromvalue(sendto[name_]);

        ctxout out(namount, scriptpubkey);
        rawtx.vout.push_back(out);
    }

    return encodehextx(rawtx);
}

univalue decoderawtransaction(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "decoderawtransaction \"hexstring\"\n"
            "\nreturn a json object representing the serialized, hex-encoded transaction.\n"

            "\narguments:\n"
            "1. \"hex\"      (string, required) the transaction hex string\n"

            "\nresult:\n"
            "{\n"
            "  \"txid\" : \"id\",        (string) the transaction id\n"
            "  \"version\" : n,          (numeric) the version\n"
            "  \"locktime\" : ttt,       (numeric) the lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) the transaction id\n"
            "       \"vout\": n,         (numeric) the output number\n"
            "       \"scriptsig\": {     (json object) the script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n     (numeric) the script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [             (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) the value in btc\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptpubkey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqsigs\" : n,            (numeric) the required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) the type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"12tvkaxcxzjsmdnbao16dkxc8trwfcf5oc\"   (string) moorecoin address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "}\n"

            "\nexamples:\n"
            + helpexamplecli("decoderawtransaction", "\"hexstring\"")
            + helpexamplerpc("decoderawtransaction", "\"hexstring\"")
        );

    lock(cs_main);
    rpctypecheck(params, boost::assign::list_of(univalue::vstr));

    ctransaction tx;

    if (!decodehextx(tx, params[0].get_str()))
        throw jsonrpcerror(rpc_deserialization_error, "tx decode failed");

    univalue result(univalue::vobj);
    txtojson(tx, uint256(), result);

    return result;
}

univalue decodescript(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "decodescript \"hex\"\n"
            "\ndecode a hex-encoded script.\n"
            "\narguments:\n"
            "1. \"hex\"     (string) the hex encoded script\n"
            "\nresult:\n"
            "{\n"
            "  \"asm\":\"asm\",   (string) script public key\n"
            "  \"hex\":\"hex\",   (string) hex encoded public key\n"
            "  \"type\":\"type\", (string) the output type\n"
            "  \"reqsigs\": n,    (numeric) the required signatures\n"
            "  \"addresses\": [   (json array of string)\n"
            "     \"address\"     (string) moorecoin address\n"
            "     ,...\n"
            "  ],\n"
            "  \"p2sh\",\"address\" (string) script address\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("decodescript", "\"hexstring\"")
            + helpexamplerpc("decodescript", "\"hexstring\"")
        );

    lock(cs_main);
    rpctypecheck(params, boost::assign::list_of(univalue::vstr));

    univalue r(univalue::vobj);
    cscript script;
    if (params[0].get_str().size() > 0){
        vector<unsigned char> scriptdata(parsehexv(params[0], "argument"));
        script = cscript(scriptdata.begin(), scriptdata.end());
    } else {
        // empty scripts are valid
    }
    scriptpubkeytojson(script, r, false);

    r.push_back(pair("p2sh", cmoorecoinaddress(cscriptid(script)).tostring()));
    return r;
}

/** pushes a json object for script verification or signing errors to verrorsret. */
static void txinerrortojson(const ctxin& txin, univalue& verrorsret, const std::string& strmessage)
{
    univalue entry(univalue::vobj);
    entry.push_back(pair("txid", txin.prevout.hash.tostring()));
    entry.push_back(pair("vout", (uint64_t)txin.prevout.n));
    entry.push_back(pair("scriptsig", hexstr(txin.scriptsig.begin(), txin.scriptsig.end())));
    entry.push_back(pair("sequence", (uint64_t)txin.nsequence));
    entry.push_back(pair("error", strmessage));
    verrorsret.push_back(entry);
}

univalue signrawtransaction(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "signrawtransaction \"hexstring\" ( [{\"txid\":\"id\",\"vout\":n,\"scriptpubkey\":\"hex\",\"redeemscript\":\"hex\"},...] [\"privatekey1\",...] sighashtype )\n"
            "\nsign inputs for raw transaction (serialized, hex-encoded).\n"
            "the second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            "the third optional argument (may be null) is an array of base58-encoded private\n"
            "keys that, if given, will be the only keys used to sign the transaction.\n"
#ifdef enable_wallet
            + helprequiringpassphrase() + "\n"
#endif

            "\narguments:\n"
            "1. \"hexstring\"     (string, required) the transaction hex string\n"
            "2. \"prevtxs\"       (string, optional) an json array of previous dependent transaction outputs\n"
            "     [               (json array of json objects, or 'null' if none provided)\n"
            "       {\n"
            "         \"txid\":\"id\",             (string, required) the transaction id\n"
            "         \"vout\":n,                  (numeric, required) the output number\n"
            "         \"scriptpubkey\": \"hex\",   (string, required) script key\n"
            "         \"redeemscript\": \"hex\"    (string, required for p2sh) redeem script\n"
            "       }\n"
            "       ,...\n"
            "    ]\n"
            "3. \"privatekeys\"     (string, optional) a json array of base58-encoded private keys for signing\n"
            "    [                  (json array of strings, or 'null' if none provided)\n"
            "      \"privatekey\"   (string) private key in base58-encoding\n"
            "      ,...\n"
            "    ]\n"
            "4. \"sighashtype\"     (string, optional, default=all) the signature hash type. must be one of\n"
            "       \"all\"\n"
            "       \"none\"\n"
            "       \"single\"\n"
            "       \"all|anyonecanpay\"\n"
            "       \"none|anyonecanpay\"\n"
            "       \"single|anyonecanpay\"\n"

            "\nresult:\n"
            "{\n"
            "  \"hex\" : \"value\",           (string) the hex-encoded raw transaction with signature(s)\n"
            "  \"complete\" : true|false,   (boolean) if the transaction has a complete set of signatures\n"
            "  \"errors\" : [                 (json array of objects) script verification errors (if there are any)\n"
            "    {\n"
            "      \"txid\" : \"hash\",           (string) the hash of the referenced, previous transaction\n"
            "      \"vout\" : n,                (numeric) the index of the output to spent and used as input\n"
            "      \"scriptsig\" : \"hex\",       (string) the hex-encoded signature script\n"
            "      \"sequence\" : n,            (numeric) script sequence number\n"
            "      \"error\" : \"text\"           (string) verification or signing error related to the input\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nexamples:\n"
            + helpexamplecli("signrawtransaction", "\"myhex\"")
            + helpexamplerpc("signrawtransaction", "\"myhex\"")
        );

#ifdef enable_wallet
    lock2(cs_main, pwalletmain ? &pwalletmain->cs_wallet : null);
#else
    lock(cs_main);
#endif
    rpctypecheck(params, boost::assign::list_of(univalue::vstr)(univalue::varr)(univalue::varr)(univalue::vstr), true);

    vector<unsigned char> txdata(parsehexv(params[0], "argument 1"));
    cdatastream ssdata(txdata, ser_network, protocol_version);
    vector<cmutabletransaction> txvariants;
    while (!ssdata.empty()) {
        try {
            cmutabletransaction tx;
            ssdata >> tx;
            txvariants.push_back(tx);
        }
        catch (const std::exception&) {
            throw jsonrpcerror(rpc_deserialization_error, "tx decode failed");
        }
    }

    if (txvariants.empty())
        throw jsonrpcerror(rpc_deserialization_error, "missing transaction");

    // mergedtx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    cmutabletransaction mergedtx(txvariants[0]);

    // fetch previous transactions (inputs):
    ccoinsview viewdummy;
    ccoinsviewcache view(&viewdummy);
    {
        lock(mempool.cs);
        ccoinsviewcache &viewchain = *pcoinstip;
        ccoinsviewmempool viewmempool(&viewchain, mempool);
        view.setbackend(viewmempool); // temporarily switch cache backend to db+mempool view

        boost_foreach(const ctxin& txin, mergedtx.vin) {
            const uint256& prevhash = txin.prevout.hash;
            ccoins coins;
            view.accesscoins(prevhash); // this is certainly allowed to fail
        }

        view.setbackend(viewdummy); // switch back to avoid locking mempool for too long
    }

    bool fgivenkeys = false;
    cbasickeystore tempkeystore;
    if (params.size() > 2 && !params[2].isnull()) {
        fgivenkeys = true;
        univalue keys = params[2].get_array();
        for (unsigned int idx = 0; idx < keys.size(); idx++) {
            univalue k = keys[idx];
            cmoorecoinsecret vchsecret;
            bool fgood = vchsecret.setstring(k.get_str());
            if (!fgood)
                throw jsonrpcerror(rpc_invalid_address_or_key, "invalid private key");
            ckey key = vchsecret.getkey();
            if (!key.isvalid())
                throw jsonrpcerror(rpc_invalid_address_or_key, "private key outside allowed range");
            tempkeystore.addkey(key);
        }
    }
#ifdef enable_wallet
    else if (pwalletmain)
        ensurewalletisunlocked();
#endif

    // add previous txouts given in the rpc call:
    if (params.size() > 1 && !params[1].isnull()) {
        univalue prevtxs = params[1].get_array();
        for (unsigned int idx = 0; idx < prevtxs.size(); idx++) {
            const univalue& p = prevtxs[idx];
            if (!p.isobject())
                throw jsonrpcerror(rpc_deserialization_error, "expected object with {\"txid'\",\"vout\",\"scriptpubkey\"}");

            univalue prevout = p.get_obj();

            rpctypecheckobj(prevout, boost::assign::map_list_of("txid", univalue::vstr)("vout", univalue::vnum)("scriptpubkey", univalue::vstr));

            uint256 txid = parsehasho(prevout, "txid");

            int nout = find_value(prevout, "vout").get_int();
            if (nout < 0)
                throw jsonrpcerror(rpc_deserialization_error, "vout must be positive");

            vector<unsigned char> pkdata(parsehexo(prevout, "scriptpubkey"));
            cscript scriptpubkey(pkdata.begin(), pkdata.end());

            {
                ccoinsmodifier coins = view.modifycoins(txid);
                if (coins->isavailable(nout) && coins->vout[nout].scriptpubkey != scriptpubkey) {
                    string err("previous output scriptpubkey mismatch:\n");
                    err = err + coins->vout[nout].scriptpubkey.tostring() + "\nvs:\n"+
                        scriptpubkey.tostring();
                    throw jsonrpcerror(rpc_deserialization_error, err);
                }
                if ((unsigned int)nout >= coins->vout.size())
                    coins->vout.resize(nout+1);
                coins->vout[nout].scriptpubkey = scriptpubkey;
                coins->vout[nout].nvalue = 0; // we don't know the actual output value
            }

            // if redeemscript given and not using the local wallet (private keys
            // given), add redeemscript to the tempkeystore so it can be signed:
            if (fgivenkeys && scriptpubkey.ispaytoscripthash()) {
                rpctypecheckobj(prevout, boost::assign::map_list_of("txid", univalue::vstr)("vout", univalue::vnum)("scriptpubkey", univalue::vstr)("redeemscript",univalue::vstr));
                univalue v = find_value(prevout, "redeemscript");
                if (!v.isnull()) {
                    vector<unsigned char> rsdata(parsehexv(v, "redeemscript"));
                    cscript redeemscript(rsdata.begin(), rsdata.end());
                    tempkeystore.addcscript(redeemscript);
                }
            }
        }
    }

#ifdef enable_wallet
    const ckeystore& keystore = ((fgivenkeys || !pwalletmain) ? tempkeystore : *pwalletmain);
#else
    const ckeystore& keystore = tempkeystore;
#endif

    int nhashtype = sighash_all;
    if (params.size() > 3 && !params[3].isnull()) {
        static map<string, int> mapsighashvalues =
            boost::assign::map_list_of
            (string("all"), int(sighash_all))
            (string("all|anyonecanpay"), int(sighash_all|sighash_anyonecanpay))
            (string("none"), int(sighash_none))
            (string("none|anyonecanpay"), int(sighash_none|sighash_anyonecanpay))
            (string("single"), int(sighash_single))
            (string("single|anyonecanpay"), int(sighash_single|sighash_anyonecanpay))
            ;
        string strhashtype = params[3].get_str();
        if (mapsighashvalues.count(strhashtype))
            nhashtype = mapsighashvalues[strhashtype];
        else
            throw jsonrpcerror(rpc_invalid_parameter, "invalid sighash param");
    }

    bool fhashsingle = ((nhashtype & ~sighash_anyonecanpay) == sighash_single);

    // script verification errors
    univalue verrors(univalue::varr);

    // sign what we can:
    for (unsigned int i = 0; i < mergedtx.vin.size(); i++) {
        ctxin& txin = mergedtx.vin[i];
        const ccoins* coins = view.accesscoins(txin.prevout.hash);
        if (coins == null || !coins->isavailable(txin.prevout.n)) {
            txinerrortojson(txin, verrors, "input not found or already spent");
            continue;
        }
        const cscript& prevpubkey = coins->vout[txin.prevout.n].scriptpubkey;

        txin.scriptsig.clear();
        // only sign sighash_single if there's a corresponding output:
        if (!fhashsingle || (i < mergedtx.vout.size()))
            signsignature(keystore, prevpubkey, mergedtx, i, nhashtype);

        // ... and merge in other signatures:
        boost_foreach(const cmutabletransaction& txv, txvariants) {
            txin.scriptsig = combinesignatures(prevpubkey, mergedtx, i, txin.scriptsig, txv.vin[i].scriptsig);
        }
        scripterror serror = script_err_ok;
        if (!verifyscript(txin.scriptsig, prevpubkey, standard_script_verify_flags, mutabletransactionsignaturechecker(&mergedtx, i), &serror)) {
            txinerrortojson(txin, verrors, scripterrorstring(serror));
        }
    }
    bool fcomplete = verrors.empty();

    univalue result(univalue::vobj);
    result.push_back(pair("hex", encodehextx(mergedtx)));
    result.push_back(pair("complete", fcomplete));
    if (!verrors.empty()) {
        result.push_back(pair("errors", verrors));
    }

    return result;
}

univalue sendrawtransaction(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "sendrawtransaction \"hexstring\" ( allowhighfees )\n"
            "\nsubmits raw transaction (serialized, hex-encoded) to local node and network.\n"
            "\nalso see createrawtransaction and signrawtransaction calls.\n"
            "\narguments:\n"
            "1. \"hexstring\"    (string, required) the hex string of the raw transaction)\n"
            "2. allowhighfees    (boolean, optional, default=false) allow high fees\n"
            "\nresult:\n"
            "\"hex\"             (string) the transaction hash in hex\n"
            "\nexamples:\n"
            "\ncreate a transaction\n"
            + helpexamplecli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "sign the transaction, and get back the hex\n"
            + helpexamplecli("signrawtransaction", "\"myhex\"") +
            "\nsend the transaction (signed hex)\n"
            + helpexamplecli("sendrawtransaction", "\"signedhex\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("sendrawtransaction", "\"signedhex\"")
        );

    lock(cs_main);
    rpctypecheck(params, boost::assign::list_of(univalue::vstr)(univalue::vbool));

    // parse hex string from parameter
    ctransaction tx;
    if (!decodehextx(tx, params[0].get_str()))
        throw jsonrpcerror(rpc_deserialization_error, "tx decode failed");
    uint256 hashtx = tx.gethash();

    bool foverridefees = false;
    if (params.size() > 1)
        foverridefees = params[1].get_bool();

    ccoinsviewcache &view = *pcoinstip;
    const ccoins* existingcoins = view.accesscoins(hashtx);
    bool fhavemempool = mempool.exists(hashtx);
    bool fhavechain = existingcoins && existingcoins->nheight < 1000000000;
    if (!fhavemempool && !fhavechain) {
        // push to local node and sync with wallets
        cvalidationstate state;
        bool fmissinginputs;
        if (!accepttomemorypool(mempool, state, tx, false, &fmissinginputs, !foverridefees)) {
            if (state.isinvalid()) {
                throw jsonrpcerror(rpc_transaction_rejected, strprintf("%i: %s", state.getrejectcode(), state.getrejectreason()));
            } else {
                if (fmissinginputs) {
                    throw jsonrpcerror(rpc_transaction_error, "missing inputs");
                }
                throw jsonrpcerror(rpc_transaction_error, state.getrejectreason());
            }
        }
    } else if (fhavechain) {
        throw jsonrpcerror(rpc_transaction_already_in_chain, "transaction already in block chain");
    }
    relaytransaction(tx);

    return hashtx.gethex();
}
