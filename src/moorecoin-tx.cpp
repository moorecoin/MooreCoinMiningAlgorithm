// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "clientversion.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "core_io.h"
#include "keystore.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/sign.h"
#include "univalue/univalue.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"

#include <stdio.h>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>

using namespace std;

static bool fcreateblank;
static map<string,univalue> registers;

static bool appinitrawtx(int argc, char* argv[])
{
    //
    // parameters
    //
    parseparameters(argc, argv);

    // check for -testnet or -regtest parameter (params() calls are only valid after this clause)
    if (!selectparamsfromcommandline()) {
        fprintf(stderr, "error: invalid combination of -regtest and -testnet.\n");
        return false;
    }

    fcreateblank = getboolarg("-create", false);

    if (argc<2 || mapargs.count("-?") || mapargs.count("-help"))
    {
        // first part of help message is specific to this utility
        std::string strusage = _("moorecoin core moorecoin-tx utility version") + " " + formatfullversion() + "\n\n" +
            _("usage:") + "\n" +
              "  moorecoin-tx [options] <hex-tx> [commands]  " + _("update hex-encoded moorecoin transaction") + "\n" +
              "  moorecoin-tx [options] -create [commands]   " + _("create hex-encoded moorecoin transaction") + "\n" +
              "\n";

        fprintf(stdout, "%s", strusage.c_str());

        strusage = helpmessagegroup(_("options:"));
        strusage += helpmessageopt("-?", _("this help message"));
        strusage += helpmessageopt("-create", _("create new, empty tx."));
        strusage += helpmessageopt("-json", _("select json output"));
        strusage += helpmessageopt("-txid", _("output only the hex-encoded transaction id of the resultant transaction."));
        strusage += helpmessageopt("-regtest", _("enter regression test mode, which uses a special chain in which blocks can be solved instantly."));
        strusage += helpmessageopt("-testnet", _("use the test network"));

        fprintf(stdout, "%s", strusage.c_str());

        strusage = helpmessagegroup(_("commands:"));
        strusage += helpmessageopt("delin=n", _("delete input n from tx"));
        strusage += helpmessageopt("delout=n", _("delete output n from tx"));
        strusage += helpmessageopt("in=txid:vout", _("add input to tx"));
        strusage += helpmessageopt("locktime=n", _("set tx lock time to n"));
        strusage += helpmessageopt("nversion=n", _("set tx version to n"));
        strusage += helpmessageopt("outaddr=value:address", _("add address-based output to tx"));
        strusage += helpmessageopt("outscript=value:script", _("add raw script output to tx"));
        strusage += helpmessageopt("sign=sighash-flags", _("add zero or more signatures to transaction") + ". " +
            _("this command requires json registers:") +
            _("prevtxs=json object") + ", " +
            _("privatekeys=json object") + ". " +
            _("see signrawtransaction docs for format of sighash flags, json objects."));
        fprintf(stdout, "%s", strusage.c_str());

        strusage = helpmessagegroup(_("register commands:"));
        strusage += helpmessageopt("load=name:filename", _("load json file filename into register name"));
        strusage += helpmessageopt("set=name:json-string", _("set register name to given json-string"));
        fprintf(stdout, "%s", strusage.c_str());

        return false;
    }
    return true;
}

static void registersetjson(const string& key, const string& rawjson)
{
    univalue val;
    if (!val.read(rawjson)) {
        string strerr = "cannot parse json for key " + key;
        throw runtime_error(strerr);
    }

    registers[key] = val;
}

static void registerset(const string& strinput)
{
    // separate name:value in string
    size_t pos = strinput.find(':');
    if ((pos == string::npos) ||
        (pos == 0) ||
        (pos == (strinput.size() - 1)))
        throw runtime_error("register input requires name:value");

    string key = strinput.substr(0, pos);
    string valstr = strinput.substr(pos + 1, string::npos);

    registersetjson(key, valstr);
}

static void registerload(const string& strinput)
{
    // separate name:filename in string
    size_t pos = strinput.find(':');
    if ((pos == string::npos) ||
        (pos == 0) ||
        (pos == (strinput.size() - 1)))
        throw runtime_error("register load requires name:filename");

    string key = strinput.substr(0, pos);
    string filename = strinput.substr(pos + 1, string::npos);

    file *f = fopen(filename.c_str(), "r");
    if (!f) {
        string strerr = "cannot open file " + filename;
        throw runtime_error(strerr);
    }

    // load file chunks into one big buffer
    string valstr;
    while ((!feof(f)) && (!ferror(f))) {
        char buf[4096];
        int bread = fread(buf, 1, sizeof(buf), f);
        if (bread <= 0)
            break;

        valstr.insert(valstr.size(), buf, bread);
    }

    if (ferror(f)) {
        string strerr = "error reading file " + filename;
        throw runtime_error(strerr);
    }

    fclose(f);

    // evaluate as json buffer register
    registersetjson(key, valstr);
}

static void mutatetxversion(cmutabletransaction& tx, const string& cmdval)
{
    int64_t newversion = atoi64(cmdval);
    if (newversion < 1 || newversion > ctransaction::current_version)
        throw runtime_error("invalid tx version requested");

    tx.nversion = (int) newversion;
}

static void mutatetxlocktime(cmutabletransaction& tx, const string& cmdval)
{
    int64_t newlocktime = atoi64(cmdval);
    if (newlocktime < 0ll || newlocktime > 0xffffffffll)
        throw runtime_error("invalid tx locktime requested");

    tx.nlocktime = (unsigned int) newlocktime;
}

static void mutatetxaddinput(cmutabletransaction& tx, const string& strinput)
{
    // separate txid:vout in string
    size_t pos = strinput.find(':');
    if ((pos == string::npos) ||
        (pos == 0) ||
        (pos == (strinput.size() - 1)))
        throw runtime_error("tx input missing separator");

    // extract and validate txid
    string strtxid = strinput.substr(0, pos);
    if ((strtxid.size() != 64) || !ishex(strtxid))
        throw runtime_error("invalid tx input txid");
    uint256 txid(uint256s(strtxid));

    static const unsigned int mintxoutsz = 9;
    static const unsigned int maxvout = max_block_size / mintxoutsz;

    // extract and validate vout
    string strvout = strinput.substr(pos + 1, string::npos);
    int vout = atoi(strvout);
    if ((vout < 0) || (vout > (int)maxvout))
        throw runtime_error("invalid tx input vout");

    // append to transaction input list
    ctxin txin(txid, vout);
    tx.vin.push_back(txin);
}

static void mutatetxaddoutaddr(cmutabletransaction& tx, const string& strinput)
{
    // separate value:address in string
    size_t pos = strinput.find(':');
    if ((pos == string::npos) ||
        (pos == 0) ||
        (pos == (strinput.size() - 1)))
        throw runtime_error("tx output missing separator");

    // extract and validate value
    string strvalue = strinput.substr(0, pos);
    camount value;
    if (!parsemoney(strvalue, value))
        throw runtime_error("invalid tx output value");

    // extract and validate address
    string straddr = strinput.substr(pos + 1, string::npos);
    cmoorecoinaddress addr(straddr);
    if (!addr.isvalid())
        throw runtime_error("invalid tx output address");

    // build standard output script via getscriptfordestination()
    cscript scriptpubkey = getscriptfordestination(addr.get());

    // construct txout, append to transaction output list
    ctxout txout(value, scriptpubkey);
    tx.vout.push_back(txout);
}

static void mutatetxaddoutscript(cmutabletransaction& tx, const string& strinput)
{
    // separate value:script in string
    size_t pos = strinput.find(':');
    if ((pos == string::npos) ||
        (pos == 0))
        throw runtime_error("tx output missing separator");

    // extract and validate value
    string strvalue = strinput.substr(0, pos);
    camount value;
    if (!parsemoney(strvalue, value))
        throw runtime_error("invalid tx output value");

    // extract and validate script
    string strscript = strinput.substr(pos + 1, string::npos);
    cscript scriptpubkey = parsescript(strscript); // throws on err

    // construct txout, append to transaction output list
    ctxout txout(value, scriptpubkey);
    tx.vout.push_back(txout);
}

static void mutatetxdelinput(cmutabletransaction& tx, const string& strinidx)
{
    // parse requested deletion index
    int inidx = atoi(strinidx);
    if (inidx < 0 || inidx >= (int)tx.vin.size()) {
        string strerr = "invalid tx input index '" + strinidx + "'";
        throw runtime_error(strerr.c_str());
    }

    // delete input from transaction
    tx.vin.erase(tx.vin.begin() + inidx);
}

static void mutatetxdeloutput(cmutabletransaction& tx, const string& stroutidx)
{
    // parse requested deletion index
    int outidx = atoi(stroutidx);
    if (outidx < 0 || outidx >= (int)tx.vout.size()) {
        string strerr = "invalid tx output index '" + stroutidx + "'";
        throw runtime_error(strerr.c_str());
    }

    // delete output from transaction
    tx.vout.erase(tx.vout.begin() + outidx);
}

static const unsigned int n_sighash_opts = 6;
static const struct {
    const char *flagstr;
    int flags;
} sighashoptions[n_sighash_opts] = {
    {"all", sighash_all},
    {"none", sighash_none},
    {"single", sighash_single},
    {"all|anyonecanpay", sighash_all|sighash_anyonecanpay},
    {"none|anyonecanpay", sighash_none|sighash_anyonecanpay},
    {"single|anyonecanpay", sighash_single|sighash_anyonecanpay},
};

static bool findsighashflags(int& flags, const string& flagstr)
{
    flags = 0;

    for (unsigned int i = 0; i < n_sighash_opts; i++) {
        if (flagstr == sighashoptions[i].flagstr) {
            flags = sighashoptions[i].flags;
            return true;
        }
    }

    return false;
}

uint256 parsehashuo(map<string,univalue>& o, string strkey)
{
    if (!o.count(strkey))
        return uint256();
    return parsehashuv(o[strkey], strkey);
}

vector<unsigned char> parsehexuo(map<string,univalue>& o, string strkey)
{
    if (!o.count(strkey)) {
        vector<unsigned char> emptyvec;
        return emptyvec;
    }
    return parsehexuv(o[strkey], strkey);
}

static void mutatetxsign(cmutabletransaction& tx, const string& flagstr)
{
    int nhashtype = sighash_all;

    if (flagstr.size() > 0)
        if (!findsighashflags(nhashtype, flagstr))
            throw runtime_error("unknown sighash flag/sign option");

    vector<ctransaction> txvariants;
    txvariants.push_back(tx);

    // mergedtx will end up with all the signatures; it
    // starts as a clone of the raw tx:
    cmutabletransaction mergedtx(txvariants[0]);
    bool fcomplete = true;
    ccoinsview viewdummy;
    ccoinsviewcache view(&viewdummy);

    if (!registers.count("privatekeys"))
        throw runtime_error("privatekeys register variable must be set.");
    bool fgivenkeys = false;
    cbasickeystore tempkeystore;
    univalue keysobj = registers["privatekeys"];
    fgivenkeys = true;

    for (unsigned int kidx = 0; kidx < keysobj.size(); kidx++) {
        if (!keysobj[kidx].isstr())
            throw runtime_error("privatekey not a string");
        cmoorecoinsecret vchsecret;
        bool fgood = vchsecret.setstring(keysobj[kidx].getvalstr());
        if (!fgood)
            throw runtime_error("privatekey not valid");

        ckey key = vchsecret.getkey();
        tempkeystore.addkey(key);
    }

    // add previous txouts given in the rpc call:
    if (!registers.count("prevtxs"))
        throw runtime_error("prevtxs register variable must be set.");
    univalue prevtxsobj = registers["prevtxs"];
    {
        for (unsigned int previdx = 0; previdx < prevtxsobj.size(); previdx++) {
            univalue prevout = prevtxsobj[previdx];
            if (!prevout.isobject())
                throw runtime_error("expected prevtxs internal object");

            map<string,univalue::vtype> types = boost::assign::map_list_of("txid", univalue::vstr)("vout",univalue::vnum)("scriptpubkey",univalue::vstr);
            if (!prevout.checkobject(types))
                throw runtime_error("prevtxs internal object typecheck fail");

            uint256 txid = parsehashuv(prevout["txid"], "txid");

            int nout = atoi(prevout["vout"].getvalstr());
            if (nout < 0)
                throw runtime_error("vout must be positive");

            vector<unsigned char> pkdata(parsehexuv(prevout["scriptpubkey"], "scriptpubkey"));
            cscript scriptpubkey(pkdata.begin(), pkdata.end());

            {
                ccoinsmodifier coins = view.modifycoins(txid);
                if (coins->isavailable(nout) && coins->vout[nout].scriptpubkey != scriptpubkey) {
                    string err("previous output scriptpubkey mismatch:\n");
                    err = err + coins->vout[nout].scriptpubkey.tostring() + "\nvs:\n"+
                        scriptpubkey.tostring();
                    throw runtime_error(err);
                }
                if ((unsigned int)nout >= coins->vout.size())
                    coins->vout.resize(nout+1);
                coins->vout[nout].scriptpubkey = scriptpubkey;
                coins->vout[nout].nvalue = 0; // we don't know the actual output value
            }

            // if redeemscript given and private keys given,
            // add redeemscript to the tempkeystore so it can be signed:
            if (fgivenkeys && scriptpubkey.ispaytoscripthash() &&
                prevout.exists("redeemscript")) {
                univalue v = prevout["redeemscript"];
                vector<unsigned char> rsdata(parsehexuv(v, "redeemscript"));
                cscript redeemscript(rsdata.begin(), rsdata.end());
                tempkeystore.addcscript(redeemscript);
            }
        }
    }

    const ckeystore& keystore = tempkeystore;

    bool fhashsingle = ((nhashtype & ~sighash_anyonecanpay) == sighash_single);

    // sign what we can:
    for (unsigned int i = 0; i < mergedtx.vin.size(); i++) {
        ctxin& txin = mergedtx.vin[i];
        const ccoins* coins = view.accesscoins(txin.prevout.hash);
        if (!coins || !coins->isavailable(txin.prevout.n)) {
            fcomplete = false;
            continue;
        }
        const cscript& prevpubkey = coins->vout[txin.prevout.n].scriptpubkey;

        txin.scriptsig.clear();
        // only sign sighash_single if there's a corresponding output:
        if (!fhashsingle || (i < mergedtx.vout.size()))
            signsignature(keystore, prevpubkey, mergedtx, i, nhashtype);

        // ... and merge in other signatures:
        boost_foreach(const ctransaction& txv, txvariants) {
            txin.scriptsig = combinesignatures(prevpubkey, mergedtx, i, txin.scriptsig, txv.vin[i].scriptsig);
        }
        if (!verifyscript(txin.scriptsig, prevpubkey, standard_script_verify_flags, mutabletransactionsignaturechecker(&mergedtx, i)))
            fcomplete = false;
    }

    if (fcomplete) {
        // do nothing... for now
        // perhaps store this for later optional json output
    }

    tx = mergedtx;
}

class secp256k1init
{
public:
    secp256k1init() { ecc_start(); }
    ~secp256k1init() { ecc_stop(); }
};

static void mutatetx(cmutabletransaction& tx, const string& command,
                     const string& commandval)
{
    boost::scoped_ptr<secp256k1init> ecc;

    if (command == "nversion")
        mutatetxversion(tx, commandval);
    else if (command == "locktime")
        mutatetxlocktime(tx, commandval);

    else if (command == "delin")
        mutatetxdelinput(tx, commandval);
    else if (command == "in")
        mutatetxaddinput(tx, commandval);

    else if (command == "delout")
        mutatetxdeloutput(tx, commandval);
    else if (command == "outaddr")
        mutatetxaddoutaddr(tx, commandval);
    else if (command == "outscript")
        mutatetxaddoutscript(tx, commandval);

    else if (command == "sign") {
        if (!ecc) { ecc.reset(new secp256k1init()); }
        mutatetxsign(tx, commandval);
    }

    else if (command == "load")
        registerload(commandval);

    else if (command == "set")
        registerset(commandval);

    else
        throw runtime_error("unknown command");
}

static void outputtxjson(const ctransaction& tx)
{
    univalue entry(univalue::vobj);
    txtouniv(tx, uint256(), entry);

    string jsonoutput = entry.write(4);
    fprintf(stdout, "%s\n", jsonoutput.c_str());
}

static void outputtxhash(const ctransaction& tx)
{
    string strhexhash = tx.gethash().gethex(); // the hex-encoded transaction hash (aka the transaction id)

    fprintf(stdout, "%s\n", strhexhash.c_str());
}

static void outputtxhex(const ctransaction& tx)
{
    string strhex = encodehextx(tx);

    fprintf(stdout, "%s\n", strhex.c_str());
}

static void outputtx(const ctransaction& tx)
{
    if (getboolarg("-json", false))
        outputtxjson(tx);
    else if (getboolarg("-txid", false))
        outputtxhash(tx);
    else
        outputtxhex(tx);
}

static string readstdin()
{
    char buf[4096];
    string ret;

    while (!feof(stdin)) {
        size_t bread = fread(buf, 1, sizeof(buf), stdin);
        ret.append(buf, bread);
        if (bread < sizeof(buf))
            break;
    }

    if (ferror(stdin))
        throw runtime_error("error reading stdin");

    boost::algorithm::trim_right(ret);

    return ret;
}

static int commandlinerawtx(int argc, char* argv[])
{
    string strprint;
    int nret = 0;
    try {
        // skip switches; permit common stdin convention "-"
        while (argc > 1 && isswitchchar(argv[1][0]) &&
               (argv[1][1] != 0)) {
            argc--;
            argv++;
        }

        ctransaction txdecodetmp;
        int startarg;

        if (!fcreateblank) {
            // require at least one param
            if (argc < 2)
                throw runtime_error("too few parameters");

            // param: hex-encoded moorecoin transaction
            string strhextx(argv[1]);
            if (strhextx == "-")                 // "-" implies standard input
                strhextx = readstdin();

            if (!decodehextx(txdecodetmp, strhextx))
                throw runtime_error("invalid transaction encoding");

            startarg = 2;
        } else
            startarg = 1;

        cmutabletransaction tx(txdecodetmp);

        for (int i = startarg; i < argc; i++) {
            string arg = argv[i];
            string key, value;
            size_t eqpos = arg.find('=');
            if (eqpos == string::npos)
                key = arg;
            else {
                key = arg.substr(0, eqpos);
                value = arg.substr(eqpos + 1);
            }

            mutatetx(tx, key, value);
        }

        outputtx(tx);
    }

    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (const std::exception& e) {
        strprint = string("error: ") + e.what();
        nret = exit_failure;
    }
    catch (...) {
        printexceptioncontinue(null, "commandlinerawtx()");
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
        if(!appinitrawtx(argc, argv))
            return exit_failure;
    }
    catch (const std::exception& e) {
        printexceptioncontinue(&e, "appinitrawtx()");
        return exit_failure;
    } catch (...) {
        printexceptioncontinue(null, "appinitrawtx()");
        return exit_failure;
    }

    int ret = exit_failure;
    try {
        ret = commandlinerawtx(argc, argv);
    }
    catch (const std::exception& e) {
        printexceptioncontinue(&e, "commandlinerawtx()");
    } catch (...) {
        printexceptioncontinue(null, "commandlinerawtx()");
    }
    return ret;
}
