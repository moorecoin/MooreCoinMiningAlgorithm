// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_coins_h
#define moorecoin_coins_h

#include "compressor.h"
#include "memusage.h"
#include "serialize.h"
#include "uint256.h"

#include <assert.h>
#include <stdint.h>

#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>

/** 
 * pruned version of ctransaction: only retains metadata and unspent transaction outputs
 *
 * serialized format:
 * - varint(nversion)
 * - varint(ncode)
 * - unspentness bitvector, for vout[2] and further; least significant byte first
 * - the non-spent ctxouts (via ctxoutcompressor)
 * - varint(nheight)
 *
 * the ncode value consists of:
 * - bit 1: iscoinbase()
 * - bit 2: vout[0] is not spent
 * - bit 4: vout[1] is not spent
 * - the higher bits encode n, the number of non-zero bytes in the following bitvector.
 *   - in case both bit 2 and bit 4 are unset, they encode n-1, as there must be at
 *     least one non-spent output).
 *
 * example: 0104835800816115944e077fe7c803cfa57f29b36bf87c1d358bb85e
 *          <><><--------------------------------------------><---->
 *          |  \                  |                             /
 *    version   code             vout[1]                  height
 *
 *    - version = 1
 *    - code = 4 (vout[1] is not spent, and 0 non-zero bytes of bitvector follow)
 *    - unspentness bitvector: as 0 non-zero bytes follow, it has length 0
 *    - vout[1]: 835800816115944e077fe7c803cfa57f29b36bf87c1d35
 *               * 8358: compact amount representation for 60000000000 (600 btc)
 *               * 00: special txout type pay-to-pubkey-hash
 *               * 816115944e077fe7c803cfa57f29b36bf87c1d35: address uint160
 *    - height = 203998
 *
 *
 * example: 0109044086ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4eebbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa486af3b
 *          <><><--><--------------------------------------------------><----------------------------------------------><---->
 *         /  \   \                     |                                                           |                     /
 *  version  code  unspentness       vout[4]                                                     vout[16]           height
 *
 *  - version = 1
 *  - code = 9 (coinbase, neither vout[0] or vout[1] are unspent,
 *                2 (1, +1 because both bit 2 and bit 4 are unset) non-zero bitvector bytes follow)
 *  - unspentness bitvector: bits 2 (0x04) and 14 (0x4000) are set, so vout[2+2] and vout[14+2] are unspent
 *  - vout[4]: 86ef97d5790061b01caab50f1b8e9c50a5057eb43c2d9563a4ee
 *             * 86ef97d579: compact amount representation for 234925952 (2.35 btc)
 *             * 00: special txout type pay-to-pubkey-hash
 *             * 61b01caab50f1b8e9c50a5057eb43c2d9563a4ee: address uint160
 *  - vout[16]: bbd123008c988f1a4a4de2161e0f50aac7f17e7f9555caa4
 *              * bbd123: compact amount representation for 110397 (0.001 btc)
 *              * 00: special txout type pay-to-pubkey-hash
 *              * 8c988f1a4a4de2161e0f50aac7f17e7f9555caa4: address uint160
 *  - height = 120891
 */
class ccoins
{
public:
    //! whether transaction is a coinbase
    bool fcoinbase;

    //! unspent transaction outputs; spent outputs are .isnull(); spent outputs at the end of the array are dropped
    std::vector<ctxout> vout;

    //! at which height this transaction was included in the active block chain
    int nheight;

    //! version of the ctransaction; accesses to this value should probably check for nheight as well,
    //! as new tx version will probably only be introduced at certain heights
    int nversion;

    void fromtx(const ctransaction &tx, int nheightin) {
        fcoinbase = tx.iscoinbase();
        vout = tx.vout;
        nheight = nheightin;
        nversion = tx.nversion;
        clearunspendable();
    }

    //! construct a ccoins from a ctransaction, at a given height
    ccoins(const ctransaction &tx, int nheightin) {
        fromtx(tx, nheightin);
    }

    void clear() {
        fcoinbase = false;
        std::vector<ctxout>().swap(vout);
        nheight = 0;
        nversion = 0;
    }

    //! empty constructor
    ccoins() : fcoinbase(false), vout(0), nheight(0), nversion(0) { }

    //!remove spent outputs at the end of vout
    void cleanup() {
        while (vout.size() > 0 && vout.back().isnull())
            vout.pop_back();
        if (vout.empty())
            std::vector<ctxout>().swap(vout);
    }

    void clearunspendable() {
        boost_foreach(ctxout &txout, vout) {
            if (txout.scriptpubkey.isunspendable())
                txout.setnull();
        }
        cleanup();
    }

    void swap(ccoins &to) {
        std::swap(to.fcoinbase, fcoinbase);
        to.vout.swap(vout);
        std::swap(to.nheight, nheight);
        std::swap(to.nversion, nversion);
    }

    //! equality test
    friend bool operator==(const ccoins &a, const ccoins &b) {
         // empty ccoins objects are always equal.
         if (a.ispruned() && b.ispruned())
             return true;
         return a.fcoinbase == b.fcoinbase &&
                a.nheight == b.nheight &&
                a.nversion == b.nversion &&
                a.vout == b.vout;
    }
    friend bool operator!=(const ccoins &a, const ccoins &b) {
        return !(a == b);
    }

    void calcmasksize(unsigned int &nbytes, unsigned int &nnonzerobytes) const;

    bool iscoinbase() const {
        return fcoinbase;
    }

    unsigned int getserializesize(int ntype, int nversion) const {
        unsigned int nsize = 0;
        unsigned int nmasksize = 0, nmaskcode = 0;
        calcmasksize(nmasksize, nmaskcode);
        bool ffirst = vout.size() > 0 && !vout[0].isnull();
        bool fsecond = vout.size() > 1 && !vout[1].isnull();
        assert(ffirst || fsecond || nmaskcode);
        unsigned int ncode = 8*(nmaskcode - (ffirst || fsecond ? 0 : 1)) + (fcoinbase ? 1 : 0) + (ffirst ? 2 : 0) + (fsecond ? 4 : 0);
        // version
        nsize += ::getserializesize(varint(this->nversion), ntype, nversion);
        // size of header code
        nsize += ::getserializesize(varint(ncode), ntype, nversion);
        // spentness bitmask
        nsize += nmasksize;
        // txouts themself
        for (unsigned int i = 0; i < vout.size(); i++)
            if (!vout[i].isnull())
                nsize += ::getserializesize(ctxoutcompressor(ref(vout[i])), ntype, nversion);
        // height
        nsize += ::getserializesize(varint(nheight), ntype, nversion);
        return nsize;
    }

    template<typename stream>
    void serialize(stream &s, int ntype, int nversion) const {
        unsigned int nmasksize = 0, nmaskcode = 0;
        calcmasksize(nmasksize, nmaskcode);
        bool ffirst = vout.size() > 0 && !vout[0].isnull();
        bool fsecond = vout.size() > 1 && !vout[1].isnull();
        assert(ffirst || fsecond || nmaskcode);
        unsigned int ncode = 8*(nmaskcode - (ffirst || fsecond ? 0 : 1)) + (fcoinbase ? 1 : 0) + (ffirst ? 2 : 0) + (fsecond ? 4 : 0);
        // version
        ::serialize(s, varint(this->nversion), ntype, nversion);
        // header code
        ::serialize(s, varint(ncode), ntype, nversion);
        // spentness bitmask
        for (unsigned int b = 0; b<nmasksize; b++) {
            unsigned char chavail = 0;
            for (unsigned int i = 0; i < 8 && 2+b*8+i < vout.size(); i++)
                if (!vout[2+b*8+i].isnull())
                    chavail |= (1 << i);
            ::serialize(s, chavail, ntype, nversion);
        }
        // txouts themself
        for (unsigned int i = 0; i < vout.size(); i++) {
            if (!vout[i].isnull())
                ::serialize(s, ctxoutcompressor(ref(vout[i])), ntype, nversion);
        }
        // coinbase height
        ::serialize(s, varint(nheight), ntype, nversion);
    }

    template<typename stream>
    void unserialize(stream &s, int ntype, int nversion) {
        unsigned int ncode = 0;
        // version
        ::unserialize(s, varint(this->nversion), ntype, nversion);
        // header code
        ::unserialize(s, varint(ncode), ntype, nversion);
        fcoinbase = ncode & 1;
        std::vector<bool> vavail(2, false);
        vavail[0] = (ncode & 2) != 0;
        vavail[1] = (ncode & 4) != 0;
        unsigned int nmaskcode = (ncode / 8) + ((ncode & 6) != 0 ? 0 : 1);
        // spentness bitmask
        while (nmaskcode > 0) {
            unsigned char chavail = 0;
            ::unserialize(s, chavail, ntype, nversion);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chavail & (1 << p)) != 0;
                vavail.push_back(f);
            }
            if (chavail != 0)
                nmaskcode--;
        }
        // txouts themself
        vout.assign(vavail.size(), ctxout());
        for (unsigned int i = 0; i < vavail.size(); i++) {
            if (vavail[i])
                ::unserialize(s, ref(ctxoutcompressor(vout[i])), ntype, nversion);
        }
        // coinbase height
        ::unserialize(s, varint(nheight), ntype, nversion);
        cleanup();
    }

    //! mark a vout spent
    bool spend(uint32_t npos);

    //! check whether a particular output is still available
    bool isavailable(unsigned int npos) const {
        return (npos < vout.size() && !vout[npos].isnull());
    }

    //! check whether the entire ccoins is spent
    //! note that only !ispruned() ccoins can be serialized
    bool ispruned() const {
        boost_foreach(const ctxout &out, vout)
            if (!out.isnull())
                return false;
        return true;
    }

    size_t dynamicmemoryusage() const {
        size_t ret = memusage::dynamicusage(vout);
        boost_foreach(const ctxout &out, vout) {
            const std::vector<unsigned char> *script = &out.scriptpubkey;
            ret += memusage::dynamicusage(*script);
        }
        return ret;
    }
};

class ccoinskeyhasher
{
private:
    uint256 salt;

public:
    ccoinskeyhasher();

    /**
     * this *must* return size_t. with boost 1.46 on 32-bit systems the
     * unordered_map will behave unpredictably if the custom hasher returns a
     * uint64_t, resulting in failures when syncing the chain (#4634).
     */
    size_t operator()(const uint256& key) const {
        return key.gethash(salt);
    }
};

struct ccoinscacheentry
{
    ccoins coins; // the actual cached data.
    unsigned char flags;

    enum flags {
        dirty = (1 << 0), // this cache entry is potentially different from the version in the parent view.
        fresh = (1 << 1), // the parent view does not have this entry (or it is pruned).
    };

    ccoinscacheentry() : coins(), flags(0) {}
};

typedef boost::unordered_map<uint256, ccoinscacheentry, ccoinskeyhasher> ccoinsmap;

struct ccoinsstats
{
    int nheight;
    uint256 hashblock;
    uint64_t ntransactions;
    uint64_t ntransactionoutputs;
    uint64_t nserializedsize;
    uint256 hashserialized;
    camount ntotalamount;

    ccoinsstats() : nheight(0), ntransactions(0), ntransactionoutputs(0), nserializedsize(0), ntotalamount(0) {}
};


/** abstract view on the open txout dataset. */
class ccoinsview
{
public:
    //! retrieve the ccoins (unspent transaction outputs) for a given txid
    virtual bool getcoins(const uint256 &txid, ccoins &coins) const;

    //! just check whether we have data for a given txid.
    //! this may (but cannot always) return true for fully spent transactions
    virtual bool havecoins(const uint256 &txid) const;

    //! retrieve the block hash whose state this ccoinsview currently represents
    virtual uint256 getbestblock() const;

    //! do a bulk modification (multiple ccoins changes + bestblock change).
    //! the passed mapcoins can be modified.
    virtual bool batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock);

    //! calculate statistics about the unspent transaction output set
    virtual bool getstats(ccoinsstats &stats) const;

    //! as we use ccoinsviews polymorphically, have a virtual destructor
    virtual ~ccoinsview() {}
};


/** ccoinsview backed by another ccoinsview */
class ccoinsviewbacked : public ccoinsview
{
protected:
    ccoinsview *base;

public:
    ccoinsviewbacked(ccoinsview *viewin);
    bool getcoins(const uint256 &txid, ccoins &coins) const;
    bool havecoins(const uint256 &txid) const;
    uint256 getbestblock() const;
    void setbackend(ccoinsview &viewin);
    bool batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock);
    bool getstats(ccoinsstats &stats) const;
};


class ccoinsviewcache;

/** 
 * a reference to a mutable cache entry. encapsulating it allows us to run
 *  cleanup code after the modification is finished, and keeping track of
 *  concurrent modifications. 
 */
class ccoinsmodifier
{
private:
    ccoinsviewcache& cache;
    ccoinsmap::iterator it;
    size_t cachedcoinusage; // cached memory usage of the ccoins object before modification
    ccoinsmodifier(ccoinsviewcache& cache_, ccoinsmap::iterator it_, size_t usage);

public:
    ccoins* operator->() { return &it->second.coins; }
    ccoins& operator*() { return it->second.coins; }
    ~ccoinsmodifier();
    friend class ccoinsviewcache;
};

/** ccoinsview that adds a memory cache for transactions to another ccoinsview */
class ccoinsviewcache : public ccoinsviewbacked
{
protected:
    /* whether this cache has an active modifier. */
    bool hasmodifier;


    /**
     * make mutable so that we can "fill the cache" even from get-methods
     * declared as "const".  
     */
    mutable uint256 hashblock;
    mutable ccoinsmap cachecoins;

    /* cached dynamic memory usage for the inner ccoins objects. */
    mutable size_t cachedcoinsusage;

public:
    ccoinsviewcache(ccoinsview *basein);
    ~ccoinsviewcache();

    // standard ccoinsview methods
    bool getcoins(const uint256 &txid, ccoins &coins) const;
    bool havecoins(const uint256 &txid) const;
    uint256 getbestblock() const;
    void setbestblock(const uint256 &hashblock);
    bool batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock);

    /**
     * return a pointer to ccoins in the cache, or null if not found. this is
     * more efficient than getcoins. modifications to other cache entries are
     * allowed while accessing the returned pointer.
     */
    const ccoins* accesscoins(const uint256 &txid) const;

    /**
     * return a modifiable reference to a ccoins. if no entry with the given
     * txid exists, a new one is created. simultaneous modifications are not
     * allowed.
     */
    ccoinsmodifier modifycoins(const uint256 &txid);

    /**
     * push the modifications applied to this cache to its base.
     * failure to call this method before destruction will cause the changes to be forgotten.
     * if false is returned, the state of this cache (and its backing view) will be undefined.
     */
    bool flush();

    //! calculate the size of the cache (in number of transactions)
    unsigned int getcachesize() const;

    //! calculate the size of the cache (in bytes)
    size_t dynamicmemoryusage() const;

    /** 
     * amount of moorecoins coming in to a transaction
     * note that lightweight clients may not know anything besides the hash of previous transactions,
     * so may not be able to calculate this.
     *
     * @param[in] tx	transaction for which we are checking input total
     * @return	sum of value of all inputs (scriptsigs)
     */
    camount getvaluein(const ctransaction& tx) const;

    //! check whether all prevouts of the transaction are present in the utxo set represented by this view
    bool haveinputs(const ctransaction& tx) const;

    //! return priority of tx at height nheight
    double getpriority(const ctransaction &tx, int nheight) const;

    const ctxout &getoutputfor(const ctxin& input) const;

    friend class ccoinsmodifier;

private:
    ccoinsmap::iterator fetchcoins(const uint256 &txid);
    ccoinsmap::const_iterator fetchcoins(const uint256 &txid) const;

    /**
     * by making the copy constructor private, we prevent accidentally using it when one intends to create a cache on top of a base cache.
     */
    ccoinsviewcache(const ccoinsviewcache &);
};

#endif // moorecoin_coins_h
