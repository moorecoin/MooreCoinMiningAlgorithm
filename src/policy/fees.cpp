// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2015 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "policy/fees.h"

#include "amount.h"
#include "primitives/transaction.h"
#include "streams.h"
#include "txmempool.h"
#include "util.h"

void txconfirmstats::initialize(std::vector<double>& defaultbuckets,
                                unsigned int maxconfirms, double _decay, std::string _datatypestring)
{
    decay = _decay;
    datatypestring = _datatypestring;
    for (unsigned int i = 0; i < defaultbuckets.size(); i++) {
        buckets.push_back(defaultbuckets[i]);
        bucketmap[defaultbuckets[i]] = i;
    }
    confavg.resize(maxconfirms);
    curblockconf.resize(maxconfirms);
    unconftxs.resize(maxconfirms);
    for (unsigned int i = 0; i < maxconfirms; i++) {
        confavg[i].resize(buckets.size());
        curblockconf[i].resize(buckets.size());
        unconftxs[i].resize(buckets.size());
    }

    oldunconftxs.resize(buckets.size());
    curblocktxct.resize(buckets.size());
    txctavg.resize(buckets.size());
    curblockval.resize(buckets.size());
    avg.resize(buckets.size());
}

// zero out the data for the current block
void txconfirmstats::clearcurrent(unsigned int nblockheight)
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        oldunconftxs[j] += unconftxs[nblockheight%unconftxs.size()][j];
        unconftxs[nblockheight%unconftxs.size()][j] = 0;
        for (unsigned int i = 0; i < curblockconf.size(); i++)
            curblockconf[i][j] = 0;
        curblocktxct[j] = 0;
        curblockval[j] = 0;
    }
}


void txconfirmstats::record(int blockstoconfirm, double val)
{
    // blockstoconfirm is 1-based
    if (blockstoconfirm < 1)
        return;
    unsigned int bucketindex = bucketmap.lower_bound(val)->second;
    for (size_t i = blockstoconfirm; i <= curblockconf.size(); i++) {
        curblockconf[i - 1][bucketindex]++;
    }
    curblocktxct[bucketindex]++;
    curblockval[bucketindex] += val;
}

void txconfirmstats::updatemovingaverages()
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        for (unsigned int i = 0; i < confavg.size(); i++)
            confavg[i][j] = confavg[i][j] * decay + curblockconf[i][j];
        avg[j] = avg[j] * decay + curblockval[j];
        txctavg[j] = txctavg[j] * decay + curblocktxct[j];
    }
}

// returns -1 on error conditions
double txconfirmstats::estimatemedianval(int conftarget, double sufficienttxval,
                                         double successbreakpoint, bool requiregreater,
                                         unsigned int nblockheight)
{
    // counters for a bucket (or range of buckets)
    double nconf = 0; // number of tx's confirmed within the conftarget
    double totalnum = 0; // total number of tx's that were ever confirmed
    int extranum = 0;  // number of tx's still in mempool for conftarget or longer

    int maxbucketindex = buckets.size() - 1;

    // requiregreater means we are looking for the lowest fee/priority such that all higher
    // values pass, so we start at maxbucketindex (highest fee) and look at succesively
    // smaller buckets until we reach failure.  otherwise, we are looking for the highest
    // fee/priority such that all lower values fail, and we go in the opposite direction.
    unsigned int startbucket = requiregreater ? maxbucketindex : 0;
    int step = requiregreater ? -1 : 1;

    // we'll combine buckets until we have enough samples.
    // the near and far variables will define the range we've combined
    // the best variables are the last range we saw which still had a high
    // enough confirmation rate to count as success.
    // the cur variables are the current range we're counting.
    unsigned int curnearbucket = startbucket;
    unsigned int bestnearbucket = startbucket;
    unsigned int curfarbucket = startbucket;
    unsigned int bestfarbucket = startbucket;

    bool foundanswer = false;
    unsigned int bins = unconftxs.size();

    // start counting from highest(default) or lowest fee/pri transactions
    for (int bucket = startbucket; bucket >= 0 && bucket <= maxbucketindex; bucket += step) {
        curfarbucket = bucket;
        nconf += confavg[conftarget - 1][bucket];
        totalnum += txctavg[bucket];
        for (unsigned int confct = conftarget; confct < getmaxconfirms(); confct++)
            extranum += unconftxs[(nblockheight - confct)%bins][bucket];
        extranum += oldunconftxs[bucket];
        // if we have enough transaction data points in this range of buckets,
        // we can test for success
        // (only count the confirmed data points, so that each confirmation count
        // will be looking at the same amount of data and same bucket breaks)
        if (totalnum >= sufficienttxval / (1 - decay)) {
            double curpct = nconf / (totalnum + extranum);

            // check to see if we are no longer getting confirmed at the success rate
            if (requiregreater && curpct < successbreakpoint)
                break;
            if (!requiregreater && curpct > successbreakpoint)
                break;

            // otherwise update the cumulative stats, and the bucket variables
            // and reset the counters
            else {
                foundanswer = true;
                nconf = 0;
                totalnum = 0;
                extranum = 0;
                bestnearbucket = curnearbucket;
                bestfarbucket = curfarbucket;
                curnearbucket = bucket + step;
            }
        }
    }

    double median = -1;
    double txsum = 0;

    // calculate the "average" fee of the best bucket range that met success conditions
    // find the bucket with the median transaction and then report the average fee from that bucket
    // this is a compromise between finding the median which we can't since we don't save all tx's
    // and reporting the average which is less accurate
    unsigned int minbucket = bestnearbucket < bestfarbucket ? bestnearbucket : bestfarbucket;
    unsigned int maxbucket = bestnearbucket > bestfarbucket ? bestnearbucket : bestfarbucket;
    for (unsigned int j = minbucket; j <= maxbucket; j++) {
        txsum += txctavg[j];
    }
    if (foundanswer && txsum != 0) {
        txsum = txsum / 2;
        for (unsigned int j = minbucket; j <= maxbucket; j++) {
            if (txctavg[j] < txsum)
                txsum -= txctavg[j];
            else { // we're in the right bucket
                median = avg[j] / txctavg[j];
                break;
            }
        }
    }

    logprint("estimatefee", "%3d: for conf success %s %4.2f need %s %s: %12.5g from buckets %8g - %8g  cur bucket stats %6.2f%%  %8.1f/(%.1f+%d mempool)\n",
             conftarget, requiregreater ? ">" : "<", successbreakpoint, datatypestring,
             requiregreater ? ">" : "<", median, buckets[minbucket], buckets[maxbucket],
             100 * nconf / (totalnum + extranum), nconf, totalnum, extranum);

    return median;
}

void txconfirmstats::write(cautofile& fileout)
{
    fileout << decay;
    fileout << buckets;
    fileout << avg;
    fileout << txctavg;
    fileout << confavg;
}

void txconfirmstats::read(cautofile& filein)
{
    // read data file into temporary variables and do some very basic sanity checking
    std::vector<double> filebuckets;
    std::vector<double> fileavg;
    std::vector<std::vector<double> > fileconfavg;
    std::vector<double> filetxctavg;
    double filedecay;
    size_t maxconfirms;
    size_t numbuckets;

    filein >> filedecay;
    if (filedecay <= 0 || filedecay >= 1)
        throw std::runtime_error("corrupt estimates file. decay must be between 0 and 1 (non-inclusive)");
    filein >> filebuckets;
    numbuckets = filebuckets.size();
    if (numbuckets <= 1 || numbuckets > 1000)
        throw std::runtime_error("corrupt estimates file. must have between 2 and 1000 fee/pri buckets");
    filein >> fileavg;
    if (fileavg.size() != numbuckets)
        throw std::runtime_error("corrupt estimates file. mismatch in fee/pri average bucket count");
    filein >> filetxctavg;
    if (filetxctavg.size() != numbuckets)
        throw std::runtime_error("corrupt estimates file. mismatch in tx count bucket count");
    filein >> fileconfavg;
    maxconfirms = fileconfavg.size();
    if (maxconfirms <= 0 || maxconfirms > 6 * 24 * 7) // one week
        throw std::runtime_error("corrupt estimates file.  must maintain estimates for between 1 and 1008 (one week) confirms");
    for (unsigned int i = 0; i < maxconfirms; i++) {
        if (fileconfavg[i].size() != numbuckets)
            throw std::runtime_error("corrupt estimates file. mismatch in fee/pri conf average bucket count");
    }
    // now that we've processed the entire fee estimate data file and not
    // thrown any errors, we can copy it to our data structures
    decay = filedecay;
    buckets = filebuckets;
    avg = fileavg;
    confavg = fileconfavg;
    txctavg = filetxctavg;
    bucketmap.clear();

    // resize the current block variables which aren't stored in the data file
    // to match the number of confirms and buckets
    curblockconf.resize(maxconfirms);
    for (unsigned int i = 0; i < maxconfirms; i++) {
        curblockconf[i].resize(buckets.size());
    }
    curblocktxct.resize(buckets.size());
    curblockval.resize(buckets.size());

    unconftxs.resize(maxconfirms);
    for (unsigned int i = 0; i < maxconfirms; i++) {
        unconftxs[i].resize(buckets.size());
    }
    oldunconftxs.resize(buckets.size());

    for (unsigned int i = 0; i < buckets.size(); i++)
        bucketmap[buckets[i]] = i;

    logprint("estimatefee", "reading estimates: %u %s buckets counting confirms up to %u blocks\n",
             numbuckets, datatypestring, maxconfirms);
}

unsigned int txconfirmstats::newtx(unsigned int nblockheight, double val)
{
    unsigned int bucketindex = bucketmap.lower_bound(val)->second;
    unsigned int blockindex = nblockheight % unconftxs.size();
    unconftxs[blockindex][bucketindex]++;
    logprint("estimatefee", "adding to %s\n", datatypestring);
    return bucketindex;
}

void txconfirmstats::removetx(unsigned int entryheight, unsigned int nbestseenheight, unsigned int bucketindex)
{
    //nbestseenheight is not updated yet for the new block
    int blocksago = nbestseenheight - entryheight;
    if (nbestseenheight == 0)  // the blockpolicyestimator hasn't seen any blocks yet
        blocksago = 0;
    if (blocksago < 0) {
        logprint("estimatefee", "blockpolicy error, blocks ago is negative for mempool tx\n");
        return;  //this can't happen becasue we call this with our best seen height, no entries can have higher
    }

    if (blocksago >= (int)unconftxs.size()) {
        if (oldunconftxs[bucketindex] > 0)
            oldunconftxs[bucketindex]--;
        else
            logprint("estimatefee", "blockpolicy error, mempool tx removed from >25 blocks,bucketindex=%u already\n",
                     bucketindex);
    }
    else {
        unsigned int blockindex = entryheight % unconftxs.size();
        if (unconftxs[blockindex][bucketindex] > 0)
            unconftxs[blockindex][bucketindex]--;
        else
            logprint("estimatefee", "blockpolicy error, mempool tx removed from blockindex=%u,bucketindex=%u already\n",
                     blockindex, bucketindex);
    }
}

void cblockpolicyestimator::removetx(uint256 hash)
{
    std::map<uint256, txstatsinfo>::iterator pos = mapmempooltxs.find(hash);
    if (pos == mapmempooltxs.end()) {
        logprint("estimatefee", "blockpolicy error mempool tx %s not found for removetx\n",
                 hash.tostring().c_str());
        return;
    }
    txconfirmstats *stats = pos->second.stats;
    unsigned int entryheight = pos->second.blockheight;
    unsigned int bucketindex = pos->second.bucketindex;

    if (stats != null)
        stats->removetx(entryheight, nbestseenheight, bucketindex);
    mapmempooltxs.erase(hash);
}

cblockpolicyestimator::cblockpolicyestimator(const cfeerate& _minrelayfee)
    : nbestseenheight(0)
{
    mintrackedfee = _minrelayfee < cfeerate(min_feerate) ? cfeerate(min_feerate) : _minrelayfee;
    std::vector<double> vfeelist;
    for (double bucketboundary = mintrackedfee.getfeeperk(); bucketboundary <= max_feerate; bucketboundary *= fee_spacing) {
        vfeelist.push_back(bucketboundary);
    }
    vfeelist.push_back(inf_feerate);
    feestats.initialize(vfeelist, max_block_confirms, default_decay, "feerate");

    mintrackedpriority = allowfreethreshold() < min_priority ? min_priority : allowfreethreshold();
    std::vector<double> vprilist;
    for (double bucketboundary = mintrackedpriority; bucketboundary <= max_priority; bucketboundary *= pri_spacing) {
        vprilist.push_back(bucketboundary);
    }
    vprilist.push_back(inf_priority);
    pristats.initialize(vprilist, max_block_confirms, default_decay, "priority");

    feeunlikely = cfeerate(0);
    feelikely = cfeerate(inf_feerate);
    priunlikely = 0;
    prilikely = inf_priority;
}

bool cblockpolicyestimator::isfeedatapoint(const cfeerate &fee, double pri)
{
    if ((pri < mintrackedpriority && fee >= mintrackedfee) ||
        (pri < priunlikely && fee > feelikely)) {
        return true;
    }
    return false;
}

bool cblockpolicyestimator::ispridatapoint(const cfeerate &fee, double pri)
{
    if ((fee < mintrackedfee && pri >= mintrackedpriority) ||
        (fee < feeunlikely && pri > prilikely)) {
        return true;
    }
    return false;
}

void cblockpolicyestimator::processtransaction(const ctxmempoolentry& entry, bool fcurrentestimate)
{
    unsigned int txheight = entry.getheight();
    uint256 hash = entry.gettx().gethash();
    if (mapmempooltxs[hash].stats != null) {
        logprint("estimatefee", "blockpolicy error mempool tx %s already being tracked\n",
                 hash.tostring().c_str());
	return;
    }

    if (txheight < nbestseenheight) {
        // ignore side chains and re-orgs; assuming they are random they don't
        // affect the estimate.  we'll potentially double count transactions in 1-block reorgs.
        return;
    }

    // only want to be updating estimates when our blockchain is synced,
    // otherwise we'll miscalculate how many blocks its taking to get included.
    if (!fcurrentestimate)
        return;

    if (!entry.wasclearatentry()) {
        // this transaction depends on other transactions in the mempool to
        // be included in a block before it will be able to be included, so
        // we shouldn't include it in our calculations
        return;
    }

    // fees are stored and reported as btc-per-kb:
    cfeerate feerate(entry.getfee(), entry.gettxsize());

    // want the priority of the tx at confirmation. however we don't know
    // what that will be and its too hard to continue updating it
    // so use starting priority as a proxy
    double curpri = entry.getpriority(txheight);
    mapmempooltxs[hash].blockheight = txheight;

    logprint("estimatefee", "blockpolicy mempool tx %s ", hash.tostring().substr(0,10));
    // record this as a priority estimate
    if (entry.getfee() == 0 || ispridatapoint(feerate, curpri)) {
        mapmempooltxs[hash].stats = &pristats;
        mapmempooltxs[hash].bucketindex =  pristats.newtx(txheight, curpri);
    }
    // record this as a fee estimate
    else if (isfeedatapoint(feerate, curpri)) {
        mapmempooltxs[hash].stats = &feestats;
        mapmempooltxs[hash].bucketindex = feestats.newtx(txheight, (double)feerate.getfeeperk());
    }
    else {
        logprint("estimatefee", "not adding\n");
    }
}

void cblockpolicyestimator::processblocktx(unsigned int nblockheight, const ctxmempoolentry& entry)
{
    if (!entry.wasclearatentry()) {
        // this transaction depended on other transactions in the mempool to
        // be included in a block before it was able to be included, so
        // we shouldn't include it in our calculations
        return;
    }

    // how many blocks did it take for miners to include this transaction?
    // blockstoconfirm is 1-based, so a transaction included in the earliest
    // possible block has confirmation count of 1
    int blockstoconfirm = nblockheight - entry.getheight();
    if (blockstoconfirm <= 0) {
        // this can't happen because we don't process transactions from a block with a height
        // lower than our greatest seen height
        logprint("estimatefee", "blockpolicy error transaction had negative blockstoconfirm\n");
        return;
    }

    // fees are stored and reported as btc-per-kb:
    cfeerate feerate(entry.getfee(), entry.gettxsize());

    // want the priority of the tx at confirmation.  the priority when it
    // entered the mempool could easily be very small and change quickly
    double curpri = entry.getpriority(nblockheight);

    // record this as a priority estimate
    if (entry.getfee() == 0 || ispridatapoint(feerate, curpri)) {
        pristats.record(blockstoconfirm, curpri);
    }
    // record this as a fee estimate
    else if (isfeedatapoint(feerate, curpri)) {
        feestats.record(blockstoconfirm, (double)feerate.getfeeperk());
    }
}

void cblockpolicyestimator::processblock(unsigned int nblockheight,
                                         std::vector<ctxmempoolentry>& entries, bool fcurrentestimate)
{
    if (nblockheight <= nbestseenheight) {
        // ignore side chains and re-orgs; assuming they are random
        // they don't affect the estimate.
        // and if an attacker can re-org the chain at will, then
        // you've got much bigger problems than "attacker can influence
        // transaction fees."
        return;
    }
    nbestseenheight = nblockheight;

    // only want to be updating estimates when our blockchain is synced,
    // otherwise we'll miscalculate how many blocks its taking to get included.
    if (!fcurrentestimate)
        return;

    // update the dynamic cutoffs
    // a fee/priority is "likely" the reason your tx was included in a block if >85% of such tx's
    // were confirmed in 2 blocks and is "unlikely" if <50% were confirmed in 10 blocks
    logprint("estimatefee", "blockpolicy recalculating dynamic cutoffs:\n");
    prilikely = pristats.estimatemedianval(2, sufficient_pritxs, min_success_pct, true, nblockheight);
    if (prilikely == -1)
        prilikely = inf_priority;

    double feelikelyest = feestats.estimatemedianval(2, sufficient_feetxs, min_success_pct, true, nblockheight);
    if (feelikelyest == -1)
        feelikely = cfeerate(inf_feerate);
    else
        feelikely = cfeerate(feelikelyest);

    priunlikely = pristats.estimatemedianval(10, sufficient_pritxs, unlikely_pct, false, nblockheight);
    if (priunlikely == -1)
        priunlikely = 0;

    double feeunlikelyest = feestats.estimatemedianval(10, sufficient_feetxs, unlikely_pct, false, nblockheight);
    if (feeunlikelyest == -1)
        feeunlikely = cfeerate(0);
    else
        feeunlikely = cfeerate(feeunlikelyest);

    // clear the current block states
    feestats.clearcurrent(nblockheight);
    pristats.clearcurrent(nblockheight);

    // repopulate the current block states
    for (unsigned int i = 0; i < entries.size(); i++)
        processblocktx(nblockheight, entries[i]);

    // update all exponential averages with the current block states
    feestats.updatemovingaverages();
    pristats.updatemovingaverages();

    logprint("estimatefee", "blockpolicy after updating estimates for %u confirmed entries, new mempool map size %u\n",
             entries.size(), mapmempooltxs.size());
}

cfeerate cblockpolicyestimator::estimatefee(int conftarget)
{
    // return failure if trying to analyze a target we're not tracking
    if (conftarget <= 0 || (unsigned int)conftarget > feestats.getmaxconfirms())
        return cfeerate(0);

    double median = feestats.estimatemedianval(conftarget, sufficient_feetxs, min_success_pct, true, nbestseenheight);

    if (median < 0)
        return cfeerate(0);

    return cfeerate(median);
}

double cblockpolicyestimator::estimatepriority(int conftarget)
{
    // return failure if trying to analyze a target we're not tracking
    if (conftarget <= 0 || (unsigned int)conftarget > pristats.getmaxconfirms())
        return -1;

    return pristats.estimatemedianval(conftarget, sufficient_pritxs, min_success_pct, true, nbestseenheight);
}

void cblockpolicyestimator::write(cautofile& fileout)
{
    fileout << nbestseenheight;
    feestats.write(fileout);
    pristats.write(fileout);
}

void cblockpolicyestimator::read(cautofile& filein)
{
    int nfilebestseenheight;
    filein >> nfilebestseenheight;
    feestats.read(filein);
    pristats.read(filein);
    nbestseenheight = nfilebestseenheight;
}
