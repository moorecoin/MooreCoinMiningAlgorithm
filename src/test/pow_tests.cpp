// copyright (c) 2015 the moorecoin core developers
// distributed under the mit/x11 software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "pow.h"
#include "util.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(pow_tests, basictestingsetup)

/* test calculation of next difficulty target with no constraints applying */
boost_auto_test_case(get_next_work)
{
    selectparams(cbasechainparams::main);
    const consensus::params& params = params().getconsensus();

    int64_t nlastretargettime = 1261130161; // block #30240
    cblockindex pindexlast;
    pindexlast.nheight = 32255;
    pindexlast.ntime = 1262152739;  // block #32255
    pindexlast.nbits = 0x1d00ffff;
    boost_check_equal(calculatenextworkrequired(&pindexlast, nlastretargettime, params), 0x1d00d86a);
}

/* test the constraint on the upper bound for next work */
boost_auto_test_case(get_next_work_pow_limit)
{
    selectparams(cbasechainparams::main);
    const consensus::params& params = params().getconsensus();

    int64_t nlastretargettime = 1231006505; // block #0
    cblockindex pindexlast;
    pindexlast.nheight = 2015;
    pindexlast.ntime = 1233061996;  // block #2015
    pindexlast.nbits = 0x1d00ffff;
    boost_check_equal(calculatenextworkrequired(&pindexlast, nlastretargettime, params), 0x1d00ffff);
}

/* test the constraint on the lower bound for actual time taken */
boost_auto_test_case(get_next_work_lower_limit_actual)
{
    selectparams(cbasechainparams::main);
    const consensus::params& params = params().getconsensus();

    int64_t nlastretargettime = 1279008237; // block #66528
    cblockindex pindexlast;
    pindexlast.nheight = 68543;
    pindexlast.ntime = 1279297671;  // block #68543
    pindexlast.nbits = 0x1c05a3f4;
    boost_check_equal(calculatenextworkrequired(&pindexlast, nlastretargettime, params), 0x1c0168fd);
}

/* test the constraint on the upper bound for actual time taken */
boost_auto_test_case(get_next_work_upper_limit_actual)
{
    selectparams(cbasechainparams::main);
    const consensus::params& params = params().getconsensus();

    int64_t nlastretargettime = 1263163443; // note: not an actual block time
    cblockindex pindexlast;
    pindexlast.nheight = 46367;
    pindexlast.ntime = 1269211443;  // block #46367
    pindexlast.nbits = 0x1c387f6f;
    boost_check_equal(calculatenextworkrequired(&pindexlast, nlastretargettime, params), 0x1d00e1fd);
}

boost_auto_test_case(getblockproofequivalenttime_test)
{
    selectparams(cbasechainparams::main);
    const consensus::params& params = params().getconsensus();

    std::vector<cblockindex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : null;
        blocks[i].nheight = i;
        blocks[i].ntime = 1269211443 + i * params.npowtargetspacing;
        blocks[i].nbits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nchainwork = i ? blocks[i - 1].nchainwork + getblockproof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        cblockindex *p1 = &blocks[getrand(10000)];
        cblockindex *p2 = &blocks[getrand(10000)];
        cblockindex *p3 = &blocks[getrand(10000)];

        int64_t tdiff = getblockproofequivalenttime(*p1, *p2, *p3, params);
        boost_check_equal(tdiff, p1->getblocktime() - p2->getblocktime());
    }
}

boost_auto_test_suite_end()
