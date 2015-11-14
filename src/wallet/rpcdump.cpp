// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "rpcserver.h"
#include "init.h"
#include "main.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet.h"

#include <fstream>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "univalue/univalue.h"

using namespace std;

void ensurewalletisunlocked();
bool ensurewalletisavailable(bool avoidexception);

std::string static encodedumptime(int64_t ntime) {
    return datetimestrformat("%y-%m-%dt%h:%m:%sz", ntime);
}

int64_t static decodedumptime(const std::string &str) {
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%y-%m-%dt%h:%m:%sz"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static encodedumpstring(const std::string &str) {
    std::stringstream ret;
    boost_foreach(unsigned char c, str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << hexstr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string decodedumpstring(const std::string &str) {
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos+2 < str.length()) {
            c = (((str[pos+1]>>6)*9+((str[pos+1]-'0')&15)) << 4) | 
                ((str[pos+2]>>6)*9+((str[pos+2]-'0')&15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

univalue importprivkey(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importprivkey \"moorecoinprivkey\" ( \"label\" rescan )\n"
            "\nadds a private key (as returned by dumpprivkey) to your wallet.\n"
            "\narguments:\n"
            "1. \"moorecoinprivkey\"   (string, required) the private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") an optional label\n"
            "3. rescan               (boolean, optional, default=true) rescan the wallet for transactions\n"
            "\nnote: this call can take minutes to complete if rescan is true.\n"
            "\nexamples:\n"
            "\ndump a private key\n"
            + helpexamplecli("dumpprivkey", "\"myaddress\"") +
            "\nimport the private key with rescan\n"
            + helpexamplecli("importprivkey", "\"mykey\"") +
            "\nimport using a label and without rescan\n"
            + helpexamplecli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nas a json-rpc call\n"
            + helpexamplerpc("importprivkey", "\"mykey\", \"testing\", false")
        );

    if (fprunemode)
        throw jsonrpcerror(rpc_wallet_error, "importing keys is disabled in pruned mode");

    lock2(cs_main, pwalletmain->cs_wallet);

    ensurewalletisunlocked();

    string strsecret = params[0].get_str();
    string strlabel = "";
    if (params.size() > 1)
        strlabel = params[1].get_str();

    // whether to perform rescan after import
    bool frescan = true;
    if (params.size() > 2)
        frescan = params[2].get_bool();

    cmoorecoinsecret vchsecret;
    bool fgood = vchsecret.setstring(strsecret);

    if (!fgood) throw jsonrpcerror(rpc_invalid_address_or_key, "invalid private key encoding");

    ckey key = vchsecret.getkey();
    if (!key.isvalid()) throw jsonrpcerror(rpc_invalid_address_or_key, "private key outside allowed range");

    cpubkey pubkey = key.getpubkey();
    assert(key.verifypubkey(pubkey));
    ckeyid vchaddress = pubkey.getid();
    {
        pwalletmain->markdirty();
        pwalletmain->setaddressbook(vchaddress, strlabel, "receive");

        // don't throw error in case a key is already there
        if (pwalletmain->havekey(vchaddress))
            return nullunivalue;

        pwalletmain->mapkeymetadata[vchaddress].ncreatetime = 1;

        if (!pwalletmain->addkeypubkey(key, pubkey))
            throw jsonrpcerror(rpc_wallet_error, "error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletmain->ntimefirstkey = 1; // 0 would be considered 'no value'

        if (frescan) {
            pwalletmain->scanforwallettransactions(chainactive.genesis(), true);
        }
    }

    return nullunivalue;
}

univalue importaddress(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importaddress \"address\" ( \"label\" rescan )\n"
            "\nadds an address or script (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\narguments:\n"
            "1. \"address\"          (string, required) the address\n"
            "2. \"label\"            (string, optional, default=\"\") an optional label\n"
            "3. rescan               (boolean, optional, default=true) rescan the wallet for transactions\n"
            "\nnote: this call can take minutes to complete if rescan is true.\n"
            "\nexamples:\n"
            "\nimport an address with rescan\n"
            + helpexamplecli("importaddress", "\"myaddress\"") +
            "\nimport using a label without rescan\n"
            + helpexamplecli("importaddress", "\"myaddress\" \"testing\" false") +
            "\nas a json-rpc call\n"
            + helpexamplerpc("importaddress", "\"myaddress\", \"testing\", false")
        );

    if (fprunemode)
        throw jsonrpcerror(rpc_wallet_error, "importing addresses is disabled in pruned mode");

    lock2(cs_main, pwalletmain->cs_wallet);

    cscript script;

    cmoorecoinaddress address(params[0].get_str());
    if (address.isvalid()) {
        script = getscriptfordestination(address.get());
    } else if (ishex(params[0].get_str())) {
        std::vector<unsigned char> data(parsehex(params[0].get_str()));
        script = cscript(data.begin(), data.end());
    } else {
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address or script");
    }

    string strlabel = "";
    if (params.size() > 1)
        strlabel = params[1].get_str();

    // whether to perform rescan after import
    bool frescan = true;
    if (params.size() > 2)
        frescan = params[2].get_bool();

    {
        if (::ismine(*pwalletmain, script) == ismine_spendable)
            throw jsonrpcerror(rpc_wallet_error, "the wallet already contains the private key for this address or script");

        // add to address book or update label
        if (address.isvalid())
            pwalletmain->setaddressbook(address.get(), strlabel, "receive");

        // don't throw error in case an address is already there
        if (pwalletmain->havewatchonly(script))
            return nullunivalue;

        pwalletmain->markdirty();

        if (!pwalletmain->addwatchonly(script))
            throw jsonrpcerror(rpc_wallet_error, "error adding address to wallet");

        if (frescan)
        {
            pwalletmain->scanforwallettransactions(chainactive.genesis(), true);
            pwalletmain->reacceptwallettransactions();
        }
    }

    return nullunivalue;
}

univalue importwallet(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "importwallet \"filename\"\n"
            "\nimports keys from a wallet dump file (see dumpwallet).\n"
            "\narguments:\n"
            "1. \"filename\"    (string, required) the wallet file\n"
            "\nexamples:\n"
            "\ndump the wallet\n"
            + helpexamplecli("dumpwallet", "\"test\"") +
            "\nimport the wallet\n"
            + helpexamplecli("importwallet", "\"test\"") +
            "\nimport using the json rpc call\n"
            + helpexamplerpc("importwallet", "\"test\"")
        );

    if (fprunemode)
        throw jsonrpcerror(rpc_wallet_error, "importing wallets is disabled in pruned mode");

    lock2(cs_main, pwalletmain->cs_wallet);

    ensurewalletisunlocked();

    ifstream file;
    file.open(params[0].get_str().c_str(), std::ios::in | std::ios::ate);
    if (!file.is_open())
        throw jsonrpcerror(rpc_invalid_parameter, "cannot open wallet dump file");

    int64_t ntimebegin = chainactive.tip()->getblocktime();

    bool fgood = true;

    int64_t nfilesize = std::max((int64_t)1, (int64_t)file.tellg());
    file.seekg(0, file.beg);

    pwalletmain->showprogress(_("importing..."), 0); // show progress dialog in gui
    while (file.good()) {
        pwalletmain->showprogress("", std::max(1, std::min(99, (int)(((double)file.tellg() / (double)nfilesize) * 100))));
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;
        cmoorecoinsecret vchsecret;
        if (!vchsecret.setstring(vstr[0]))
            continue;
        ckey key = vchsecret.getkey();
        cpubkey pubkey = key.getpubkey();
        assert(key.verifypubkey(pubkey));
        ckeyid keyid = pubkey.getid();
        if (pwalletmain->havekey(keyid)) {
            logprintf("skipping import of %s (key already present)\n", cmoorecoinaddress(keyid).tostring());
            continue;
        }
        int64_t ntime = decodedumptime(vstr[1]);
        std::string strlabel;
        bool flabel = true;
        for (unsigned int nstr = 2; nstr < vstr.size(); nstr++) {
            if (boost::algorithm::starts_with(vstr[nstr], "#"))
                break;
            if (vstr[nstr] == "change=1")
                flabel = false;
            if (vstr[nstr] == "reserve=1")
                flabel = false;
            if (boost::algorithm::starts_with(vstr[nstr], "label=")) {
                strlabel = decodedumpstring(vstr[nstr].substr(6));
                flabel = true;
            }
        }
        logprintf("importing %s...\n", cmoorecoinaddress(keyid).tostring());
        if (!pwalletmain->addkeypubkey(key, pubkey)) {
            fgood = false;
            continue;
        }
        pwalletmain->mapkeymetadata[keyid].ncreatetime = ntime;
        if (flabel)
            pwalletmain->setaddressbook(keyid, strlabel, "receive");
        ntimebegin = std::min(ntimebegin, ntime);
    }
    file.close();
    pwalletmain->showprogress("", 100); // hide progress dialog in gui

    cblockindex *pindex = chainactive.tip();
    while (pindex && pindex->pprev && pindex->getblocktime() > ntimebegin - 7200)
        pindex = pindex->pprev;

    if (!pwalletmain->ntimefirstkey || ntimebegin < pwalletmain->ntimefirstkey)
        pwalletmain->ntimefirstkey = ntimebegin;

    logprintf("rescanning last %i blocks\n", chainactive.height() - pindex->nheight + 1);
    pwalletmain->scanforwallettransactions(pindex);
    pwalletmain->markdirty();

    if (!fgood)
        throw jsonrpcerror(rpc_wallet_error, "error adding some keys to wallet");

    return nullunivalue;
}

univalue dumpprivkey(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "dumpprivkey \"moorecoinaddress\"\n"
            "\nreveals the private key corresponding to 'moorecoinaddress'.\n"
            "then the importprivkey can be used with this output\n"
            "\narguments:\n"
            "1. \"moorecoinaddress\"   (string, required) the moorecoin address for the private key\n"
            "\nresult:\n"
            "\"key\"                (string) the private key\n"
            "\nexamples:\n"
            + helpexamplecli("dumpprivkey", "\"myaddress\"")
            + helpexamplecli("importprivkey", "\"mykey\"")
            + helpexamplerpc("dumpprivkey", "\"myaddress\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    ensurewalletisunlocked();

    string straddress = params[0].get_str();
    cmoorecoinaddress address;
    if (!address.setstring(straddress))
        throw jsonrpcerror(rpc_invalid_address_or_key, "invalid moorecoin address");
    ckeyid keyid;
    if (!address.getkeyid(keyid))
        throw jsonrpcerror(rpc_type_error, "address does not refer to a key");
    ckey vchsecret;
    if (!pwalletmain->getkey(keyid, vchsecret))
        throw jsonrpcerror(rpc_wallet_error, "private key for address " + straddress + " is not known");
    return cmoorecoinsecret(vchsecret).tostring();
}


univalue dumpwallet(const univalue& params, bool fhelp)
{
    if (!ensurewalletisavailable(fhelp))
        return nullunivalue;
    
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "dumpwallet \"filename\"\n"
            "\ndumps all wallet keys in a human-readable format.\n"
            "\narguments:\n"
            "1. \"filename\"    (string, required) the filename\n"
            "\nexamples:\n"
            + helpexamplecli("dumpwallet", "\"test\"")
            + helpexamplerpc("dumpwallet", "\"test\"")
        );

    lock2(cs_main, pwalletmain->cs_wallet);

    ensurewalletisunlocked();

    ofstream file;
    file.open(params[0].get_str().c_str());
    if (!file.is_open())
        throw jsonrpcerror(rpc_invalid_parameter, "cannot open wallet dump file");

    std::map<ckeyid, int64_t> mapkeybirth;
    std::set<ckeyid> setkeypool;
    pwalletmain->getkeybirthtimes(mapkeybirth);
    pwalletmain->getallreservekeys(setkeypool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, ckeyid> > vkeybirth;
    for (std::map<ckeyid, int64_t>::const_iterator it = mapkeybirth.begin(); it != mapkeybirth.end(); it++) {
        vkeybirth.push_back(std::make_pair(it->second, it->first));
    }
    mapkeybirth.clear();
    std::sort(vkeybirth.begin(), vkeybirth.end());

    // produce output
    file << strprintf("# wallet dump created by moorecoin %s (%s)\n", client_build, client_date);
    file << strprintf("# * created on %s\n", encodedumptime(gettime()));
    file << strprintf("# * best block at time of backup was %i (%s),\n", chainactive.height(), chainactive.tip()->getblockhash().tostring());
    file << strprintf("#   mined on %s\n", encodedumptime(chainactive.tip()->getblocktime()));
    file << "\n";
    for (std::vector<std::pair<int64_t, ckeyid> >::const_iterator it = vkeybirth.begin(); it != vkeybirth.end(); it++) {
        const ckeyid &keyid = it->second;
        std::string strtime = encodedumptime(it->first);
        std::string straddr = cmoorecoinaddress(keyid).tostring();
        ckey key;
        if (pwalletmain->getkey(keyid, key)) {
            if (pwalletmain->mapaddressbook.count(keyid)) {
                file << strprintf("%s %s label=%s # addr=%s\n", cmoorecoinsecret(key).tostring(), strtime, encodedumpstring(pwalletmain->mapaddressbook[keyid].name), straddr);
            } else if (setkeypool.count(keyid)) {
                file << strprintf("%s %s reserve=1 # addr=%s\n", cmoorecoinsecret(key).tostring(), strtime, straddr);
            } else {
                file << strprintf("%s %s change=1 # addr=%s\n", cmoorecoinsecret(key).tostring(), strtime, straddr);
            }
        }
    }
    file << "\n";
    file << "# end of dump\n";
    file.close();
    return nullunivalue;
}
