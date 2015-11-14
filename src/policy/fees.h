// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2015 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.
#ifndef moorecoin_policyestimator_h
#define moorecoin_policyestimator_h

#include "amount.h"
#include "uint256.h"

#include <map>
#include <string>
#include <vector>

class cautofile;
class cfeerate;
class ctxmempoolentry;

/** \class cblockpolicyestimator
 * the blockpolicyestimator is used for estimating the fee or priority needed
 * for a transaction to be included in a block within a certain number of
 * blocks.
 *
 * at a high level the algorithm works by grouping transactions into buckets
 * based on having similar priorities or fees and then tracking how long it
 * takes transactions in the various buckets to be mined.  it operates under
 * the assumption that in general transactions of higher fee/priority will be
 * included in blocks before transactions of lower fee/priority.   so for
 * example if you wanted to know what fee you should put on a transaction to
 * be included in a block within the next 5 blocks, you would start by looking
 * at the bucket with with the highest fee transactions and verifying that a
 * sufficiently high percentage of them were confirmed within 5 blocks and
 * then you would look at the next highest fee bucket, and so on, stopping at
 * the last bucket to pass the test.   the average fee of transactions in this
 * bucket will give you an indication of the lowest fee you can put on a
 * transaction and still have a sufficiently high chance of being confirmed
 * within your desired 5 blocks.
 *
 * when a transaction enters the mempool or is included within a block we
 * decide whether it can be used as a data point for fee estimation, priority
 * estimation or neither.  if the value of exactly one of those properties was
 * below the required minimum it can be used to estimate the other.  in
 * addition, if a priori our estimation code would indicate that the
 * transaction would be much more quickly included in a block because of one
 * of the properties compared to the other, we can also decide to use it as
 * an estimate for that property.
 *
 * here is a brief description of the implementation for fee estimation.
 * when a transaction that counts for fee estimation enters the mempool, we
 * track the height of the block chain at entry.  whenever a block comes in,
 * we count the number of transactions in each bucket and the total amount of fee
 * paid in each bucket. then we calculate how many blocks y it took each
 * transaction to be mined and we track an array of counters in each bucket
 * for how long it to took transactions to get confirmed from 1 to a max of 25
 * and we increment all the counters from y up to 25. this is because for any
 * number z>=y the transaction was successfully mined within z blocks.  we
 * want to save a history of this information, so at any time we have a
 * counter of the total number of transactions that happened in a given fee
 * bucket and the total number that were confirmed in each number 1-25 blocks
 * or less for any bucket.   we save this history by keeping an exponentially
 * decaying moving average of each one of these stats.  furthermore we also
 * keep track of the number unmined (in mempool) transactions in each bucket
 * and for how many blocks they have been outstanding and use that to increase
 * the number of transactions we've seen in that fee bucket when calculating
 * an estimate for any number of confirmations below the number of blocks
 * they've been outstanding.
 */

/**
 * we will instantiate two instances of this class, one to track transactions
 * that were included in a block due to fee, and one for tx's included due to
 * priority.  we will lump transactions into a bucket according to their approximate
 * fee or priority and then track how long it took for those txs to be included in a block
 *
 * the tracking of unconfirmed (mempool) transactions is completely independent of the
 * historical tracking of transactions that have been confirmed in a block.
 */
class txconfirmstats
{
private:
    //define the buckets we will group transactions into (both fee buckets and priority buckets)
    std::vector<double> buckets;              // the upper-bound of the range for the bucket (inclusive)
    std::map<double, unsigned int> bucketmap; // map of bucket upper-bound to index into all vectors by bucket

    // for each bucket x:
    // count the total # of txs in each bucket
    // track the historical moving average of this total over blocks
    std::vector<double> txctavg;
    // and calcuate the total for the current block to update the moving average
    std::vector<int> curblocktxct;

    // count the total # of txs confirmed within y blocks in each bucket
    // track the historical moving average of theses totals over blocks
    std::vector<std::vector<double> > confavg; // confavg[y][x]
    // and calcuate the totals for the current block to update the moving averages
    std::vector<std::vector<int> > curblockconf; // curblockconf[y][x]

    // sum the total priority/fee of all tx's in each bucket
    // track the historical moving average of this total over blocks
    std::vector<double> avg;
    // and calculate the total for the current block to update the moving average
    std::vector<double> curblockval;

    // combine the conf counts with tx counts to calculate the confirmation % for each y,x
    // combine the total value with the tx counts to calculate the avg fee/priority per bucket

    std::string datatypestring;
    double decay;

    // mempool counts of outstanding transactions
    // for each bucket x, track the number of transactions in the mempool
    // that are unconfirmed for each possible confirmation value y
    std::vector<std::vector<int> > unconftxs;  //unconftxs[y][x]
    // transactions still unconfirmed after max_confirms for each bucket
    std::vector<int> oldunconftxs;

public:
    /**
     * initialize the data structures.  this is called by blockpolicyestimator's
     * constructor with default values.
     * @param defaultbuckets contains the upper limits for the bucket boundries
     * @param maxconfirms max number of confirms to track
     * @param decay how much to decay the historical moving average per block
     * @param datatypestring for logging purposes
     */
    void initialize(std::vector<double>& defaultbuckets, unsigned int maxconfirms, double decay, std::string datatypestring);

    /** clear the state of the curblock variables to start counting for the new block */
    void clearcurrent(unsigned int nblockheight);

    /**
     * record a new transaction data point in the current block stats
     * @param blockstoconfirm the number of blocks it took this transaction to confirm
     * @param val either the fee or the priority when entered of the transaction
     * @warning blockstoconfirm is 1-based and has to be >= 1
     */
    void record(int blockstoconfirm, double val);

    /** record a new transaction entering the mempool*/
    unsigned int newtx(unsigned int nblockheight, double val);

    /** remove a transaction from mempool tracking stats*/
    void removetx(unsigned int entryheight, unsigned int nbestseenheight,
                  unsigned int bucketindex);

    /** update our estimates by decaying our historical moving average and updating
        with the data gathered from the current block */
    void updatemovingaverages();

    /**
     * calculate a fee or priority estimate.  find the lowest value bucket (or range of buckets
     * to make sure we have enough data points) whose transactions still have sufficient likelihood
     * of being confirmed within the target number of confirmations
     * @param conftarget target number of confirmations
     * @param sufficienttxval required average number of transactions per block in a bucket range
     * @param minsuccess the success probability we require
     * @param requiregreater return the lowest fee/pri such that all higher values pass minsuccess or
     *        return the highest fee/pri such that all lower values fail minsuccess
     * @param nblockheight the current block height
     */
    double estimatemedianval(int conftarget, double sufficienttxval,
                             double minsuccess, bool requiregreater, unsigned int nblockheight);

    /** return the max number of confirms we're tracking */
    unsigned int getmaxconfirms() { return confavg.size(); }

    /** write state of estimation data to a file*/
    void write(cautofile& fileout);

    /**
     * read saved state of estimation data from a file and replace all internal data structures and
     * variables with this state.
     */
    void read(cautofile& filein);
};



/** track confirm delays up to 25 blocks, can't estimate beyond that */
static const unsigned int max_block_confirms = 25;

/** decay of .998 is a half-life of 346 blocks or about 2.4 days */
static const double default_decay = .998;

/** require greater than 85% of x fee transactions to be confirmed within y blocks for x to be big enough */
static const double min_success_pct = .85;
static const double unlikely_pct = .5;

/** require an avg of 1 tx in the combined fee bucket per block to have stat significance */
static const double sufficient_feetxs = 1;

/** require only an avg of 1 tx every 5 blocks in the combined pri bucket (way less pri txs) */
static const double sufficient_pritxs = .2;

// minimum and maximum values for tracking fees and priorities
static const double min_feerate = 10;
static const double max_feerate = 1e7;
static const double inf_feerate = max_money;
static const double min_priority = 10;
static const double max_priority = 1e16;
static const double inf_priority = 1e9 * max_money;

// we have to lump transactions into buckets based on fee or priority, but we want to be able
// to give accurate estimates over a large range of potential fees and priorities
// therefore it makes sense to exponentially space the buckets
/** spacing of feerate buckets */
static const double fee_spacing = 1.1;

/** spacing of priority buckets */
static const double pri_spacing = 2;

/**
 *  we want to be able to estimate fees or priorities that are needed on tx's to be included in
 * a certain number of blocks.  every time a block is added to the best chain, this class records
 * stats on the transactions included in that block
 */
class cblockpolicyestimator
{
public:
    /** create new blockpolicyestimator and initialize stats tracking classes with default values */
    cblockpolicyestimator(const cfeerate& minrelayfee);

    /** process all the transactions that have been included in a block */
    void processblock(unsigned int nblockheight,
                      std::vector<ctxmempoolentry>& entries, bool fcurrentestimate);

    /** process a transaction confirmed in a block*/
    void processblocktx(unsigned int nblockheight, const ctxmempoolentry& entry);

    /** process a transaction accepted to the mempool*/
    void processtransaction(const ctxmempoolentry& entry, bool fcurrentestimate);

    /** remove a transaction from the mempool tracking stats*/
    void removetx(uint256 hash);

    /** is this transaction likely included in a block because of its fee?*/
    bool isfeedatapoint(const cfeerate &fee, double pri);

    /** is this transaction likely included in a block because of its priority?*/
    bool ispridatapoint(const cfeerate &fee, double pri);

    /** return a fee estimate */
    cfeerate estimatefee(int conftarget);

    /** return a priority estimate */
    double estimatepriority(int conftarget);

    /** write estimation data to a file */
    void write(cautofile& fileout);

    /** read estimation data from a file */
    void read(cautofile& filein);

private:
    cfeerate mintrackedfee; //! passed to constructor to avoid dependency on main
    double mintrackedpriority; //! set to allowfreethreshold
    unsigned int nbestseenheight;
    struct txstatsinfo
    {
        txconfirmstats *stats;
        unsigned int blockheight;
        unsigned int bucketindex;
        txstatsinfo() : stats(null), blockheight(0), bucketindex(0) {}
    };

    // map of txids to information about that transaction
    std::map<uint256, txstatsinfo> mapmempooltxs;

    /** classes to track historical data on transaction confirmations */
    txconfirmstats feestats, pristats;

    /** breakpoints to help determine whether a transaction was confirmed by priority or fee */
    cfeerate feelikely, feeunlikely;
    double prilikely, priunlikely;
};
#endif /*moorecoin_policyestimator_h */
