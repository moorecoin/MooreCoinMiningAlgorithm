// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_txmempool_h
#define moorecoin_txmempool_h

#include <list>

#include "amount.h"
#include "coins.h"
#include "primitives/transaction.h"
#include "sync.h"

class cautofile;

inline double allowfreethreshold()
{
    return coin * 144 / 250;
}

inline bool allowfree(double dpriority)
{
    // large (in bytes) low-priority (new, small-coin) transactions
    // need a fee.
    return dpriority > allowfreethreshold();
}

/** fake height value used in ccoins to signify they are only in the memory pool (since 0.8) */
static const unsigned int mempool_height = 0x7fffffff;

/**
 * ctxmempool stores these:
 */
class ctxmempoolentry
{
private:
    ctransaction tx;
    camount nfee; //! cached to avoid expensive parent-transaction lookups
    size_t ntxsize; //! ... and avoid recomputing tx size
    size_t nmodsize; //! ... and modified size for priority
    int64_t ntime; //! local time when entering the mempool
    double dpriority; //! priority when entering the mempool
    unsigned int nheight; //! chain height when entering the mempool
    bool hadnodependencies; //! not dependent on any other txs when it entered the mempool

public:
    ctxmempoolentry(const ctransaction& _tx, const camount& _nfee,
                    int64_t _ntime, double _dpriority, unsigned int _nheight, bool poolhasnoinputsof = false);
    ctxmempoolentry();
    ctxmempoolentry(const ctxmempoolentry& other);

    const ctransaction& gettx() const { return this->tx; }
    double getpriority(unsigned int currentheight) const;
    camount getfee() const { return nfee; }
    size_t gettxsize() const { return ntxsize; }
    int64_t gettime() const { return ntime; }
    unsigned int getheight() const { return nheight; }
    bool wasclearatentry() const { return hadnodependencies; }
};

class cblockpolicyestimator;

/** an inpoint - a combination of a transaction and an index n into its vin */
class cinpoint
{
public:
    const ctransaction* ptx;
    uint32_t n;

    cinpoint() { setnull(); }
    cinpoint(const ctransaction* ptxin, uint32_t nin) { ptx = ptxin; n = nin; }
    void setnull() { ptx = null; n = (uint32_t) -1; }
    bool isnull() const { return (ptx == null && n == (uint32_t) -1); }
};

/**
 * ctxmempool stores valid-according-to-the-current-best-chain
 * transactions that may be included in the next block.
 *
 * transactions are added when they are seen on the network
 * (or created by the local node), but not all transactions seen
 * are added to the pool: if a new transaction double-spends
 * an input of a transaction in the pool, it is dropped,
 * as are non-standard transactions.
 */
class ctxmempool
{
private:
    bool fsanitycheck; //! normally false, true if -checkmempool or -regtest
    unsigned int ntransactionsupdated;
    cblockpolicyestimator* minerpolicyestimator;

    uint64_t totaltxsize; //! sum of all mempool tx' byte sizes

public:
    mutable ccriticalsection cs;
    std::map<uint256, ctxmempoolentry> maptx;
    std::map<coutpoint, cinpoint> mapnexttx;
    std::map<uint256, std::pair<double, camount> > mapdeltas;

    ctxmempool(const cfeerate& _minrelayfee);
    ~ctxmempool();

    /**
     * if sanity-checking is turned on, check makes sure the pool is
     * consistent (does not contain two transactions that spend the same inputs,
     * all inputs are in the mapnexttx array). if sanity-checking is turned off,
     * check does nothing.
     */
    void check(const ccoinsviewcache *pcoins) const;
    void setsanitycheck(bool _fsanitycheck) { fsanitycheck = _fsanitycheck; }

    bool addunchecked(const uint256& hash, const ctxmempoolentry &entry, bool fcurrentestimate = true);
    void remove(const ctransaction &tx, std::list<ctransaction>& removed, bool frecursive = false);
    void removecoinbasespends(const ccoinsviewcache *pcoins, unsigned int nmempoolheight);
    void removeconflicts(const ctransaction &tx, std::list<ctransaction>& removed);
    void removeforblock(const std::vector<ctransaction>& vtx, unsigned int nblockheight,
                        std::list<ctransaction>& conflicts, bool fcurrentestimate = true);
    void clear();
    void queryhashes(std::vector<uint256>& vtxid);
    void prunespent(const uint256& hash, ccoins &coins);
    unsigned int gettransactionsupdated() const;
    void addtransactionsupdated(unsigned int n);
    /**
     * check that none of this transactions inputs are in the mempool, and thus
     * the tx is not dependent on other mempool transactions to be included in a block.
     */
    bool hasnoinputsof(const ctransaction& tx) const;

    /** affect createnewblock prioritisation of transactions */
    void prioritisetransaction(const uint256 hash, const std::string strhash, double dprioritydelta, const camount& nfeedelta);
    void applydeltas(const uint256 hash, double &dprioritydelta, camount &nfeedelta);
    void clearprioritisation(const uint256 hash);

    unsigned long size()
    {
        lock(cs);
        return maptx.size();
    }
    uint64_t gettotaltxsize()
    {
        lock(cs);
        return totaltxsize;
    }

    bool exists(uint256 hash) const
    {
        lock(cs);
        return (maptx.count(hash) != 0);
    }

    bool lookup(uint256 hash, ctransaction& result) const;

    /** estimate fee rate needed to get into the next nblocks */
    cfeerate estimatefee(int nblocks) const;

    /** estimate priority needed to get into the next nblocks */
    double estimatepriority(int nblocks) const;
    
    /** write/read estimates to disk */
    bool writefeeestimates(cautofile& fileout) const;
    bool readfeeestimates(cautofile& filein);
};

/** 
 * ccoinsview that brings transactions from a memorypool into view.
 * it does not check for spendings by memory pool transactions.
 */
class ccoinsviewmempool : public ccoinsviewbacked
{
protected:
    ctxmempool &mempool;

public:
    ccoinsviewmempool(ccoinsview *basein, ctxmempool &mempoolin);
    bool getcoins(const uint256 &txid, ccoins &coins) const;
    bool havecoins(const uint256 &txid) const;
};

#endif // moorecoin_txmempool_h
