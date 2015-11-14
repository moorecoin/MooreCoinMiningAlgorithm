// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "compressor.h"
#include "util.h"
#include "test/test_moorecoin.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>

// amounts 0.00000001 .. 0.00100000
#define num_multiples_unit 100000

// amounts 0.01 .. 100.00
#define num_multiples_cent 10000

// amounts 1 .. 10000
#define num_multiples_1btc 10000

// amounts 50 .. 21000000
#define num_multiples_50btc 420000

boost_fixture_test_suite(compress_tests, basictestingsetup)

bool static testencode(uint64_t in) {
    return in == ctxoutcompressor::decompressamount(ctxoutcompressor::compressamount(in));
}

bool static testdecode(uint64_t in) {
    return in == ctxoutcompressor::compressamount(ctxoutcompressor::decompressamount(in));
}

bool static testpair(uint64_t dec, uint64_t enc) {
    return ctxoutcompressor::compressamount(dec) == enc &&
           ctxoutcompressor::decompressamount(enc) == dec;
}

boost_auto_test_case(compress_amounts)
{
    boost_check(testpair(            0,       0x0));
    boost_check(testpair(            1,       0x1));
    boost_check(testpair(         cent,       0x7));
    boost_check(testpair(         coin,       0x9));
    boost_check(testpair(      50*coin,      0x32));
    boost_check(testpair(21000000*coin, 0x1406f40));

    for (uint64_t i = 1; i <= num_multiples_unit; i++)
        boost_check(testencode(i));

    for (uint64_t i = 1; i <= num_multiples_cent; i++)
        boost_check(testencode(i * cent));

    for (uint64_t i = 1; i <= num_multiples_1btc; i++)
        boost_check(testencode(i * coin));

    for (uint64_t i = 1; i <= num_multiples_50btc; i++)
        boost_check(testencode(i * 50 * coin));

    for (uint64_t i = 0; i < 100000; i++)
        boost_check(testdecode(i));
}

boost_auto_test_suite_end()
