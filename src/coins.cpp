// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"

#include "memusage.h"
#include "random.h"

#include <assert.h>

/**
 * calculate number of bytes for the bitmask, and its number of non-zero bytes
 * each bit in the bitmask represents the availability of one output, but the
 * availabilities of the first two outputs are encoded separately
 */
void ccoins::calcmasksize(unsigned int &nbytes, unsigned int &nnonzerobytes) const {
    unsigned int nlastusedbyte = 0;
    for (unsigned int b = 0; 2+b*8 < vout.size(); b++) {
        bool fzero = true;
        for (unsigned int i = 0; i < 8 && 2+b*8+i < vout.size(); i++) {
            if (!vout[2+b*8+i].isnull()) {
                fzero = false;
                continue;
            }
        }
        if (!fzero) {
            nlastusedbyte = b + 1;
            nnonzerobytes++;
        }
    }
    nbytes += nlastusedbyte;
}

bool ccoins::spend(uint32_t npos) 
{
    if (npos >= vout.size() || vout[npos].isnull())
        return false;
    vout[npos].setnull();
    cleanup();
    return true;
}

bool ccoinsview::getcoins(const uint256 &txid, ccoins &coins) const { return false; }
bool ccoinsview::havecoins(const uint256 &txid) const { return false; }
uint256 ccoinsview::getbestblock() const { return uint256(); }
bool ccoinsview::batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock) { return false; }
bool ccoinsview::getstats(ccoinsstats &stats) const { return false; }


ccoinsviewbacked::ccoinsviewbacked(ccoinsview *viewin) : base(viewin) { }
bool ccoinsviewbacked::getcoins(const uint256 &txid, ccoins &coins) const { return base->getcoins(txid, coins); }
bool ccoinsviewbacked::havecoins(const uint256 &txid) const { return base->havecoins(txid); }
uint256 ccoinsviewbacked::getbestblock() const { return base->getbestblock(); }
void ccoinsviewbacked::setbackend(ccoinsview &viewin) { base = &viewin; }
bool ccoinsviewbacked::batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock) { return base->batchwrite(mapcoins, hashblock); }
bool ccoinsviewbacked::getstats(ccoinsstats &stats) const { return base->getstats(stats); }

ccoinskeyhasher::ccoinskeyhasher() : salt(getrandhash()) {}

ccoinsviewcache::ccoinsviewcache(ccoinsview *basein) : ccoinsviewbacked(basein), hasmodifier(false), cachedcoinsusage(0) { }

ccoinsviewcache::~ccoinsviewcache()
{
    assert(!hasmodifier);
}

size_t ccoinsviewcache::dynamicmemoryusage() const {
    return memusage::dynamicusage(cachecoins) + cachedcoinsusage;
}

ccoinsmap::const_iterator ccoinsviewcache::fetchcoins(const uint256 &txid) const {
    ccoinsmap::iterator it = cachecoins.find(txid);
    if (it != cachecoins.end())
        return it;
    ccoins tmp;
    if (!base->getcoins(txid, tmp))
        return cachecoins.end();
    ccoinsmap::iterator ret = cachecoins.insert(std::make_pair(txid, ccoinscacheentry())).first;
    tmp.swap(ret->second.coins);
    if (ret->second.coins.ispruned()) {
        // the parent only has an empty entry for this txid; we can consider our
        // version as fresh.
        ret->second.flags = ccoinscacheentry::fresh;
    }
    cachedcoinsusage += memusage::dynamicusage(ret->second.coins);
    return ret;
}

bool ccoinsviewcache::getcoins(const uint256 &txid, ccoins &coins) const {
    ccoinsmap::const_iterator it = fetchcoins(txid);
    if (it != cachecoins.end()) {
        coins = it->second.coins;
        return true;
    }
    return false;
}

ccoinsmodifier ccoinsviewcache::modifycoins(const uint256 &txid) {
    assert(!hasmodifier);
    std::pair<ccoinsmap::iterator, bool> ret = cachecoins.insert(std::make_pair(txid, ccoinscacheentry()));
    size_t cachedcoinusage = 0;
    if (ret.second) {
        if (!base->getcoins(txid, ret.first->second.coins)) {
            // the parent view does not have this entry; mark it as fresh.
            ret.first->second.coins.clear();
            ret.first->second.flags = ccoinscacheentry::fresh;
        } else if (ret.first->second.coins.ispruned()) {
            // the parent view only has a pruned entry for this; mark it as fresh.
            ret.first->second.flags = ccoinscacheentry::fresh;
        }
    } else {
        cachedcoinusage = memusage::dynamicusage(ret.first->second.coins);
    }
    // assume that whenever modifycoins is called, the entry will be modified.
    ret.first->second.flags |= ccoinscacheentry::dirty;
    return ccoinsmodifier(*this, ret.first, cachedcoinusage);
}

const ccoins* ccoinsviewcache::accesscoins(const uint256 &txid) const {
    ccoinsmap::const_iterator it = fetchcoins(txid);
    if (it == cachecoins.end()) {
        return null;
    } else {
        return &it->second.coins;
    }
}

bool ccoinsviewcache::havecoins(const uint256 &txid) const {
    ccoinsmap::const_iterator it = fetchcoins(txid);
    // we're using vtx.empty() instead of ispruned here for performance reasons,
    // as we only care about the case where a transaction was replaced entirely
    // in a reorganization (which wipes vout entirely, as opposed to spending
    // which just cleans individual outputs).
    return (it != cachecoins.end() && !it->second.coins.vout.empty());
}

uint256 ccoinsviewcache::getbestblock() const {
    if (hashblock.isnull())
        hashblock = base->getbestblock();
    return hashblock;
}

void ccoinsviewcache::setbestblock(const uint256 &hashblockin) {
    hashblock = hashblockin;
}

bool ccoinsviewcache::batchwrite(ccoinsmap &mapcoins, const uint256 &hashblockin) {
    assert(!hasmodifier);
    for (ccoinsmap::iterator it = mapcoins.begin(); it != mapcoins.end();) {
        if (it->second.flags & ccoinscacheentry::dirty) { // ignore non-dirty entries (optimization).
            ccoinsmap::iterator itus = cachecoins.find(it->first);
            if (itus == cachecoins.end()) {
                if (!it->second.coins.ispruned()) {
                    // the parent cache does not have an entry, while the child
                    // cache does have (a non-pruned) one. move the data up, and
                    // mark it as fresh (if the grandparent did have it, we
                    // would have pulled it in at first getcoins).
                    assert(it->second.flags & ccoinscacheentry::fresh);
                    ccoinscacheentry& entry = cachecoins[it->first];
                    entry.coins.swap(it->second.coins);
                    cachedcoinsusage += memusage::dynamicusage(entry.coins);
                    entry.flags = ccoinscacheentry::dirty | ccoinscacheentry::fresh;
                }
            } else {
                if ((itus->second.flags & ccoinscacheentry::fresh) && it->second.coins.ispruned()) {
                    // the grandparent does not have an entry, and the child is
                    // modified and being pruned. this means we can just delete
                    // it from the parent.
                    cachedcoinsusage -= memusage::dynamicusage(itus->second.coins);
                    cachecoins.erase(itus);
                } else {
                    // a normal modification.
                    cachedcoinsusage -= memusage::dynamicusage(itus->second.coins);
                    itus->second.coins.swap(it->second.coins);
                    cachedcoinsusage += memusage::dynamicusage(itus->second.coins);
                    itus->second.flags |= ccoinscacheentry::dirty;
                }
            }
        }
        ccoinsmap::iterator itold = it++;
        mapcoins.erase(itold);
    }
    hashblock = hashblockin;
    return true;
}

bool ccoinsviewcache::flush() {
    bool fok = base->batchwrite(cachecoins, hashblock);
    cachecoins.clear();
    cachedcoinsusage = 0;
    return fok;
}

unsigned int ccoinsviewcache::getcachesize() const {
    return cachecoins.size();
}

const ctxout &ccoinsviewcache::getoutputfor(const ctxin& input) const
{
    const ccoins* coins = accesscoins(input.prevout.hash);
    assert(coins && coins->isavailable(input.prevout.n));
    return coins->vout[input.prevout.n];
}

camount ccoinsviewcache::getvaluein(const ctransaction& tx) const
{
    if (tx.iscoinbase())
        return 0;

    camount nresult = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
        nresult += getoutputfor(tx.vin[i]).nvalue;

    return nresult;
}

bool ccoinsviewcache::haveinputs(const ctransaction& tx) const
{
    if (!tx.iscoinbase()) {
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const coutpoint &prevout = tx.vin[i].prevout;
            const ccoins* coins = accesscoins(prevout.hash);
            if (!coins || !coins->isavailable(prevout.n)) {
                return false;
            }
        }
    }
    return true;
}

double ccoinsviewcache::getpriority(const ctransaction &tx, int nheight) const
{
    if (tx.iscoinbase())
        return 0.0;
    double dresult = 0.0;
    boost_foreach(const ctxin& txin, tx.vin)
    {
        const ccoins* coins = accesscoins(txin.prevout.hash);
        assert(coins);
        if (!coins->isavailable(txin.prevout.n)) continue;
        if (coins->nheight < nheight) {
            dresult += coins->vout[txin.prevout.n].nvalue * (nheight-coins->nheight);
        }
    }
    return tx.computepriority(dresult);
}

ccoinsmodifier::ccoinsmodifier(ccoinsviewcache& cache_, ccoinsmap::iterator it_, size_t usage) : cache(cache_), it(it_), cachedcoinusage(usage) {
    assert(!cache.hasmodifier);
    cache.hasmodifier = true;
}

ccoinsmodifier::~ccoinsmodifier()
{
    assert(cache.hasmodifier);
    cache.hasmodifier = false;
    it->second.coins.cleanup();
    cache.cachedcoinsusage -= cachedcoinusage; // subtract the old usage
    if ((it->second.flags & ccoinscacheentry::fresh) && it->second.coins.ispruned()) {
        cache.cachecoins.erase(it);
    } else {
        // if the coin still exists after the modification, add the new usage
        cache.cachedcoinsusage += memusage::dynamicusage(it->second.coins);
    }
}
