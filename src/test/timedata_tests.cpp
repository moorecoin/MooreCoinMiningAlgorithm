// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.
//
#include "timedata.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(timedata_tests, basictestingsetup)

boost_auto_test_case(util_medianfilter)
{
    cmedianfilter<int> filter(5, 15);

    boost_check_equal(filter.median(), 15);

    filter.input(20); // [15 20]
    boost_check_equal(filter.median(), 17);

    filter.input(30); // [15 20 30]
    boost_check_equal(filter.median(), 20);

    filter.input(3); // [3 15 20 30]
    boost_check_equal(filter.median(), 17);

    filter.input(7); // [3 7 15 20 30]
    boost_check_equal(filter.median(), 15);

    filter.input(18); // [3 7 18 20 30]
    boost_check_equal(filter.median(), 18);

    filter.input(0); // [0 3 7 18 30]
    boost_check_equal(filter.median(), 7);
}

boost_auto_test_suite_end()
