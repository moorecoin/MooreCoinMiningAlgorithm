// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "random.h"
#include "util.h"
#include "test/test_moorecoin.h"

#include <vector>

#include <boost/test/unit_test.hpp>

#define skiplist_length 300000

boost_fixture_test_suite(skiplist_tests, basictestingsetup)

boost_auto_test_case(skiplist_test)
{
    std::vector<cblockindex> vindex(skiplist_length);

    for (int i=0; i<skiplist_length; i++) {
        vindex[i].nheight = i;
        vindex[i].pprev = (i == 0) ? null : &vindex[i - 1];
        vindex[i].buildskip();
    }

    for (int i=0; i<skiplist_length; i++) {
        if (i > 0) {
            boost_check(vindex[i].pskip == &vindex[vindex[i].pskip->nheight]);
            boost_check(vindex[i].pskip->nheight < i);
        } else {
            boost_check(vindex[i].pskip == null);
        }
    }

    for (int i=0; i < 1000; i++) {
        int from = insecure_rand() % (skiplist_length - 1);
        int to = insecure_rand() % (from + 1);

        boost_check(vindex[skiplist_length - 1].getancestor(from) == &vindex[from]);
        boost_check(vindex[from].getancestor(to) == &vindex[to]);
        boost_check(vindex[from].getancestor(0) == &vindex[0]);
    }
}

boost_auto_test_case(getlocator_test)
{
    // build a main chain 100000 blocks long.
    std::vector<uint256> vhashmain(100000);
    std::vector<cblockindex> vblocksmain(100000);
    for (unsigned int i=0; i<vblocksmain.size(); i++) {
        vhashmain[i] = arithtouint256(i); // set the hash equal to the height, so we can quickly check the distances.
        vblocksmain[i].nheight = i;
        vblocksmain[i].pprev = i ? &vblocksmain[i - 1] : null;
        vblocksmain[i].phashblock = &vhashmain[i];
        vblocksmain[i].buildskip();
        boost_check_equal((int)uinttoarith256(vblocksmain[i].getblockhash()).getlow64(), vblocksmain[i].nheight);
        boost_check(vblocksmain[i].pprev == null || vblocksmain[i].nheight == vblocksmain[i].pprev->nheight + 1);
    }

    // build a branch that splits off at block 49999, 50000 blocks long.
    std::vector<uint256> vhashside(50000);
    std::vector<cblockindex> vblocksside(50000);
    for (unsigned int i=0; i<vblocksside.size(); i++) {
        vhashside[i] = arithtouint256(i + 50000 + (arith_uint256(1) << 128)); // add 1<<128 to the hashes, so getlow64() still returns the height.
        vblocksside[i].nheight = i + 50000;
        vblocksside[i].pprev = i ? &vblocksside[i - 1] : &vblocksmain[49999];
        vblocksside[i].phashblock = &vhashside[i];
        vblocksside[i].buildskip();
        boost_check_equal((int)uinttoarith256(vblocksside[i].getblockhash()).getlow64(), vblocksside[i].nheight);
        boost_check(vblocksside[i].pprev == null || vblocksside[i].nheight == vblocksside[i].pprev->nheight + 1);
    }

    // build a cchain for the main branch.
    cchain chain;
    chain.settip(&vblocksmain.back());

    // test 100 random starting points for locators.
    for (int n=0; n<100; n++) {
        int r = insecure_rand() % 150000;
        cblockindex* tip = (r < 100000) ? &vblocksmain[r] : &vblocksside[r - 100000];
        cblocklocator locator = chain.getlocator(tip);

        // the first result must be the block itself, the last one must be genesis.
        boost_check(locator.vhave.front() == tip->getblockhash());
        boost_check(locator.vhave.back() == vblocksmain[0].getblockhash());

        // entries 1 through 11 (inclusive) go back one step each.
        for (unsigned int i = 1; i < 12 && i < locator.vhave.size() - 1; i++) {
            boost_check_equal(uinttoarith256(locator.vhave[i]).getlow64(), tip->nheight - i);
        }

        // the further ones (excluding the last one) go back with exponential steps.
        unsigned int dist = 2;
        for (unsigned int i = 12; i < locator.vhave.size() - 1; i++) {
            boost_check_equal(uinttoarith256(locator.vhave[i - 1]).getlow64() - uinttoarith256(locator.vhave[i]).getlow64(), dist);
            dist *= 2;
        }
    }
}

boost_auto_test_suite_end()
