// copyright (c) 2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "utilstrencodings.h"
#include "test/test_moorecoin.h"

#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(hash_tests, basictestingsetup)

boost_auto_test_case(murmurhash3)
{

#define t(expected, seed, data) boost_check_equal(murmurhash3(seed, parsehex(data)), expected)

    // test murmurhash3 with various inputs. of course this is retested in the
    // bloom filter tests - they would fail if murmurhash3() had any problems -
    // but is useful for those trying to implement moorecoin libraries as a
    // source of test data for their murmurhash3() primitive during
    // development.
    //
    // the magic number 0xfba4c795 comes from cbloomfilter::hash()

    t(0x00000000, 0x00000000, "");
    t(0x6a396f08, 0xfba4c795, "");
    t(0x81f16f39, 0xffffffff, "");

    t(0x514e28b7, 0x00000000, "00");
    t(0xea3f0b17, 0xfba4c795, "00");
    t(0xfd6cf10d, 0x00000000, "ff");

    t(0x16c6b7ab, 0x00000000, "0011");
    t(0x8eb51c3d, 0x00000000, "001122");
    t(0xb4471bf8, 0x00000000, "00112233");
    t(0xe2301fa8, 0x00000000, "0011223344");
    t(0xfc2e4a15, 0x00000000, "001122334455");
    t(0xb074502c, 0x00000000, "00112233445566");
    t(0x8034d2a0, 0x00000000, "0011223344556677");
    t(0xb4698def, 0x00000000, "001122334455667788");

#undef t
}

boost_auto_test_suite_end()
