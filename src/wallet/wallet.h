// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_wallet_wallet_h
#define moorecoin_wallet_wallet_h

#include "amount.h"
#include "key.h"
#include "keystore.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "wallet/crypter.h"
#include "wallet/wallet_ismine.h"
#include "wallet/walletdb.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

/**
 * settings
 */
extern cfeerate paytxfee;
extern camount maxtxfee;
extern unsigned int ntxconfirmtarget;
extern bool bspendzeroconfchange;
extern bool fsendfreetransactions;
extern bool fpayatleastcustomfee;

//! -paytxfee default
static const camount default_transaction_fee = 0;
//! -paytxfee will warn if called with a higher fee than this amount (in satoshis) per kb
static const camount nhightransactionfeewarning = 0.01 * coin;
//! -maxtxfee default
static const camount default_transaction_maxfee = 0.1 * coin;
//! -txconfirmtarget default
static const unsigned int default_tx_confirm_target = 2;
//! -maxtxfee will warn if called with a higher fee than this amount (in satoshis)
static const camount nhightransactionmaxfeewarning = 100 * nhightransactionfeewarning;
//! largest (in bytes) free transaction we're willing to create
static const unsigned int max_free_transaction_create_size = 1000;

class caccountingentry;
class cblockindex;
class ccoincontrol;
class coutput;
class creservekey;
class cscript;
class ctxmempool;
class cwallettx;

/** (client) version numbers for particular wallet features */
enum walletfeature
{
    feature_base = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    feature_walletcrypt = 40000, // wallet encryption
    feature_comprpubkey = 60000, // compressed public keys

    feature_latest = 60000
};


/** a key pool entry */
class ckeypool
{
public:
    int64_t ntime;
    cpubkey vchpubkey;

    ckeypool();
    ckeypool(const cpubkey& vchpubkeyin);

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!(ntype & ser_gethash))
            readwrite(nversion);
        readwrite(ntime);
        readwrite(vchpubkey);
    }
};

/** address book data */
class caddressbookdata
{
public:
    std::string name;
    std::string purpose;

    caddressbookdata()
    {
        purpose = "unknown";
    }

    typedef std::map<std::string, std::string> stringmap;
    stringmap destdata;
};

struct crecipient
{
    cscript scriptpubkey;
    camount namount;
    bool fsubtractfeefromamount;
};

typedef std::map<std::string, std::string> mapvalue_t;


static void readorderpos(int64_t& norderpos, mapvalue_t& mapvalue)
{
    if (!mapvalue.count("n"))
    {
        norderpos = -1; // todo: calculate elsewhere
        return;
    }
    norderpos = atoi64(mapvalue["n"].c_str());
}


static void writeorderpos(const int64_t& norderpos, mapvalue_t& mapvalue)
{
    if (norderpos == -1)
        return;
    mapvalue["n"] = i64tostr(norderpos);
}

struct coutputentry
{
    ctxdestination destination;
    camount amount;
    int vout;
};

/** a transaction with a merkle branch linking it to the block chain. */
class cmerkletx : public ctransaction
{
private:
    int getdepthinmainchaininternal(const cblockindex* &pindexret) const;

public:
    uint256 hashblock;
    std::vector<uint256> vmerklebranch;
    int nindex;

    // memory only
    mutable bool fmerkleverified;


    cmerkletx()
    {
        init();
    }

    cmerkletx(const ctransaction& txin) : ctransaction(txin)
    {
        init();
    }

    void init()
    {
        hashblock = uint256();
        nindex = -1;
        fmerkleverified = false;
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(*(ctransaction*)this);
        nversion = this->nversion;
        readwrite(hashblock);
        readwrite(vmerklebranch);
        readwrite(nindex);
    }

    int setmerklebranch(const cblock& block);


    /**
     * return depth of transaction in blockchain:
     * -1  : not in blockchain, and not in memory pool (conflicted transaction)
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int getdepthinmainchain(const cblockindex* &pindexret) const;
    int getdepthinmainchain() const { const cblockindex *pindexret; return getdepthinmainchain(pindexret); }
    bool isinmainchain() const { const cblockindex *pindexret; return getdepthinmainchaininternal(pindexret) > 0; }
    int getblockstomaturity() const;
    bool accepttomemorypool(bool flimitfree=true, bool frejectabsurdfee=true);
};

/** 
 * a transaction with a bunch of additional info that only the owner cares about.
 * it includes any unrecorded transactions needed to link it back to the block chain.
 */
class cwallettx : public cmerkletx
{
private:
    const cwallet* pwallet;

public:
    mapvalue_t mapvalue;
    std::vector<std::pair<std::string, std::string> > vorderform;
    unsigned int ftimereceivedistxtime;
    unsigned int ntimereceived; //! time received by this node
    unsigned int ntimesmart;
    char ffromme;
    std::string strfromaccount;
    int64_t norderpos; //! position in ordered transaction list

    // memory only
    mutable bool fdebitcached;
    mutable bool fcreditcached;
    mutable bool fimmaturecreditcached;
    mutable bool favailablecreditcached;
    mutable bool fwatchdebitcached;
    mutable bool fwatchcreditcached;
    mutable bool fimmaturewatchcreditcached;
    mutable bool favailablewatchcreditcached;
    mutable bool fchangecached;
    mutable camount ndebitcached;
    mutable camount ncreditcached;
    mutable camount nimmaturecreditcached;
    mutable camount navailablecreditcached;
    mutable camount nwatchdebitcached;
    mutable camount nwatchcreditcached;
    mutable camount nimmaturewatchcreditcached;
    mutable camount navailablewatchcreditcached;
    mutable camount nchangecached;

    cwallettx()
    {
        init(null);
    }

    cwallettx(const cwallet* pwalletin)
    {
        init(pwalletin);
    }

    cwallettx(const cwallet* pwalletin, const cmerkletx& txin) : cmerkletx(txin)
    {
        init(pwalletin);
    }

    cwallettx(const cwallet* pwalletin, const ctransaction& txin) : cmerkletx(txin)
    {
        init(pwalletin);
    }

    void init(const cwallet* pwalletin)
    {
        pwallet = pwalletin;
        mapvalue.clear();
        vorderform.clear();
        ftimereceivedistxtime = false;
        ntimereceived = 0;
        ntimesmart = 0;
        ffromme = false;
        strfromaccount.clear();
        fdebitcached = false;
        fcreditcached = false;
        fimmaturecreditcached = false;
        favailablecreditcached = false;
        fwatchdebitcached = false;
        fwatchcreditcached = false;
        fimmaturewatchcreditcached = false;
        favailablewatchcreditcached = false;
        fchangecached = false;
        ndebitcached = 0;
        ncreditcached = 0;
        nimmaturecreditcached = 0;
        navailablecreditcached = 0;
        nwatchdebitcached = 0;
        nwatchcreditcached = 0;
        navailablewatchcreditcached = 0;
        nimmaturewatchcreditcached = 0;
        nchangecached = 0;
        norderpos = -1;
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (ser_action.forread())
            init(null);
        char fspent = false;

        if (!ser_action.forread())
        {
            mapvalue["fromaccount"] = strfromaccount;

            writeorderpos(norderpos, mapvalue);

            if (ntimesmart)
                mapvalue["timesmart"] = strprintf("%u", ntimesmart);
        }

        readwrite(*(cmerkletx*)this);
        std::vector<cmerkletx> vunused; //! used to be vtxprev
        readwrite(vunused);
        readwrite(mapvalue);
        readwrite(vorderform);
        readwrite(ftimereceivedistxtime);
        readwrite(ntimereceived);
        readwrite(ffromme);
        readwrite(fspent);

        if (ser_action.forread())
        {
            strfromaccount = mapvalue["fromaccount"];

            readorderpos(norderpos, mapvalue);

            ntimesmart = mapvalue.count("timesmart") ? (unsigned int)atoi64(mapvalue["timesmart"]) : 0;
        }

        mapvalue.erase("fromaccount");
        mapvalue.erase("version");
        mapvalue.erase("spent");
        mapvalue.erase("n");
        mapvalue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void markdirty()
    {
        fcreditcached = false;
        favailablecreditcached = false;
        fwatchdebitcached = false;
        fwatchcreditcached = false;
        favailablewatchcreditcached = false;
        fimmaturewatchcreditcached = false;
        fdebitcached = false;
        fchangecached = false;
    }

    void bindwallet(cwallet *pwalletin)
    {
        pwallet = pwalletin;
        markdirty();
    }

    //! filter decides which addresses will count towards the debit
    camount getdebit(const isminefilter& filter) const;
    camount getcredit(const isminefilter& filter) const;
    camount getimmaturecredit(bool fusecache=true) const;
    camount getavailablecredit(bool fusecache=true) const;
    camount getimmaturewatchonlycredit(const bool& fusecache=true) const;
    camount getavailablewatchonlycredit(const bool& fusecache=true) const;
    camount getchange() const;

    void getamounts(std::list<coutputentry>& listreceived,
                    std::list<coutputentry>& listsent, camount& nfee, std::string& strsentaccount, const isminefilter& filter) const;

    void getaccountamounts(const std::string& straccount, camount& nreceived,
                           camount& nsent, camount& nfee, const isminefilter& filter) const;

    bool isfromme(const isminefilter& filter) const
    {
        return (getdebit(filter) > 0);
    }

    bool istrusted() const;

    bool writetodisk(cwalletdb *pwalletdb);

    int64_t gettxtime() const;
    int getrequestcount() const;

    bool relaywallettransaction();

    std::set<uint256> getconflicts() const;
};




class coutput
{
public:
    const cwallettx *tx;
    int i;
    int ndepth;
    bool fspendable;

    coutput(const cwallettx *txin, int iin, int ndepthin, bool fspendablein)
    {
        tx = txin; i = iin; ndepth = ndepthin; fspendable = fspendablein;
    }

    std::string tostring() const;
};




/** private key that includes an expiration date in case it never gets used. */
class cwalletkey
{
public:
    cprivkey vchprivkey;
    int64_t ntimecreated;
    int64_t ntimeexpires;
    std::string strcomment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    cwalletkey(int64_t nexpires=0);

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!(ntype & ser_gethash))
            readwrite(nversion);
        readwrite(vchprivkey);
        readwrite(ntimecreated);
        readwrite(ntimeexpires);
        readwrite(limited_string(strcomment, 65536));
    }
};



/** 
 * a cwallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class cwallet : public ccryptokeystore, public cvalidationinterface
{
private:
    bool selectcoins(const camount& ntargetvalue, std::set<std::pair<const cwallettx*,unsigned int> >& setcoinsret, camount& nvalueret, const ccoincontrol *coincontrol = null) const;

    cwalletdb *pwalletdbencryption;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nwalletversion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nwalletmaxversion;

    int64_t nnextresend;
    int64_t nlastresend;
    bool fbroadcasttransactions;

    /**
     * used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<coutpoint, uint256> txspends;
    txspends maptxspends;
    void addtospends(const coutpoint& outpoint, const uint256& wtxid);
    void addtospends(const uint256& wtxid);

    void syncmetadata(std::pair<txspends::iterator, txspends::iterator>);

public:
    /*
     * main wallet lock.
     * this lock protects all the fields added by cwallet
     *   except for:
     *      ffilebacked (immutable after instantiation)
     *      strwalletfile (immutable after instantiation)
     */
    mutable ccriticalsection cs_wallet;

    bool ffilebacked;
    std::string strwalletfile;

    std::set<int64_t> setkeypool;
    std::map<ckeyid, ckeymetadata> mapkeymetadata;

    typedef std::map<unsigned int, cmasterkey> masterkeymap;
    masterkeymap mapmasterkeys;
    unsigned int nmasterkeymaxid;

    cwallet()
    {
        setnull();
    }

    cwallet(const std::string& strwalletfilein)
    {
        setnull();

        strwalletfile = strwalletfilein;
        ffilebacked = true;
    }

    ~cwallet()
    {
        delete pwalletdbencryption;
        pwalletdbencryption = null;
    }

    void setnull()
    {
        nwalletversion = feature_base;
        nwalletmaxversion = feature_base;
        ffilebacked = false;
        nmasterkeymaxid = 0;
        pwalletdbencryption = null;
        norderposnext = 0;
        nnextresend = 0;
        nlastresend = 0;
        ntimefirstkey = 0;
        fbroadcasttransactions = false;
    }

    std::map<uint256, cwallettx> mapwallet;

    int64_t norderposnext;
    std::map<uint256, int> maprequestcount;

    std::map<ctxdestination, caddressbookdata> mapaddressbook;

    cpubkey vchdefaultkey;

    std::set<coutpoint> setlockedcoins;

    int64_t ntimefirstkey;

    const cwallettx* getwallettx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool cansupportfeature(enum walletfeature wf) { assertlockheld(cs_wallet); return nwalletmaxversion >= wf; }

    void availablecoins(std::vector<coutput>& vcoins, bool fonlyconfirmed=true, const ccoincontrol *coincontrol = null, bool fincludezerovalue=false) const;
    bool selectcoinsminconf(const camount& ntargetvalue, int nconfmine, int nconftheirs, std::vector<coutput> vcoins, std::set<std::pair<const cwallettx*,unsigned int> >& setcoinsret, camount& nvalueret) const;

    bool isspent(const uint256& hash, unsigned int n) const;

    bool islockedcoin(uint256 hash, unsigned int n) const;
    void lockcoin(coutpoint& output);
    void unlockcoin(coutpoint& output);
    void unlockallcoins();
    void listlockedcoins(std::vector<coutpoint>& voutpts);

    /**
     * keystore implementation
     * generate a new key
     */
    cpubkey generatenewkey();
    //! adds a key to the store, and saves it to disk.
    bool addkeypubkey(const ckey& key, const cpubkey &pubkey);
    //! adds a key to the store, without saving it to disk (used by loadwallet)
    bool loadkey(const ckey& key, const cpubkey &pubkey) { return ccryptokeystore::addkeypubkey(key, pubkey); }
    //! load metadata (used by loadwallet)
    bool loadkeymetadata(const cpubkey &pubkey, const ckeymetadata &metadata);

    bool loadminversion(int nversion) { assertlockheld(cs_wallet); nwalletversion = nversion; nwalletmaxversion = std::max(nwalletmaxversion, nversion); return true; }

    //! adds an encrypted key to the store, and saves it to disk.
    bool addcryptedkey(const cpubkey &vchpubkey, const std::vector<unsigned char> &vchcryptedsecret);
    //! adds an encrypted key to the store, without saving it to disk (used by loadwallet)
    bool loadcryptedkey(const cpubkey &vchpubkey, const std::vector<unsigned char> &vchcryptedsecret);
    bool addcscript(const cscript& redeemscript);
    bool loadcscript(const cscript& redeemscript);

    //! adds a destination data tuple to the store, and saves it to disk
    bool adddestdata(const ctxdestination &dest, const std::string &key, const std::string &value);
    //! erases a destination data tuple in the store and on disk
    bool erasedestdata(const ctxdestination &dest, const std::string &key);
    //! adds a destination data tuple to the store, without saving it to disk
    bool loaddestdata(const ctxdestination &dest, const std::string &key, const std::string &value);
    //! look up a destination data tuple in the store, return true if found false otherwise
    bool getdestdata(const ctxdestination &dest, const std::string &key, std::string *value) const;

    //! adds a watch-only address to the store, and saves it to disk.
    bool addwatchonly(const cscript &dest);
    bool removewatchonly(const cscript &dest);
    //! adds a watch-only address to the store, without saving it to disk (used by loadwallet)
    bool loadwatchonly(const cscript &dest);

    bool unlock(const securestring& strwalletpassphrase);
    bool changewalletpassphrase(const securestring& stroldwalletpassphrase, const securestring& strnewwalletpassphrase);
    bool encryptwallet(const securestring& strwalletpassphrase);

    void getkeybirthtimes(std::map<ckeyid, int64_t> &mapkeybirth) const;

    /** 
     * increment the next transaction order id
     * @return next transaction order id
     */
    int64_t incorderposnext(cwalletdb *pwalletdb = null);

    typedef std::pair<cwallettx*, caccountingentry*> txpair;
    typedef std::multimap<int64_t, txpair > txitems;

    /**
     * get the wallet's activity log
     * @return multimap of ordered transactions and accounting entries
     * @warning returned pointers are *only* valid within the scope of passed acentries
     */
    txitems orderedtxitems(std::list<caccountingentry>& acentries, std::string straccount = "");

    void markdirty();
    bool addtowallet(const cwallettx& wtxin, bool ffromloadwallet, cwalletdb* pwalletdb);
    void synctransaction(const ctransaction& tx, const cblock* pblock);
    bool addtowalletifinvolvingme(const ctransaction& tx, const cblock* pblock, bool fupdate);
    int scanforwallettransactions(cblockindex* pindexstart, bool fupdate = false);
    void reacceptwallettransactions();
    void resendwallettransactions(int64_t nbestblocktime);
    std::vector<uint256> resendwallettransactionsbefore(int64_t ntime);
    camount getbalance() const;
    camount getunconfirmedbalance() const;
    camount getimmaturebalance() const;
    camount getwatchonlybalance() const;
    camount getunconfirmedwatchonlybalance() const;
    camount getimmaturewatchonlybalance() const;
    bool createtransaction(const std::vector<crecipient>& vecsend,
                           cwallettx& wtxnew, creservekey& reservekey, camount& nfeeret, int& nchangeposret, std::string& strfailreason, const ccoincontrol *coincontrol = null);
    bool committransaction(cwallettx& wtxnew, creservekey& reservekey);

    static cfeerate mintxfee;
    static camount getminimumfee(unsigned int ntxbytes, unsigned int nconfirmtarget, const ctxmempool& pool);

    bool newkeypool();
    bool topupkeypool(unsigned int kpsize = 0);
    void reservekeyfromkeypool(int64_t& nindex, ckeypool& keypool);
    void keepkey(int64_t nindex);
    void returnkey(int64_t nindex);
    bool getkeyfrompool(cpubkey &key);
    int64_t getoldestkeypooltime();
    void getallreservekeys(std::set<ckeyid>& setaddress) const;

    std::set< std::set<ctxdestination> > getaddressgroupings();
    std::map<ctxdestination, camount> getaddressbalances();

    std::set<ctxdestination> getaccountaddresses(const std::string& straccount) const;

    isminetype ismine(const ctxin& txin) const;
    camount getdebit(const ctxin& txin, const isminefilter& filter) const;
    isminetype ismine(const ctxout& txout) const;
    camount getcredit(const ctxout& txout, const isminefilter& filter) const;
    bool ischange(const ctxout& txout) const;
    camount getchange(const ctxout& txout) const;
    bool ismine(const ctransaction& tx) const;
    /** should probably be renamed to isrelevanttome */
    bool isfromme(const ctransaction& tx) const;
    camount getdebit(const ctransaction& tx, const isminefilter& filter) const;
    camount getcredit(const ctransaction& tx, const isminefilter& filter) const;
    camount getchange(const ctransaction& tx) const;
    void setbestchain(const cblocklocator& loc);

    dberrors loadwallet(bool& ffirstrunret);
    dberrors zapwallettx(std::vector<cwallettx>& vwtx);

    bool setaddressbook(const ctxdestination& address, const std::string& strname, const std::string& purpose);

    bool deladdressbook(const ctxdestination& address);

    void updatedtransaction(const uint256 &hashtx);

    void inventory(const uint256 &hash)
    {
        {
            lock(cs_wallet);
            std::map<uint256, int>::iterator mi = maprequestcount.find(hash);
            if (mi != maprequestcount.end())
                (*mi).second++;
        }
    }

    unsigned int getkeypoolsize()
    {
        assertlockheld(cs_wallet); // setkeypool
        return setkeypool.size();
    }

    bool setdefaultkey(const cpubkey &vchpubkey);

    //! signify that a particular wallet feature is now used. this may change nwalletversion and nwalletmaxversion if those are lower
    bool setminversion(enum walletfeature, cwalletdb* pwalletdbin = null, bool fexplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool setmaxversion(int nversion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int getversion() { lock(cs_wallet); return nwalletversion; }

    //! get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> getconflicts(const uint256& txid) const;

    //! flush wallet (bitdb flush)
    void flush(bool shutdown=false);

    //! verify the wallet database and perform salvage if required
    static bool verify(const std::string& walletfile, std::string& warningstring, std::string& errorstring);
    
    /** 
     * address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (cwallet *wallet, const ctxdestination
            &address, const std::string &label, bool ismine,
            const std::string &purpose,
            changetype status)> notifyaddressbookchanged;

    /** 
     * wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (cwallet *wallet, const uint256 &hashtx,
            changetype status)> notifytransactionchanged;

    /** show progress e.g. for rescan */
    boost::signals2::signal<void (const std::string &title, int nprogress)> showprogress;

    /** watch-only address added */
    boost::signals2::signal<void (bool fhavewatchonly)> notifywatchonlychanged;

    /** inquire whether this wallet broadcasts transactions. */
    bool getbroadcasttransactions() const { return fbroadcasttransactions; }
    /** set whether this wallet broadcasts transactions. */
    void setbroadcasttransactions(bool broadcast) { fbroadcasttransactions = broadcast; }
};

/** a key allocated from the key pool. */
class creservekey
{
protected:
    cwallet* pwallet;
    int64_t nindex;
    cpubkey vchpubkey;
public:
    creservekey(cwallet* pwalletin)
    {
        nindex = -1;
        pwallet = pwalletin;
    }

    ~creservekey()
    {
        returnkey();
    }

    void returnkey();
    bool getreservedkey(cpubkey &pubkey);
    void keepkey();
};


/** 
 * account information.
 * stored in wallet with key "acc"+string account name.
 */
class caccount
{
public:
    cpubkey vchpubkey;

    caccount()
    {
        setnull();
    }

    void setnull()
    {
        vchpubkey = cpubkey();
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!(ntype & ser_gethash))
            readwrite(nversion);
        readwrite(vchpubkey);
    }
};



/** 
 * internal transfers.
 * database key is acentry<account><counter>.
 */
class caccountingentry
{
public:
    std::string straccount;
    camount ncreditdebit;
    int64_t ntime;
    std::string strotheraccount;
    std::string strcomment;
    mapvalue_t mapvalue;
    int64_t norderpos;  //! position in ordered transaction list
    uint64_t nentryno;

    caccountingentry()
    {
        setnull();
    }

    void setnull()
    {
        ncreditdebit = 0;
        ntime = 0;
        straccount.clear();
        strotheraccount.clear();
        strcomment.clear();
        norderpos = -1;
        nentryno = 0;
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!(ntype & ser_gethash))
            readwrite(nversion);
        //! note: straccount is serialized as part of the key, not here.
        readwrite(ncreditdebit);
        readwrite(ntime);
        readwrite(limited_string(strotheraccount, 65536));

        if (!ser_action.forread())
        {
            writeorderpos(norderpos, mapvalue);

            if (!(mapvalue.empty() && _ssextra.empty()))
            {
                cdatastream ss(ntype, nversion);
                ss.insert(ss.begin(), '\0');
                ss << mapvalue;
                ss.insert(ss.end(), _ssextra.begin(), _ssextra.end());
                strcomment.append(ss.str());
            }
        }

        readwrite(limited_string(strcomment, 65536));

        size_t nseppos = strcomment.find("\0", 0, 1);
        if (ser_action.forread())
        {
            mapvalue.clear();
            if (std::string::npos != nseppos)
            {
                cdatastream ss(std::vector<char>(strcomment.begin() + nseppos + 1, strcomment.end()), ntype, nversion);
                ss >> mapvalue;
                _ssextra = std::vector<char>(ss.begin(), ss.end());
            }
            readorderpos(norderpos, mapvalue);
        }
        if (std::string::npos != nseppos)
            strcomment.erase(nseppos);

        mapvalue.erase("n");
    }

private:
    std::vector<char> _ssextra;
};

#endif // moorecoin_wallet_wallet_h
