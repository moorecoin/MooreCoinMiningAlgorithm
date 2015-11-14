// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "clientversion.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "timedata.h"
#include "util.h"
#ifdef enable_wallet
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "univalue/univalue.h"

using namespace std;

/**
 * @note do not add or change anything in the information returned by this
 * method. `getinfo` exists for backwards-compatibility only. it combines
 * information from wildly different sources in the program, which is a mess,
 * and is thus planned to be deprecated eventually.
 *
 * based on the source of the information, new information should be added to:
 * - `getblockchaininfo`,
 * - `getnetworkinfo` or
 * - `getwalletinfo`
 *
 * or alternatively, create a specific query method for the information.
 **/
univalue getinfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "returns an object containing various state info.\n"
            "\nresult:\n"
            "{\n"
            "  \"version\": xxxxx,           (numeric) the server version\n"
            "  \"protocolversion\": xxxxx,   (numeric) the protocol version\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total moorecoin balance of the wallet\n"
            "  \"blocks\": xxxxxx,           (numeric) the current number of blocks processed in the server\n"
            "  \"timeoffset\": xxxxx,        (numeric) the time offset\n"
            "  \"connections\": xxxxx,       (numeric) the number of connections\n"
            "  \"proxy\": \"host:port\",     (string, optional) the proxy used by the server\n"
            "  \"difficulty\": xxxxxx,       (numeric) the current difficulty\n"
            "  \"testnet\": true|false,      (boolean) if the server is using testnet or not\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since gmt epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight jan 1 1970 gmt) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee set in btc/kb\n"
            "  \"relayfee\": x.xxxx,         (numeric) minimum relay fee for non-free transactions in btc/kb\n"
            "  \"errors\": \"...\"           (string) any error messages\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getinfo", "")
            + helpexamplerpc("getinfo", "")
        );

#ifdef enable_wallet
    lock2(cs_main, pwalletmain ? &pwalletmain->cs_wallet : null);
#else
    lock(cs_main);
#endif

    proxytype proxy;
    getproxy(net_ipv4, proxy);

    univalue obj(univalue::vobj);
    obj.push_back(pair("version", client_version));
    obj.push_back(pair("protocolversion", protocol_version));
#ifdef enable_wallet
    if (pwalletmain) {
        obj.push_back(pair("walletversion", pwalletmain->getversion()));
        obj.push_back(pair("balance",       valuefromamount(pwalletmain->getbalance())));
    }
#endif
    obj.push_back(pair("blocks",        (int)chainactive.height()));
    obj.push_back(pair("timeoffset",    gettimeoffset()));
    obj.push_back(pair("connections",   (int)vnodes.size()));
    obj.push_back(pair("proxy",         (proxy.isvalid() ? proxy.proxy.tostringipport() : string())));
    obj.push_back(pair("difficulty",    (double)getdifficulty()));
    obj.push_back(pair("testnet",       params().testnettobedeprecatedfieldrpc()));
#ifdef enable_wallet
    if (pwalletmain) {
        obj.push_back(pair("keypoololdest", pwalletmain->getoldestkeypooltime()));
        obj.push_back(pair("keypoolsize",   (int)pwalletmain->getkeypoolsize()));
    }
    if (pwalletmain && pwalletmain->iscrypted())
        obj.push_back(pair("unlocked_until", nwalletunlocktime));
    obj.push_back(pair("paytxfee",      valuefromamount(paytxfee.getfeeperk())));
#endif
    obj.push_back(pair("relayfee",      valuefromamount(::minrelaytxfee.getfeeperk())));
    obj.push_back(pair("errors",        getwarnings("statusbar")));
    return obj;
}

#ifdef enable_wallet
class describeaddressvisitor : public boost::static_visitor<univalue>
{
public:
    univalue operator()(const cnodestination &dest) const { return univalue(univalue::vobj); }

    univalue operator()(const ckeyid &keyid) const {
        univalue obj(univalue::vobj);
        cpubkey vchpubkey;
        obj.push_back(pair("isscript", false));
        if (pwalletmain->getpubkey(keyid, vchpubkey)) {
            obj.push_back(pair("pubkey", hexstr(vchpubkey)));
            obj.push_back(pair("iscompressed", vchpubkey.iscompressed()));
        }
        return obj;
    }

    univalue operator()(const cscriptid &scriptid) const {
        univalue obj(univalue::vobj);
        cscript subscript;
        obj.push_back(pair("isscript", true));
        if (pwalletmain->getcscript(scriptid, subscript)) {
            std::vector<ctxdestination> addresses;
            txnouttype whichtype;
            int nrequired;
            extractdestinations(subscript, whichtype, addresses, nrequired);
            obj.push_back(pair("script", gettxnoutputtype(whichtype)));
            obj.push_back(pair("hex", hexstr(subscript.begin(), subscript.end())));
            univalue a(univalue::varr);
            boost_foreach(const ctxdestination& addr, addresses)
                a.push_back(cmoorecoinaddress(addr).tostring());
            obj.push_back(pair("addresses", a));
            if (whichtype == tx_multisig)
                obj.push_back(pair("sigsrequired", nrequired));
        }
        return obj;
    }
};
#endif

univalue validateaddress(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "validateaddress \"moorecoinaddress\"\n"
            "\nreturn information about the given moorecoin address.\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"     (string, required) the moorecoin address to validate\n"
            "\nresult:\n"
            "{\n"
            "  \"isvalid\" : true|false,         (boolean) if the address is valid or not. if not, this is the only property returned.\n"
            "  \"address\" : \"moorecoinaddress\", (string) the moorecoin address validated\n"
            "  \"scriptpubkey\" : \"hex\",       (string) the hex encoded scriptpubkey generated by the address\n"
            "  \"ismine\" : true|false,          (boolean) if the address is yours or not\n"
            "  \"isscript\" : true|false,        (boolean) if the key is a script\n"
            "  \"pubkey\" : \"publickeyhex\",    (string) the hex value of the raw public key\n"
            "  \"iscompressed\" : true|false,    (boolean) if the address is compressed\n"
            "  \"account\" : \"account\"         (string) deprecated. the account associated with the address, \"\" is the default account\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("validateaddress", "\"1pssgefhdnknxieyfrd1wceahr9hrqddwc\"")
            + helpexamplerpc("validateaddress", "\"1pssgefhdnknxieyfrd1wceahr9hrqddwc\"")
        );

#ifdef enable_wallet
    lock2(cs_main, pwalletmain ? &pwalletmain->cs_wallet : null);
#else
    lock(cs_main);
#endif

    cmoorecoinaddress address(params[0].get_str());
    bool isvalid = address.isvalid();

    univalue ret(univalue::vobj);
    ret.push_back(pair("isvalid", isvalid));
    if (isvalid)
    {
        ctxdestination dest = address.get();
        string currentaddress = address.tostring();
        ret.push_back(pair("address", currentaddress));

        cscript scriptpubkey = getscriptfordestination(dest);
        ret.push_back(pair("scriptpubkey", hexstr(scriptpubkey.begin(), scriptpubkey.end())));

#ifdef enable_wallet
        isminetype mine = pwalletmain ? ismine(*pwalletmain, dest) : ismine_no;
        ret.push_back(pair("ismine", (mine & ismine_spendable) ? true : false));
        ret.push_back(pair("iswatchonly", (mine & ismine_watch_only) ? true: false));
        univalue detail = boost::apply_visitor(describeaddressvisitor(), dest);
        ret.pushkvs(detail);
        if (pwalletmain && pwalletmain->mapaddressbook.count(dest))
            ret.push_back(pair("account", pwalletmain->mapaddressbook[dest].name));
#endif
    }
    return ret;
}

/**
 * used by addmultisigaddress / createmultisig:
 */
cscript _createmultisig_redeemscript(const univalue& params)
{
    int nrequired = params[0].get_int();
    const univalue& keys = params[1].get_array();

    // gather public keys
    if (nrequired < 1)
        throw runtime_error("a multisignature address must require at least one key to redeem");
    if ((int)keys.size() < nrequired)
        throw runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)", keys.size(), nrequired));
    if (keys.size() > 16)
        throw runtime_error("number of addresses involved in the multisignature address creation > 16\nreduce the number");
    std::vector<cpubkey> pubkeys;
    pubkeys.resize(keys.size());
    for (unsigned int i = 0; i < keys.size(); i++)
    {
        const std::string& ks = keys[i].get_str();
#ifdef enable_wallet
        // case 1: moorecoin address and we have full public key:
        cmoorecoinaddress address(ks);
        if (pwalletmain && address.isvalid())
        {
            ckeyid keyid;
            if (!address.getkeyid(keyid))
                throw runtime_error(
                    strprintf("%s does not refer to a key",ks));
            cpubkey vchpubkey;
            if (!pwalletmain->getpubkey(keyid, vchpubkey))
                throw runtime_error(
                    strprintf("no full public key for address %s",ks));
            if (!vchpubkey.isfullyvalid())
                throw runtime_error(" invalid public key: "+ks);
            pubkeys[i] = vchpubkey;
        }

        // case 2: hex public key
        else
#endif
        if (ishex(ks))
        {
            cpubkey vchpubkey(parsehex(ks));
            if (!vchpubkey.isfullyvalid())
                throw runtime_error(" invalid public key: "+ks);
            pubkeys[i] = vchpubkey;
        }
        else
        {
            throw runtime_error(" invalid public key: "+ks);
        }
    }
    cscript result = getscriptformultisig(nrequired, pubkeys);

    if (result.size() > max_script_element_size)
        throw runtime_error(
                strprintf("redeemscript exceeds size limit: %d > %d", result.size(), max_script_element_size));

    return result;
}

univalue createmultisig(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 2 || params.size() > 2)
    {
        string msg = "createmultisig nrequired [\"key\",...]\n"
            "\ncreates a multi-signature address with n signature of m keys required.\n"
            "it returns a json object with the address and redeemscript.\n"

            "\narguments:\n"
            "1. nrequired      (numeric, required) the number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"       (string, required) a json array of keys which are moorecoin addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"key\"    (string) moorecoin address or hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"

            "\nresult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) the value of the new multisig address.\n"
            "  \"redeemscript\":\"script\"       (string) the string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nexamples:\n"
            "\ncreate a multisig address from 2 addresses\n"
            + helpexamplecli("createmultisig", "2 \"[\\\"16ssausf5pf2ukuwvkgq4qjnrzbzyqgel5\\\",\\\"171sgjn4ytpu27adkkgrddwzrtxnrkbfkv\\\"]\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("createmultisig", "2, \"[\\\"16ssausf5pf2ukuwvkgq4qjnrzbzyqgel5\\\",\\\"171sgjn4ytpu27adkkgrddwzrtxnrkbfkv\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    // construct using pay-to-script-hash:
    cscript inner = _createmultisig_redeemscript(params);
    cscriptid innerid(inner);
    cmoorecoinaddress address(innerid);

    univalue result(univalue::vobj);
    result.push_back(pair("address", address.tostring()));
    result.push_back(pair("redeemscript", hexstr(inner.begin(), inner.end())));

    return result;
}

univalue verifymessage(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 3)
        throw runtime_error(
            "verifymessage \"moorecoinaddress\" \"signature\" \"message\"\n"
            "\nverify a signed message\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"  (string, required) the moorecoin address to use for the signature.\n"
            "2. \"signature\"       (string, required) the signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"         (string, required) the message that was signed.\n"
            "\nresult:\n"
            "true|false   (boolean) if the signature is verified or not.\n"
            "\nexamples:\n"
            "\nunlock the wallet for 30 seconds\n"
            + helpexamplecli("walletpassphrase", "\"mypassphrase\" 30") +
            "\ncreate the signature\n"
            + helpexamplecli("signmessage", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" \"my message\"") +
            "\nverify the signature\n"
            + helpexamplecli("verifymessage", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" \"signature\" \"my message\"") +
            "\nas json rpc\n"
            + helpexamplerpc("verifymessage", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\", \"signature\", \"my message\"")
        );

    lock(cs_main);

    string straddress  = params[0].get_str();
    string strsign     = params[1].get_str();
    string strmessage  = params[2].get_str();

    cmoorecoinaddress addr(straddress);
    if (!addr.isvalid())
        throw jsonrpcerror(rpc_type_error, "invalid address");

    ckeyid keyid;
    if (!addr.getkeyid(keyid))
        throw jsonrpcerror(rpc_type_error, "address does not refer to key");

    bool finvalid = false;
    vector<unsigned char> vchsig = decodebase64(strsign.c_str(), &finvalid);

    if (finvalid)
        throw jsonrpcerror(rpc_invalid_address_or_key, "malformed base64 encoding");

    chashwriter ss(ser_gethash, 0);
    ss << strmessagemagic;
    ss << strmessage;

    cpubkey pubkey;
    if (!pubkey.recovercompact(ss.gethash(), vchsig))
        return false;

    return (pubkey.getid() == keyid);
}

univalue setmocktime(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "setmocktime timestamp\n"
            "\nset the local time to given timestamp (-regtest only)\n"
            "\narguments:\n"
            "1. timestamp  (integer, required) unix seconds-since-epoch timestamp\n"
            "   pass 0 to go back to using the system time."
        );

    if (!params().mineblocksondemand())
        throw runtime_error("setmocktime for regression testing (-regtest mode) only");

    lock(cs_main);

    rpctypecheck(params, boost::assign::list_of(univalue::vnum));
    setmocktime(params[0].get_int64());

    return nullunivalue;
}
