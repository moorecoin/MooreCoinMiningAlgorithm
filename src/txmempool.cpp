// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "txmempool.h"

#include "clientversion.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "main.h"
#include "policy/fees.h"
#include "streams.h"
#include "util.h"
#include "utilmoneystr.h"
#include "version.h"

using namespace std;

ctxmempoolentry::ctxmempoolentry():
    nfee(0), ntxsize(0), nmodsize(0), ntime(0), dpriority(0.0), hadnodependencies(false)
{
    nheight = mempool_height;
}

ctxmempoolentry::ctxmempoolentry(const ctransaction& _tx, const camount& _nfee,
                                 int64_t _ntime, double _dpriority,
                                 unsigned int _nheight, bool poolhasnoinputsof):
    tx(_tx), nfee(_nfee), ntime(_ntime), dpriority(_dpriority), nheight(_nheight),
    hadnodependencies(poolhasnoinputsof)
{
    ntxsize = ::getserializesize(tx, ser_network, protocol_version);
    nmodsize = tx.calculatemodifiedsize(ntxsize);
}

ctxmempoolentry::ctxmempoolentry(const ctxmempoolentry& other)
{
    *this = other;
}

double
ctxmempoolentry::getpriority(unsigned int currentheight) const
{
    camount nvaluein = tx.getvalueout()+nfee;
    double deltapriority = ((double)(currentheight-nheight)*nvaluein)/nmodsize;
    double dresult = dpriority + deltapriority;
    return dresult;
}

ctxmempool::ctxmempool(const cfeerate& _minrelayfee) :
    ntransactionsupdated(0)
{
    // sanity checks off by default for performance, because otherwise
    // accepting transactions becomes o(n^2) where n is the number
    // of transactions in the pool
    fsanitycheck = false;

    minerpolicyestimator = new cblockpolicyestimator(_minrelayfee);
}

ctxmempool::~ctxmempool()
{
    delete minerpolicyestimator;
}

void ctxmempool::prunespent(const uint256 &hashtx, ccoins &coins)
{
    lock(cs);

    std::map<coutpoint, cinpoint>::iterator it = mapnexttx.lower_bound(coutpoint(hashtx, 0));

    // iterate over all coutpoints in mapnexttx whose hash equals the provided hashtx
    while (it != mapnexttx.end() && it->first.hash == hashtx) {
        coins.spend(it->first.n); // and remove those outputs from coins
        it++;
    }
}

unsigned int ctxmempool::gettransactionsupdated() const
{
    lock(cs);
    return ntransactionsupdated;
}

void ctxmempool::addtransactionsupdated(unsigned int n)
{
    lock(cs);
    ntransactionsupdated += n;
}


bool ctxmempool::addunchecked(const uint256& hash, const ctxmempoolentry &entry, bool fcurrentestimate)
{
    // add to memory pool without checking anything.
    // used by main.cpp accepttomemorypool(), which does do
    // all the appropriate checks.
    lock(cs);
    maptx[hash] = entry;
    const ctransaction& tx = maptx[hash].gettx();
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        mapnexttx[tx.vin[i].prevout] = cinpoint(&tx, i);
    ntransactionsupdated++;
    totaltxsize += entry.gettxsize();
    minerpolicyestimator->processtransaction(entry, fcurrentestimate);

    return true;
}


void ctxmempool::remove(const ctransaction &origtx, std::list<ctransaction>& removed, bool frecursive)
{
    // remove transaction from memory pool
    {
        lock(cs);
        std::deque<uint256> txtoremove;
        txtoremove.push_back(origtx.gethash());
        if (frecursive && !maptx.count(origtx.gethash())) {
            // if recursively removing but origtx isn't in the mempool
            // be sure to remove any children that are in the pool. this can
            // happen during chain re-orgs if origtx isn't re-accepted into
            // the mempool for any reason.
            for (unsigned int i = 0; i < origtx.vout.size(); i++) {
                std::map<coutpoint, cinpoint>::iterator it = mapnexttx.find(coutpoint(origtx.gethash(), i));
                if (it == mapnexttx.end())
                    continue;
                txtoremove.push_back(it->second.ptx->gethash());
            }
        }
        while (!txtoremove.empty())
        {
            uint256 hash = txtoremove.front();
            txtoremove.pop_front();
            if (!maptx.count(hash))
                continue;
            const ctransaction& tx = maptx[hash].gettx();
            if (frecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<coutpoint, cinpoint>::iterator it = mapnexttx.find(coutpoint(hash, i));
                    if (it == mapnexttx.end())
                        continue;
                    txtoremove.push_back(it->second.ptx->gethash());
                }
            }
            boost_foreach(const ctxin& txin, tx.vin)
                mapnexttx.erase(txin.prevout);

            removed.push_back(tx);
            totaltxsize -= maptx[hash].gettxsize();
            maptx.erase(hash);
            ntransactionsupdated++;
            minerpolicyestimator->removetx(hash);
        }
    }
}

void ctxmempool::removecoinbasespends(const ccoinsviewcache *pcoins, unsigned int nmempoolheight)
{
    // remove transactions spending a coinbase which are now immature
    lock(cs);
    list<ctransaction> transactionstoremove;
    for (std::map<uint256, ctxmempoolentry>::const_iterator it = maptx.begin(); it != maptx.end(); it++) {
        const ctransaction& tx = it->second.gettx();
        boost_foreach(const ctxin& txin, tx.vin) {
            std::map<uint256, ctxmempoolentry>::const_iterator it2 = maptx.find(txin.prevout.hash);
            if (it2 != maptx.end())
                continue;
            const ccoins *coins = pcoins->accesscoins(txin.prevout.hash);
            if (fsanitycheck) assert(coins);
            if (!coins || (coins->iscoinbase() && nmempoolheight - coins->nheight < coinbase_maturity)) {
                transactionstoremove.push_back(tx);
                break;
            }
        }
    }
    boost_foreach(const ctransaction& tx, transactionstoremove) {
        list<ctransaction> removed;
        remove(tx, removed, true);
    }
}

void ctxmempool::removeconflicts(const ctransaction &tx, std::list<ctransaction>& removed)
{
    // remove transactions which depend on inputs of tx, recursively
    list<ctransaction> result;
    lock(cs);
    boost_foreach(const ctxin &txin, tx.vin) {
        std::map<coutpoint, cinpoint>::iterator it = mapnexttx.find(txin.prevout);
        if (it != mapnexttx.end()) {
            const ctransaction &txconflict = *it->second.ptx;
            if (txconflict != tx)
            {
                remove(txconflict, removed, true);
            }
        }
    }
}

/**
 * called when a block is connected. removes from mempool and updates the miner fee estimator.
 */
void ctxmempool::removeforblock(const std::vector<ctransaction>& vtx, unsigned int nblockheight,
                                std::list<ctransaction>& conflicts, bool fcurrentestimate)
{
    lock(cs);
    std::vector<ctxmempoolentry> entries;
    boost_foreach(const ctransaction& tx, vtx)
    {
        uint256 hash = tx.gethash();
        if (maptx.count(hash))
            entries.push_back(maptx[hash]);
    }
    boost_foreach(const ctransaction& tx, vtx)
    {
        std::list<ctransaction> dummy;
        remove(tx, dummy, false);
        removeconflicts(tx, conflicts);
        clearprioritisation(tx.gethash());
    }
    // after the txs in the new block have been removed from the mempool, update policy estimates
    minerpolicyestimator->processblock(nblockheight, entries, fcurrentestimate);
}

void ctxmempool::clear()
{
    lock(cs);
    maptx.clear();
    mapnexttx.clear();
    totaltxsize = 0;
    ++ntransactionsupdated;
}

void ctxmempool::check(const ccoinsviewcache *pcoins) const
{
    if (!fsanitycheck)
        return;

    logprint("mempool", "checking mempool with %u transactions and %u inputs\n", (unsigned int)maptx.size(), (unsigned int)mapnexttx.size());

    uint64_t checktotal = 0;

    ccoinsviewcache mempoolduplicate(const_cast<ccoinsviewcache*>(pcoins));

    lock(cs);
    list<const ctxmempoolentry*> waitingondependants;
    for (std::map<uint256, ctxmempoolentry>::const_iterator it = maptx.begin(); it != maptx.end(); it++) {
        unsigned int i = 0;
        checktotal += it->second.gettxsize();
        const ctransaction& tx = it->second.gettx();
        bool fdependswait = false;
        boost_foreach(const ctxin &txin, tx.vin) {
            // check that every mempool transaction's inputs refer to available coins, or other mempool tx's.
            std::map<uint256, ctxmempoolentry>::const_iterator it2 = maptx.find(txin.prevout.hash);
            if (it2 != maptx.end()) {
                const ctransaction& tx2 = it2->second.gettx();
                assert(tx2.vout.size() > txin.prevout.n && !tx2.vout[txin.prevout.n].isnull());
                fdependswait = true;
            } else {
                const ccoins* coins = pcoins->accesscoins(txin.prevout.hash);
                assert(coins && coins->isavailable(txin.prevout.n));
            }
            // check whether its inputs are marked in mapnexttx.
            std::map<coutpoint, cinpoint>::const_iterator it3 = mapnexttx.find(txin.prevout);
            assert(it3 != mapnexttx.end());
            assert(it3->second.ptx == &tx);
            assert(it3->second.n == i);
            i++;
        }
        if (fdependswait)
            waitingondependants.push_back(&it->second);
        else {
            cvalidationstate state;
            assert(checkinputs(tx, state, mempoolduplicate, false, 0, false, null));
            updatecoins(tx, state, mempoolduplicate, 1000000);
        }
    }
    unsigned int stepssincelastremove = 0;
    while (!waitingondependants.empty()) {
        const ctxmempoolentry* entry = waitingondependants.front();
        waitingondependants.pop_front();
        cvalidationstate state;
        if (!mempoolduplicate.haveinputs(entry->gettx())) {
            waitingondependants.push_back(entry);
            stepssincelastremove++;
            assert(stepssincelastremove < waitingondependants.size());
        } else {
            assert(checkinputs(entry->gettx(), state, mempoolduplicate, false, 0, false, null));
            updatecoins(entry->gettx(), state, mempoolduplicate, 1000000);
            stepssincelastremove = 0;
        }
    }
    for (std::map<coutpoint, cinpoint>::const_iterator it = mapnexttx.begin(); it != mapnexttx.end(); it++) {
        uint256 hash = it->second.ptx->gethash();
        map<uint256, ctxmempoolentry>::const_iterator it2 = maptx.find(hash);
        const ctransaction& tx = it2->second.gettx();
        assert(it2 != maptx.end());
        assert(&tx == it->second.ptx);
        assert(tx.vin.size() > it->second.n);
        assert(it->first == it->second.ptx->vin[it->second.n].prevout);
    }

    assert(totaltxsize == checktotal);
}

void ctxmempool::queryhashes(vector<uint256>& vtxid)
{
    vtxid.clear();

    lock(cs);
    vtxid.reserve(maptx.size());
    for (map<uint256, ctxmempoolentry>::iterator mi = maptx.begin(); mi != maptx.end(); ++mi)
        vtxid.push_back((*mi).first);
}

bool ctxmempool::lookup(uint256 hash, ctransaction& result) const
{
    lock(cs);
    map<uint256, ctxmempoolentry>::const_iterator i = maptx.find(hash);
    if (i == maptx.end()) return false;
    result = i->second.gettx();
    return true;
}

cfeerate ctxmempool::estimatefee(int nblocks) const
{
    lock(cs);
    return minerpolicyestimator->estimatefee(nblocks);
}
double ctxmempool::estimatepriority(int nblocks) const
{
    lock(cs);
    return minerpolicyestimator->estimatepriority(nblocks);
}

bool
ctxmempool::writefeeestimates(cautofile& fileout) const
{
    try {
        lock(cs);
        fileout << 109900; // version required to read: 0.10.99 or later
        fileout << client_version; // version that wrote the file
        minerpolicyestimator->write(fileout);
    }
    catch (const std::exception&) {
        logprintf("ctxmempool::writefeeestimates(): unable to write policy estimator data (non-fatal)");
        return false;
    }
    return true;
}

bool
ctxmempool::readfeeestimates(cautofile& filein)
{
    try {
        int nversionrequired, nversionthatwrote;
        filein >> nversionrequired >> nversionthatwrote;
        if (nversionrequired > client_version)
            return error("ctxmempool::readfeeestimates(): up-version (%d) fee estimate file", nversionrequired);

        lock(cs);
        minerpolicyestimator->read(filein);
    }
    catch (const std::exception&) {
        logprintf("ctxmempool::readfeeestimates(): unable to read policy estimator data (non-fatal)");
        return false;
    }
    return true;
}

void ctxmempool::prioritisetransaction(const uint256 hash, const string strhash, double dprioritydelta, const camount& nfeedelta)
{
    {
        lock(cs);
        std::pair<double, camount> &deltas = mapdeltas[hash];
        deltas.first += dprioritydelta;
        deltas.second += nfeedelta;
    }
    logprintf("prioritisetransaction: %s priority += %f, fee += %d\n", strhash, dprioritydelta, formatmoney(nfeedelta));
}

void ctxmempool::applydeltas(const uint256 hash, double &dprioritydelta, camount &nfeedelta)
{
    lock(cs);
    std::map<uint256, std::pair<double, camount> >::iterator pos = mapdeltas.find(hash);
    if (pos == mapdeltas.end())
        return;
    const std::pair<double, camount> &deltas = pos->second;
    dprioritydelta += deltas.first;
    nfeedelta += deltas.second;
}

void ctxmempool::clearprioritisation(const uint256 hash)
{
    lock(cs);
    mapdeltas.erase(hash);
}

bool ctxmempool::hasnoinputsof(const ctransaction &tx) const
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        if (exists(tx.vin[i].prevout.hash))
            return false;
    return true;
}

ccoinsviewmempool::ccoinsviewmempool(ccoinsview *basein, ctxmempool &mempoolin) : ccoinsviewbacked(basein), mempool(mempoolin) { }

bool ccoinsviewmempool::getcoins(const uint256 &txid, ccoins &coins) const {
    // if an entry in the mempool exists, always return that one, as it's guaranteed to never
    // conflict with the underlying cache, and it cannot have pruned entries (as it contains full)
    // transactions. first checking the underlying cache risks returning a pruned entry instead.
    ctransaction tx;
    if (mempool.lookup(txid, tx)) {
        coins = ccoins(tx, mempool_height);
        return true;
    }
    return (base->getcoins(txid, coins) && !coins.ispruned());
}

bool ccoinsviewmempool::havecoins(const uint256 &txid) const {
    return mempool.exists(txid) || base->havecoins(txid);
}
