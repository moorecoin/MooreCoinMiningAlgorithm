// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "mruset.h"

#include "random.h"
#include "util.h"
#include "test/test_moorecoin.h"

#include <set>

#include <boost/test/unit_test.hpp>

#define num_tests 16
#define max_size 100

using namespace std;

boost_fixture_test_suite(mruset_tests, basictestingsetup)

boost_auto_test_case(mruset_test)
{
    // the mruset being tested.
    mruset<int> mru(5000);

    // run the test 10 times.
    for (int test = 0; test < 10; test++) {
        // reset mru.
        mru.clear();

        // a deque + set to simulate the mruset.
        std::deque<int> rep;
        std::set<int> all;

        // insert 10000 random integers below 15000.
        for (int j=0; j<10000; j++) {
            int add = getrandint(15000);
            mru.insert(add);

            // add the number to rep/all as well.
            if (all.count(add) == 0) {
               all.insert(add);
               rep.push_back(add);
               if (all.size() == 5001) {
                   all.erase(rep.front());
                   rep.pop_front();
               }
            }

            // do a full comparison between mru and the simulated mru every 1000 and every 5001 elements.
            if (j % 1000 == 0 || j % 5001 == 0) {
                mruset<int> mru2 = mru; // also try making a copy

                // check that all elements that should be in there, are in there.
                boost_foreach(int x, rep) {
                    boost_check(mru.count(x));
                    boost_check(mru2.count(x));
                }

                // check that all elements that are in there, should be in there.
                boost_foreach(int x, mru) {
                    boost_check(all.count(x));
                }

                // check that all elements that are in there, should be in there.
                boost_foreach(int x, mru2) {
                    boost_check(all.count(x));
                }

                for (int t = 0; t < 10; t++) {
                    int r = getrandint(15000);
                    boost_check(all.count(r) == mru.count(r));
                    boost_check(all.count(r) == mru2.count(r));
                }
            }
        }
    }
}

boost_auto_test_suite_end()
