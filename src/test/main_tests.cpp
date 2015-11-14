// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "main.h"

#include "test/test_moorecoin.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(main_tests, testingsetup)

static void testblocksubsidyhalvings(const consensus::params& consensusparams)
{
    int maxhalvings = 64;
    camount ninitialsubsidy = 50 * coin;

    camount nprevioussubsidy = ninitialsubsidy * 2; // for height == 0
    boost_check_equal(nprevioussubsidy, ninitialsubsidy * 2);
    for (int nhalvings = 0; nhalvings < maxhalvings; nhalvings++) {
        int nheight = nhalvings * consensusparams.nsubsidyhalvinginterval;
        camount nsubsidy = getblocksubsidy(nheight, consensusparams);
        boost_check(nsubsidy <= ninitialsubsidy);
        boost_check_equal(nsubsidy, nprevioussubsidy / 2);
        nprevioussubsidy = nsubsidy;
    }
    boost_check_equal(getblocksubsidy(maxhalvings * consensusparams.nsubsidyhalvinginterval, consensusparams), 0);
}

static void testblocksubsidyhalvings(int nsubsidyhalvinginterval)
{
    consensus::params consensusparams;
    consensusparams.nsubsidyhalvinginterval = nsubsidyhalvinginterval;
    testblocksubsidyhalvings(consensusparams);
}

boost_auto_test_case(block_subsidy_test)
{
    testblocksubsidyhalvings(params(cbasechainparams::main).getconsensus()); // as in main
    testblocksubsidyhalvings(150); // as in regtest
    testblocksubsidyhalvings(1000); // just another interval
}

boost_auto_test_case(subsidy_limit_test)
{
    const consensus::params& consensusparams = params(cbasechainparams::main).getconsensus();
    camount nsum = 0;
    for (int nheight = 0; nheight < 14000000; nheight += 1000) {
        camount nsubsidy = getblocksubsidy(nheight, consensusparams);
        boost_check(nsubsidy <= 50 * coin);
        nsum += nsubsidy * 1000;
        boost_check(moneyrange(nsum));
    }
    boost_check_equal(nsum, 2099999997690000ull);
}

bool returnfalse() { return false; }
bool returntrue() { return true; }

boost_auto_test_case(test_combiner_all)
{
    boost::signals2::signal<bool (), combinerall> test;
    boost_check(test());
    test.connect(&returnfalse);
    boost_check(!test());
    test.connect(&returntrue);
    boost_check(!test());
    test.disconnect(&returnfalse);
    boost_check(test());
    test.disconnect(&returntrue);
    boost_check(test());
}

boost_auto_test_suite_end()
