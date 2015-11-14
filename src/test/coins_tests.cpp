// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "random.h"
#include "uint256.h"
#include "test/test_moorecoin.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>

namespace
{
class ccoinsviewtest : public ccoinsview
{
    uint256 hashbestblock_;
    std::map<uint256, ccoins> map_;

public:
    bool getcoins(const uint256& txid, ccoins& coins) const
    {
        std::map<uint256, ccoins>::const_iterator it = map_.find(txid);
        if (it == map_.end()) {
            return false;
        }
        coins = it->second;
        if (coins.ispruned() && insecure_rand() % 2 == 0) {
            // randomly return false in case of an empty entry.
            return false;
        }
        return true;
    }

    bool havecoins(const uint256& txid) const
    {
        ccoins coins;
        return getcoins(txid, coins);
    }

    uint256 getbestblock() const { return hashbestblock_; }

    bool batchwrite(ccoinsmap& mapcoins, const uint256& hashblock)
    {
        for (ccoinsmap::iterator it = mapcoins.begin(); it != mapcoins.end(); ) {
            map_[it->first] = it->second.coins;
            if (it->second.coins.ispruned() && insecure_rand() % 3 == 0) {
                // randomly delete empty entries on write.
                map_.erase(it->first);
            }
            mapcoins.erase(it++);
        }
        mapcoins.clear();
        hashbestblock_ = hashblock;
        return true;
    }

    bool getstats(ccoinsstats& stats) const { return false; }
};

class ccoinsviewcachetest : public ccoinsviewcache
{
public:
    ccoinsviewcachetest(ccoinsview* base) : ccoinsviewcache(base) {}

    void selftest() const
    {
        // manually recompute the dynamic usage of the whole data, and compare it.
        size_t ret = memusage::dynamicusage(cachecoins);
        for (ccoinsmap::iterator it = cachecoins.begin(); it != cachecoins.end(); it++) {
            ret += memusage::dynamicusage(it->second.coins);
        }
        boost_check_equal(memusage::dynamicusage(*this), ret);
    }

};

}

boost_fixture_test_suite(coins_tests, basictestingsetup)

static const unsigned int num_simulation_iterations = 40000;

// this is a large randomized insert/remove simulation test on a variable-size
// stack of caches on top of ccoinsviewtest.
//
// it will randomly create/update/delete ccoins entries to a tip of caches, with
// txids picked from a limited list of random 256-bit hashes. occasionally, a
// new tip is added to the stack of caches, or the tip is flushed and removed.
//
// during the process, booleans are kept to make sure that the randomized
// operation hits all branches.
boost_auto_test_case(coins_cache_simulation_test)
{
    // various coverage trackers.
    bool removed_all_caches = false;
    bool reached_4_caches = false;
    bool added_an_entry = false;
    bool removed_an_entry = false;
    bool updated_an_entry = false;
    bool found_an_entry = false;
    bool missed_an_entry = false;

    // a simple map to track what we expect the cache stack to represent.
    std::map<uint256, ccoins> result;

    // the cache stack.
    ccoinsviewtest base; // a ccoinsviewtest at the bottom.
    std::vector<ccoinsviewcachetest*> stack; // a stack of ccoinsviewcaches on top.
    stack.push_back(new ccoinsviewcachetest(&base)); // start with one cache.

    // use a limited set of random transaction ids, so we do test overwriting entries.
    std::vector<uint256> txids;
    txids.resize(num_simulation_iterations / 8);
    for (unsigned int i = 0; i < txids.size(); i++) {
        txids[i] = getrandhash();
    }

    for (unsigned int i = 0; i < num_simulation_iterations; i++) {
        // do a random modification.
        {
            uint256 txid = txids[insecure_rand() % txids.size()]; // txid we're going to modify in this iteration.
            ccoins& coins = result[txid];
            ccoinsmodifier entry = stack.back()->modifycoins(txid);
            boost_check(coins == *entry);
            if (insecure_rand() % 5 == 0 || coins.ispruned()) {
                if (coins.ispruned()) {
                    added_an_entry = true;
                } else {
                    updated_an_entry = true;
                }
                coins.nversion = insecure_rand();
                coins.vout.resize(1);
                coins.vout[0].nvalue = insecure_rand();
                *entry = coins;
            } else {
                coins.clear();
                entry->clear();
                removed_an_entry = true;
            }
        }

        // once every 1000 iterations and at the end, verify the full cache.
        if (insecure_rand() % 1000 == 1 || i == num_simulation_iterations - 1) {
            for (std::map<uint256, ccoins>::iterator it = result.begin(); it != result.end(); it++) {
                const ccoins* coins = stack.back()->accesscoins(it->first);
                if (coins) {
                    boost_check(*coins == it->second);
                    found_an_entry = true;
                } else {
                    boost_check(it->second.ispruned());
                    missed_an_entry = true;
                }
            }
            boost_foreach(const ccoinsviewcachetest *test, stack) {
                test->selftest();
            }
        }

        if (insecure_rand() % 100 == 0) {
            // every 100 iterations, change the cache stack.
            if (stack.size() > 0 && insecure_rand() % 2 == 0) {
                stack.back()->flush();
                delete stack.back();
                stack.pop_back();
            }
            if (stack.size() == 0 || (stack.size() < 4 && insecure_rand() % 2)) {
                ccoinsview* tip = &base;
                if (stack.size() > 0) {
                    tip = stack.back();
                } else {
                    removed_all_caches = true;
                }
                stack.push_back(new ccoinsviewcachetest(tip));
                if (stack.size() == 4) {
                    reached_4_caches = true;
                }
            }
        }
    }

    // clean up the stack.
    while (stack.size() > 0) {
        delete stack.back();
        stack.pop_back();
    }

    // verify coverage.
    boost_check(removed_all_caches);
    boost_check(reached_4_caches);
    boost_check(added_an_entry);
    boost_check(removed_an_entry);
    boost_check(updated_an_entry);
    boost_check(found_an_entry);
    boost_check(missed_an_entry);
}

boost_auto_test_suite_end()
