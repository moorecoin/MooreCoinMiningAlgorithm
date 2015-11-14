// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include "base58.h"
#include "checkpoints.h"
#include "coincontrol.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "main.h"
#include "net.h"
#include "script/script.h"
#include "script/sign.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"

#include <assert.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

using namespace std;

/**
 * settings
 */
cfeerate paytxfee(default_transaction_fee);
camount maxtxfee = default_transaction_maxfee;
unsigned int ntxconfirmtarget = default_tx_confirm_target;
bool bspendzeroconfchange = true;
bool fsendfreetransactions = false;
bool fpayatleastcustomfee = true;

/**
 * fees smaller than this (in satoshi) are considered zero fee (for transaction creation)
 * override with -mintxfee
 */
cfeerate cwallet::mintxfee = cfeerate(1000);

/** @defgroup mapwallet
 *
 * @{
 */

struct comparevalueonly
{
    bool operator()(const pair<camount, pair<const cwallettx*, unsigned int> >& t1,
                    const pair<camount, pair<const cwallettx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

std::string coutput::tostring() const
{
    return strprintf("coutput(%s, %d, %d) [%s]", tx->gethash().tostring(), i, ndepth, formatmoney(tx->vout[i].nvalue));
}

const cwallettx* cwallet::getwallettx(const uint256& hash) const
{
    lock(cs_wallet);
    std::map<uint256, cwallettx>::const_iterator it = mapwallet.find(hash);
    if (it == mapwallet.end())
        return null;
    return &(it->second);
}

cpubkey cwallet::generatenewkey()
{
    assertlockheld(cs_wallet); // mapkeymetadata
    bool fcompressed = cansupportfeature(feature_comprpubkey); // default to compressed public keys if we want 0.6.0 wallets

    ckey secret;
    secret.makenewkey(fcompressed);

    // compressed public keys were introduced in version 0.6.0
    if (fcompressed)
        setminversion(feature_comprpubkey);

    cpubkey pubkey = secret.getpubkey();
    assert(secret.verifypubkey(pubkey));

    // create new metadata
    int64_t ncreationtime = gettime();
    mapkeymetadata[pubkey.getid()] = ckeymetadata(ncreationtime);
    if (!ntimefirstkey || ncreationtime < ntimefirstkey)
        ntimefirstkey = ncreationtime;

    if (!addkeypubkey(secret, pubkey))
        throw std::runtime_error("cwallet::generatenewkey(): addkey failed");
    return pubkey;
}

bool cwallet::addkeypubkey(const ckey& secret, const cpubkey &pubkey)
{
    assertlockheld(cs_wallet); // mapkeymetadata
    if (!ccryptokeystore::addkeypubkey(secret, pubkey))
        return false;

    // check if we need to remove from watch-only
    cscript script;
    script = getscriptfordestination(pubkey.getid());
    if (havewatchonly(script))
        removewatchonly(script);

    if (!ffilebacked)
        return true;
    if (!iscrypted()) {
        return cwalletdb(strwalletfile).writekey(pubkey,
                                                 secret.getprivkey(),
                                                 mapkeymetadata[pubkey.getid()]);
    }
    return true;
}

bool cwallet::addcryptedkey(const cpubkey &vchpubkey,
                            const vector<unsigned char> &vchcryptedsecret)
{
    if (!ccryptokeystore::addcryptedkey(vchpubkey, vchcryptedsecret))
        return false;
    if (!ffilebacked)
        return true;
    {
        lock(cs_wallet);
        if (pwalletdbencryption)
            return pwalletdbencryption->writecryptedkey(vchpubkey,
                                                        vchcryptedsecret,
                                                        mapkeymetadata[vchpubkey.getid()]);
        else
            return cwalletdb(strwalletfile).writecryptedkey(vchpubkey,
                                                            vchcryptedsecret,
                                                            mapkeymetadata[vchpubkey.getid()]);
    }
    return false;
}

bool cwallet::loadkeymetadata(const cpubkey &pubkey, const ckeymetadata &meta)
{
    assertlockheld(cs_wallet); // mapkeymetadata
    if (meta.ncreatetime && (!ntimefirstkey || meta.ncreatetime < ntimefirstkey))
        ntimefirstkey = meta.ncreatetime;

    mapkeymetadata[pubkey.getid()] = meta;
    return true;
}

bool cwallet::loadcryptedkey(const cpubkey &vchpubkey, const std::vector<unsigned char> &vchcryptedsecret)
{
    return ccryptokeystore::addcryptedkey(vchpubkey, vchcryptedsecret);
}

bool cwallet::addcscript(const cscript& redeemscript)
{
    if (!ccryptokeystore::addcscript(redeemscript))
        return false;
    if (!ffilebacked)
        return true;
    return cwalletdb(strwalletfile).writecscript(hash160(redeemscript), redeemscript);
}

bool cwallet::loadcscript(const cscript& redeemscript)
{
    /* a sanity check was added in pull #3843 to avoid adding redeemscripts
     * that never can be redeemed. however, old wallets may still contain
     * these. do not add them to the wallet and warn. */
    if (redeemscript.size() > max_script_element_size)
    {
        std::string straddr = cmoorecoinaddress(cscriptid(redeemscript)).tostring();
        logprintf("%s: warning: this wallet contains a redeemscript of size %i which exceeds maximum size %i thus can never be redeemed. do not use address %s.\n",
            __func__, redeemscript.size(), max_script_element_size, straddr);
        return true;
    }

    return ccryptokeystore::addcscript(redeemscript);
}

bool cwallet::addwatchonly(const cscript &dest)
{
    if (!ccryptokeystore::addwatchonly(dest))
        return false;
    ntimefirstkey = 1; // no birthday information for watch-only keys.
    notifywatchonlychanged(true);
    if (!ffilebacked)
        return true;
    return cwalletdb(strwalletfile).writewatchonly(dest);
}

bool cwallet::removewatchonly(const cscript &dest)
{
    assertlockheld(cs_wallet);
    if (!ccryptokeystore::removewatchonly(dest))
        return false;
    if (!havewatchonly())
        notifywatchonlychanged(false);
    if (ffilebacked)
        if (!cwalletdb(strwalletfile).erasewatchonly(dest))
            return false;

    return true;
}

bool cwallet::loadwatchonly(const cscript &dest)
{
    return ccryptokeystore::addwatchonly(dest);
}

bool cwallet::unlock(const securestring& strwalletpassphrase)
{
    ccrypter crypter;
    ckeyingmaterial vmasterkey;

    {
        lock(cs_wallet);
        boost_foreach(const masterkeymap::value_type& pmasterkey, mapmasterkeys)
        {
            if(!crypter.setkeyfrompassphrase(strwalletpassphrase, pmasterkey.second.vchsalt, pmasterkey.second.nderiveiterations, pmasterkey.second.nderivationmethod))
                return false;
            if (!crypter.decrypt(pmasterkey.second.vchcryptedkey, vmasterkey))
                continue; // try another master key
            if (ccryptokeystore::unlock(vmasterkey))
                return true;
        }
    }
    return false;
}

bool cwallet::changewalletpassphrase(const securestring& stroldwalletpassphrase, const securestring& strnewwalletpassphrase)
{
    bool fwaslocked = islocked();

    {
        lock(cs_wallet);
        lock();

        ccrypter crypter;
        ckeyingmaterial vmasterkey;
        boost_foreach(masterkeymap::value_type& pmasterkey, mapmasterkeys)
        {
            if(!crypter.setkeyfrompassphrase(stroldwalletpassphrase, pmasterkey.second.vchsalt, pmasterkey.second.nderiveiterations, pmasterkey.second.nderivationmethod))
                return false;
            if (!crypter.decrypt(pmasterkey.second.vchcryptedkey, vmasterkey))
                return false;
            if (ccryptokeystore::unlock(vmasterkey))
            {
                int64_t nstarttime = gettimemillis();
                crypter.setkeyfrompassphrase(strnewwalletpassphrase, pmasterkey.second.vchsalt, pmasterkey.second.nderiveiterations, pmasterkey.second.nderivationmethod);
                pmasterkey.second.nderiveiterations = pmasterkey.second.nderiveiterations * (100 / ((double)(gettimemillis() - nstarttime)));

                nstarttime = gettimemillis();
                crypter.setkeyfrompassphrase(strnewwalletpassphrase, pmasterkey.second.vchsalt, pmasterkey.second.nderiveiterations, pmasterkey.second.nderivationmethod);
                pmasterkey.second.nderiveiterations = (pmasterkey.second.nderiveiterations + pmasterkey.second.nderiveiterations * 100 / ((double)(gettimemillis() - nstarttime))) / 2;

                if (pmasterkey.second.nderiveiterations < 25000)
                    pmasterkey.second.nderiveiterations = 25000;

                logprintf("wallet passphrase changed to an nderiveiterations of %i\n", pmasterkey.second.nderiveiterations);

                if (!crypter.setkeyfrompassphrase(strnewwalletpassphrase, pmasterkey.second.vchsalt, pmasterkey.second.nderiveiterations, pmasterkey.second.nderivationmethod))
                    return false;
                if (!crypter.encrypt(vmasterkey, pmasterkey.second.vchcryptedkey))
                    return false;
                cwalletdb(strwalletfile).writemasterkey(pmasterkey.first, pmasterkey.second);
                if (fwaslocked)
                    lock();
                return true;
            }
        }
    }

    return false;
}

void cwallet::setbestchain(const cblocklocator& loc)
{
    cwalletdb walletdb(strwalletfile);
    walletdb.writebestblock(loc);
}

bool cwallet::setminversion(enum walletfeature nversion, cwalletdb* pwalletdbin, bool fexplicit)
{
    lock(cs_wallet); // nwalletversion
    if (nwalletversion >= nversion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fexplicit && nversion > nwalletmaxversion)
            nversion = feature_latest;

    nwalletversion = nversion;

    if (nversion > nwalletmaxversion)
        nwalletmaxversion = nversion;

    if (ffilebacked)
    {
        cwalletdb* pwalletdb = pwalletdbin ? pwalletdbin : new cwalletdb(strwalletfile);
        if (nwalletversion > 40000)
            pwalletdb->writeminversion(nwalletversion);
        if (!pwalletdbin)
            delete pwalletdb;
    }

    return true;
}

bool cwallet::setmaxversion(int nversion)
{
    lock(cs_wallet); // nwalletversion, nwalletmaxversion
    // cannot downgrade below current version
    if (nwalletversion > nversion)
        return false;

    nwalletmaxversion = nversion;

    return true;
}

set<uint256> cwallet::getconflicts(const uint256& txid) const
{
    set<uint256> result;
    assertlockheld(cs_wallet);

    std::map<uint256, cwallettx>::const_iterator it = mapwallet.find(txid);
    if (it == mapwallet.end())
        return result;
    const cwallettx& wtx = it->second;

    std::pair<txspends::const_iterator, txspends::const_iterator> range;

    boost_foreach(const ctxin& txin, wtx.vin)
    {
        if (maptxspends.count(txin.prevout) <= 1)
            continue;  // no conflict if zero or one spends
        range = maptxspends.equal_range(txin.prevout);
        for (txspends::const_iterator it = range.first; it != range.second; ++it)
            result.insert(it->second);
    }
    return result;
}

void cwallet::flush(bool shutdown)
{
    bitdb.flush(shutdown);
}

bool cwallet::verify(const string& walletfile, string& warningstring, string& errorstring)
{
    if (!bitdb.open(getdatadir()))
    {
        // try moving the database env out of the way
        boost::filesystem::path pathdatabase = getdatadir() / "database";
        boost::filesystem::path pathdatabasebak = getdatadir() / strprintf("database.%d.bak", gettime());
        try {
            boost::filesystem::rename(pathdatabase, pathdatabasebak);
            logprintf("moved old %s to %s. retrying.\n", pathdatabase.string(), pathdatabasebak.string());
        } catch (const boost::filesystem::filesystem_error&) {
            // failure is ok (well, not really, but it's not worse than what we started with)
        }
        
        // try again
        if (!bitdb.open(getdatadir())) {
            // if it still fails, it probably means we can't even create the database env
            string msg = strprintf(_("error initializing wallet database environment %s!"), getdatadir());
            errorstring += msg;
            return true;
        }
    }
    
    if (getboolarg("-salvagewallet", false))
    {
        // recover readable keypairs:
        if (!cwalletdb::recover(bitdb, walletfile, true))
            return false;
    }
    
    if (boost::filesystem::exists(getdatadir() / walletfile))
    {
        cdbenv::verifyresult r = bitdb.verify(walletfile, cwalletdb::recover);
        if (r == cdbenv::recover_ok)
        {
            warningstring += strprintf(_("warning: wallet.dat corrupt, data salvaged!"
                                     " original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."), getdatadir());
        }
        if (r == cdbenv::recover_fail)
            errorstring += _("wallet.dat corrupt, salvage failed");
    }
    
    return true;
}

void cwallet::syncmetadata(pair<txspends::iterator, txspends::iterator> range)
{
    // we want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest norderpos).
    // so: find smallest norderpos:

    int nminorderpos = std::numeric_limits<int>::max();
    const cwallettx* copyfrom = null;
    for (txspends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        int n = mapwallet[hash].norderpos;
        if (n < nminorderpos)
        {
            nminorderpos = n;
            copyfrom = &mapwallet[hash];
        }
    }
    // now copy data from copyfrom to rest:
    for (txspends::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        cwallettx* copyto = &mapwallet[hash];
        if (copyfrom == copyto) continue;
        copyto->mapvalue = copyfrom->mapvalue;
        copyto->vorderform = copyfrom->vorderform;
        // ftimereceivedistxtime not copied on purpose
        // ntimereceived not copied on purpose
        copyto->ntimesmart = copyfrom->ntimesmart;
        copyto->ffromme = copyfrom->ffromme;
        copyto->strfromaccount = copyfrom->strfromaccount;
        // norderpos not copied on purpose
        // cached members not copied on purpose
    }
}

/**
 * outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool cwallet::isspent(const uint256& hash, unsigned int n) const
{
    const coutpoint outpoint(hash, n);
    pair<txspends::const_iterator, txspends::const_iterator> range;
    range = maptxspends.equal_range(outpoint);

    for (txspends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, cwallettx>::const_iterator mit = mapwallet.find(wtxid);
        if (mit != mapwallet.end() && mit->second.getdepthinmainchain() >= 0)
            return true; // spent
    }
    return false;
}

void cwallet::addtospends(const coutpoint& outpoint, const uint256& wtxid)
{
    maptxspends.insert(make_pair(outpoint, wtxid));

    pair<txspends::iterator, txspends::iterator> range;
    range = maptxspends.equal_range(outpoint);
    syncmetadata(range);
}


void cwallet::addtospends(const uint256& wtxid)
{
    assert(mapwallet.count(wtxid));
    cwallettx& thistx = mapwallet[wtxid];
    if (thistx.iscoinbase()) // coinbases don't spend anything!
        return;

    boost_foreach(const ctxin& txin, thistx.vin)
        addtospends(txin.prevout, wtxid);
}

bool cwallet::encryptwallet(const securestring& strwalletpassphrase)
{
    if (iscrypted())
        return false;

    ckeyingmaterial vmasterkey;
    randaddseedperfmon();

    vmasterkey.resize(wallet_crypto_key_size);
    getrandbytes(&vmasterkey[0], wallet_crypto_key_size);

    cmasterkey kmasterkey;
    randaddseedperfmon();

    kmasterkey.vchsalt.resize(wallet_crypto_salt_size);
    getrandbytes(&kmasterkey.vchsalt[0], wallet_crypto_salt_size);

    ccrypter crypter;
    int64_t nstarttime = gettimemillis();
    crypter.setkeyfrompassphrase(strwalletpassphrase, kmasterkey.vchsalt, 25000, kmasterkey.nderivationmethod);
    kmasterkey.nderiveiterations = 2500000 / ((double)(gettimemillis() - nstarttime));

    nstarttime = gettimemillis();
    crypter.setkeyfrompassphrase(strwalletpassphrase, kmasterkey.vchsalt, kmasterkey.nderiveiterations, kmasterkey.nderivationmethod);
    kmasterkey.nderiveiterations = (kmasterkey.nderiveiterations + kmasterkey.nderiveiterations * 100 / ((double)(gettimemillis() - nstarttime))) / 2;

    if (kmasterkey.nderiveiterations < 25000)
        kmasterkey.nderiveiterations = 25000;

    logprintf("encrypting wallet with an nderiveiterations of %i\n", kmasterkey.nderiveiterations);

    if (!crypter.setkeyfrompassphrase(strwalletpassphrase, kmasterkey.vchsalt, kmasterkey.nderiveiterations, kmasterkey.nderivationmethod))
        return false;
    if (!crypter.encrypt(vmasterkey, kmasterkey.vchcryptedkey))
        return false;

    {
        lock(cs_wallet);
        mapmasterkeys[++nmasterkeymaxid] = kmasterkey;
        if (ffilebacked)
        {
            assert(!pwalletdbencryption);
            pwalletdbencryption = new cwalletdb(strwalletfile);
            if (!pwalletdbencryption->txnbegin()) {
                delete pwalletdbencryption;
                pwalletdbencryption = null;
                return false;
            }
            pwalletdbencryption->writemasterkey(nmasterkeymaxid, kmasterkey);
        }

        if (!encryptkeys(vmasterkey))
        {
            if (ffilebacked) {
                pwalletdbencryption->txnabort();
                delete pwalletdbencryption;
            }
            // we now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload the unencrypted wallet.
            assert(false);
        }

        // encryption was introduced in version 0.4.0
        setminversion(feature_walletcrypt, pwalletdbencryption, true);

        if (ffilebacked)
        {
            if (!pwalletdbencryption->txncommit()) {
                delete pwalletdbencryption;
                // we now have keys encrypted in memory, but not on disk...
                // die to avoid confusion and let the user reload the unencrypted wallet.
                assert(false);
            }

            delete pwalletdbencryption;
            pwalletdbencryption = null;
        }

        lock();
        unlock(strwalletpassphrase);
        newkeypool();
        lock();

        // need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        cdb::rewrite(strwalletfile);

    }
    notifystatuschanged(this);

    return true;
}

int64_t cwallet::incorderposnext(cwalletdb *pwalletdb)
{
    assertlockheld(cs_wallet); // norderposnext
    int64_t nret = norderposnext++;
    if (pwalletdb) {
        pwalletdb->writeorderposnext(norderposnext);
    } else {
        cwalletdb(strwalletfile).writeorderposnext(norderposnext);
    }
    return nret;
}

cwallet::txitems cwallet::orderedtxitems(std::list<caccountingentry>& acentries, std::string straccount)
{
    assertlockheld(cs_wallet); // mapwallet
    cwalletdb walletdb(strwalletfile);

    // first: get all cwallettx and caccountingentry into a sorted-by-order multimap.
    txitems txordered;

    // note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, cwallettx>::iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
    {
        cwallettx* wtx = &((*it).second);
        txordered.insert(make_pair(wtx->norderpos, txpair(wtx, (caccountingentry*)0)));
    }
    acentries.clear();
    walletdb.listaccountcreditdebit(straccount, acentries);
    boost_foreach(caccountingentry& entry, acentries)
    {
        txordered.insert(make_pair(entry.norderpos, txpair((cwallettx*)0, &entry)));
    }

    return txordered;
}

void cwallet::markdirty()
{
    {
        lock(cs_wallet);
        boost_foreach(pairtype(const uint256, cwallettx)& item, mapwallet)
            item.second.markdirty();
    }
}

bool cwallet::addtowallet(const cwallettx& wtxin, bool ffromloadwallet, cwalletdb* pwalletdb)
{
    uint256 hash = wtxin.gethash();

    if (ffromloadwallet)
    {
        mapwallet[hash] = wtxin;
        mapwallet[hash].bindwallet(this);
        addtospends(hash);
    }
    else
    {
        lock(cs_wallet);
        // inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, cwallettx>::iterator, bool> ret = mapwallet.insert(make_pair(hash, wtxin));
        cwallettx& wtx = (*ret.first).second;
        wtx.bindwallet(this);
        bool finsertednew = ret.second;
        if (finsertednew)
        {
            wtx.ntimereceived = getadjustedtime();
            wtx.norderpos = incorderposnext(pwalletdb);

            wtx.ntimesmart = wtx.ntimereceived;
            if (!wtxin.hashblock.isnull())
            {
                if (mapblockindex.count(wtxin.hashblock))
                {
                    int64_t latestnow = wtx.ntimereceived;
                    int64_t latestentry = 0;
                    {
                        // tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latesttolerated = latestnow + 300;
                        std::list<caccountingentry> acentries;
                        txitems txordered = orderedtxitems(acentries);
                        for (txitems::reverse_iterator it = txordered.rbegin(); it != txordered.rend(); ++it)
                        {
                            cwallettx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            caccountingentry *const pacentry = (*it).second.second;
                            int64_t nsmarttime;
                            if (pwtx)
                            {
                                nsmarttime = pwtx->ntimesmart;
                                if (!nsmarttime)
                                    nsmarttime = pwtx->ntimereceived;
                            }
                            else
                                nsmarttime = pacentry->ntime;
                            if (nsmarttime <= latesttolerated)
                            {
                                latestentry = nsmarttime;
                                if (nsmarttime > latestnow)
                                    latestnow = nsmarttime;
                                break;
                            }
                        }
                    }

                    int64_t blocktime = mapblockindex[wtxin.hashblock]->getblocktime();
                    wtx.ntimesmart = std::max(latestentry, std::min(blocktime, latestnow));
                }
                else
                    logprintf("addtowallet(): found %s in block %s not in index\n",
                             wtxin.gethash().tostring(),
                             wtxin.hashblock.tostring());
            }
            addtospends(hash);
        }

        bool fupdated = false;
        if (!finsertednew)
        {
            // merge
            if (!wtxin.hashblock.isnull() && wtxin.hashblock != wtx.hashblock)
            {
                wtx.hashblock = wtxin.hashblock;
                fupdated = true;
            }
            if (wtxin.nindex != -1 && (wtxin.vmerklebranch != wtx.vmerklebranch || wtxin.nindex != wtx.nindex))
            {
                wtx.vmerklebranch = wtxin.vmerklebranch;
                wtx.nindex = wtxin.nindex;
                fupdated = true;
            }
            if (wtxin.ffromme && wtxin.ffromme != wtx.ffromme)
            {
                wtx.ffromme = wtxin.ffromme;
                fupdated = true;
            }
        }

        //// debug print
        logprintf("addtowallet %s  %s%s\n", wtxin.gethash().tostring(), (finsertednew ? "new" : ""), (fupdated ? "update" : ""));

        // write to disk
        if (finsertednew || fupdated)
            if (!wtx.writetodisk(pwalletdb))
                return false;

        // break debit/credit balance caches:
        wtx.markdirty();

        // notify ui of new or updated transaction
        notifytransactionchanged(this, hash, finsertednew ? ct_new : ct_updated);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strcmd = getarg("-walletnotify", "");

        if ( !strcmd.empty())
        {
            boost::replace_all(strcmd, "%s", wtxin.gethash().gethex());
            boost::thread t(runcommand, strcmd); // thread runs free
        }

    }
    return true;
}

/**
 * add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * if fupdate is true, existing transactions will be updated.
 */
bool cwallet::addtowalletifinvolvingme(const ctransaction& tx, const cblock* pblock, bool fupdate)
{
    {
        assertlockheld(cs_wallet);
        bool fexisted = mapwallet.count(tx.gethash()) != 0;
        if (fexisted && !fupdate) return false;
        if (fexisted || ismine(tx) || isfromme(tx))
        {
            cwallettx wtx(this,tx);

            // get merkle branch if transaction was found in a block
            if (pblock)
                wtx.setmerklebranch(*pblock);

            // do not flush the wallet here for performance reasons
            // this is safe, as in case of a crash, we rescan the necessary blocks on startup through our setbestchain-mechanism
            cwalletdb walletdb(strwalletfile, "r+", false);

            return addtowallet(wtx, false, &walletdb);
        }
    }
    return false;
}

void cwallet::synctransaction(const ctransaction& tx, const cblock* pblock)
{
    lock2(cs_main, cs_wallet);
    if (!addtowalletifinvolvingme(tx, pblock, true))
        return; // not one of ours

    // if a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. so force those to be
    // recomputed, also:
    boost_foreach(const ctxin& txin, tx.vin)
    {
        if (mapwallet.count(txin.prevout.hash))
            mapwallet[txin.prevout.hash].markdirty();
    }
}


isminetype cwallet::ismine(const ctxin &txin) const
{
    {
        lock(cs_wallet);
        map<uint256, cwallettx>::const_iterator mi = mapwallet.find(txin.prevout.hash);
        if (mi != mapwallet.end())
        {
            const cwallettx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return ismine(prev.vout[txin.prevout.n]);
        }
    }
    return ismine_no;
}

camount cwallet::getdebit(const ctxin &txin, const isminefilter& filter) const
{
    {
        lock(cs_wallet);
        map<uint256, cwallettx>::const_iterator mi = mapwallet.find(txin.prevout.hash);
        if (mi != mapwallet.end())
        {
            const cwallettx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (ismine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nvalue;
        }
    }
    return 0;
}

isminetype cwallet::ismine(const ctxout& txout) const
{
    return ::ismine(*this, txout.scriptpubkey);
}

camount cwallet::getcredit(const ctxout& txout, const isminefilter& filter) const
{
    if (!moneyrange(txout.nvalue))
        throw std::runtime_error("cwallet::getcredit(): value out of range");
    return ((ismine(txout) & filter) ? txout.nvalue : 0);
}

bool cwallet::ischange(const ctxout& txout) const
{
    // todo: fix handling of 'change' outputs. the assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. that assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend cwallettx to remember
    // which output, if any, was change).
    if (::ismine(*this, txout.scriptpubkey))
    {
        ctxdestination address;
        if (!extractdestination(txout.scriptpubkey, address))
            return true;

        lock(cs_wallet);
        if (!mapaddressbook.count(address))
            return true;
    }
    return false;
}

camount cwallet::getchange(const ctxout& txout) const
{
    if (!moneyrange(txout.nvalue))
        throw std::runtime_error("cwallet::getchange(): value out of range");
    return (ischange(txout) ? txout.nvalue : 0);
}

bool cwallet::ismine(const ctransaction& tx) const
{
    boost_foreach(const ctxout& txout, tx.vout)
        if (ismine(txout))
            return true;
    return false;
}

bool cwallet::isfromme(const ctransaction& tx) const
{
    return (getdebit(tx, ismine_all) > 0);
}

camount cwallet::getdebit(const ctransaction& tx, const isminefilter& filter) const
{
    camount ndebit = 0;
    boost_foreach(const ctxin& txin, tx.vin)
    {
        ndebit += getdebit(txin, filter);
        if (!moneyrange(ndebit))
            throw std::runtime_error("cwallet::getdebit(): value out of range");
    }
    return ndebit;
}

camount cwallet::getcredit(const ctransaction& tx, const isminefilter& filter) const
{
    camount ncredit = 0;
    boost_foreach(const ctxout& txout, tx.vout)
    {
        ncredit += getcredit(txout, filter);
        if (!moneyrange(ncredit))
            throw std::runtime_error("cwallet::getcredit(): value out of range");
    }
    return ncredit;
}

camount cwallet::getchange(const ctransaction& tx) const
{
    camount nchange = 0;
    boost_foreach(const ctxout& txout, tx.vout)
    {
        nchange += getchange(txout);
        if (!moneyrange(nchange))
            throw std::runtime_error("cwallet::getchange(): value out of range");
    }
    return nchange;
}

int64_t cwallettx::gettxtime() const
{
    int64_t n = ntimesmart;
    return n ? n : ntimereceived;
}

int cwallettx::getrequestcount() const
{
    // returns -1 if it wasn't being tracked
    int nrequests = -1;
    {
        lock(pwallet->cs_wallet);
        if (iscoinbase())
        {
            // generated block
            if (!hashblock.isnull())
            {
                map<uint256, int>::const_iterator mi = pwallet->maprequestcount.find(hashblock);
                if (mi != pwallet->maprequestcount.end())
                    nrequests = (*mi).second;
            }
        }
        else
        {
            // did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->maprequestcount.find(gethash());
            if (mi != pwallet->maprequestcount.end())
            {
                nrequests = (*mi).second;

                // how about the block it's in?
                if (nrequests == 0 && !hashblock.isnull())
                {
                    map<uint256, int>::const_iterator mi = pwallet->maprequestcount.find(hashblock);
                    if (mi != pwallet->maprequestcount.end())
                        nrequests = (*mi).second;
                    else
                        nrequests = 1; // if it's in someone else's block it must have got out
                }
            }
        }
    }
    return nrequests;
}

void cwallettx::getamounts(list<coutputentry>& listreceived,
                           list<coutputentry>& listsent, camount& nfee, string& strsentaccount, const isminefilter& filter) const
{
    nfee = 0;
    listreceived.clear();
    listsent.clear();
    strsentaccount = strfromaccount;

    // compute fee:
    camount ndebit = getdebit(filter);
    if (ndebit > 0) // debit>0 means we signed/sent this transaction
    {
        camount nvalueout = getvalueout();
        nfee = ndebit - nvalueout;
    }

    // sent/received.
    for (unsigned int i = 0; i < vout.size(); ++i)
    {
        const ctxout& txout = vout[i];
        isminetype fismine = pwallet->ismine(txout);
        // only need to handle txouts if at least one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (ndebit > 0)
        {
            // don't report 'change' txouts
            if (pwallet->ischange(txout))
                continue;
        }
        else if (!(fismine & filter))
            continue;

        // in either case, we need to get the destination address
        ctxdestination address;
        if (!extractdestination(txout.scriptpubkey, address))
        {
            logprintf("cwallettx::getamounts: unknown transaction type found, txid %s\n",
                     this->gethash().tostring());
            address = cnodestination();
        }

        coutputentry output = {address, txout.nvalue, (int)i};

        // if we are debited by the transaction, add the output as a "sent" entry
        if (ndebit > 0)
            listsent.push_back(output);

        // if we are receiving the output, add it as a "received" entry
        if (fismine & filter)
            listreceived.push_back(output);
    }

}

void cwallettx::getaccountamounts(const string& straccount, camount& nreceived,
                                  camount& nsent, camount& nfee, const isminefilter& filter) const
{
    nreceived = nsent = nfee = 0;

    camount allfee;
    string strsentaccount;
    list<coutputentry> listreceived;
    list<coutputentry> listsent;
    getamounts(listreceived, listsent, allfee, strsentaccount, filter);

    if (straccount == strsentaccount)
    {
        boost_foreach(const coutputentry& s, listsent)
            nsent += s.amount;
        nfee = allfee;
    }
    {
        lock(pwallet->cs_wallet);
        boost_foreach(const coutputentry& r, listreceived)
        {
            if (pwallet->mapaddressbook.count(r.destination))
            {
                map<ctxdestination, caddressbookdata>::const_iterator mi = pwallet->mapaddressbook.find(r.destination);
                if (mi != pwallet->mapaddressbook.end() && (*mi).second.name == straccount)
                    nreceived += r.amount;
            }
            else if (straccount.empty())
            {
                nreceived += r.amount;
            }
        }
    }
}


bool cwallettx::writetodisk(cwalletdb *pwalletdb)
{
    return pwalletdb->writetx(gethash(), *this);
}

/**
 * scan the block chain (starting in pindexstart) for transactions
 * from or to us. if fupdate is true, found transactions that already
 * exist in the wallet will be updated.
 */
int cwallet::scanforwallettransactions(cblockindex* pindexstart, bool fupdate)
{
    int ret = 0;
    int64_t nnow = gettime();
    const cchainparams& chainparams = params();

    cblockindex* pindex = pindexstart;
    {
        lock2(cs_main, cs_wallet);

        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)
        while (pindex && ntimefirstkey && (pindex->getblocktime() < (ntimefirstkey - 7200)))
            pindex = chainactive.next(pindex);

        showprogress(_("rescanning..."), 0); // show rescan progress in gui as dialog or on splashscreen, if -rescan on startup
        double dprogressstart = checkpoints::guessverificationprogress(chainparams.checkpoints(), pindex, false);
        double dprogresstip = checkpoints::guessverificationprogress(chainparams.checkpoints(), chainactive.tip(), false);
        while (pindex)
        {
            if (pindex->nheight % 100 == 0 && dprogresstip - dprogressstart > 0.0)
                showprogress(_("rescanning..."), std::max(1, std::min(99, (int)((checkpoints::guessverificationprogress(chainparams.checkpoints(), pindex, false) - dprogressstart) / (dprogresstip - dprogressstart) * 100))));

            cblock block;
            readblockfromdisk(block, pindex);
            boost_foreach(ctransaction& tx, block.vtx)
            {
                if (addtowalletifinvolvingme(tx, &block, fupdate))
                    ret++;
            }
            pindex = chainactive.next(pindex);
            if (gettime() >= nnow + 60) {
                nnow = gettime();
                logprintf("still rescanning. at block %d. progress=%f\n", pindex->nheight, checkpoints::guessverificationprogress(chainparams.checkpoints(), pindex));
            }
        }
        showprogress(_("rescanning..."), 100); // hide progress dialog in gui
    }
    return ret;
}

void cwallet::reacceptwallettransactions()
{
    // if transactions aren't being broadcasted, don't let them into local mempool either
    if (!fbroadcasttransactions)
        return;
    lock2(cs_main, cs_wallet);
    std::map<int64_t, cwallettx*> mapsorted;

    // sort pending wallet transactions based on their initial wallet insertion order
    boost_foreach(pairtype(const uint256, cwallettx)& item, mapwallet)
    {
        const uint256& wtxid = item.first;
        cwallettx& wtx = item.second;
        assert(wtx.gethash() == wtxid);

        int ndepth = wtx.getdepthinmainchain();

        if (!wtx.iscoinbase() && ndepth < 0) {
            mapsorted.insert(std::make_pair(wtx.norderpos, &wtx));
        }
    }

    // try to add wallet transactions to memory pool
    boost_foreach(pairtype(const int64_t, cwallettx*)& item, mapsorted)
    {
        cwallettx& wtx = *(item.second);

        lock(mempool.cs);
        wtx.accepttomemorypool(false);
    }
}

bool cwallettx::relaywallettransaction()
{
    assert(pwallet->getbroadcasttransactions());
    if (!iscoinbase())
    {
        if (getdepthinmainchain() == 0) {
            logprintf("relaying wtx %s\n", gethash().tostring());
            relaytransaction((ctransaction)*this);
            return true;
        }
    }
    return false;
}

set<uint256> cwallettx::getconflicts() const
{
    set<uint256> result;
    if (pwallet != null)
    {
        uint256 myhash = gethash();
        result = pwallet->getconflicts(myhash);
        result.erase(myhash);
    }
    return result;
}

camount cwallettx::getdebit(const isminefilter& filter) const
{
    if (vin.empty())
        return 0;

    camount debit = 0;
    if(filter & ismine_spendable)
    {
        if (fdebitcached)
            debit += ndebitcached;
        else
        {
            ndebitcached = pwallet->getdebit(*this, ismine_spendable);
            fdebitcached = true;
            debit += ndebitcached;
        }
    }
    if(filter & ismine_watch_only)
    {
        if(fwatchdebitcached)
            debit += nwatchdebitcached;
        else
        {
            nwatchdebitcached = pwallet->getdebit(*this, ismine_watch_only);
            fwatchdebitcached = true;
            debit += nwatchdebitcached;
        }
    }
    return debit;
}

camount cwallettx::getcredit(const isminefilter& filter) const
{
    // must wait until coinbase is safely deep enough in the chain before valuing it
    if (iscoinbase() && getblockstomaturity() > 0)
        return 0;

    int64_t credit = 0;
    if (filter & ismine_spendable)
    {
        // getbalance can assume transactions in mapwallet won't change
        if (fcreditcached)
            credit += ncreditcached;
        else
        {
            ncreditcached = pwallet->getcredit(*this, ismine_spendable);
            fcreditcached = true;
            credit += ncreditcached;
        }
    }
    if (filter & ismine_watch_only)
    {
        if (fwatchcreditcached)
            credit += nwatchcreditcached;
        else
        {
            nwatchcreditcached = pwallet->getcredit(*this, ismine_watch_only);
            fwatchcreditcached = true;
            credit += nwatchcreditcached;
        }
    }
    return credit;
}

camount cwallettx::getimmaturecredit(bool fusecache) const
{
    if (iscoinbase() && getblockstomaturity() > 0 && isinmainchain())
    {
        if (fusecache && fimmaturecreditcached)
            return nimmaturecreditcached;
        nimmaturecreditcached = pwallet->getcredit(*this, ismine_spendable);
        fimmaturecreditcached = true;
        return nimmaturecreditcached;
    }

    return 0;
}

camount cwallettx::getavailablecredit(bool fusecache) const
{
    if (pwallet == 0)
        return 0;

    // must wait until coinbase is safely deep enough in the chain before valuing it
    if (iscoinbase() && getblockstomaturity() > 0)
        return 0;

    if (fusecache && favailablecreditcached)
        return navailablecreditcached;

    camount ncredit = 0;
    uint256 hashtx = gethash();
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        if (!pwallet->isspent(hashtx, i))
        {
            const ctxout &txout = vout[i];
            ncredit += pwallet->getcredit(txout, ismine_spendable);
            if (!moneyrange(ncredit))
                throw std::runtime_error("cwallettx::getavailablecredit() : value out of range");
        }
    }

    navailablecreditcached = ncredit;
    favailablecreditcached = true;
    return ncredit;
}

camount cwallettx::getimmaturewatchonlycredit(const bool& fusecache) const
{
    if (iscoinbase() && getblockstomaturity() > 0 && isinmainchain())
    {
        if (fusecache && fimmaturewatchcreditcached)
            return nimmaturewatchcreditcached;
        nimmaturewatchcreditcached = pwallet->getcredit(*this, ismine_watch_only);
        fimmaturewatchcreditcached = true;
        return nimmaturewatchcreditcached;
    }

    return 0;
}

camount cwallettx::getavailablewatchonlycredit(const bool& fusecache) const
{
    if (pwallet == 0)
        return 0;

    // must wait until coinbase is safely deep enough in the chain before valuing it
    if (iscoinbase() && getblockstomaturity() > 0)
        return 0;

    if (fusecache && favailablewatchcreditcached)
        return navailablewatchcreditcached;

    camount ncredit = 0;
    for (unsigned int i = 0; i < vout.size(); i++)
    {
        if (!pwallet->isspent(gethash(), i))
        {
            const ctxout &txout = vout[i];
            ncredit += pwallet->getcredit(txout, ismine_watch_only);
            if (!moneyrange(ncredit))
                throw std::runtime_error("cwallettx::getavailablecredit() : value out of range");
        }
    }

    navailablewatchcreditcached = ncredit;
    favailablewatchcreditcached = true;
    return ncredit;
}

camount cwallettx::getchange() const
{
    if (fchangecached)
        return nchangecached;
    nchangecached = pwallet->getchange(*this);
    fchangecached = true;
    return nchangecached;
}

bool cwallettx::istrusted() const
{
    // quick answer in most cases
    if (!checkfinaltx(*this))
        return false;
    int ndepth = getdepthinmainchain();
    if (ndepth >= 1)
        return true;
    if (ndepth < 0)
        return false;
    if (!bspendzeroconfchange || !isfromme(ismine_all)) // using wtx's cached debit
        return false;

    // trusted if all inputs are from us and are in the mempool:
    boost_foreach(const ctxin& txin, vin)
    {
        // transactions not sent by us: not trusted
        const cwallettx* parent = pwallet->getwallettx(txin.prevout.hash);
        if (parent == null)
            return false;
        const ctxout& parentout = parent->vout[txin.prevout.n];
        if (pwallet->ismine(parentout) != ismine_spendable)
            return false;
    }
    return true;
}

std::vector<uint256> cwallet::resendwallettransactionsbefore(int64_t ntime)
{
    std::vector<uint256> result;

    lock(cs_wallet);
    // sort them in chronological order
    multimap<unsigned int, cwallettx*> mapsorted;
    boost_foreach(pairtype(const uint256, cwallettx)& item, mapwallet)
    {
        cwallettx& wtx = item.second;
        // don't rebroadcast if newer than ntime:
        if (wtx.ntimereceived > ntime)
            continue;
        mapsorted.insert(make_pair(wtx.ntimereceived, &wtx));
    }
    boost_foreach(pairtype(const unsigned int, cwallettx*)& item, mapsorted)
    {
        cwallettx& wtx = *item.second;
        if (wtx.relaywallettransaction())
            result.push_back(wtx.gethash());
    }
    return result;
}

void cwallet::resendwallettransactions(int64_t nbestblocktime)
{
    // do this infrequently and randomly to avoid giving away
    // that these are our transactions.
    if (gettime() < nnextresend || !fbroadcasttransactions)
        return;
    bool ffirst = (nnextresend == 0);
    nnextresend = gettime() + getrand(30 * 60);
    if (ffirst)
        return;

    // only do it if there's been a new block since last time
    if (nbestblocktime < nlastresend)
        return;
    nlastresend = gettime();

    // rebroadcast unconfirmed txes older than 5 minutes before the last
    // block was found:
    std::vector<uint256> relayed = resendwallettransactionsbefore(nbestblocktime-5*60);
    if (!relayed.empty())
        logprintf("%s: rebroadcast %u unconfirmed transactions\n", __func__, relayed.size());
}

/** @} */ // end of mapwallet




/** @defgroup actions
 *
 * @{
 */


camount cwallet::getbalance() const
{
    camount ntotal = 0;
    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const cwallettx* pcoin = &(*it).second;
            if (pcoin->istrusted())
                ntotal += pcoin->getavailablecredit();
        }
    }

    return ntotal;
}

camount cwallet::getunconfirmedbalance() const
{
    camount ntotal = 0;
    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const cwallettx* pcoin = &(*it).second;
            if (!checkfinaltx(*pcoin) || (!pcoin->istrusted() && pcoin->getdepthinmainchain() == 0))
                ntotal += pcoin->getavailablecredit();
        }
    }
    return ntotal;
}

camount cwallet::getimmaturebalance() const
{
    camount ntotal = 0;
    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const cwallettx* pcoin = &(*it).second;
            ntotal += pcoin->getimmaturecredit();
        }
    }
    return ntotal;
}

camount cwallet::getwatchonlybalance() const
{
    camount ntotal = 0;
    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const cwallettx* pcoin = &(*it).second;
            if (pcoin->istrusted())
                ntotal += pcoin->getavailablewatchonlycredit();
        }
    }

    return ntotal;
}

camount cwallet::getunconfirmedwatchonlybalance() const
{
    camount ntotal = 0;
    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const cwallettx* pcoin = &(*it).second;
            if (!checkfinaltx(*pcoin) || (!pcoin->istrusted() && pcoin->getdepthinmainchain() == 0))
                ntotal += pcoin->getavailablewatchonlycredit();
        }
    }
    return ntotal;
}

camount cwallet::getimmaturewatchonlybalance() const
{
    camount ntotal = 0;
    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const cwallettx* pcoin = &(*it).second;
            ntotal += pcoin->getimmaturewatchonlycredit();
        }
    }
    return ntotal;
}

/**
 * populate vcoins with vector of available coutputs.
 */
void cwallet::availablecoins(vector<coutput>& vcoins, bool fonlyconfirmed, const ccoincontrol *coincontrol, bool fincludezerovalue) const
{
    vcoins.clear();

    {
        lock2(cs_main, cs_wallet);
        for (map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); ++it)
        {
            const uint256& wtxid = it->first;
            const cwallettx* pcoin = &(*it).second;

            if (!checkfinaltx(*pcoin))
                continue;

            if (fonlyconfirmed && !pcoin->istrusted())
                continue;

            if (pcoin->iscoinbase() && pcoin->getblockstomaturity() > 0)
                continue;

            int ndepth = pcoin->getdepthinmainchain();
            if (ndepth < 0)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                isminetype mine = ismine(pcoin->vout[i]);
                if (!(isspent(wtxid, i)) && mine != ismine_no &&
                    !islockedcoin((*it).first, i) && (pcoin->vout[i].nvalue > 0 || fincludezerovalue) &&
                    (!coincontrol || !coincontrol->hasselected() || coincontrol->isselected((*it).first, i)))
                        vcoins.push_back(coutput(pcoin, i, ndepth, (mine & ismine_spendable) != ismine_no));
            }
        }
    }
}

static void approximatebestsubset(vector<pair<camount, pair<const cwallettx*,unsigned int> > >vvalue, const camount& ntotallower, const camount& ntargetvalue,
                                  vector<char>& vfbest, camount& nbest, int iterations = 1000)
{
    vector<char> vfincluded;

    vfbest.assign(vvalue.size(), true);
    nbest = ntotallower;

    seed_insecure_rand();

    for (int nrep = 0; nrep < iterations && nbest != ntargetvalue; nrep++)
    {
        vfincluded.assign(vvalue.size(), false);
        camount ntotal = 0;
        bool freachedtarget = false;
        for (int npass = 0; npass < 2 && !freachedtarget; npass++)
        {
            for (unsigned int i = 0; i < vvalue.size(); i++)
            {
                //the solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng is fast. we do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (npass == 0 ? insecure_rand()&1 : !vfincluded[i])
                {
                    ntotal += vvalue[i].first;
                    vfincluded[i] = true;
                    if (ntotal >= ntargetvalue)
                    {
                        freachedtarget = true;
                        if (ntotal < nbest)
                        {
                            nbest = ntotal;
                            vfbest = vfincluded;
                        }
                        ntotal -= vvalue[i].first;
                        vfincluded[i] = false;
                    }
                }
            }
        }
    }
}

bool cwallet::selectcoinsminconf(const camount& ntargetvalue, int nconfmine, int nconftheirs, vector<coutput> vcoins,
                                 set<pair<const cwallettx*,unsigned int> >& setcoinsret, camount& nvalueret) const
{
    setcoinsret.clear();
    nvalueret = 0;

    // list of values less than target
    pair<camount, pair<const cwallettx*,unsigned int> > coinlowestlarger;
    coinlowestlarger.first = std::numeric_limits<camount>::max();
    coinlowestlarger.second.first = null;
    vector<pair<camount, pair<const cwallettx*,unsigned int> > > vvalue;
    camount ntotallower = 0;

    random_shuffle(vcoins.begin(), vcoins.end(), getrandint);

    boost_foreach(const coutput &output, vcoins)
    {
        if (!output.fspendable)
            continue;

        const cwallettx *pcoin = output.tx;

        if (output.ndepth < (pcoin->isfromme(ismine_all) ? nconfmine : nconftheirs))
            continue;

        int i = output.i;
        camount n = pcoin->vout[i].nvalue;

        pair<camount,pair<const cwallettx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == ntargetvalue)
        {
            setcoinsret.insert(coin.second);
            nvalueret += coin.first;
            return true;
        }
        else if (n < ntargetvalue + cent)
        {
            vvalue.push_back(coin);
            ntotallower += n;
        }
        else if (n < coinlowestlarger.first)
        {
            coinlowestlarger = coin;
        }
    }

    if (ntotallower == ntargetvalue)
    {
        for (unsigned int i = 0; i < vvalue.size(); ++i)
        {
            setcoinsret.insert(vvalue[i].second);
            nvalueret += vvalue[i].first;
        }
        return true;
    }

    if (ntotallower < ntargetvalue)
    {
        if (coinlowestlarger.second.first == null)
            return false;
        setcoinsret.insert(coinlowestlarger.second);
        nvalueret += coinlowestlarger.first;
        return true;
    }

    // solve subset sum by stochastic approximation
    sort(vvalue.rbegin(), vvalue.rend(), comparevalueonly());
    vector<char> vfbest;
    camount nbest;

    approximatebestsubset(vvalue, ntotallower, ntargetvalue, vfbest, nbest, 1000);
    if (nbest != ntargetvalue && ntotallower >= ntargetvalue + cent)
        approximatebestsubset(vvalue, ntotallower, ntargetvalue + cent, vfbest, nbest, 1000);

    // if we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinlowestlarger.second.first &&
        ((nbest != ntargetvalue && nbest < ntargetvalue + cent) || coinlowestlarger.first <= nbest))
    {
        setcoinsret.insert(coinlowestlarger.second);
        nvalueret += coinlowestlarger.first;
    }
    else {
        for (unsigned int i = 0; i < vvalue.size(); i++)
            if (vfbest[i])
            {
                setcoinsret.insert(vvalue[i].second);
                nvalueret += vvalue[i].first;
            }

        logprint("selectcoins", "selectcoins() best subset: ");
        for (unsigned int i = 0; i < vvalue.size(); i++)
            if (vfbest[i])
                logprint("selectcoins", "%s ", formatmoney(vvalue[i].first));
        logprint("selectcoins", "total %s\n", formatmoney(nbest));
    }

    return true;
}

bool cwallet::selectcoins(const camount& ntargetvalue, set<pair<const cwallettx*,unsigned int> >& setcoinsret, camount& nvalueret, const ccoincontrol* coincontrol) const
{
    vector<coutput> vcoins;
    availablecoins(vcoins, true, coincontrol);

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coincontrol && coincontrol->hasselected())
    {
        boost_foreach(const coutput& out, vcoins)
        {
            if(!out.fspendable)
                continue;
            nvalueret += out.tx->vout[out.i].nvalue;
            setcoinsret.insert(make_pair(out.tx, out.i));
        }
        return (nvalueret >= ntargetvalue);
    }

    return (selectcoinsminconf(ntargetvalue, 1, 6, vcoins, setcoinsret, nvalueret) ||
            selectcoinsminconf(ntargetvalue, 1, 1, vcoins, setcoinsret, nvalueret) ||
            (bspendzeroconfchange && selectcoinsminconf(ntargetvalue, 0, 1, vcoins, setcoinsret, nvalueret)));
}

bool cwallet::createtransaction(const vector<crecipient>& vecsend,
                                cwallettx& wtxnew, creservekey& reservekey, camount& nfeeret, int& nchangeposret, std::string& strfailreason, const ccoincontrol* coincontrol)
{
    camount nvalue = 0;
    unsigned int nsubtractfeefromamount = 0;
    boost_foreach (const crecipient& recipient, vecsend)
    {
        if (nvalue < 0 || recipient.namount < 0)
        {
            strfailreason = _("transaction amounts must be positive");
            return false;
        }
        nvalue += recipient.namount;

        if (recipient.fsubtractfeefromamount)
            nsubtractfeefromamount++;
    }
    if (vecsend.empty() || nvalue < 0)
    {
        strfailreason = _("transaction amounts must be positive");
        return false;
    }

    wtxnew.ftimereceivedistxtime = true;
    wtxnew.bindwallet(this);
    cmutabletransaction txnew;

    // discourage fee sniping.
    //
    // however because of a off-by-one-error in previous versions we need to
    // neuter it by setting nlocktime to at least one less than nbestheight.
    // secondly currently propagation of transactions created for block heights
    // corresponding to blocks that were just mined may be iffy - transactions
    // aren't re-accepted into the mempool - we additionally neuter the code by
    // going ten blocks back. doesn't yet do anything for sniping, but does act
    // to shake out wallet bugs like not showing nlocktime'd transactions at
    // all.
    txnew.nlocktime = std::max(0, chainactive.height() - 10);

    // secondly occasionally randomly pick a nlocktime even further back, so
    // that transactions that are delayed after signing for whatever reason,
    // e.g. high-latency mix networks and some coinjoin implementations, have
    // better privacy.
    if (getrandint(10) == 0)
        txnew.nlocktime = std::max(0, (int)txnew.nlocktime - getrandint(100));

    assert(txnew.nlocktime <= (unsigned int)chainactive.height());
    assert(txnew.nlocktime < locktime_threshold);

    {
        lock2(cs_main, cs_wallet);
        {
            nfeeret = 0;
            while (true)
            {
                txnew.vin.clear();
                txnew.vout.clear();
                wtxnew.ffromme = true;
                nchangeposret = -1;
                bool ffirst = true;

                camount ntotalvalue = nvalue;
                if (nsubtractfeefromamount == 0)
                    ntotalvalue += nfeeret;
                double dpriority = 0;
                // vouts to the payees
                boost_foreach (const crecipient& recipient, vecsend)
                {
                    ctxout txout(recipient.namount, recipient.scriptpubkey);

                    if (recipient.fsubtractfeefromamount)
                    {
                        txout.nvalue -= nfeeret / nsubtractfeefromamount; // subtract fee equally from each selected recipient

                        if (ffirst) // first receiver pays the remainder not divisible by output count
                        {
                            ffirst = false;
                            txout.nvalue -= nfeeret % nsubtractfeefromamount;
                        }
                    }

                    if (txout.isdust(::minrelaytxfee))
                    {
                        if (recipient.fsubtractfeefromamount && nfeeret > 0)
                        {
                            if (txout.nvalue < 0)
                                strfailreason = _("the transaction amount is too small to pay the fee");
                            else
                                strfailreason = _("the transaction amount is too small to send after the fee has been deducted");
                        }
                        else
                            strfailreason = _("transaction amount too small");
                        return false;
                    }
                    txnew.vout.push_back(txout);
                }

                // choose coins to use
                set<pair<const cwallettx*,unsigned int> > setcoins;
                camount nvaluein = 0;
                if (!selectcoins(ntotalvalue, setcoins, nvaluein, coincontrol))
                {
                    strfailreason = _("insufficient funds");
                    return false;
                }
                boost_foreach(pairtype(const cwallettx*, unsigned int) pcoin, setcoins)
                {
                    camount ncredit = pcoin.first->vout[pcoin.second].nvalue;
                    //the coin age after the next block (depth+1) is used instead of the current,
                    //reflecting an assumption the user would accept a bit more delay for
                    //a chance at a free transaction.
                    //but mempool inputs might still be in the mempool, so their age stays 0
                    int age = pcoin.first->getdepthinmainchain();
                    if (age != 0)
                        age += 1;
                    dpriority += (double)ncredit * age;
                }

                camount nchange = nvaluein - nvalue;
                if (nsubtractfeefromamount == 0)
                    nchange -= nfeeret;

                if (nchange > 0)
                {
                    // fill a vout to ourself
                    // todo: pass in scriptchange instead of reservekey so
                    // change transaction isn't always pay-to-moorecoin-address
                    cscript scriptchange;

                    // coin control: send change to custom address
                    if (coincontrol && !boost::get<cnodestination>(&coincontrol->destchange))
                        scriptchange = getscriptfordestination(coincontrol->destchange);

                    // no coin control: send change to newly generated address
                    else
                    {
                        // note: we use a new key here to keep it from being obvious which side is the change.
                        //  the drawback is that by not reusing a previous key, the change may be lost if a
                        //  backup is restored, if the backup doesn't have the new private key for the change.
                        //  if we reused the old key, it would be possible to add code to look for and
                        //  rediscover unknown transactions that were written with keys of ours to recover
                        //  post-backup change.

                        // reserve a new key pair from key pool
                        cpubkey vchpubkey;
                        bool ret;
                        ret = reservekey.getreservedkey(vchpubkey);
                        assert(ret); // should never fail, as we just unlocked

                        scriptchange = getscriptfordestination(vchpubkey.getid());
                    }

                    ctxout newtxout(nchange, scriptchange);

                    // we do not move dust-change to fees, because the sender would end up paying more than requested.
                    // this would be against the purpose of the all-inclusive feature.
                    // so instead we raise the change and deduct from the recipient.
                    if (nsubtractfeefromamount > 0 && newtxout.isdust(::minrelaytxfee))
                    {
                        camount ndust = newtxout.getdustthreshold(::minrelaytxfee) - newtxout.nvalue;
                        newtxout.nvalue += ndust; // raise change until no more dust
                        for (unsigned int i = 0; i < vecsend.size(); i++) // subtract from first recipient
                        {
                            if (vecsend[i].fsubtractfeefromamount)
                            {
                                txnew.vout[i].nvalue -= ndust;
                                if (txnew.vout[i].isdust(::minrelaytxfee))
                                {
                                    strfailreason = _("the transaction amount is too small to send after the fee has been deducted");
                                    return false;
                                }
                                break;
                            }
                        }
                    }

                    // never create dust outputs; if we would, just
                    // add the dust to the fee.
                    if (newtxout.isdust(::minrelaytxfee))
                    {
                        nfeeret += nchange;
                        reservekey.returnkey();
                    }
                    else
                    {
                        // insert change txn at random position:
                        nchangeposret = getrandint(txnew.vout.size()+1);
                        vector<ctxout>::iterator position = txnew.vout.begin()+nchangeposret;
                        txnew.vout.insert(position, newtxout);
                    }
                }
                else
                    reservekey.returnkey();

                // fill vin
                //
                // note how the sequence number is set to max()-1 so that the
                // nlocktime set above actually works.
                boost_foreach(const pairtype(const cwallettx*,unsigned int)& coin, setcoins)
                    txnew.vin.push_back(ctxin(coin.first->gethash(),coin.second,cscript(),
                                              std::numeric_limits<unsigned int>::max()-1));

                // sign
                int nin = 0;
                boost_foreach(const pairtype(const cwallettx*,unsigned int)& coin, setcoins)
                    if (!signsignature(*this, *coin.first, txnew, nin++))
                    {
                        strfailreason = _("signing transaction failed");
                        return false;
                    }

                // embed the constructed transaction data in wtxnew.
                *static_cast<ctransaction*>(&wtxnew) = ctransaction(txnew);

                // limit size
                unsigned int nbytes = ::getserializesize(*(ctransaction*)&wtxnew, ser_network, protocol_version);
                if (nbytes >= max_standard_tx_size)
                {
                    strfailreason = _("transaction too large");
                    return false;
                }
                dpriority = wtxnew.computepriority(dpriority, nbytes);

                // can we complete this as a free transaction?
                if (fsendfreetransactions && nbytes <= max_free_transaction_create_size)
                {
                    // not enough fee: enough priority?
                    double dpriorityneeded = mempool.estimatepriority(ntxconfirmtarget);
                    // not enough mempool history to estimate: use hard-coded allowfree.
                    if (dpriorityneeded <= 0 && allowfree(dpriority))
                        break;

                    // small enough, and priority high enough, to send for free
                    if (dpriorityneeded > 0 && dpriority >= dpriorityneeded)
                        break;
                }

                camount nfeeneeded = getminimumfee(nbytes, ntxconfirmtarget, mempool);

                // if we made it here and we aren't even able to meet the relay fee on the next pass, give up
                // because we must be at the maximum allowed fee.
                if (nfeeneeded < ::minrelaytxfee.getfee(nbytes))
                {
                    strfailreason = _("transaction too large for fee policy");
                    return false;
                }

                if (nfeeret >= nfeeneeded)
                    break; // done, enough fee included.

                // include more fee and try again.
                nfeeret = nfeeneeded;
                continue;
            }
        }
    }

    return true;
}

/**
 * call after createtransaction unless you want to abort
 */
bool cwallet::committransaction(cwallettx& wtxnew, creservekey& reservekey)
{
    {
        lock2(cs_main, cs_wallet);
        logprintf("committransaction:\n%s", wtxnew.tostring());
        {
            // this is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  this is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            cwalletdb* pwalletdb = ffilebacked ? new cwalletdb(strwalletfile,"r+") : null;

            // take key pair from key pool so it won't be used again
            reservekey.keepkey();

            // add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            addtowallet(wtxnew, false, pwalletdb);

            // notify that old coins are spent
            set<cwallettx*> setcoins;
            boost_foreach(const ctxin& txin, wtxnew.vin)
            {
                cwallettx &coin = mapwallet[txin.prevout.hash];
                coin.bindwallet(this);
                notifytransactionchanged(this, coin.gethash(), ct_updated);
            }

            if (ffilebacked)
                delete pwalletdb;
        }

        // track how many getdata requests our transaction gets
        maprequestcount[wtxnew.gethash()] = 0;

        if (fbroadcasttransactions)
        {
            // broadcast
            if (!wtxnew.accepttomemorypool(false))
            {
                // this must not fail. the transaction has already been signed and recorded.
                logprintf("committransaction(): error: transaction not valid");
                return false;
            }
            wtxnew.relaywallettransaction();
        }
    }
    return true;
}

camount cwallet::getminimumfee(unsigned int ntxbytes, unsigned int nconfirmtarget, const ctxmempool& pool)
{
    // paytxfee is user-set "i want to pay this much"
    camount nfeeneeded = paytxfee.getfee(ntxbytes);
    // user selected total at least (default=true)
    if (fpayatleastcustomfee && nfeeneeded > 0 && nfeeneeded < paytxfee.getfeeperk())
        nfeeneeded = paytxfee.getfeeperk();
    // user didn't set: use -txconfirmtarget to estimate...
    if (nfeeneeded == 0)
        nfeeneeded = pool.estimatefee(nconfirmtarget).getfee(ntxbytes);
    // ... unless we don't have enough mempool data, in which case fall
    // back to a hard-coded fee
    if (nfeeneeded == 0)
        nfeeneeded = mintxfee.getfee(ntxbytes);
    // prevent user from paying a non-sense fee (like 1 satoshi): 0 < fee < minrelayfee
    if (nfeeneeded < ::minrelaytxfee.getfee(ntxbytes))
        nfeeneeded = ::minrelaytxfee.getfee(ntxbytes);
    // but always obey the maximum
    if (nfeeneeded > maxtxfee)
        nfeeneeded = maxtxfee;
    return nfeeneeded;
}




dberrors cwallet::loadwallet(bool& ffirstrunret)
{
    if (!ffilebacked)
        return db_load_ok;
    ffirstrunret = false;
    dberrors nloadwalletret = cwalletdb(strwalletfile,"cr+").loadwallet(this);
    if (nloadwalletret == db_need_rewrite)
    {
        if (cdb::rewrite(strwalletfile, "\x04pool"))
        {
            lock(cs_wallet);
            setkeypool.clear();
            // note: can't top-up keypool here, because wallet is locked.
            // user will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nloadwalletret != db_load_ok)
        return nloadwalletret;
    ffirstrunret = !vchdefaultkey.isvalid();

    uiinterface.loadwallet(this);

    return db_load_ok;
}


dberrors cwallet::zapwallettx(std::vector<cwallettx>& vwtx)
{
    if (!ffilebacked)
        return db_load_ok;
    dberrors nzapwallettxret = cwalletdb(strwalletfile,"cr+").zapwallettx(this, vwtx);
    if (nzapwallettxret == db_need_rewrite)
    {
        if (cdb::rewrite(strwalletfile, "\x04pool"))
        {
            lock(cs_wallet);
            setkeypool.clear();
            // note: can't top-up keypool here, because wallet is locked.
            // user will be prompted to unlock wallet the next operation
            // that requires a new key.
        }
    }

    if (nzapwallettxret != db_load_ok)
        return nzapwallettxret;

    return db_load_ok;
}


bool cwallet::setaddressbook(const ctxdestination& address, const string& strname, const string& strpurpose)
{
    bool fupdated = false;
    {
        lock(cs_wallet); // mapaddressbook
        std::map<ctxdestination, caddressbookdata>::iterator mi = mapaddressbook.find(address);
        fupdated = mi != mapaddressbook.end();
        mapaddressbook[address].name = strname;
        if (!strpurpose.empty()) /* update purpose only if requested */
            mapaddressbook[address].purpose = strpurpose;
    }
    notifyaddressbookchanged(this, address, strname, ::ismine(*this, address) != ismine_no,
                             strpurpose, (fupdated ? ct_updated : ct_new) );
    if (!ffilebacked)
        return false;
    if (!strpurpose.empty() && !cwalletdb(strwalletfile).writepurpose(cmoorecoinaddress(address).tostring(), strpurpose))
        return false;
    return cwalletdb(strwalletfile).writename(cmoorecoinaddress(address).tostring(), strname);
}

bool cwallet::deladdressbook(const ctxdestination& address)
{
    {
        lock(cs_wallet); // mapaddressbook

        if(ffilebacked)
        {
            // delete destdata tuples associated with address
            std::string straddress = cmoorecoinaddress(address).tostring();
            boost_foreach(const pairtype(string, string) &item, mapaddressbook[address].destdata)
            {
                cwalletdb(strwalletfile).erasedestdata(straddress, item.first);
            }
        }
        mapaddressbook.erase(address);
    }

    notifyaddressbookchanged(this, address, "", ::ismine(*this, address) != ismine_no, "", ct_deleted);

    if (!ffilebacked)
        return false;
    cwalletdb(strwalletfile).erasepurpose(cmoorecoinaddress(address).tostring());
    return cwalletdb(strwalletfile).erasename(cmoorecoinaddress(address).tostring());
}

bool cwallet::setdefaultkey(const cpubkey &vchpubkey)
{
    if (ffilebacked)
    {
        if (!cwalletdb(strwalletfile).writedefaultkey(vchpubkey))
            return false;
    }
    vchdefaultkey = vchpubkey;
    return true;
}

/**
 * mark old keypool keys as used,
 * and generate all new keys 
 */
bool cwallet::newkeypool()
{
    {
        lock(cs_wallet);
        cwalletdb walletdb(strwalletfile);
        boost_foreach(int64_t nindex, setkeypool)
            walletdb.erasepool(nindex);
        setkeypool.clear();

        if (islocked())
            return false;

        int64_t nkeys = max(getarg("-keypool", 100), (int64_t)0);
        for (int i = 0; i < nkeys; i++)
        {
            int64_t nindex = i+1;
            walletdb.writepool(nindex, ckeypool(generatenewkey()));
            setkeypool.insert(nindex);
        }
        logprintf("cwallet::newkeypool wrote %d new keys\n", nkeys);
    }
    return true;
}

bool cwallet::topupkeypool(unsigned int kpsize)
{
    {
        lock(cs_wallet);

        if (islocked())
            return false;

        cwalletdb walletdb(strwalletfile);

        // top up key pool
        unsigned int ntargetsize;
        if (kpsize > 0)
            ntargetsize = kpsize;
        else
            ntargetsize = max(getarg("-keypool", 100), (int64_t) 0);

        while (setkeypool.size() < (ntargetsize + 1))
        {
            int64_t nend = 1;
            if (!setkeypool.empty())
                nend = *(--setkeypool.end()) + 1;
            if (!walletdb.writepool(nend, ckeypool(generatenewkey())))
                throw runtime_error("topupkeypool(): writing generated key failed");
            setkeypool.insert(nend);
            logprintf("keypool added key %d, size=%u\n", nend, setkeypool.size());
        }
    }
    return true;
}

void cwallet::reservekeyfromkeypool(int64_t& nindex, ckeypool& keypool)
{
    nindex = -1;
    keypool.vchpubkey = cpubkey();
    {
        lock(cs_wallet);

        if (!islocked())
            topupkeypool();

        // get the oldest key
        if(setkeypool.empty())
            return;

        cwalletdb walletdb(strwalletfile);

        nindex = *(setkeypool.begin());
        setkeypool.erase(setkeypool.begin());
        if (!walletdb.readpool(nindex, keypool))
            throw runtime_error("reservekeyfromkeypool(): read failed");
        if (!havekey(keypool.vchpubkey.getid()))
            throw runtime_error("reservekeyfromkeypool(): unknown key in key pool");
        assert(keypool.vchpubkey.isvalid());
        logprintf("keypool reserve %d\n", nindex);
    }
}

void cwallet::keepkey(int64_t nindex)
{
    // remove from key pool
    if (ffilebacked)
    {
        cwalletdb walletdb(strwalletfile);
        walletdb.erasepool(nindex);
    }
    logprintf("keypool keep %d\n", nindex);
}

void cwallet::returnkey(int64_t nindex)
{
    // return to key pool
    {
        lock(cs_wallet);
        setkeypool.insert(nindex);
    }
    logprintf("keypool return %d\n", nindex);
}

bool cwallet::getkeyfrompool(cpubkey& result)
{
    int64_t nindex = 0;
    ckeypool keypool;
    {
        lock(cs_wallet);
        reservekeyfromkeypool(nindex, keypool);
        if (nindex == -1)
        {
            if (islocked()) return false;
            result = generatenewkey();
            return true;
        }
        keepkey(nindex);
        result = keypool.vchpubkey;
    }
    return true;
}

int64_t cwallet::getoldestkeypooltime()
{
    int64_t nindex = 0;
    ckeypool keypool;
    reservekeyfromkeypool(nindex, keypool);
    if (nindex == -1)
        return gettime();
    returnkey(nindex);
    return keypool.ntime;
}

std::map<ctxdestination, camount> cwallet::getaddressbalances()
{
    map<ctxdestination, camount> balances;

    {
        lock(cs_wallet);
        boost_foreach(pairtype(uint256, cwallettx) walletentry, mapwallet)
        {
            cwallettx *pcoin = &walletentry.second;

            if (!checkfinaltx(*pcoin) || !pcoin->istrusted())
                continue;

            if (pcoin->iscoinbase() && pcoin->getblockstomaturity() > 0)
                continue;

            int ndepth = pcoin->getdepthinmainchain();
            if (ndepth < (pcoin->isfromme(ismine_all) ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                ctxdestination addr;
                if (!ismine(pcoin->vout[i]))
                    continue;
                if(!extractdestination(pcoin->vout[i].scriptpubkey, addr))
                    continue;

                camount n = isspent(walletentry.first, i) ? 0 : pcoin->vout[i].nvalue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<ctxdestination> > cwallet::getaddressgroupings()
{
    assertlockheld(cs_wallet); // mapwallet
    set< set<ctxdestination> > groupings;
    set<ctxdestination> grouping;

    boost_foreach(pairtype(uint256, cwallettx) walletentry, mapwallet)
    {
        cwallettx *pcoin = &walletentry.second;

        if (pcoin->vin.size() > 0)
        {
            bool any_mine = false;
            // group all input addresses with each other
            boost_foreach(ctxin txin, pcoin->vin)
            {
                ctxdestination address;
                if(!ismine(txin)) /* if this input isn't mine, ignore it */
                    continue;
                if(!extractdestination(mapwallet[txin.prevout.hash].vout[txin.prevout.n].scriptpubkey, address))
                    continue;
                grouping.insert(address);
                any_mine = true;
            }

            // group change with input addresses
            if (any_mine)
            {
               boost_foreach(ctxout txout, pcoin->vout)
                   if (ischange(txout))
                   {
                       ctxdestination txoutaddr;
                       if(!extractdestination(txout.scriptpubkey, txoutaddr))
                           continue;
                       grouping.insert(txoutaddr);
                   }
            }
            if (grouping.size() > 0)
            {
                groupings.insert(grouping);
                grouping.clear();
            }
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (ismine(pcoin->vout[i]))
            {
                ctxdestination address;
                if(!extractdestination(pcoin->vout[i].scriptpubkey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set< set<ctxdestination>* > uniquegroupings; // a set of pointers to groups of addresses
    map< ctxdestination, set<ctxdestination>* > setmap;  // map addresses to the unique group containing it
    boost_foreach(set<ctxdestination> grouping, groupings)
    {
        // make a set of all the groups hit by this new group
        set< set<ctxdestination>* > hits;
        map< ctxdestination, set<ctxdestination>* >::iterator it;
        boost_foreach(ctxdestination address, grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<ctxdestination>* merged = new set<ctxdestination>(grouping);
        boost_foreach(set<ctxdestination>* hit, hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniquegroupings.erase(hit);
            delete hit;
        }
        uniquegroupings.insert(merged);

        // update setmap
        boost_foreach(ctxdestination element, *merged)
            setmap[element] = merged;
    }

    set< set<ctxdestination> > ret;
    boost_foreach(set<ctxdestination>* uniquegrouping, uniquegroupings)
    {
        ret.insert(*uniquegrouping);
        delete uniquegrouping;
    }

    return ret;
}

std::set<ctxdestination> cwallet::getaccountaddresses(const std::string& straccount) const
{
    lock(cs_wallet);
    set<ctxdestination> result;
    boost_foreach(const pairtype(ctxdestination, caddressbookdata)& item, mapaddressbook)
    {
        const ctxdestination& address = item.first;
        const string& strname = item.second.name;
        if (strname == straccount)
            result.insert(address);
    }
    return result;
}

bool creservekey::getreservedkey(cpubkey& pubkey)
{
    if (nindex == -1)
    {
        ckeypool keypool;
        pwallet->reservekeyfromkeypool(nindex, keypool);
        if (nindex != -1)
            vchpubkey = keypool.vchpubkey;
        else {
            return false;
        }
    }
    assert(vchpubkey.isvalid());
    pubkey = vchpubkey;
    return true;
}

void creservekey::keepkey()
{
    if (nindex != -1)
        pwallet->keepkey(nindex);
    nindex = -1;
    vchpubkey = cpubkey();
}

void creservekey::returnkey()
{
    if (nindex != -1)
        pwallet->returnkey(nindex);
    nindex = -1;
    vchpubkey = cpubkey();
}

void cwallet::getallreservekeys(set<ckeyid>& setaddress) const
{
    setaddress.clear();

    cwalletdb walletdb(strwalletfile);

    lock2(cs_main, cs_wallet);
    boost_foreach(const int64_t& id, setkeypool)
    {
        ckeypool keypool;
        if (!walletdb.readpool(id, keypool))
            throw runtime_error("getallreservekeyhashes(): read failed");
        assert(keypool.vchpubkey.isvalid());
        ckeyid keyid = keypool.vchpubkey.getid();
        if (!havekey(keyid))
            throw runtime_error("getallreservekeyhashes(): unknown key in key pool");
        setaddress.insert(keyid);
    }
}

void cwallet::updatedtransaction(const uint256 &hashtx)
{
    {
        lock(cs_wallet);
        // only notify ui if this transaction is in this wallet
        map<uint256, cwallettx>::const_iterator mi = mapwallet.find(hashtx);
        if (mi != mapwallet.end())
            notifytransactionchanged(this, hashtx, ct_updated);
    }
}

void cwallet::lockcoin(coutpoint& output)
{
    assertlockheld(cs_wallet); // setlockedcoins
    setlockedcoins.insert(output);
}

void cwallet::unlockcoin(coutpoint& output)
{
    assertlockheld(cs_wallet); // setlockedcoins
    setlockedcoins.erase(output);
}

void cwallet::unlockallcoins()
{
    assertlockheld(cs_wallet); // setlockedcoins
    setlockedcoins.clear();
}

bool cwallet::islockedcoin(uint256 hash, unsigned int n) const
{
    assertlockheld(cs_wallet); // setlockedcoins
    coutpoint outpt(hash, n);

    return (setlockedcoins.count(outpt) > 0);
}

void cwallet::listlockedcoins(std::vector<coutpoint>& voutpts)
{
    assertlockheld(cs_wallet); // setlockedcoins
    for (std::set<coutpoint>::iterator it = setlockedcoins.begin();
         it != setlockedcoins.end(); it++) {
        coutpoint outpt = (*it);
        voutpts.push_back(outpt);
    }
}

/** @} */ // end of actions

class caffectedkeysvisitor : public boost::static_visitor<void> {
private:
    const ckeystore &keystore;
    std::vector<ckeyid> &vkeys;

public:
    caffectedkeysvisitor(const ckeystore &keystorein, std::vector<ckeyid> &vkeysin) : keystore(keystorein), vkeys(vkeysin) {}

    void process(const cscript &script) {
        txnouttype type;
        std::vector<ctxdestination> vdest;
        int nrequired;
        if (extractdestinations(script, type, vdest, nrequired)) {
            boost_foreach(const ctxdestination &dest, vdest)
                boost::apply_visitor(*this, dest);
        }
    }

    void operator()(const ckeyid &keyid) {
        if (keystore.havekey(keyid))
            vkeys.push_back(keyid);
    }

    void operator()(const cscriptid &scriptid) {
        cscript script;
        if (keystore.getcscript(scriptid, script))
            process(script);
    }

    void operator()(const cnodestination &none) {}
};

void cwallet::getkeybirthtimes(std::map<ckeyid, int64_t> &mapkeybirth) const {
    assertlockheld(cs_wallet); // mapkeymetadata
    mapkeybirth.clear();

    // get birth times for keys with metadata
    for (std::map<ckeyid, ckeymetadata>::const_iterator it = mapkeymetadata.begin(); it != mapkeymetadata.end(); it++)
        if (it->second.ncreatetime)
            mapkeybirth[it->first] = it->second.ncreatetime;

    // map in which we'll infer heights of other keys
    cblockindex *pindexmax = chainactive[std::max(0, chainactive.height() - 144)]; // the tip can be reorganised; use a 144-block safety margin
    std::map<ckeyid, cblockindex*> mapkeyfirstblock;
    std::set<ckeyid> setkeys;
    getkeys(setkeys);
    boost_foreach(const ckeyid &keyid, setkeys) {
        if (mapkeybirth.count(keyid) == 0)
            mapkeyfirstblock[keyid] = pindexmax;
    }
    setkeys.clear();

    // if there are no such keys, we're done
    if (mapkeyfirstblock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<ckeyid> vaffected;
    for (std::map<uint256, cwallettx>::const_iterator it = mapwallet.begin(); it != mapwallet.end(); it++) {
        // iterate over all wallet transactions...
        const cwallettx &wtx = (*it).second;
        blockmap::const_iterator blit = mapblockindex.find(wtx.hashblock);
        if (blit != mapblockindex.end() && chainactive.contains(blit->second)) {
            // ... which are already in a block
            int nheight = blit->second->nheight;
            boost_foreach(const ctxout &txout, wtx.vout) {
                // iterate over all their outputs
                caffectedkeysvisitor(*this, vaffected).process(txout.scriptpubkey);
                boost_foreach(const ckeyid &keyid, vaffected) {
                    // ... and all their affected keys
                    std::map<ckeyid, cblockindex*>::iterator rit = mapkeyfirstblock.find(keyid);
                    if (rit != mapkeyfirstblock.end() && nheight < rit->second->nheight)
                        rit->second = blit->second;
                }
                vaffected.clear();
            }
        }
    }

    // extract block timestamps for those keys
    for (std::map<ckeyid, cblockindex*>::const_iterator it = mapkeyfirstblock.begin(); it != mapkeyfirstblock.end(); it++)
        mapkeybirth[it->first] = it->second->getblocktime() - 7200; // block times can be 2h off
}

bool cwallet::adddestdata(const ctxdestination &dest, const std::string &key, const std::string &value)
{
    if (boost::get<cnodestination>(&dest))
        return false;

    mapaddressbook[dest].destdata.insert(std::make_pair(key, value));
    if (!ffilebacked)
        return true;
    return cwalletdb(strwalletfile).writedestdata(cmoorecoinaddress(dest).tostring(), key, value);
}

bool cwallet::erasedestdata(const ctxdestination &dest, const std::string &key)
{
    if (!mapaddressbook[dest].destdata.erase(key))
        return false;
    if (!ffilebacked)
        return true;
    return cwalletdb(strwalletfile).erasedestdata(cmoorecoinaddress(dest).tostring(), key);
}

bool cwallet::loaddestdata(const ctxdestination &dest, const std::string &key, const std::string &value)
{
    mapaddressbook[dest].destdata.insert(std::make_pair(key, value));
    return true;
}

bool cwallet::getdestdata(const ctxdestination &dest, const std::string &key, std::string *value) const
{
    std::map<ctxdestination, caddressbookdata>::const_iterator i = mapaddressbook.find(dest);
    if(i != mapaddressbook.end())
    {
        caddressbookdata::stringmap::const_iterator j = i->second.destdata.find(key);
        if(j != i->second.destdata.end())
        {
            if(value)
                *value = j->second;
            return true;
        }
    }
    return false;
}

ckeypool::ckeypool()
{
    ntime = gettime();
}

ckeypool::ckeypool(const cpubkey& vchpubkeyin)
{
    ntime = gettime();
    vchpubkey = vchpubkeyin;
}

cwalletkey::cwalletkey(int64_t nexpires)
{
    ntimecreated = (nexpires ? gettime() : 0);
    ntimeexpires = nexpires;
}

int cmerkletx::setmerklebranch(const cblock& block)
{
    assertlockheld(cs_main);
    cblock blocktmp;

    // update the tx's hashblock
    hashblock = block.gethash();

    // locate the transaction
    for (nindex = 0; nindex < (int)block.vtx.size(); nindex++)
        if (block.vtx[nindex] == *(ctransaction*)this)
            break;
    if (nindex == (int)block.vtx.size())
    {
        vmerklebranch.clear();
        nindex = -1;
        logprintf("error: setmerklebranch(): couldn't find tx in block\n");
        return 0;
    }

    // fill in merkle branch
    vmerklebranch = block.getmerklebranch(nindex);

    // is the tx in a block that's in the main chain
    blockmap::iterator mi = mapblockindex.find(hashblock);
    if (mi == mapblockindex.end())
        return 0;
    const cblockindex* pindex = (*mi).second;
    if (!pindex || !chainactive.contains(pindex))
        return 0;

    return chainactive.height() - pindex->nheight + 1;
}

int cmerkletx::getdepthinmainchaininternal(const cblockindex* &pindexret) const
{
    if (hashblock.isnull() || nindex == -1)
        return 0;
    assertlockheld(cs_main);

    // find the block it claims to be in
    blockmap::iterator mi = mapblockindex.find(hashblock);
    if (mi == mapblockindex.end())
        return 0;
    cblockindex* pindex = (*mi).second;
    if (!pindex || !chainactive.contains(pindex))
        return 0;

    // make sure the merkle branch connects to this block
    if (!fmerkleverified)
    {
        if (cblock::checkmerklebranch(gethash(), vmerklebranch, nindex) != pindex->hashmerkleroot)
            return 0;
        fmerkleverified = true;
    }

    pindexret = pindex;
    return chainactive.height() - pindex->nheight + 1;
}

int cmerkletx::getdepthinmainchain(const cblockindex* &pindexret) const
{
    assertlockheld(cs_main);
    int nresult = getdepthinmainchaininternal(pindexret);
    if (nresult == 0 && !mempool.exists(gethash()))
        return -1; // not in chain, not in mempool

    return nresult;
}

int cmerkletx::getblockstomaturity() const
{
    if (!iscoinbase())
        return 0;
    return max(0, (coinbase_maturity+1) - getdepthinmainchain());
}


bool cmerkletx::accepttomemorypool(bool flimitfree, bool frejectabsurdfee)
{
    cvalidationstate state;
    return ::accepttomemorypool(mempool, state, *this, flimitfree, null, frejectabsurdfee);
}

