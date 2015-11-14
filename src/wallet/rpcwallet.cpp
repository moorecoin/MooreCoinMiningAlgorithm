// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "base58.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet.h"
#include "walletdb.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "univalue/univalue.h"

using namespace std;

int64_t nwalletunlocktime;
static ccriticalsection cs_nwalletunlocktime;

std::string helprequiringpassphrase()
{
    return pwalletmain && pwalletmain->iscrypted()
        ? "\nrequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool ensurewalletisavailable(bool avoidexception)
{
    if (!pwalletmain)
    {
        if (!avoidexception)
            throw jsonrpcerror(rpc_method_not_found, "method not found (disabled)");
        else
            return false;
    }
    return true;
}

void ensurewalletisunlocked()
{
    if (pwalletmain->islocked())
        throw jsonrpcerror(rpc_wallet_unlock_needed, "error: please enter the wallet passphrase with walletpassphrase first.");
}

void wallettxtojson(const cwallettx& wtx, univalue& entry)
{
    int confirms = wtx.getdepthinmainchain();
    entry.push_back(pair("confirmations", confirms));
    if (wtx.iscoinbase())
        entry.push_back(pair("generated", true));
    if (confirms > 0)
    {
        entry.push_back(pair("blockhash", wtx.hashblock.gethex()));
        entry.push_back(pair("blockindex", wtx.nindex));
        entry.push_back(pair("blocktime", mapblockindex[wtx.hashblock]->getblocktime()));
    }
    uint256 hash = wtx.gethash();
    entry.push_back(pair("txid", hash.gethex()));
    univalue conflicts(univalue::varr);
    boost_foreach(const uint256& conflict, wtx.getconflicts())
        conflicts.push_back(conflict.gethex());
    entry.push_back(pair("walletconflicts", conflicts));
    entry.push_back(pair("time", wtx.gettxtime()));
    entry.push_back(pair("timereceived", (int64_t)wtx.ntimereceived));
    boost_foreach(const pairtype(string,string)& item, wtx.mapvalue)
        entry.push_back(pair(item.first, item.second));
}

string accountfromvalue(const univalue& value)
{
    string straccount = value.get_str();
    if (straccount == "*")
        throw jsonrpcerror(rpc_wallet_invalid_account_name, "invalid account name");
    return straccount;
}

univalue getnewaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress ( \"account\" )\n"
            "\nreturns a new moorecoin address for receiving payments.\n"
            "if 'account' is specified (deprecated), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\narguments:\n"
            "1. \"account\"        (string, optional) deprecated. the account name for the address to be linked to. if not provided, the default account \"\" is used. it can also be set to the empty string \"\" to represent the default account. the account does not need to exist, it will be created if there is no account by the given name.\n"
            "\nresult:\n"
            "\"moorecoinaddress\"    (string) the new moorecoin address\n"
            "\nexamples:\n"
            + helpexamplecli("getnewaddress", "")
            + helpexamplerpc("getnewaddress", "")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    // parse the account first so we don't generate a key if there's an error
    string straccount;
    if (params.size() > 0)
        straccount = accountfromvalue(params[0]);

    if (!pwalletmain->islocked())
        pwalletmain->topupkeypool();

    // generate a new key that is added to wallet
    cpubkey newkey;
    if (!pwalletmain->getkeyfrompool(newkey))
        throw jsonrpcerror(rpc_wallet_keypool_ran_out, "error: keypool ran out, please call keypoolrefill first");
    ckeyid keyid = newkey.getid();

    pwalletmain->setaddressbook(keyid, straccount, "receive");

    return cmoorecoinaddress(keyid).tostring();
}


cmoorecoinaddress getaccountaddress(string straccount, bool bforcenew=false)
{
    cwalletdb walletdb(pwalletmain->strwalletfile);

    caccount account;
    walletdb.readaccount(straccount, account);

    bool bkeyused = false;

    // check if the current key has been used
    if (account.vchpubkey.isvalid())
    {
        cscript scriptpubkey = getscriptfordestination(account.vchpubkey.getid());
        for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin();
             it != pwalletmain->mapwallet.end() && account.vchpubkey.isvalid();
             ++it)
        {
            const cwallettx& wtx = (*it).second;
            boost_foreach(const ctxout& txout, wtx.vout)
                if (txout.scriptpubkey == scriptpubkey)
                    bkeyused = true;
        }
    }

    // generate a new key
    if (!account.vchpubkey.isvalid() || bforcenew || bkeyused)
    {
        if (!pwalletmain->getkeyfrompool(account.vchpubkey))
            throw jsonrpcerror(rpc_wallet_keypool_ran_out, "error: keypool ran out, please call keypoolrefill first");

        pwalletmain->setaddressbook(account.vchpubkey.getid(), straccount, "receive");
        walletdb.writeaccount(straccount, account);
    }

    return cmoorecoinaddress(account.vchpubkey.getid());
}

univalue getaccountaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "getaccountaddress \"account\"\n"
            "\ndeprecated. returns the current moorecoin address for receiving payments to this account.\n"
            "\narguments:\n"
            "1. \"account\"       (string, required) the account name for the address. it can also be set to the empty string \"\" to represent the default account. the account does not need to exist, it will be created and a new address created  if there is no account by the given name.\n"
            "\nresult:\n"
            "\"moorecoinaddress\"   (string) the account moorecoin address\n"
            "\nexamples:\n"
            + helpexamplecli("getaccountaddress", "")
            + helpexamplecli("getaccountaddress", "\"\"")
            + helpexamplecli("getaccountaddress", "\"myaccount\"")
            + helpexamplerpc("getaccountaddress", "\"myaccount\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    // parse the account first so we don't generate a key if there's an error
    string straccount = accountfromvalue(params[0]);

    univalue ret(univalue::vstr);

    ret = getaccountaddress(straccount).tostring();
    return ret;
}


univalue getrawchangeaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "getrawchangeaddress\n"
            "\nreturns a new moorecoin address, for receiving change.\n"
            "this is for use with raw transactions, not normal use.\n"
            "\nresult:\n"
            "\"address\"    (string) the address\n"
            "\nexamples:\n"
            + helpexamplecli("getrawchangeaddress", "")
            + helpexamplerpc("getrawchangeaddress", "")
       );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (!pwalletmain->islocked())
        pwalletmain->topupkeypool();

    creservekey reservekey(pwalletmain);
    cpubkey vchpubkey;
    if (!reservekey.getreservedkey(vchpubkey))
        throw jsonrpcerror(rpc_wallet_keypool_ran_out, "error: keypool ran out, please call keypoolrefill first");

    reservekey.keepkey();

    ckeyid keyid = vchpubkey.getid();

    return cmoorecoinaddress(keyid).tostring();
}


univalue setaccount(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setaccount \"moorecoinaddress\" \"account\"\n"
            "\ndeprecated. sets the account associated with the given address.\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"  (string, required) the moorecoin address to be associated with an account.\n"
            "2. \"account\"         (string, required) the account to assign the address to.\n"
            "\nexamples:\n"
            + helpexamplecli("setaccount", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" \"tabby\"")
            + helpexamplerpc("setaccount", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\", \"tabby\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    cmoorecoinaddress address(params[0].get_str());
    if (!address.isvalid())
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address");

    string straccount;
    if (params.size() > 1)
        straccount = accountfromvalue(params[1]);

    // only add the account if the address is yours.
    if (ismine(*pwalletmain, address.get()))
    {
        // detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwalletmain->mapaddressbook.count(address.get()))
        {
            string stroldaccount = pwalletmain->mapaddressbook[address.get()].name;
            if (address == getaccountaddress(stroldaccount))
                getaccountaddress(stroldaccount, true);
        }
        pwalletmain->setaddressbook(address.get(), straccount, "receive");
    }
    else
        throw jsonrpcerror(rpc_misc_error, "setaccount can only be used with own address");

    return nullunivalue;
}


univalue getaccount(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "getaccount \"moorecoinaddress\"\n"
            "\ndeprecated. returns the account associated with the given address.\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"  (string, required) the moorecoin address for account lookup.\n"
            "\nresult:\n"
            "\"accountname\"        (string) the account address\n"
            "\nexamples:\n"
            + helpexamplecli("getaccount", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\"")
            + helpexamplerpc("getaccount", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    cmoorecoinaddress address(params[0].get_str());
    if (!address.isvalid())
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address");

    string straccount;
    map<ctxdestination, caddressbookdata>::iterator mi = pwalletmain->mapaddressbook.find(address.get());
    if (mi != pwalletmain->mapaddressbook.end() && !(*mi).second.name.empty())
        straccount = (*mi).second.name;
    return straccount;
}


univalue getaddressesbyaccount(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "getaddressesbyaccount \"account\"\n"
            "\ndeprecated. returns the list of addresses for the given account.\n"
            "\narguments:\n"
            "1. \"account\"  (string, required) the account name.\n"
            "\nresult:\n"
            "[                     (json array of string)\n"
            "  \"moorecoinaddress\"  (string) a moorecoin address associated with the given account\n"
            "  ,...\n"
            "]\n"
            "\nexamples:\n"
            + helpexamplecli("getaddressesbyaccount", "\"tabby\"")
            + helpexamplerpc("getaddressesbyaccount", "\"tabby\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    string straccount = accountfromvalue(params[0]);

    // find all addresses that have the given account
    univalue ret(univalue::varr);
    boost_foreach(const pairtype(cmoorecoinaddress, caddressbookdata)& item, pwalletmain->mapaddressbook)
    {
        const cmoorecoinaddress& address = item.first;
        const string& strname = item.second.name;
        if (strname == straccount)
            ret.push_back(address.tostring());
    }
    return ret;
}

static void sendmoney(const ctxdestination &address, camount nvalue, bool fsubtractfeefromamount, cwallettx& wtxnew)
{
    camount curbalance = pwalletmain->getbalance();

    // check amount
    if (nvalue <= 0)
        throw jsonrpcerror(rpc_invalid_parameter, "invalid amount");

    if (nvalue > curbalance)
        throw jsonrpcerror(rpc_wallet_insufficient_funds, "insufficient funds");

    // parse moorecoin address
    cscript scriptpubkey = getscriptfordestination(address);

    // create and send the transaction
    creservekey reservekey(pwalletmain);
    camount nfeerequired;
    std::string strerror;
    vector<crecipient> vecsend;
    int nchangeposret = -1;
    crecipient recipient = {scriptpubkey, nvalue, fsubtractfeefromamount};
    vecsend.push_back(recipient);
    if (!pwalletmain->createtransaction(vecsend, wtxnew, reservekey, nfeerequired, nchangeposret, strerror)) {
        if (!fsubtractfeefromamount && nvalue + nfeerequired > pwalletmain->getbalance())
            strerror = strprintf("error: this transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", formatmoney(nfeerequired));
        throw jsonrpcerror(rpc_wallet_error, strerror);
    }
    if (!pwalletmain->committransaction(wtxnew, reservekey))
        throw jsonrpcerror(rpc_wallet_error, "error: the transaction was rejected! this might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}

univalue sendtoaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendtoaddress \"moorecoinaddress\" amount ( \"comment\" \"comment-to\" subtractfeefromamount )\n"
            "\nsend an amount to a given address. the amount is a real and is rounded to the nearest 0.00000001\n"
            + helprequiringpassphrase() +
            "\narguments:\n"
            "1. \"moorecoinaddress\"  (string, required) the moorecoin address to send to.\n"
            "2. \"amount\"      (numeric, required) the amount in btc to send. eg 0.1\n"
            "3. \"comment\"     (string, optional) a comment used to store what the transaction is for. \n"
            "                             this is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment-to\"  (string, optional) a comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. this is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "5. subtractfeefromamount  (boolean, optional, default=false) the fee will be deducted from the amount being sent.\n"
            "                             the recipient will receive less moorecoins than you enter in the amount field.\n"
            "\nresult:\n"
            "\"transactionid\"  (string) the transaction id.\n"
            "\nexamples:\n"
            + helpexamplecli("sendtoaddress", "\"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\" 0.1")
            + helpexamplecli("sendtoaddress", "\"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + helpexamplecli("sendtoaddress", "\"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\" 0.1 \"\" \"\" true")
            + helpexamplerpc("sendtoaddress", "\"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    cmoorecoinaddress address(params[0].get_str());
    if (!address.isvalid())
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address");

    // amount
    camount namount = amountfromvalue(params[1]);
    if (namount <= 0)
        throw jsonrpcerror(rpc_type_error, "invalid amount for send");

    // wallet comments
    cwallettx wtx;
    if (params.size() > 2 && !params[2].isnull() && !params[2].get_str().empty())
        wtx.mapvalue["comment"] = params[2].get_str();
    if (params.size() > 3 && !params[3].isnull() && !params[3].get_str().empty())
        wtx.mapvalue["to"]      = params[3].get_str();

    bool fsubtractfeefromamount = false;
    if (params.size() > 4)
        fsubtractfeefromamount = params[4].get_bool();

    ensurewalletisunlocked();

    sendmoney(address.get(), namount, fsubtractfeefromamount, wtx);

    return wtx.gethash().gethex();
}

univalue listaddressgroupings(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp)
        throw runtime_error(
            "listaddressgroupings\n"
            "\nlists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nresult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"moorecoinaddress\",     (string) the moorecoin address\n"
            "      amount,                 (numeric) the amount in btc\n"
            "      \"account\"             (string, optional) the account (deprecated)\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nexamples:\n"
            + helpexamplecli("listaddressgroupings", "")
            + helpexamplerpc("listaddressgroupings", "")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    univalue jsongroupings(univalue::varr);
    map<ctxdestination, camount> balances = pwalletmain->getaddressbalances();
    boost_foreach(set<ctxdestination> grouping, pwalletmain->getaddressgroupings())
    {
        univalue jsongrouping(univalue::varr);
        boost_foreach(ctxdestination address, grouping)
        {
            univalue addressinfo(univalue::varr);
            addressinfo.push_back(cmoorecoinaddress(address).tostring());
            addressinfo.push_back(valuefromamount(balances[address]));
            {
                lock(pwalletmain->cs_wallet);
                if (pwalletmain->mapaddressbook.find(cmoorecoinaddress(address).get()) != pwalletmain->mapaddressbook.end())
                    addressinfo.push_back(pwalletmain->mapaddressbook.find(cmoorecoinaddress(address).get())->second.name);
            }
            jsongrouping.push_back(addressinfo);
        }
        jsongroupings.push_back(jsongrouping);
    }
    return jsongroupings;
}

univalue signmessage(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 2)
        throw runtime_error(
            "signmessage \"moorecoinaddress\" \"message\"\n"
            "\nsign a message with the private key of an address"
            + helprequiringpassphrase() + "\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"  (string, required) the moorecoin address to use for the private key.\n"
            "2. \"message\"         (string, required) the message to create a signature of.\n"
            "\nresult:\n"
            "\"signature\"          (string) the signature of the message encoded in base 64\n"
            "\nexamples:\n"
            "\nunlock the wallet for 30 seconds\n"
            + helpexamplecli("walletpassphrase", "\"mypassphrase\" 30") +
            "\ncreate the signature\n"
            + helpexamplecli("signmessage", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" \"my message\"") +
            "\nverify the signature\n"
            + helpexamplecli("verifymessage", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" \"signature\" \"my message\"") +
            "\nas json rpc\n"
            + helpexamplerpc("signmessage", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\", \"my message\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    ensurewalletisunlocked();

    string straddress = params[0].get_str();
    string strmessage = params[1].get_str();

    cmoorecoinaddress addr(straddress);
    if (!addr.isvalid())
        throw jsonrpcerror(rpc_type_error, "invalid address");

    ckeyid keyid;
    if (!addr.getkeyid(keyid))
        throw jsonrpcerror(rpc_type_error, "address does not refer to key");

    ckey key;
    if (!pwalletmain->getkey(keyid, key))
        throw jsonrpcerror(rpc_wallet_error, "private key not available");

    chashwriter ss(ser_gethash, 0);
    ss << strmessagemagic;
    ss << strmessage;

    vector<unsigned char> vchsig;
    if (!key.signcompact(ss.gethash(), vchsig))
        throw jsonrpcerror(rpc_invalid_address_or_key, "sign failed");

    return encodebase64(&vchsig[0], vchsig.size());
}

univalue getreceivedbyaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaddress \"moorecoinaddress\" ( minconf )\n"
            "\nreturns the total amount received by the given moorecoinaddress in transactions with at least minconf confirmations.\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"  (string, required) the moorecoin address for transactions.\n"
            "2. minconf             (numeric, optional, default=1) only include transactions confirmed at least this many times.\n"
            "\nresult:\n"
            "amount   (numeric) the total amount in btc received at this address.\n"
            "\nexamples:\n"
            "\nthe amount from transactions with at least 1 confirmation\n"
            + helpexamplecli("getreceivedbyaddress", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\"") +
            "\nthe amount including unconfirmed transactions, zero confirmations\n"
            + helpexamplecli("getreceivedbyaddress", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" 0") +
            "\nthe amount with at least 6 confirmation, very safe\n"
            + helpexamplecli("getreceivedbyaddress", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\" 6") +
            "\nas a json rpc call\n"
            + helpexamplerpc("getreceivedbyaddress", "\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\", 6")
       );

    lock2(cs_main, pwalletmain->cs_wallet);

    // moorecoin address
    cmoorecoinaddress address = cmoorecoinaddress(params[0].get_str());
    if (!address.isvalid())
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address");
    cscript scriptpubkey = getscriptfordestination(address.get());
    if (!ismine(*pwalletmain,scriptpubkey))
        return (double)0.0;

    // minimum confirmations
    int nmindepth = 1;
    if (params.size() > 1)
        nmindepth = params[1].get_int();

    // tally
    camount namount = 0;
    for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); ++it)
    {
        const cwallettx& wtx = (*it).second;
        if (wtx.iscoinbase() || !checkfinaltx(wtx))
            continue;

        boost_foreach(const ctxout& txout, wtx.vout)
            if (txout.scriptpubkey == scriptpubkey)
                if (wtx.getdepthinmainchain() >= nmindepth)
                    namount += txout.nvalue;
    }

    return  valuefromamount(namount);
}


univalue getreceivedbyaccount(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\ndeprecated. returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"
            "\narguments:\n"
            "1. \"account\"      (string, required) the selected account, may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) only include transactions confirmed at least this many times.\n"
            "\nresult:\n"
            "amount              (numeric) the total amount in btc received for this account.\n"
            "\nexamples:\n"
            "\namount received by the default account with at least 1 confirmation\n"
            + helpexamplecli("getreceivedbyaccount", "\"\"") +
            "\namount received at the tabby account including unconfirmed amounts with zero confirmations\n"
            + helpexamplecli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nthe amount with at least 6 confirmation, very safe\n"
            + helpexamplecli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nas a json rpc call\n"
            + helpexamplerpc("getreceivedbyaccount", "\"tabby\", 6")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    // minimum confirmations
    int nmindepth = 1;
    if (params.size() > 1)
        nmindepth = params[1].get_int();

    // get the set of pub keys assigned to account
    string straccount = accountfromvalue(params[0]);
    set<ctxdestination> setaddress = pwalletmain->getaccountaddresses(straccount);

    // tally
    camount namount = 0;
    for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); ++it)
    {
        const cwallettx& wtx = (*it).second;
        if (wtx.iscoinbase() || !checkfinaltx(wtx))
            continue;

        boost_foreach(const ctxout& txout, wtx.vout)
        {
            ctxdestination address;
            if (extractdestination(txout.scriptpubkey, address) && ismine(*pwalletmain, address) && setaddress.count(address))
                if (wtx.getdepthinmainchain() >= nmindepth)
                    namount += txout.nvalue;
        }
    }

    return (double)namount / (double)coin;
}


camount getaccountbalance(cwalletdb& walletdb, const string& straccount, int nmindepth, const isminefilter& filter)
{
    camount nbalance = 0;

    // tally wallet transactions
    for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); ++it)
    {
        const cwallettx& wtx = (*it).second;
        if (!checkfinaltx(wtx) || wtx.getblockstomaturity() > 0 || wtx.getdepthinmainchain() < 0)
            continue;

        camount nreceived, nsent, nfee;
        wtx.getaccountamounts(straccount, nreceived, nsent, nfee, filter);

        if (nreceived != 0 && wtx.getdepthinmainchain() >= nmindepth)
            nbalance += nreceived;
        nbalance -= nsent + nfee;
    }

    // tally internal accounting entries
    nbalance += walletdb.getaccountcreditdebit(straccount);

    return nbalance;
}

camount getaccountbalance(const string& straccount, int nmindepth, const isminefilter& filter)
{
    cwalletdb walletdb(pwalletmain->strwalletfile);
    return getaccountbalance(walletdb, straccount, nmindepth, filter);
}


univalue getbalance(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 3)
        throw runtime_error(
            "getbalance ( \"account\" minconf includewatchonly )\n"
            "\nif account is not specified, returns the server's total available balance.\n"
            "if account is specified (deprecated), returns the balance in the account.\n"
            "note that the account \"\" is not the same as leaving the parameter out.\n"
            "the server total may be different to the balance in the default \"\" account.\n"
            "\narguments:\n"
            "1. \"account\"      (string, optional) deprecated. the selected account, or \"*\" for entire wallet. it may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) only include transactions confirmed at least this many times.\n"
            "3. includewatchonly (bool, optional, default=false) also include balance in watchonly addresses (see 'importaddress')\n"
            "\nresult:\n"
            "amount              (numeric) the total amount in btc received for this account.\n"
            "\nexamples:\n"
            "\nthe total amount in the wallet\n"
            + helpexamplecli("getbalance", "") +
            "\nthe total amount in the wallet at least 5 blocks confirmed\n"
            + helpexamplecli("getbalance", "\"*\" 6") +
            "\nas a json rpc call\n"
            + helpexamplerpc("getbalance", "\"*\", 6")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (params.size() == 0)
        return  valuefromamount(pwalletmain->getbalance());

    int nmindepth = 1;
    if (params.size() > 1)
        nmindepth = params[1].get_int();
    isminefilter filter = ismine_spendable;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ismine_watch_only;

    if (params[0].get_str() == "*") {
        // calculate total balance a different way from getbalance()
        // (getbalance() sums up all unspent txouts)
        // getbalance and "getbalance * 1 true" should return the same number
        camount nbalance = 0;
        for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); ++it)
        {
            const cwallettx& wtx = (*it).second;
            if (!checkfinaltx(wtx) || wtx.getblockstomaturity() > 0 || wtx.getdepthinmainchain() < 0)
                continue;

            camount allfee;
            string strsentaccount;
            list<coutputentry> listreceived;
            list<coutputentry> listsent;
            wtx.getamounts(listreceived, listsent, allfee, strsentaccount, filter);
            if (wtx.getdepthinmainchain() >= nmindepth)
            {
                boost_foreach(const coutputentry& r, listreceived)
                    nbalance += r.amount;
            }
            boost_foreach(const coutputentry& s, listsent)
                nbalance -= s.amount;
            nbalance -= allfee;
        }
        return  valuefromamount(nbalance);
    }

    string straccount = accountfromvalue(params[0]);

    camount nbalance = getaccountbalance(straccount, nmindepth, filter);

    return valuefromamount(nbalance);
}

univalue getunconfirmedbalance(const univalue &params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 0)
        throw runtime_error(
                "getunconfirmedbalance\n"
                "returns the server's total unconfirmed balance\n");

    lock2(cs_main, pwalletmain->cs_wallet);

    return valuefromamount(pwalletmain->getunconfirmedbalance());
}


univalue movecmd(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
            "move \"fromaccount\" \"toaccount\" amount ( minconf \"comment\" )\n"
            "\ndeprecated. move a specified amount from one account in your wallet to another.\n"
            "\narguments:\n"
            "1. \"fromaccount\"   (string, required) the name of the account to move funds from. may be the default account using \"\".\n"
            "2. \"toaccount\"     (string, required) the name of the account to move funds to. may be the default account using \"\".\n"
            "3. minconf           (numeric, optional, default=1) only use funds with at least this many confirmations.\n"
            "4. \"comment\"       (string, optional) an optional comment, stored in the wallet only.\n"
            "\nresult:\n"
            "true|false           (boolean) true if successfull.\n"
            "\nexamples:\n"
            "\nmove 0.01 btc from the default account to the account named tabby\n"
            + helpexamplecli("move", "\"\" \"tabby\" 0.01") +
            "\nmove 0.01 btc timotei to akiko with a comment and funds have 6 confirmations\n"
            + helpexamplecli("move", "\"timotei\" \"akiko\" 0.01 6 \"happy birthday!\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("move", "\"timotei\", \"akiko\", 0.01, 6, \"happy birthday!\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    string strfrom = accountfromvalue(params[0]);
    string strto = accountfromvalue(params[1]);
    camount namount = amountfromvalue(params[2]);
    if (namount <= 0)
        throw jsonrpcerror(rpc_type_error, "invalid amount for send");
    if (params.size() > 3)
        // unused parameter, used to be nmindepth, keep type-checking it though
        (void)params[3].get_int();
    string strcomment;
    if (params.size() > 4)
        strcomment = params[4].get_str();

    cwalletdb walletdb(pwalletmain->strwalletfile);
    if (!walletdb.txnbegin())
        throw jsonrpcerror(rpc_database_error, "database error");

    int64_t nnow = getadjustedtime();

    // debit
    caccountingentry debit;
    debit.norderpos = pwalletmain->incorderposnext(&walletdb);
    debit.straccount = strfrom;
    debit.ncreditdebit = -namount;
    debit.ntime = nnow;
    debit.strotheraccount = strto;
    debit.strcomment = strcomment;
    walletdb.writeaccountingentry(debit);

    // credit
    caccountingentry credit;
    credit.norderpos = pwalletmain->incorderposnext(&walletdb);
    credit.straccount = strto;
    credit.ncreditdebit = namount;
    credit.ntime = nnow;
    credit.strotheraccount = strfrom;
    credit.strcomment = strcomment;
    walletdb.writeaccountingentry(credit);

    if (!walletdb.txncommit())
        throw jsonrpcerror(rpc_database_error, "database error");

    return true;
}


univalue sendfrom(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendfrom \"fromaccount\" \"tomoorecoinaddress\" amount ( minconf \"comment\" \"comment-to\" )\n"
            "\ndeprecated (use sendtoaddress). sent an amount from an account to a moorecoin address.\n"
            "the amount is a real and is rounded to the nearest 0.00000001."
            + helprequiringpassphrase() + "\n"
            "\narguments:\n"
            "1. \"fromaccount\"       (string, required) the name of the account to send funds from. may be the default account using \"\".\n"
            "2. \"tomoorecoinaddress\"  (string, required) the moorecoin address to send funds to.\n"
            "3. amount                (numeric, required) the amount in btc. (transaction fee is added on top).\n"
            "4. minconf               (numeric, optional, default=1) only use funds with at least this many confirmations.\n"
            "5. \"comment\"           (string, optional) a comment used to store what the transaction is for. \n"
            "                                     this is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"        (string, optional) an optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. this is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"
            "\nresult:\n"
            "\"transactionid\"        (string) the transaction id.\n"
            "\nexamples:\n"
            "\nsend 0.01 btc from the default account to the address, must have at least 1 confirmation\n"
            + helpexamplecli("sendfrom", "\"\" \"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\" 0.01") +
            "\nsend 0.01 from the tabby account to the given address, funds must have at least 6 confirmations\n"
            + helpexamplecli("sendfrom", "\"tabby\" \"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\" 0.01 6 \"donation\" \"seans outpost\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("sendfrom", "\"tabby\", \"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\", 0.01, 6, \"donation\", \"seans outpost\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    string straccount = accountfromvalue(params[0]);
    cmoorecoinaddress address(params[1].get_str());
    if (!address.isvalid())
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address");
    camount namount = amountfromvalue(params[2]);
    if (namount <= 0)
        throw jsonrpcerror(rpc_type_error, "invalid amount for send");
    int nmindepth = 1;
    if (params.size() > 3)
        nmindepth = params[3].get_int();

    cwallettx wtx;
    wtx.strfromaccount = straccount;
    if (params.size() > 4 && !params[4].isnull() && !params[4].get_str().empty())
        wtx.mapvalue["comment"] = params[4].get_str();
    if (params.size() > 5 && !params[5].isnull() && !params[5].get_str().empty())
        wtx.mapvalue["to"]      = params[5].get_str();

    ensurewalletisunlocked();

    // check funds
    camount nbalance = getaccountbalance(straccount, nmindepth, ismine_spendable);
    if (namount > nbalance)
        throw jsonrpcerror(rpc_wallet_insufficient_funds, "account has insufficient funds");

    sendmoney(address.get(), namount, false, wtx);

    return wtx.gethash().gethex();
}


univalue sendmany(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 2 || params.size() > 5)
        throw runtime_error(
            "sendmany \"fromaccount\" {\"address\":amount,...} ( minconf \"comment\" [\"address\",...] )\n"
            "\nsend multiple times. amounts are double-precision floating point numbers."
            + helprequiringpassphrase() + "\n"
            "\narguments:\n"
            "1. \"fromaccount\"         (string, required) deprecated. the account to send the funds from. should be \"\" for the default account\n"
            "2. \"amounts\"             (string, required) a json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) the moorecoin address is the key, the numeric amount in btc is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) a comment\n"
            "5. subtractfeefromamount   (string, optional) a json array with addresses.\n"
            "                           the fee will be equally deducted from the amount of each selected address.\n"
            "                           those recipients will receive less moorecoins than you enter in their corresponding amount field.\n"
            "                           if no addresses are specified here, the sender pays the fee.\n"
            "    [\n"
            "      \"address\"            (string) subtract fee from this address\n"
            "      ,...\n"
            "    ]\n"
            "\nresult:\n"
            "\"transactionid\"          (string) the transaction id for the send. only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nexamples:\n"
            "\nsend two amounts to two different addresses:\n"
            + helpexamplecli("sendmany", "\"\" \"{\\\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\\\":0.01,\\\"1353tse8ymta4euv7dguxgjnff9kpvvkhz\\\":0.02}\"") +
            "\nsend two amounts to two different addresses setting the confirmation and comment:\n"
            + helpexamplecli("sendmany", "\"\" \"{\\\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\\\":0.01,\\\"1353tse8ymta4euv7dguxgjnff9kpvvkhz\\\":0.02}\" 6 \"testing\"") +
            "\nsend two amounts to two different addresses, subtract fee from amount:\n"
            + helpexamplecli("sendmany", "\"\" \"{\\\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\\\":0.01,\\\"1353tse8ymta4euv7dguxgjnff9kpvvkhz\\\":0.02}\" 1 \"\" \"[\\\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\\\",\\\"1353tse8ymta4euv7dguxgjnff9kpvvkhz\\\"]\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("sendmany", "\"\", \"{\\\"1d1zrzne3juo7zyckeyqqiqawd9y54f4xz\\\":0.01,\\\"1353tse8ymta4euv7dguxgjnff9kpvvkhz\\\":0.02}\", 6, \"testing\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    string straccount = accountfromvalue(params[0]);
    univalue sendto = params[1].get_obj();
    int nmindepth = 1;
    if (params.size() > 2)
        nmindepth = params[2].get_int();

    cwallettx wtx;
    wtx.strfromaccount = straccount;
    if (params.size() > 3 && !params[3].isnull() && !params[3].get_str().empty())
        wtx.mapvalue["comment"] = params[3].get_str();

    univalue subtractfeefromamount(univalue::varr);
    if (params.size() > 4)
        subtractfeefromamount = params[4].get_array();

    set<cmoorecoinaddress> setaddress;
    vector<crecipient> vecsend;

    camount totalamount = 0;
    vector<string> keys = sendto.getkeys();
    boost_foreach(const string& name_, keys)
    {
        cmoorecoinaddress address(name_);
        if (!address.isvalid())
            throw jsonrpcerror(rpc_invalid_address_or_key, string("invalid moorecoin address: ")+name_);

        if (setaddress.count(address))
            throw jsonrpcerror(rpc_invalid_parameter, string("invalid parameter, duplicated address: ")+name_);
        setaddress.insert(address);

        cscript scriptpubkey = getscriptfordestination(address.get());
        camount namount = amountfromvalue(sendto[name_]);
        if (namount <= 0)
            throw jsonrpcerror(rpc_type_error, "invalid amount for send");
        totalamount += namount;

        bool fsubtractfeefromamount = false;
        for (unsigned int idx = 0; idx < subtractfeefromamount.size(); idx++) {
            const univalue& addr = subtractfeefromamount[idx];
            if (addr.get_str() == name_)
                fsubtractfeefromamount = true;
        }

        crecipient recipient = {scriptpubkey, namount, fsubtractfeefromamount};
        vecsend.push_back(recipient);
    }

    ensurewalletisunlocked();

    // check funds
    camount nbalance = getaccountbalance(straccount, nmindepth, ismine_spendable);
    if (totalamount > nbalance)
        throw jsonrpcerror(rpc_wallet_insufficient_funds, "account has insufficient funds");

    // send
    creservekey keychange(pwalletmain);
    camount nfeerequired = 0;
    int nchangeposret = -1;
    string strfailreason;
    bool fcreated = pwalletmain->createtransaction(vecsend, wtx, keychange, nfeerequired, nchangeposret, strfailreason);
    if (!fcreated)
        throw jsonrpcerror(rpc_wallet_insufficient_funds, strfailreason);
    if (!pwalletmain->committransaction(wtx, keychange))
        throw jsonrpcerror(rpc_wallet_error, "transaction commit failed");

    return wtx.gethash().gethex();
}

// defined in rpcmisc.cpp
extern cscript _createmultisig_redeemscript(const univalue& params);

univalue addmultisigaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 2 || params.size() > 3)
    {
        string msg = "addmultisigaddress nrequired [\"key\",...] ( \"account\" )\n"
            "\nadd a nrequired-to-sign multisignature address to the wallet.\n"
            "each key is a moorecoin address or hex-encoded public key.\n"
            "if 'account' is specified (deprecated), assign address to that account.\n"

            "\narguments:\n"
            "1. nrequired        (numeric, required) the number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) a json array of moorecoin addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) moorecoin address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"      (string, optional) deprecated. an account to assign the addresses to.\n"

            "\nresult:\n"
            "\"moorecoinaddress\"  (string) a moorecoin address associated with the keys.\n"

            "\nexamples:\n"
            "\nadd a multisig address from 2 addresses\n"
            + helpexamplecli("addmultisigaddress", "2 \"[\\\"16ssausf5pf2ukuwvkgq4qjnrzbzyqgel5\\\",\\\"171sgjn4ytpu27adkkgrddwzrtxnrkbfkv\\\"]\"") +
            "\nas json rpc call\n"
            + helpexamplerpc("addmultisigaddress", "2, \"[\\\"16ssausf5pf2ukuwvkgq4qjnrzbzyqgel5\\\",\\\"171sgjn4ytpu27adkkgrddwzrtxnrkbfkv\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    lock2(cs_main, pwalletmain->cs_wallet);

    string straccount;
    if (params.size() > 2)
        straccount = accountfromvalue(params[2]);

    // construct using pay-to-script-hash:
    cscript inner = _createmultisig_redeemscript(params);
    cscriptid innerid(inner);
    pwalletmain->addcscript(inner);

    pwalletmain->setaddressbook(innerid, straccount, "send");
    return cmoorecoinaddress(innerid).tostring();
}


struct tallyitem
{
    camount namount;
    int nconf;
    vector<uint256> txids;
    bool fiswatchonly;
    tallyitem()
    {
        namount = 0;
        nconf = std::numeric_limits<int>::max();
        fiswatchonly = false;
    }
};

univalue listreceived(const univalue& params, bool fbyaccounts)
{
    // minimum confirmations
    int nmindepth = 1;
    if (params.size() > 0)
        nmindepth = params[0].get_int();

    // whether to include empty accounts
    bool fincludeempty = false;
    if (params.size() > 1)
        fincludeempty = params[1].get_bool();

    isminefilter filter = ismine_spendable;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ismine_watch_only;

    // tally
    map<cmoorecoinaddress, tallyitem> maptally;
    for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); ++it)
    {
        const cwallettx& wtx = (*it).second;

        if (wtx.iscoinbase() || !checkfinaltx(wtx))
            continue;

        int ndepth = wtx.getdepthinmainchain();
        if (ndepth < nmindepth)
            continue;

        boost_foreach(const ctxout& txout, wtx.vout)
        {
            ctxdestination address;
            if (!extractdestination(txout.scriptpubkey, address))
                continue;

            isminefilter mine = ismine(*pwalletmain, address);
            if(!(mine & filter))
                continue;

            tallyitem& item = maptally[address];
            item.namount += txout.nvalue;
            item.nconf = min(item.nconf, ndepth);
            item.txids.push_back(wtx.gethash());
            if (mine & ismine_watch_only)
                item.fiswatchonly = true;
        }
    }

    // reply
    univalue ret(univalue::varr);
    map<string, tallyitem> mapaccounttally;
    boost_foreach(const pairtype(cmoorecoinaddress, caddressbookdata)& item, pwalletmain->mapaddressbook)
    {
        const cmoorecoinaddress& address = item.first;
        const string& straccount = item.second.name;
        map<cmoorecoinaddress, tallyitem>::iterator it = maptally.find(address);
        if (it == maptally.end() && !fincludeempty)
            continue;

        camount namount = 0;
        int nconf = std::numeric_limits<int>::max();
        bool fiswatchonly = false;
        if (it != maptally.end())
        {
            namount = (*it).second.namount;
            nconf = (*it).second.nconf;
            fiswatchonly = (*it).second.fiswatchonly;
        }

        if (fbyaccounts)
        {
            tallyitem& item = mapaccounttally[straccount];
            item.namount += namount;
            item.nconf = min(item.nconf, nconf);
            item.fiswatchonly = fiswatchonly;
        }
        else
        {
            univalue obj(univalue::vobj);
            if(fiswatchonly)
                obj.push_back(pair("involveswatchonly", true));
            obj.push_back(pair("address",       address.tostring()));
            obj.push_back(pair("account",       straccount));
            obj.push_back(pair("amount",        valuefromamount(namount)));
            obj.push_back(pair("confirmations", (nconf == std::numeric_limits<int>::max() ? 0 : nconf)));
            univalue transactions(univalue::varr);
            if (it != maptally.end())
            {
                boost_foreach(const uint256& item, (*it).second.txids)
                {
                    transactions.push_back(item.gethex());
                }
            }
            obj.push_back(pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fbyaccounts)
    {
        for (map<string, tallyitem>::iterator it = mapaccounttally.begin(); it != mapaccounttally.end(); ++it)
        {
            camount namount = (*it).second.namount;
            int nconf = (*it).second.nconf;
            univalue obj(univalue::vobj);
            if((*it).second.fiswatchonly)
                obj.push_back(pair("involveswatchonly", true));
            obj.push_back(pair("account",       (*it).first));
            obj.push_back(pair("amount",        valuefromamount(namount)));
            obj.push_back(pair("confirmations", (nconf == std::numeric_limits<int>::max() ? 0 : nconf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

univalue listreceivedbyaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaddress ( minconf includeempty includewatchonly)\n"
            "\nlist balances by receiving address.\n"
            "\narguments:\n"
            "1. minconf       (numeric, optional, default=1) the minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) whether to include addresses that haven't received any payments.\n"
            "3. includewatchonly (bool, optional, default=false) whether to include watchonly addresses (see 'importaddress').\n"

            "\nresult:\n"
            "[\n"
            "  {\n"
            "    \"involveswatchonly\" : true,        (bool) only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) the receiving address\n"
            "    \"account\" : \"accountname\",       (string) deprecated. the account of the receiving address. the default account is \"\".\n"
            "    \"amount\" : x.xxx,                  (numeric) the total amount in btc received by the address\n"
            "    \"confirmations\" : n                (numeric) the number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nexamples:\n"
            + helpexamplecli("listreceivedbyaddress", "")
            + helpexamplecli("listreceivedbyaddress", "6 true")
            + helpexamplerpc("listreceivedbyaddress", "6, true, true")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    return listreceived(params, false);
}

univalue listreceivedbyaccount(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaccount ( minconf includeempty includewatchonly)\n"
            "\ndeprecated. list balances by account.\n"
            "\narguments:\n"
            "1. minconf      (numeric, optional, default=1) the minimum number of confirmations before payments are included.\n"
            "2. includeempty (boolean, optional, default=false) whether to include accounts that haven't received any payments.\n"
            "3. includewatchonly (bool, optional, default=false) whether to include watchonly addresses (see 'importaddress').\n"

            "\nresult:\n"
            "[\n"
            "  {\n"
            "    \"involveswatchonly\" : true,   (bool) only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) the account name of the receiving account\n"
            "    \"amount\" : x.xxx,             (numeric) the total amount received by addresses with this account\n"
            "    \"confirmations\" : n           (numeric) the number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nexamples:\n"
            + helpexamplecli("listreceivedbyaccount", "")
            + helpexamplecli("listreceivedbyaccount", "6 true")
            + helpexamplerpc("listreceivedbyaccount", "6, true, true")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    return listreceived(params, true);
}

static void maybepushaddress(univalue & entry, const ctxdestination &dest)
{
    cmoorecoinaddress addr;
    if (addr.set(dest))
        entry.push_back(pair("address", addr.tostring()));
}

void listtransactions(const cwallettx& wtx, const string& straccount, int nmindepth, bool flong, univalue& ret, const isminefilter& filter)
{
    camount nfee;
    string strsentaccount;
    list<coutputentry> listreceived;
    list<coutputentry> listsent;

    wtx.getamounts(listreceived, listsent, nfee, strsentaccount, filter);

    bool fallaccounts = (straccount == string("*"));
    bool involveswatchonly = wtx.isfromme(ismine_watch_only);

    // sent
    if ((!listsent.empty() || nfee != 0) && (fallaccounts || straccount == strsentaccount))
    {
        boost_foreach(const coutputentry& s, listsent)
        {
            univalue entry(univalue::vobj);
            if(involveswatchonly || (::ismine(*pwalletmain, s.destination) & ismine_watch_only))
                entry.push_back(pair("involveswatchonly", true));
            entry.push_back(pair("account", strsentaccount));
            maybepushaddress(entry, s.destination);
            entry.push_back(pair("category", "send"));
            entry.push_back(pair("amount", valuefromamount(-s.amount)));
            entry.push_back(pair("vout", s.vout));
            entry.push_back(pair("fee", valuefromamount(-nfee)));
            if (flong)
                wallettxtojson(wtx, entry);
            ret.push_back(entry);
        }
    }

    // received
    if (listreceived.size() > 0 && wtx.getdepthinmainchain() >= nmindepth)
    {
        boost_foreach(const coutputentry& r, listreceived)
        {
            string account;
            if (pwalletmain->mapaddressbook.count(r.destination))
                account = pwalletmain->mapaddressbook[r.destination].name;
            if (fallaccounts || (account == straccount))
            {
                univalue entry(univalue::vobj);
                if(involveswatchonly || (::ismine(*pwalletmain, r.destination) & ismine_watch_only))
                    entry.push_back(pair("involveswatchonly", true));
                entry.push_back(pair("account", account));
                maybepushaddress(entry, r.destination);
                if (wtx.iscoinbase())
                {
                    if (wtx.getdepthinmainchain() < 1)
                        entry.push_back(pair("category", "orphan"));
                    else if (wtx.getblockstomaturity() > 0)
                        entry.push_back(pair("category", "immature"));
                    else
                        entry.push_back(pair("category", "generate"));
                }
                else
                {
                    entry.push_back(pair("category", "receive"));
                }
                entry.push_back(pair("amount", valuefromamount(r.amount)));
                entry.push_back(pair("vout", r.vout));
                if (flong)
                    wallettxtojson(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

void acentrytojson(const caccountingentry& acentry, const string& straccount, univalue& ret)
{
    bool fallaccounts = (straccount == string("*"));

    if (fallaccounts || acentry.straccount == straccount)
    {
        univalue entry(univalue::vobj);
        entry.push_back(pair("account", acentry.straccount));
        entry.push_back(pair("category", "move"));
        entry.push_back(pair("time", acentry.ntime));
        entry.push_back(pair("amount", valuefromamount(acentry.ncreditdebit)));
        entry.push_back(pair("otheraccount", acentry.strotheraccount));
        entry.push_back(pair("comment", acentry.strcomment));
        ret.push_back(entry);
    }
}

univalue listtransactions(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 4)
        throw runtime_error(
            "listtransactions ( \"account\" count from includewatchonly)\n"
            "\nreturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\narguments:\n"
            "1. \"account\"    (string, optional) deprecated. the account name. should be \"*\".\n"
            "2. count          (numeric, optional, default=10) the number of transactions to return\n"
            "3. from           (numeric, optional, default=0) the number of transactions to skip\n"
            "4. includewatchonly (bool, optional, default=false) include transactions to watchonly addresses (see 'importaddress')\n"
            "\nresult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",       (string) deprecated. the account name associated with the transaction. \n"
            "                                                it will be \"\" for the default account.\n"
            "    \"address\":\"moorecoinaddress\",    (string) the moorecoin address of the transaction. not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) the transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,          (numeric) the amount in btc. this is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. it is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) the amount of the fee in btc. this is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) the number of confirmations for the transaction. available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\", (string) the block hash containing the transaction. available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) the block index containing the transaction. available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\", (string) the transaction id. available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) the transaction time in seconds since epoch (midnight jan 1 1970 gmt).\n"
            "    \"timereceived\": xxx,      (numeric) the time received in seconds since epoch (midnight jan 1 1970 gmt). available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) if a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) for the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "  }\n"
            "]\n"

            "\nexamples:\n"
            "\nlist the most recent 10 transactions in the systems\n"
            + helpexamplecli("listtransactions", "") +
            "\nlist transactions 100 to 120\n"
            + helpexamplecli("listtransactions", "\"*\" 20 100") +
            "\nas a json rpc call\n"
            + helpexamplerpc("listtransactions", "\"*\", 20, 100")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    string straccount = "*";
    if (params.size() > 0)
        straccount = params[0].get_str();
    int ncount = 10;
    if (params.size() > 1)
        ncount = params[1].get_int();
    int nfrom = 0;
    if (params.size() > 2)
        nfrom = params[2].get_int();
    isminefilter filter = ismine_spendable;
    if(params.size() > 3)
        if(params[3].get_bool())
            filter = filter | ismine_watch_only;

    if (ncount < 0)
        throw jsonrpcerror(rpc_invalid_parameter, "negative count");
    if (nfrom < 0)
        throw jsonrpcerror(rpc_invalid_parameter, "negative from");

    univalue ret(univalue::varr);

    std::list<caccountingentry> acentries;
    cwallet::txitems txordered = pwalletmain->orderedtxitems(acentries, straccount);

    // iterate backwards until we have ncount items to return:
    for (cwallet::txitems::reverse_iterator it = txordered.rbegin(); it != txordered.rend(); ++it)
    {
        cwallettx *const pwtx = (*it).second.first;
        if (pwtx != 0)
            listtransactions(*pwtx, straccount, 0, true, ret, filter);
        caccountingentry *const pacentry = (*it).second.second;
        if (pacentry != 0)
            acentrytojson(*pacentry, straccount, ret);

        if ((int)ret.size() >= (ncount+nfrom)) break;
    }
    // ret is newest to oldest

    if (nfrom > (int)ret.size())
        nfrom = ret.size();
    if ((nfrom + ncount) > (int)ret.size())
        ncount = ret.size() - nfrom;

    vector<univalue> arrtmp = ret.getvalues();

    vector<univalue>::iterator first = arrtmp.begin();
    std::advance(first, nfrom);
    vector<univalue>::iterator last = arrtmp.begin();
    std::advance(last, nfrom+ncount);

    if (last != arrtmp.end()) arrtmp.erase(last, arrtmp.end());
    if (first != arrtmp.begin()) arrtmp.erase(arrtmp.begin(), first);

    std::reverse(arrtmp.begin(), arrtmp.end()); // return oldest to newest

    ret.clear();
    ret.setarray();
    ret.push_backv(arrtmp);

    return ret;
}

univalue listaccounts(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 2)
        throw runtime_error(
            "listaccounts ( minconf includewatchonly)\n"
            "\ndeprecated. returns object that has account names as keys, account balances as values.\n"
            "\narguments:\n"
            "1. minconf          (numeric, optional, default=1) only include transactions with at least this many confirmations\n"
            "2. includewatchonly (bool, optional, default=false) include balances in watchonly addresses (see 'importaddress')\n"
            "\nresult:\n"
            "{                      (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,  (numeric) the property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"
            "\nexamples:\n"
            "\nlist account balances where there at least 1 confirmation\n"
            + helpexamplecli("listaccounts", "") +
            "\nlist account balances including zero confirmation transactions\n"
            + helpexamplecli("listaccounts", "0") +
            "\nlist account balances for 6 or more confirmations\n"
            + helpexamplecli("listaccounts", "6") +
            "\nas json rpc call\n"
            + helpexamplerpc("listaccounts", "6")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    int nmindepth = 1;
    if (params.size() > 0)
        nmindepth = params[0].get_int();
    isminefilter includewatchonly = ismine_spendable;
    if(params.size() > 1)
        if(params[1].get_bool())
            includewatchonly = includewatchonly | ismine_watch_only;

    map<string, camount> mapaccountbalances;
    boost_foreach(const pairtype(ctxdestination, caddressbookdata)& entry, pwalletmain->mapaddressbook) {
        if (ismine(*pwalletmain, entry.first) & includewatchonly) // this address belongs to me
            mapaccountbalances[entry.second.name] = 0;
    }

    for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); ++it)
    {
        const cwallettx& wtx = (*it).second;
        camount nfee;
        string strsentaccount;
        list<coutputentry> listreceived;
        list<coutputentry> listsent;
        int ndepth = wtx.getdepthinmainchain();
        if (wtx.getblockstomaturity() > 0 || ndepth < 0)
            continue;
        wtx.getamounts(listreceived, listsent, nfee, strsentaccount, includewatchonly);
        mapaccountbalances[strsentaccount] -= nfee;
        boost_foreach(const coutputentry& s, listsent)
            mapaccountbalances[strsentaccount] -= s.amount;
        if (ndepth >= nmindepth)
        {
            boost_foreach(const coutputentry& r, listreceived)
                if (pwalletmain->mapaddressbook.count(r.destination))
                    mapaccountbalances[pwalletmain->mapaddressbook[r.destination].name] += r.amount;
                else
                    mapaccountbalances[""] += r.amount;
        }
    }

    list<caccountingentry> acentries;
    cwalletdb(pwalletmain->strwalletfile).listaccountcreditdebit("*", acentries);
    boost_foreach(const caccountingentry& entry, acentries)
        mapaccountbalances[entry.straccount] += entry.ncreditdebit;

    univalue ret(univalue::vobj);
    boost_foreach(const pairtype(string, camount)& accountbalance, mapaccountbalances) {
        ret.push_back(pair(accountbalance.first, valuefromamount(accountbalance.second)));
    }
    return ret;
}

univalue listsinceblock(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp)
        throw runtime_error(
            "listsinceblock ( \"blockhash\" target-confirmations includewatchonly)\n"
            "\nget all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\narguments:\n"
            "1. \"blockhash\"   (string, optional) the block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) the confirmations required, must be 1 or more\n"
            "3. includewatchonly:        (bool, optional, default=false) include transactions to watchonly addresses (see 'importaddress')"
            "\nresult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",       (string) deprecated. the account name associated with the transaction. will be \"\" for the default account.\n"
            "    \"address\":\"moorecoinaddress\",    (string) the moorecoin address of the transaction. not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) the transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) the amount in btc. this is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. it is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) the amount of the fee in btc. this is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) the number of confirmations for the transaction. available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",     (string) the block hash containing the transaction. available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) the block index containing the transaction. available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) the block time in seconds since epoch (1 jan 1970 gmt).\n"
            "    \"txid\": \"transactionid\",  (string) the transaction id. available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) the transaction time in seconds since epoch (jan 1 1970 gmt).\n"
            "    \"timereceived\": xxx,      (numeric) the time received in seconds since epoch (jan 1 1970 gmt). available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) if a comment is associated with the transaction.\n"
            "    \"to\": \"...\",            (string) if a comment to is associated with the transaction.\n"
             "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) the hash of the last block\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("listsinceblock", "")
            + helpexamplecli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + helpexamplerpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    cblockindex *pindex = null;
    int target_confirms = 1;
    isminefilter filter = ismine_spendable;

    if (params.size() > 0)
    {
        uint256 blockid;

        blockid.sethex(params[0].get_str());
        blockmap::iterator it = mapblockindex.find(blockid);
        if (it != mapblockindex.end())
            pindex = it->second;
    }

    if (params.size() > 1)
    {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter");
    }

    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ismine_watch_only;

    int depth = pindex ? (1 + chainactive.height() - pindex->nheight) : -1;

    univalue transactions(univalue::varr);

    for (map<uint256, cwallettx>::iterator it = pwalletmain->mapwallet.begin(); it != pwalletmain->mapwallet.end(); it++)
    {
        cwallettx tx = (*it).second;

        if (depth == -1 || tx.getdepthinmainchain() < depth)
            listtransactions(tx, "*", 0, true, transactions, filter);
    }

    cblockindex *pblocklast = chainactive[chainactive.height() + 1 - target_confirms];
    uint256 lastblock = pblocklast ? pblocklast->getblockhash() : uint256();

    univalue ret(univalue::vobj);
    ret.push_back(pair("transactions", transactions));
    ret.push_back(pair("lastblock", lastblock.gethex()));

    return ret;
}

univalue gettransaction(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettransaction \"txid\" ( includewatchonly )\n"
            "\nget detailed information about in-wallet transaction <txid>\n"
            "\narguments:\n"
            "1. \"txid\"    (string, required) the transaction id\n"
            "2. \"includewatchonly\"    (bool, optional, default=false) whether to include watchonly addresses in balance calculation and details[]\n"
            "\nresult:\n"
            "{\n"
            "  \"amount\" : x.xxx,        (numeric) the transaction amount in btc\n"
            "  \"confirmations\" : n,     (numeric) the number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) the block hash\n"
            "  \"blockindex\" : xx,       (numeric) the block index\n"
            "  \"blocktime\" : ttt,       (numeric) the time in seconds since epoch (1 jan 1970 gmt)\n"
            "  \"txid\" : \"transactionid\",   (string) the transaction id.\n"
            "  \"time\" : ttt,            (numeric) the transaction time in seconds since epoch (1 jan 1970 gmt)\n"
            "  \"timereceived\" : ttt,    (numeric) the time received in seconds since epoch (1 jan 1970 gmt)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",  (string) deprecated. the account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"moorecoinaddress\",   (string) the moorecoin address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) the category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx                  (numeric) the amount in btc\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) raw data for transaction\n"
            "}\n"

            "\nexamples:\n"
            + helpexamplecli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + helpexamplecli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + helpexamplerpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    uint256 hash;
    hash.sethex(params[0].get_str());

    isminefilter filter = ismine_spendable;
    if(params.size() > 1)
        if(params[1].get_bool())
            filter = filter | ismine_watch_only;

    univalue entry(univalue::vobj);
    if (!pwalletmain->mapwallet.count(hash))
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid or non-wallet transaction id");
    const cwallettx& wtx = pwalletmain->mapwallet[hash];

    camount ncredit = wtx.getcredit(filter);
    camount ndebit = wtx.getdebit(filter);
    camount nnet = ncredit - ndebit;
    camount nfee = (wtx.isfromme(filter) ? wtx.getvalueout() - ndebit : 0);

    entry.push_back(pair("amount", valuefromamount(nnet - nfee)));
    if (wtx.isfromme(filter))
        entry.push_back(pair("fee", valuefromamount(nfee)));

    wallettxtojson(wtx, entry);

    univalue details(univalue::varr);
    listtransactions(wtx, "*", 0, false, details, filter);
    entry.push_back(pair("details", details));

    string strhex = encodehextx(static_cast<ctransaction>(wtx));
    entry.push_back(pair("hex", strhex));

    return entry;
}


univalue backupwallet(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "backupwallet \"destination\"\n"
            "\nsafely copies wallet.dat to destination, which can be a directory or a path with filename.\n"
            "\narguments:\n"
            "1. \"destination\"   (string) the destination directory or file\n"
            "\nexamples:\n"
            + helpexamplecli("backupwallet", "\"backup.dat\"")
            + helpexamplerpc("backupwallet", "\"backup.dat\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    string strdest = params[0].get_str();
    if (!backupwallet(*pwalletmain, strdest))
        throw jsonrpcerror(rpc_wallet_error, "error: wallet backup failed!");

    return nullunivalue;
}


univalue keypoolrefill(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 1)
        throw runtime_error(
            "keypoolrefill ( newsize )\n"
            "\nfills the keypool."
            + helprequiringpassphrase() + "\n"
            "\narguments\n"
            "1. newsize     (numeric, optional, default=100) the new keypool size\n"
            "\nexamples:\n"
            + helpexamplecli("keypoolrefill", "")
            + helpexamplerpc("keypoolrefill", "")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    // 0 is interpreted by topupkeypool() as the default keypool size given by -keypool
    unsigned int kpsize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter, expected valid size.");
        kpsize = (unsigned int)params[0].get_int();
    }

    ensurewalletisunlocked();
    pwalletmain->topupkeypool(kpsize);

    if (pwalletmain->getkeypoolsize() < kpsize)
        throw jsonrpcerror(rpc_wallet_error, "error refreshing keypool.");

    return nullunivalue;
}


static void lockwallet(cwallet* pwallet)
{
    lock(cs_nwalletunlocktime);
    nwalletunlocktime = 0;
    pwallet->lock();
}

univalue walletpassphrase(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (pwalletmain->iscrypted() && (fhelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrase \"passphrase\" timeout\n"
            "\nstores the wallet decryption key in memory for 'timeout' seconds.\n"
            "this is needed prior to performing transactions related to private keys such as sending moorecoins\n"
            "\narguments:\n"
            "1. \"passphrase\"     (string, required) the wallet passphrase\n"
            "2. timeout            (numeric, required) the time to keep the decryption key in seconds.\n"
            "\nnote:\n"
            "issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nexamples:\n"
            "\nunlock the wallet for 60 seconds\n"
            + helpexamplecli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nlock the wallet again (before 60 seconds)\n"
            + helpexamplecli("walletlock", "") +
            "\nas json rpc call\n"
            + helpexamplerpc("walletpassphrase", "\"my pass phrase\", 60")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (fhelp)
        return true;
    if (!pwalletmain->iscrypted())
        throw jsonrpcerror(rpc_wallet_wrong_enc_state, "error: running with an unencrypted wallet, but walletpassphrase was called.");

    // note that the walletpassphrase is stored in params[0] which is not mlock()ed
    securestring strwalletpass;
    strwalletpass.reserve(100);
    // todo: get rid of this .c_str() by implementing securestring::operator=(std::string)
    // alternately, find a way to make params[0] mlock()'d to begin with.
    strwalletpass = params[0].get_str().c_str();

    if (strwalletpass.length() > 0)
    {
        if (!pwalletmain->unlock(strwalletpass))
            throw jsonrpcerror(rpc_wallet_passphrase_incorrect, "error: the wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "stores the wallet decryption key in memory for <timeout> seconds.");

    pwalletmain->topupkeypool();

    int64_t nsleeptime = params[1].get_int64();
    lock(cs_nwalletunlocktime);
    nwalletunlocktime = gettime() + nsleeptime;
    rpcrunlater("lockwallet", boost::bind(lockwallet, pwalletmain), nsleeptime);

    return nullunivalue;
}


univalue walletpassphrasechange(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (pwalletmain->iscrypted() && (fhelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nchanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\narguments:\n"
            "1. \"oldpassphrase\"      (string) the current passphrase\n"
            "2. \"newpassphrase\"      (string) the new passphrase\n"
            "\nexamples:\n"
            + helpexamplecli("walletpassphrasechange", "\"old one\" \"new one\"")
            + helpexamplerpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (fhelp)
        return true;
    if (!pwalletmain->iscrypted())
        throw jsonrpcerror(rpc_wallet_wrong_enc_state, "error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // todo: get rid of these .c_str() calls by implementing securestring::operator=(std::string)
    // alternately, find a way to make params[0] mlock()'d to begin with.
    securestring stroldwalletpass;
    stroldwalletpass.reserve(100);
    stroldwalletpass = params[0].get_str().c_str();

    securestring strnewwalletpass;
    strnewwalletpass.reserve(100);
    strnewwalletpass = params[1].get_str().c_str();

    if (stroldwalletpass.length() < 1 || strnewwalletpass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletmain->changewalletpassphrase(stroldwalletpass, strnewwalletpass))
        throw jsonrpcerror(rpc_wallet_passphrase_incorrect, "error: the wallet passphrase entered was incorrect.");

    return nullunivalue;
}


univalue walletlock(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (pwalletmain->iscrypted() && (fhelp || params.size() != 0))
        throw runtime_error(
            "walletlock\n"
            "\nremoves the wallet encryption key from memory, locking the wallet.\n"
            "after calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nexamples:\n"
            "\nset the passphrase for 2 minutes to perform a transaction\n"
            + helpexamplecli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nperform a send (requires passphrase set)\n"
            + helpexamplecli("sendtoaddress", "\"1m72sfpbz1bppxfhz9m3cdqatr44jvaydd\" 1.0") +
            "\nclear the passphrase since we are done before 2 minutes is up\n"
            + helpexamplecli("walletlock", "") +
            "\nas json rpc call\n"
            + helpexamplerpc("walletlock", "")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (fhelp)
        return true;
    if (!pwalletmain->iscrypted())
        throw jsonrpcerror(rpc_wallet_wrong_enc_state, "error: running with an unencrypted wallet, but walletlock was called.");

    {
        lock(cs_nwalletunlocktime);
        pwalletmain->lock();
        nwalletunlocktime = 0;
    }

    return nullunivalue;
}


univalue encryptwallet(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (!pwalletmain->iscrypted() && (fhelp || params.size() != 1))
        throw runtime_error(
            "encryptwallet \"passphrase\"\n"
            "\nencrypts the wallet with 'passphrase'. this is for first time encryption.\n"
            "after this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "use the walletpassphrase call for this, and then walletlock call.\n"
            "if the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "note that this will shutdown the server.\n"
            "\narguments:\n"
            "1. \"passphrase\"    (string) the pass phrase to encrypt the wallet with. it must be at least 1 character, but should be long.\n"
            "\nexamples:\n"
            "\nencrypt you wallet\n"
            + helpexamplecli("encryptwallet", "\"my pass phrase\"") +
            "\nnow set the passphrase to use the wallet, such as for signing or sending moorecoin\n"
            + helpexamplecli("walletpassphrase", "\"my pass phrase\"") +
            "\nnow we can so something like sign\n"
            + helpexamplecli("signmessage", "\"moorecoinaddress\" \"test message\"") +
            "\nnow lock the wallet again by removing the passphrase\n"
            + helpexamplecli("walletlock", "") +
            "\nas a json rpc call\n"
            + helpexamplerpc("encryptwallet", "\"my pass phrase\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (fhelp)
        return true;
    if (pwalletmain->iscrypted())
        throw jsonrpcerror(rpc_wallet_wrong_enc_state, "error: running with an encrypted wallet, but encryptwallet was called.");

    // todo: get rid of this .c_str() by implementing securestring::operator=(std::string)
    // alternately, find a way to make params[0] mlock()'d to begin with.
    securestring strwalletpass;
    strwalletpass.reserve(100);
    strwalletpass = params[0].get_str().c_str();

    if (strwalletpass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "encrypts the wallet with <passphrase>.");

    if (!pwalletmain->encryptwallet(strwalletpass))
        throw jsonrpcerror(rpc_wallet_encryption_failed, "error: failed to encrypt the wallet.");

    // bdb seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. so:
    startshutdown();
    return "wallet encrypted; moorecoin server stopping, restart to run with encrypted wallet. the keypool has been flushed, you need to make a new backup.";
}

univalue lockunspent(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "lockunspent unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nupdates list of temporarily unspendable outputs.\n"
            "temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "a locked transaction output will not be chosen by automatic coin selection, when spending moorecoins.\n"
            "locks are stored in memory only. nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "also see the listunspent call\n"
            "\narguments:\n"
            "1. unlock            (boolean, required) whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, required) a json array of objects. each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) the transaction id\n"
            "         \"vout\": n         (numeric) the output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nresult:\n"
            "true|false    (boolean) whether the command was successful or not\n"

            "\nexamples:\n"
            "\nlist the unspent transactions\n"
            + helpexamplecli("listunspent", "") +
            "\nlock an unspent transaction\n"
            + helpexamplecli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nlist the locked transactions\n"
            + helpexamplecli("listlockunspent", "") +
            "\nunlock the transaction again\n"
            + helpexamplecli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    if (params.size() == 1)
        rpctypecheck(params, boost::assign::list_of(univalue::vbool));
    else
        rpctypecheck(params, boost::assign::list_of(univalue::vbool)(univalue::varr));

    bool funlock = params[0].get_bool();

    if (params.size() == 1) {
        if (funlock)
            pwalletmain->unlockallcoins();
        return true;
    }

    univalue outputs = params[1].get_array();
    for (unsigned int idx = 0; idx < outputs.size(); idx++) {
        const univalue& output = outputs[idx];
        if (!output.isobject())
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter, expected object");
        const univalue& o = output.get_obj();

        rpctypecheckobj(o, boost::assign::map_list_of("txid", univalue::vstr)("vout", univalue::vnum));

        string txid = find_value(o, "txid").get_str();
        if (!ishex(txid))
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter, expected hex txid");

        int noutput = find_value(o, "vout").get_int();
        if (noutput < 0)
            throw jsonrpcerror(rpc_invalid_parameter, "invalid parameter, vout must be positive");

        coutpoint outpt(uint256s(txid), noutput);

        if (funlock)
            pwalletmain->unlockcoin(outpt);
        else
            pwalletmain->lockcoin(outpt);
    }

    return true;
}

univalue listlockunspent(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 0)
        throw runtime_error(
            "listlockunspent\n"
            "\nreturns list of temporarily unspendable outputs.\n"
            "see the lockunspent call to lock and unlock transactions for spending.\n"
            "\nresult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) the transaction id locked\n"
            "    \"vout\" : n                      (numeric) the vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nexamples:\n"
            "\nlist the unspent transactions\n"
            + helpexamplecli("listunspent", "") +
            "\nlock an unspent transaction\n"
            + helpexamplecli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nlist the locked transactions\n"
            + helpexamplecli("listlockunspent", "") +
            "\nunlock the transaction again\n"
            + helpexamplecli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nas a json rpc call\n"
            + helpexamplerpc("listlockunspent", "")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    vector<coutpoint> voutpts;
    pwalletmain->listlockedcoins(voutpts);

    univalue ret(univalue::varr);

    boost_foreach(coutpoint &outpt, voutpts) {
        univalue o(univalue::vobj);

        o.push_back(pair("txid", outpt.hash.gethex()));
        o.push_back(pair("vout", (int)outpt.n));
        ret.push_back(o);
    }

    return ret;
}

univalue settxfee(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "settxfee amount\n"
            "\nset the transaction fee per kb.\n"
            "\narguments:\n"
            "1. amount         (numeric, required) the transaction fee in btc/kb rounded to the nearest 0.00000001\n"
            "\nresult\n"
            "true|false        (boolean) returns true if successful\n"
            "\nexamples:\n"
            + helpexamplecli("settxfee", "0.00001")
            + helpexamplerpc("settxfee", "0.00001")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    // amount
    camount namount = amountfromvalue(params[0]);

    paytxfee = cfeerate(namount, 1000);
    return true;
}

univalue getwalletinfo(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getwalletinfo\n"
            "returns an object containing various wallet state info.\n"
            "\nresult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,     (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,         (numeric) the total confirmed moorecoin balance of the wallet\n"
            "  \"unconfirmed_balance\": xxx, (numeric) the total unconfirmed moorecoin balance of the wallet\n"
            "  \"immature_balance\": xxxxxx, (numeric) the total immature balance of the wallet\n"
            "  \"txcount\": xxxxxxx,         (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,    (numeric) the timestamp (seconds since gmt epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,        (numeric) how many new keys are pre-generated\n"
            "  \"unlocked_until\": ttt,      (numeric) the timestamp in seconds since epoch (midnight jan 1 1970 gmt) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,         (numeric) the transaction fee configuration, set in btc/kb\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getwalletinfo", "")
            + helpexamplerpc("getwalletinfo", "")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    univalue obj(univalue::vobj);
    obj.push_back(pair("walletversion", pwalletmain->getversion()));
    obj.push_back(pair("balance",       valuefromamount(pwalletmain->getbalance())));
    obj.push_back(pair("unconfirmed_balance", valuefromamount(pwalletmain->getunconfirmedbalance())));
    obj.push_back(pair("immature_balance",    valuefromamount(pwalletmain->getimmaturebalance())));
    obj.push_back(pair("txcount",       (int)pwalletmain->mapwallet.size()));
    obj.push_back(pair("keypoololdest", pwalletmain->getoldestkeypooltime()));
    obj.push_back(pair("keypoolsize",   (int)pwalletmain->getkeypoolsize()));
    if (pwalletmain->iscrypted())
        obj.push_back(pair("unlocked_until", nwalletunlocktime));
    obj.push_back(pair("paytxfee",      valuefromamount(paytxfee.getfeeperk())));
    return obj;
}

univalue resendwallettransactions(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "resendwallettransactions\n"
            "immediately re-broadcast unconfirmed wallet transactions to all peers.\n"
            "intended only for testing; the wallet code periodically re-broadcasts\n"
            "automatically.\n"
            "returns array of transaction ids that were re-broadcast.\n"
            );

    lock2(cs_main, pwalletmain->cs_wallet);

    std::vector<uint256> txids = pwalletmain->resendwallettransactionsbefore(gettime());
    univalue result(univalue::varr);
    boost_foreach(const uint256& txid, txids)
    {
        result.push_back(txid.tostring());
    }
    return result;
}

univalue listunspent(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() > 3)
        throw runtime_error(
            "listunspent ( minconf maxconf  [\"address\",...] )\n"
            "\nreturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "optionally filter to only include txouts paid to specified addresses.\n"
            "results are an array of objects, each of which has:\n"
            "{txid, vout, scriptpubkey, amount, confirmations}\n"
            "\narguments:\n"
            "1. minconf          (numeric, optional, default=1) the minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) the maximum confirmations to filter\n"
            "3. \"addresses\"    (string) a json array of moorecoin addresses to filter\n"
            "    [\n"
            "      \"address\"   (string) moorecoin address\n"
            "      ,...\n"
            "    ]\n"
            "\nresult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",        (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"address\" : \"address\",  (string) the moorecoin address\n"
            "    \"account\" : \"account\",  (string) deprecated. the associated account, or \"\" for the default account\n"
            "    \"scriptpubkey\" : \"key\", (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction amount in btc\n"
            "    \"confirmations\" : n       (numeric) the number of confirmations\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nexamples\n"
            + helpexamplecli("listunspent", "")
            + helpexamplecli("listunspent", "6 9999999 \"[\\\"1pgfqezfmqch1gkd3ra4k18pnj3ttuusqg\\\",\\\"1ltvqcaapedugfkpkmm4mstjcal4dkg8sp\\\"]\"")
            + helpexamplerpc("listunspent", "6, 9999999 \"[\\\"1pgfqezfmqch1gkd3ra4k18pnj3ttuusqg\\\",\\\"1ltvqcaapedugfkpkmm4mstjcal4dkg8sp\\\"]\"")
        );

    rpctypecheck(params, boost::assign::list_of(univalue::vnum)(univalue::vnum)(univalue::varr));

    int nmindepth = 1;
    if (params.size() > 0)
        nmindepth = params[0].get_int();

    int nmaxdepth = 9999999;
    if (params.size() > 1)
        nmaxdepth = params[1].get_int();

    set<cmoorecoinaddress> setaddress;
    if (params.size() > 2) {
        univalue inputs = params[2].get_array();
        for (unsigned int idx = 0; idx < inputs.size(); idx++) {
            const univalue& input = inputs[idx];
            cmoorecoinaddress address(input.get_str());
            if (!address.isvalid())
                throw jsonrpcerror(rpc_invalid_address_or_key, string("invalid moorecoin address: ")+input.get_str());
            if (setaddress.count(address))
                throw jsonrpcerror(rpc_invalid_parameter, string("invalid parameter, duplicated address: ")+input.get_str());
           setaddress.insert(address);
        }
    }

    univalue results(univalue::varr);
    vector<coutput> vecoutputs;
    assert(pwalletmain != null);
    lock2(cs_main, pwalletmain->cs_wallet);
    pwalletmain->availablecoins(vecoutputs, false, null, true);
    boost_foreach(const coutput& out, vecoutputs) {
        if (out.ndepth < nmindepth || out.ndepth > nmaxdepth)
            continue;

        if (setaddress.size()) {
            ctxdestination address;
            if (!extractdestination(out.tx->vout[out.i].scriptpubkey, address))
                continue;

            if (!setaddress.count(address))
                continue;
        }

        camount nvalue = out.tx->vout[out.i].nvalue;
        const cscript& pk = out.tx->vout[out.i].scriptpubkey;
        univalue entry(univalue::vobj);
        entry.push_back(pair("txid", out.tx->gethash().gethex()));
        entry.push_back(pair("vout", out.i));
        ctxdestination address;
        if (extractdestination(out.tx->vout[out.i].scriptpubkey, address)) {
            entry.push_back(pair("address", cmoorecoinaddress(address).tostring()));
            if (pwalletmain->mapaddressbook.count(address))
                entry.push_back(pair("account", pwalletmain->mapaddressbook[address].name));
        }
        entry.push_back(pair("scriptpubkey", hexstr(pk.begin(), pk.end())));
        if (pk.ispaytoscripthash()) {
            ctxdestination address;
            if (extractdestination(pk, address)) {
                const cscriptid& hash = boost::get<cscriptid>(address);
                cscript redeemscript;
                if (pwalletmain->getcscript(hash, redeemscript))
                    entry.push_back(pair("redeemscript", hexstr(redeemscript.begin(), redeemscript.end())));
            }
        }
        entry.push_back(pair("amount",valuefromamount(nvalue)));
        entry.push_back(pair("confirmations",out.ndepth));
        entry.push_back(pair("spendable", out.fspendable));
        results.push_back(entry);
    }

    return results;
}
