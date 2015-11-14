// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb.h"

#include "base58.h"
#include "consensus/validation.h"
#include "main.h"
#include "protocol.h"
#include "serialize.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet/wallet.h"

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

using namespace std;

static uint64_t naccountingentrynumber = 0;

//
// cwalletdb
//

bool cwalletdb::writename(const string& straddress, const string& strname)
{
    nwalletdbupdated++;
    return write(make_pair(string("name"), straddress), strname);
}

bool cwalletdb::erasename(const string& straddress)
{
    // this should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nwalletdbupdated++;
    return erase(make_pair(string("name"), straddress));
}

bool cwalletdb::writepurpose(const string& straddress, const string& strpurpose)
{
    nwalletdbupdated++;
    return write(make_pair(string("purpose"), straddress), strpurpose);
}

bool cwalletdb::erasepurpose(const string& strpurpose)
{
    nwalletdbupdated++;
    return erase(make_pair(string("purpose"), strpurpose));
}

bool cwalletdb::writetx(uint256 hash, const cwallettx& wtx)
{
    nwalletdbupdated++;
    return write(std::make_pair(std::string("tx"), hash), wtx);
}

bool cwalletdb::erasetx(uint256 hash)
{
    nwalletdbupdated++;
    return erase(std::make_pair(std::string("tx"), hash));
}

bool cwalletdb::writekey(const cpubkey& vchpubkey, const cprivkey& vchprivkey, const ckeymetadata& keymeta)
{
    nwalletdbupdated++;

    if (!write(std::make_pair(std::string("keymeta"), vchpubkey),
               keymeta, false))
        return false;

    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchkey;
    vchkey.reserve(vchpubkey.size() + vchprivkey.size());
    vchkey.insert(vchkey.end(), vchpubkey.begin(), vchpubkey.end());
    vchkey.insert(vchkey.end(), vchprivkey.begin(), vchprivkey.end());

    return write(std::make_pair(std::string("key"), vchpubkey), std::make_pair(vchprivkey, hash(vchkey.begin(), vchkey.end())), false);
}

bool cwalletdb::writecryptedkey(const cpubkey& vchpubkey,
                                const std::vector<unsigned char>& vchcryptedsecret,
                                const ckeymetadata &keymeta)
{
    const bool feraseunencryptedkey = true;
    nwalletdbupdated++;

    if (!write(std::make_pair(std::string("keymeta"), vchpubkey),
            keymeta))
        return false;

    if (!write(std::make_pair(std::string("ckey"), vchpubkey), vchcryptedsecret, false))
        return false;
    if (feraseunencryptedkey)
    {
        erase(std::make_pair(std::string("key"), vchpubkey));
        erase(std::make_pair(std::string("wkey"), vchpubkey));
    }
    return true;
}

bool cwalletdb::writemasterkey(unsigned int nid, const cmasterkey& kmasterkey)
{
    nwalletdbupdated++;
    return write(std::make_pair(std::string("mkey"), nid), kmasterkey, true);
}

bool cwalletdb::writecscript(const uint160& hash, const cscript& redeemscript)
{
    nwalletdbupdated++;
    return write(std::make_pair(std::string("cscript"), hash), redeemscript, false);
}

bool cwalletdb::writewatchonly(const cscript &dest)
{
    nwalletdbupdated++;
    return write(std::make_pair(std::string("watchs"), dest), '1');
}

bool cwalletdb::erasewatchonly(const cscript &dest)
{
    nwalletdbupdated++;
    return erase(std::make_pair(std::string("watchs"), dest));
}

bool cwalletdb::writebestblock(const cblocklocator& locator)
{
    nwalletdbupdated++;
    return write(std::string("bestblock"), locator);
}

bool cwalletdb::readbestblock(cblocklocator& locator)
{
    return read(std::string("bestblock"), locator);
}

bool cwalletdb::writeorderposnext(int64_t norderposnext)
{
    nwalletdbupdated++;
    return write(std::string("orderposnext"), norderposnext);
}

bool cwalletdb::writedefaultkey(const cpubkey& vchpubkey)
{
    nwalletdbupdated++;
    return write(std::string("defaultkey"), vchpubkey);
}

bool cwalletdb::readpool(int64_t npool, ckeypool& keypool)
{
    return read(std::make_pair(std::string("pool"), npool), keypool);
}

bool cwalletdb::writepool(int64_t npool, const ckeypool& keypool)
{
    nwalletdbupdated++;
    return write(std::make_pair(std::string("pool"), npool), keypool);
}

bool cwalletdb::erasepool(int64_t npool)
{
    nwalletdbupdated++;
    return erase(std::make_pair(std::string("pool"), npool));
}

bool cwalletdb::writeminversion(int nversion)
{
    return write(std::string("minversion"), nversion);
}

bool cwalletdb::readaccount(const string& straccount, caccount& account)
{
    account.setnull();
    return read(make_pair(string("acc"), straccount), account);
}

bool cwalletdb::writeaccount(const string& straccount, const caccount& account)
{
    return write(make_pair(string("acc"), straccount), account);
}

bool cwalletdb::writeaccountingentry(const uint64_t naccentrynum, const caccountingentry& acentry)
{
    return write(std::make_pair(std::string("acentry"), std::make_pair(acentry.straccount, naccentrynum)), acentry);
}

bool cwalletdb::writeaccountingentry(const caccountingentry& acentry)
{
    return writeaccountingentry(++naccountingentrynumber, acentry);
}

camount cwalletdb::getaccountcreditdebit(const string& straccount)
{
    list<caccountingentry> entries;
    listaccountcreditdebit(straccount, entries);

    camount ncreditdebit = 0;
    boost_foreach (const caccountingentry& entry, entries)
        ncreditdebit += entry.ncreditdebit;

    return ncreditdebit;
}

void cwalletdb::listaccountcreditdebit(const string& straccount, list<caccountingentry>& entries)
{
    bool fallaccounts = (straccount == "*");

    dbc* pcursor = getcursor();
    if (!pcursor)
        throw runtime_error("cwalletdb::listaccountcreditdebit(): cannot create db cursor");
    unsigned int fflags = db_set_range;
    while (true)
    {
        // read next record
        cdatastream sskey(ser_disk, client_version);
        if (fflags == db_set_range)
            sskey << std::make_pair(std::string("acentry"), std::make_pair((fallaccounts ? string("") : straccount), uint64_t(0)));
        cdatastream ssvalue(ser_disk, client_version);
        int ret = readatcursor(pcursor, sskey, ssvalue, fflags);
        fflags = db_next;
        if (ret == db_notfound)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw runtime_error("cwalletdb::listaccountcreditdebit(): error scanning db");
        }

        // unserialize
        string strtype;
        sskey >> strtype;
        if (strtype != "acentry")
            break;
        caccountingentry acentry;
        sskey >> acentry.straccount;
        if (!fallaccounts && acentry.straccount != straccount)
            break;

        ssvalue >> acentry;
        sskey >> acentry.nentryno;
        entries.push_back(acentry);
    }

    pcursor->close();
}

dberrors cwalletdb::reordertransactions(cwallet* pwallet)
{
    lock(pwallet->cs_wallet);
    // old wallets didn't have any defined order for transactions
    // probably a bad idea to change the output of this

    // first: get all cwallettx and caccountingentry into a sorted-by-time multimap.
    typedef pair<cwallettx*, caccountingentry*> txpair;
    typedef multimap<int64_t, txpair > txitems;
    txitems txbytime;

    for (map<uint256, cwallettx>::iterator it = pwallet->mapwallet.begin(); it != pwallet->mapwallet.end(); ++it)
    {
        cwallettx* wtx = &((*it).second);
        txbytime.insert(make_pair(wtx->ntimereceived, txpair(wtx, (caccountingentry*)0)));
    }
    list<caccountingentry> acentries;
    listaccountcreditdebit("", acentries);
    boost_foreach(caccountingentry& entry, acentries)
    {
        txbytime.insert(make_pair(entry.ntime, txpair((cwallettx*)0, &entry)));
    }

    int64_t& norderposnext = pwallet->norderposnext;
    norderposnext = 0;
    std::vector<int64_t> norderposoffsets;
    for (txitems::iterator it = txbytime.begin(); it != txbytime.end(); ++it)
    {
        cwallettx *const pwtx = (*it).second.first;
        caccountingentry *const pacentry = (*it).second.second;
        int64_t& norderpos = (pwtx != 0) ? pwtx->norderpos : pacentry->norderpos;

        if (norderpos == -1)
        {
            norderpos = norderposnext++;
            norderposoffsets.push_back(norderpos);

            if (pwtx)
            {
                if (!writetx(pwtx->gethash(), *pwtx))
                    return db_load_fail;
            }
            else
                if (!writeaccountingentry(pacentry->nentryno, *pacentry))
                    return db_load_fail;
        }
        else
        {
            int64_t norderposoff = 0;
            boost_foreach(const int64_t& noffsetstart, norderposoffsets)
            {
                if (norderpos >= noffsetstart)
                    ++norderposoff;
            }
            norderpos += norderposoff;
            norderposnext = std::max(norderposnext, norderpos + 1);

            if (!norderposoff)
                continue;

            // since we're changing the order, write it back
            if (pwtx)
            {
                if (!writetx(pwtx->gethash(), *pwtx))
                    return db_load_fail;
            }
            else
                if (!writeaccountingentry(pacentry->nentryno, *pacentry))
                    return db_load_fail;
        }
    }
    writeorderposnext(norderposnext);

    return db_load_ok;
}

class cwalletscanstate {
public:
    unsigned int nkeys;
    unsigned int nckeys;
    unsigned int nkeymeta;
    bool fisencrypted;
    bool fanyunordered;
    int nfileversion;
    vector<uint256> vwalletupgrade;

    cwalletscanstate() {
        nkeys = nckeys = nkeymeta = 0;
        fisencrypted = false;
        fanyunordered = false;
        nfileversion = 0;
    }
};

bool
readkeyvalue(cwallet* pwallet, cdatastream& sskey, cdatastream& ssvalue,
             cwalletscanstate &wss, string& strtype, string& strerr)
{
    try {
        // unserialize
        // taking advantage of the fact that pair serialization
        // is just the two items serialized one after the other
        sskey >> strtype;
        if (strtype == "name")
        {
            string straddress;
            sskey >> straddress;
            ssvalue >> pwallet->mapaddressbook[cmoorecoinaddress(straddress).get()].name;
        }
        else if (strtype == "purpose")
        {
            string straddress;
            sskey >> straddress;
            ssvalue >> pwallet->mapaddressbook[cmoorecoinaddress(straddress).get()].purpose;
        }
        else if (strtype == "tx")
        {
            uint256 hash;
            sskey >> hash;
            cwallettx wtx;
            ssvalue >> wtx;
            cvalidationstate state;
            if (!(checktransaction(wtx, state) && (wtx.gethash() == hash) && state.isvalid()))
                return false;

            // undo serialize changes in 31600
            if (31404 <= wtx.ftimereceivedistxtime && wtx.ftimereceivedistxtime <= 31703)
            {
                if (!ssvalue.empty())
                {
                    char ftmp;
                    char funused;
                    ssvalue >> ftmp >> funused >> wtx.strfromaccount;
                    strerr = strprintf("loadwallet() upgrading tx ver=%d %d '%s' %s",
                                       wtx.ftimereceivedistxtime, ftmp, wtx.strfromaccount, hash.tostring());
                    wtx.ftimereceivedistxtime = ftmp;
                }
                else
                {
                    strerr = strprintf("loadwallet() repairing tx ver=%d %s", wtx.ftimereceivedistxtime, hash.tostring());
                    wtx.ftimereceivedistxtime = 0;
                }
                wss.vwalletupgrade.push_back(hash);
            }

            if (wtx.norderpos == -1)
                wss.fanyunordered = true;

            pwallet->addtowallet(wtx, true, null);
        }
        else if (strtype == "acentry")
        {
            string straccount;
            sskey >> straccount;
            uint64_t nnumber;
            sskey >> nnumber;
            if (nnumber > naccountingentrynumber)
                naccountingentrynumber = nnumber;

            if (!wss.fanyunordered)
            {
                caccountingentry acentry;
                ssvalue >> acentry;
                if (acentry.norderpos == -1)
                    wss.fanyunordered = true;
            }
        }
        else if (strtype == "watchs")
        {
            cscript script;
            sskey >> script;
            char fyes;
            ssvalue >> fyes;
            if (fyes == '1')
                pwallet->loadwatchonly(script);

            // watch-only addresses have no birthday information for now,
            // so set the wallet birthday to the beginning of time.
            pwallet->ntimefirstkey = 1;
        }
        else if (strtype == "key" || strtype == "wkey")
        {
            cpubkey vchpubkey;
            sskey >> vchpubkey;
            if (!vchpubkey.isvalid())
            {
                strerr = "error reading wallet database: cpubkey corrupt";
                return false;
            }
            ckey key;
            cprivkey pkey;
            uint256 hash;

            if (strtype == "key")
            {
                wss.nkeys++;
                ssvalue >> pkey;
            } else {
                cwalletkey wkey;
                ssvalue >> wkey;
                pkey = wkey.vchprivkey;
            }

            // old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the public key is re-derived from the private key
            // using ec operations as a checksum.
            // newer wallets store keys as "key"[pubkey] => [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            try
            {
                ssvalue >> hash;
            }
            catch (...) {}

            bool fskipcheck = false;

            if (!hash.isnull())
            {
                // hash pubkey/privkey to accelerate wallet load
                std::vector<unsigned char> vchkey;
                vchkey.reserve(vchpubkey.size() + pkey.size());
                vchkey.insert(vchkey.end(), vchpubkey.begin(), vchpubkey.end());
                vchkey.insert(vchkey.end(), pkey.begin(), pkey.end());

                if (hash(vchkey.begin(), vchkey.end()) != hash)
                {
                    strerr = "error reading wallet database: cpubkey/cprivkey corrupt";
                    return false;
                }

                fskipcheck = true;
            }

            if (!key.load(pkey, vchpubkey, fskipcheck))
            {
                strerr = "error reading wallet database: cprivkey corrupt";
                return false;
            }
            if (!pwallet->loadkey(key, vchpubkey))
            {
                strerr = "error reading wallet database: loadkey failed";
                return false;
            }
        }
        else if (strtype == "mkey")
        {
            unsigned int nid;
            sskey >> nid;
            cmasterkey kmasterkey;
            ssvalue >> kmasterkey;
            if(pwallet->mapmasterkeys.count(nid) != 0)
            {
                strerr = strprintf("error reading wallet database: duplicate cmasterkey id %u", nid);
                return false;
            }
            pwallet->mapmasterkeys[nid] = kmasterkey;
            if (pwallet->nmasterkeymaxid < nid)
                pwallet->nmasterkeymaxid = nid;
        }
        else if (strtype == "ckey")
        {
            vector<unsigned char> vchpubkey;
            sskey >> vchpubkey;
            vector<unsigned char> vchprivkey;
            ssvalue >> vchprivkey;
            wss.nckeys++;

            if (!pwallet->loadcryptedkey(vchpubkey, vchprivkey))
            {
                strerr = "error reading wallet database: loadcryptedkey failed";
                return false;
            }
            wss.fisencrypted = true;
        }
        else if (strtype == "keymeta")
        {
            cpubkey vchpubkey;
            sskey >> vchpubkey;
            ckeymetadata keymeta;
            ssvalue >> keymeta;
            wss.nkeymeta++;

            pwallet->loadkeymetadata(vchpubkey, keymeta);

            // find earliest key creation time, as wallet birthday
            if (!pwallet->ntimefirstkey ||
                (keymeta.ncreatetime < pwallet->ntimefirstkey))
                pwallet->ntimefirstkey = keymeta.ncreatetime;
        }
        else if (strtype == "defaultkey")
        {
            ssvalue >> pwallet->vchdefaultkey;
        }
        else if (strtype == "pool")
        {
            int64_t nindex;
            sskey >> nindex;
            ckeypool keypool;
            ssvalue >> keypool;
            pwallet->setkeypool.insert(nindex);

            // if no metadata exists yet, create a default with the pool key's
            // creation time. note that this may be overwritten by actually
            // stored metadata for that key later, which is fine.
            ckeyid keyid = keypool.vchpubkey.getid();
            if (pwallet->mapkeymetadata.count(keyid) == 0)
                pwallet->mapkeymetadata[keyid] = ckeymetadata(keypool.ntime);
        }
        else if (strtype == "version")
        {
            ssvalue >> wss.nfileversion;
            if (wss.nfileversion == 10300)
                wss.nfileversion = 300;
        }
        else if (strtype == "cscript")
        {
            uint160 hash;
            sskey >> hash;
            cscript script;
            ssvalue >> script;
            if (!pwallet->loadcscript(script))
            {
                strerr = "error reading wallet database: loadcscript failed";
                return false;
            }
        }
        else if (strtype == "orderposnext")
        {
            ssvalue >> pwallet->norderposnext;
        }
        else if (strtype == "destdata")
        {
            std::string straddress, strkey, strvalue;
            sskey >> straddress;
            sskey >> strkey;
            ssvalue >> strvalue;
            if (!pwallet->loaddestdata(cmoorecoinaddress(straddress).get(), strkey, strvalue))
            {
                strerr = "error reading wallet database: loaddestdata failed";
                return false;
            }
        }
    } catch (...)
    {
        return false;
    }
    return true;
}

static bool iskeytype(string strtype)
{
    return (strtype== "key" || strtype == "wkey" ||
            strtype == "mkey" || strtype == "ckey");
}

dberrors cwalletdb::loadwallet(cwallet* pwallet)
{
    pwallet->vchdefaultkey = cpubkey();
    cwalletscanstate wss;
    bool fnoncriticalerrors = false;
    dberrors result = db_load_ok;

    try {
        lock(pwallet->cs_wallet);
        int nminversion = 0;
        if (read((string)"minversion", nminversion))
        {
            if (nminversion > client_version)
                return db_too_new;
            pwallet->loadminversion(nminversion);
        }

        // get cursor
        dbc* pcursor = getcursor();
        if (!pcursor)
        {
            logprintf("error getting wallet database cursor\n");
            return db_corrupt;
        }

        while (true)
        {
            // read next record
            cdatastream sskey(ser_disk, client_version);
            cdatastream ssvalue(ser_disk, client_version);
            int ret = readatcursor(pcursor, sskey, ssvalue);
            if (ret == db_notfound)
                break;
            else if (ret != 0)
            {
                logprintf("error reading next record from wallet database\n");
                return db_corrupt;
            }

            // try to be tolerant of single corrupt records:
            string strtype, strerr;
            if (!readkeyvalue(pwallet, sskey, ssvalue, wss, strtype, strerr))
            {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (iskeytype(strtype))
                    result = db_corrupt;
                else
                {
                    // leave other errors alone, if we try to fix them we might make things worse.
                    fnoncriticalerrors = true; // ... but do warn the user there is something wrong.
                    if (strtype == "tx")
                        // rescan if there is a bad transaction record:
                        softsetboolarg("-rescan", true);
                }
            }
            if (!strerr.empty())
                logprintf("%s\n", strerr);
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = db_corrupt;
    }

    if (fnoncriticalerrors && result == db_load_ok)
        result = db_noncritical_error;

    // any wallet corruption at all: skip any rewriting or
    // upgrading, we don't want to make it worse.
    if (result != db_load_ok)
        return result;

    logprintf("nfileversion = %d\n", wss.nfileversion);

    logprintf("keys: %u plaintext, %u encrypted, %u w/ metadata, %u total\n",
           wss.nkeys, wss.nckeys, wss.nkeymeta, wss.nkeys + wss.nckeys);

    // ntimefirstkey is only reliable if all keys have metadata
    if ((wss.nkeys + wss.nckeys) != wss.nkeymeta)
        pwallet->ntimefirstkey = 1; // 0 would be considered 'no value'

    boost_foreach(uint256 hash, wss.vwalletupgrade)
        writetx(hash, pwallet->mapwallet[hash]);

    // rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fisencrypted && (wss.nfileversion == 40000 || wss.nfileversion == 50000))
        return db_need_rewrite;

    if (wss.nfileversion < client_version) // update
        writeversion(client_version);

    if (wss.fanyunordered)
        result = reordertransactions(pwallet);

    return result;
}

dberrors cwalletdb::findwallettx(cwallet* pwallet, vector<uint256>& vtxhash, vector<cwallettx>& vwtx)
{
    pwallet->vchdefaultkey = cpubkey();
    bool fnoncriticalerrors = false;
    dberrors result = db_load_ok;

    try {
        lock(pwallet->cs_wallet);
        int nminversion = 0;
        if (read((string)"minversion", nminversion))
        {
            if (nminversion > client_version)
                return db_too_new;
            pwallet->loadminversion(nminversion);
        }

        // get cursor
        dbc* pcursor = getcursor();
        if (!pcursor)
        {
            logprintf("error getting wallet database cursor\n");
            return db_corrupt;
        }

        while (true)
        {
            // read next record
            cdatastream sskey(ser_disk, client_version);
            cdatastream ssvalue(ser_disk, client_version);
            int ret = readatcursor(pcursor, sskey, ssvalue);
            if (ret == db_notfound)
                break;
            else if (ret != 0)
            {
                logprintf("error reading next record from wallet database\n");
                return db_corrupt;
            }

            string strtype;
            sskey >> strtype;
            if (strtype == "tx") {
                uint256 hash;
                sskey >> hash;

                cwallettx wtx;
                ssvalue >> wtx;

                vtxhash.push_back(hash);
                vwtx.push_back(wtx);
            }
        }
        pcursor->close();
    }
    catch (const boost::thread_interrupted&) {
        throw;
    }
    catch (...) {
        result = db_corrupt;
    }

    if (fnoncriticalerrors && result == db_load_ok)
        result = db_noncritical_error;

    return result;
}

dberrors cwalletdb::zapwallettx(cwallet* pwallet, vector<cwallettx>& vwtx)
{
    // build list of wallet txs
    vector<uint256> vtxhash;
    dberrors err = findwallettx(pwallet, vtxhash, vwtx);
    if (err != db_load_ok)
        return err;

    // erase each wallet tx
    boost_foreach (uint256& hash, vtxhash) {
        if (!erasetx(hash))
            return db_corrupt;
    }

    return db_load_ok;
}

void threadflushwalletdb(const string& strfile)
{
    // make this thread recognisable as the wallet flushing thread
    renamethread("moorecoin-wallet");

    static bool fonethread;
    if (fonethread)
        return;
    fonethread = true;
    if (!getboolarg("-flushwallet", true))
        return;

    unsigned int nlastseen = nwalletdbupdated;
    unsigned int nlastflushed = nwalletdbupdated;
    int64_t nlastwalletupdate = gettime();
    while (true)
    {
        millisleep(500);

        if (nlastseen != nwalletdbupdated)
        {
            nlastseen = nwalletdbupdated;
            nlastwalletupdate = gettime();
        }

        if (nlastflushed != nwalletdbupdated && gettime() - nlastwalletupdate >= 2)
        {
            try_lock(bitdb.cs_db,lockdb);
            if (lockdb)
            {
                // don't do this if any databases are in use
                int nrefcount = 0;
                map<string, int>::iterator mi = bitdb.mapfileusecount.begin();
                while (mi != bitdb.mapfileusecount.end())
                {
                    nrefcount += (*mi).second;
                    mi++;
                }

                if (nrefcount == 0)
                {
                    boost::this_thread::interruption_point();
                    map<string, int>::iterator mi = bitdb.mapfileusecount.find(strfile);
                    if (mi != bitdb.mapfileusecount.end())
                    {
                        logprint("db", "flushing wallet.dat\n");
                        nlastflushed = nwalletdbupdated;
                        int64_t nstart = gettimemillis();

                        // flush wallet.dat so it's self contained
                        bitdb.closedb(strfile);
                        bitdb.checkpointlsn(strfile);

                        bitdb.mapfileusecount.erase(mi++);
                        logprint("db", "flushed wallet.dat %dms\n", gettimemillis() - nstart);
                    }
                }
            }
        }
    }
}

bool backupwallet(const cwallet& wallet, const string& strdest)
{
    if (!wallet.ffilebacked)
        return false;
    while (true)
    {
        {
            lock(bitdb.cs_db);
            if (!bitdb.mapfileusecount.count(wallet.strwalletfile) || bitdb.mapfileusecount[wallet.strwalletfile] == 0)
            {
                // flush log data to the dat file
                bitdb.closedb(wallet.strwalletfile);
                bitdb.checkpointlsn(wallet.strwalletfile);
                bitdb.mapfileusecount.erase(wallet.strwalletfile);

                // copy wallet.dat
                boost::filesystem::path pathsrc = getdatadir() / wallet.strwalletfile;
                boost::filesystem::path pathdest(strdest);
                if (boost::filesystem::is_directory(pathdest))
                    pathdest /= wallet.strwalletfile;

                try {
#if boost_version >= 104000
                    boost::filesystem::copy_file(pathsrc, pathdest, boost::filesystem::copy_option::overwrite_if_exists);
#else
                    boost::filesystem::copy_file(pathsrc, pathdest);
#endif
                    logprintf("copied wallet.dat to %s\n", pathdest.string());
                    return true;
                } catch (const boost::filesystem::filesystem_error& e) {
                    logprintf("error copying wallet.dat to %s - %s\n", pathdest.string(), e.what());
                    return false;
                }
            }
        }
        millisleep(100);
    }
    return false;
}

//
// try to (very carefully!) recover wallet.dat if there is a problem.
//
bool cwalletdb::recover(cdbenv& dbenv, const std::string& filename, bool fonlykeys)
{
    // recovery procedure:
    // move wallet.dat to wallet.timestamp.bak
    // call salvage with faggressive=true to
    // get as much data as possible.
    // rewrite salvaged data to wallet.dat
    // set -rescan so any missing transactions will be
    // found.
    int64_t now = gettime();
    std::string newfilename = strprintf("wallet.%d.bak", now);

    int result = dbenv.dbenv->dbrename(null, filename.c_str(), null,
                                       newfilename.c_str(), db_auto_commit);
    if (result == 0)
        logprintf("renamed %s to %s\n", filename, newfilename);
    else
    {
        logprintf("failed to rename %s to %s\n", filename, newfilename);
        return false;
    }

    std::vector<cdbenv::keyvalpair> salvageddata;
    bool fsuccess = dbenv.salvage(newfilename, true, salvageddata);
    if (salvageddata.empty())
    {
        logprintf("salvage(aggressive) found no records in %s.\n", newfilename);
        return false;
    }
    logprintf("salvage(aggressive) found %u records\n", salvageddata.size());

    boost::scoped_ptr<db> pdbcopy(new db(dbenv.dbenv, 0));
    int ret = pdbcopy->open(null,               // txn pointer
                            filename.c_str(),   // filename
                            "main",             // logical db name
                            db_btree,           // database type
                            db_create,          // flags
                            0);
    if (ret > 0)
    {
        logprintf("cannot create database file %s\n", filename);
        return false;
    }
    cwallet dummywallet;
    cwalletscanstate wss;

    dbtxn* ptxn = dbenv.txnbegin();
    boost_foreach(cdbenv::keyvalpair& row, salvageddata)
    {
        if (fonlykeys)
        {
            cdatastream sskey(row.first, ser_disk, client_version);
            cdatastream ssvalue(row.second, ser_disk, client_version);
            string strtype, strerr;
            bool freadok = readkeyvalue(&dummywallet, sskey, ssvalue,
                                        wss, strtype, strerr);
            if (!iskeytype(strtype))
                continue;
            if (!freadok)
            {
                logprintf("warning: cwalletdb::recover skipping %s: %s\n", strtype, strerr);
                continue;
            }
        }
        dbt datkey(&row.first[0], row.first.size());
        dbt datvalue(&row.second[0], row.second.size());
        int ret2 = pdbcopy->put(ptxn, &datkey, &datvalue, db_nooverwrite);
        if (ret2 > 0)
            fsuccess = false;
    }
    ptxn->commit(0);
    pdbcopy->close(0);

    return fsuccess;
}

bool cwalletdb::recover(cdbenv& dbenv, const std::string& filename)
{
    return cwalletdb::recover(dbenv, filename, false);
}

bool cwalletdb::writedestdata(const std::string &address, const std::string &key, const std::string &value)
{
    nwalletdbupdated++;
    return write(std::make_pair(std::string("destdata"), std::make_pair(address, key)), value);
}

bool cwalletdb::erasedestdata(const std::string &address, const std::string &key)
{
    nwalletdbupdated++;
    return erase(std::make_pair(std::string("destdata"), std::make_pair(address, key)));
}
