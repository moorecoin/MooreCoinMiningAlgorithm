// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"

#include "clientversion.h"
#include "primitives/transaction.h"
#include "random.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "utilmoneystr.h"
#include "test/test_moorecoin.h"

#include <stdint.h>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(util_tests, basictestingsetup)

boost_auto_test_case(util_criticalsection)
{
    ccriticalsection cs;

    do {
        lock(cs);
        break;

        boost_error("break was swallowed!");
    } while(0);

    do {
        try_lock(cs, locktest);
        if (locktest)
            break;

        boost_error("break was swallowed!");
    } while(0);
}

static const unsigned char parsehex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67, 0xf1, 0xa6, 0x71, 0x30, 0xb7,
    0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0, 0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde,
    0xb6, 0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04, 0xe5, 0x1e, 0xc1, 0x12,
    0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b, 0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d,
    0x5f
};
boost_auto_test_case(util_parsehex)
{
    std::vector<unsigned char> result;
    std::vector<unsigned char> expected(parsehex_expected, parsehex_expected + sizeof(parsehex_expected));
    // basic test vector
    result = parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");
    boost_check_equal_collections(result.begin(), result.end(), expected.begin(), expected.end());

    // spaces between bytes must be supported
    result = parsehex("12 34 56 78");
    boost_check(result.size() == 4 && result[0] == 0x12 && result[1] == 0x34 && result[2] == 0x56 && result[3] == 0x78);

    // stop parsing at invalid value
    result = parsehex("1234 invalid 1234");
    boost_check(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

boost_auto_test_case(util_hexstr)
{
    boost_check_equal(
        hexstr(parsehex_expected, parsehex_expected + sizeof(parsehex_expected)),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    boost_check_equal(
        hexstr(parsehex_expected, parsehex_expected + 5, true),
        "04 67 8a fd b0");

    boost_check_equal(
        hexstr(parsehex_expected, parsehex_expected, true),
        "");

    std::vector<unsigned char> parsehex_vec(parsehex_expected, parsehex_expected + 5);

    boost_check_equal(
        hexstr(parsehex_vec, true),
        "04 67 8a fd b0");
}


boost_auto_test_case(util_datetimestrformat)
{
    boost_check_equal(datetimestrformat("%y-%m-%d %h:%m:%s", 0), "1970-01-01 00:00:00");
    boost_check_equal(datetimestrformat("%y-%m-%d %h:%m:%s", 0x7fffffff), "2038-01-19 03:14:07");
    boost_check_equal(datetimestrformat("%y-%m-%d %h:%m:%s", 1317425777), "2011-09-30 23:36:17");
    boost_check_equal(datetimestrformat("%y-%m-%d %h:%m", 1317425777), "2011-09-30 23:36");
    boost_check_equal(datetimestrformat("%a, %d %b %y %h:%m:%s +0000", 1317425777), "fri, 30 sep 2011 23:36:17 +0000");
}

boost_auto_test_case(util_parseparameters)
{
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    parseparameters(0, (char**)argv_test);
    boost_check(mapargs.empty() && mapmultiargs.empty());

    parseparameters(1, (char**)argv_test);
    boost_check(mapargs.empty() && mapmultiargs.empty());

    parseparameters(5, (char**)argv_test);
    // expectation: -ignored is ignored (program name argument),
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-gnu option parsing)
    boost_check(mapargs.size() == 3 && mapmultiargs.size() == 3);
    boost_check(mapargs.count("-a") && mapargs.count("-b") && mapargs.count("-ccc")
                && !mapargs.count("f") && !mapargs.count("-d"));
    boost_check(mapmultiargs.count("-a") && mapmultiargs.count("-b") && mapmultiargs.count("-ccc")
                && !mapmultiargs.count("f") && !mapmultiargs.count("-d"));

    boost_check(mapargs["-a"] == "" && mapargs["-ccc"] == "multiple");
    boost_check(mapmultiargs["-ccc"].size() == 2);
}

boost_auto_test_case(util_getarg)
{
    mapargs.clear();
    mapargs["strtest1"] = "string...";
    // strtest2 undefined on purpose
    mapargs["inttest1"] = "12345";
    mapargs["inttest2"] = "81985529216486895";
    // inttest3 undefined on purpose
    mapargs["booltest1"] = "";
    // booltest2 undefined on purpose
    mapargs["booltest3"] = "0";
    mapargs["booltest4"] = "1";

    boost_check_equal(getarg("strtest1", "default"), "string...");
    boost_check_equal(getarg("strtest2", "default"), "default");
    boost_check_equal(getarg("inttest1", -1), 12345);
    boost_check_equal(getarg("inttest2", -1), 81985529216486895ll);
    boost_check_equal(getarg("inttest3", -1), -1);
    boost_check_equal(getboolarg("booltest1", false), true);
    boost_check_equal(getboolarg("booltest2", false), false);
    boost_check_equal(getboolarg("booltest3", false), false);
    boost_check_equal(getboolarg("booltest4", false), true);
}

boost_auto_test_case(util_formatmoney)
{
    boost_check_equal(formatmoney(0), "0.00");
    boost_check_equal(formatmoney((coin/10000)*123456789), "12345.6789");
    boost_check_equal(formatmoney(-coin), "-1.00");

    boost_check_equal(formatmoney(coin*100000000), "100000000.00");
    boost_check_equal(formatmoney(coin*10000000), "10000000.00");
    boost_check_equal(formatmoney(coin*1000000), "1000000.00");
    boost_check_equal(formatmoney(coin*100000), "100000.00");
    boost_check_equal(formatmoney(coin*10000), "10000.00");
    boost_check_equal(formatmoney(coin*1000), "1000.00");
    boost_check_equal(formatmoney(coin*100), "100.00");
    boost_check_equal(formatmoney(coin*10), "10.00");
    boost_check_equal(formatmoney(coin), "1.00");
    boost_check_equal(formatmoney(coin/10), "0.10");
    boost_check_equal(formatmoney(coin/100), "0.01");
    boost_check_equal(formatmoney(coin/1000), "0.001");
    boost_check_equal(formatmoney(coin/10000), "0.0001");
    boost_check_equal(formatmoney(coin/100000), "0.00001");
    boost_check_equal(formatmoney(coin/1000000), "0.000001");
    boost_check_equal(formatmoney(coin/10000000), "0.0000001");
    boost_check_equal(formatmoney(coin/100000000), "0.00000001");
}

boost_auto_test_case(util_parsemoney)
{
    camount ret = 0;
    boost_check(parsemoney("0.0", ret));
    boost_check_equal(ret, 0);

    boost_check(parsemoney("12345.6789", ret));
    boost_check_equal(ret, (coin/10000)*123456789);

    boost_check(parsemoney("100000000.00", ret));
    boost_check_equal(ret, coin*100000000);
    boost_check(parsemoney("10000000.00", ret));
    boost_check_equal(ret, coin*10000000);
    boost_check(parsemoney("1000000.00", ret));
    boost_check_equal(ret, coin*1000000);
    boost_check(parsemoney("100000.00", ret));
    boost_check_equal(ret, coin*100000);
    boost_check(parsemoney("10000.00", ret));
    boost_check_equal(ret, coin*10000);
    boost_check(parsemoney("1000.00", ret));
    boost_check_equal(ret, coin*1000);
    boost_check(parsemoney("100.00", ret));
    boost_check_equal(ret, coin*100);
    boost_check(parsemoney("10.00", ret));
    boost_check_equal(ret, coin*10);
    boost_check(parsemoney("1.00", ret));
    boost_check_equal(ret, coin);
    boost_check(parsemoney("0.1", ret));
    boost_check_equal(ret, coin/10);
    boost_check(parsemoney("0.01", ret));
    boost_check_equal(ret, coin/100);
    boost_check(parsemoney("0.001", ret));
    boost_check_equal(ret, coin/1000);
    boost_check(parsemoney("0.0001", ret));
    boost_check_equal(ret, coin/10000);
    boost_check(parsemoney("0.00001", ret));
    boost_check_equal(ret, coin/100000);
    boost_check(parsemoney("0.000001", ret));
    boost_check_equal(ret, coin/1000000);
    boost_check(parsemoney("0.0000001", ret));
    boost_check_equal(ret, coin/10000000);
    boost_check(parsemoney("0.00000001", ret));
    boost_check_equal(ret, coin/100000000);

    // attempted 63 bit overflow should fail
    boost_check(!parsemoney("92233720368.54775808", ret));
}

boost_auto_test_case(util_ishex)
{
    boost_check(ishex("00"));
    boost_check(ishex("00112233445566778899aabbccddeeffaabbccddeeff"));
    boost_check(ishex("ff"));
    boost_check(ishex("ff"));

    boost_check(!ishex(""));
    boost_check(!ishex("0"));
    boost_check(!ishex("a"));
    boost_check(!ishex("eleven"));
    boost_check(!ishex("00xx00"));
    boost_check(!ishex("0x0000"));
}

boost_auto_test_case(util_seed_insecure_rand)
{
    int i;
    int count=0;

    seed_insecure_rand(true);

    for (int mod=2;mod<11;mod++)
    {
        int mask = 1;
        // really rough binomal confidence approximation.
        int err = 30*10000./mod*sqrt((1./mod*(1-1./mod))/10000.);
        //mask is 2^ceil(log2(mod))-1
        while(mask<mod-1)mask=(mask<<1)+1;

        count = 0;
        //how often does it get a zero from the uniform range [0,mod)?
        for (i=0;i<10000;i++)
        {
            uint32_t rval;
            do{
                rval=insecure_rand()&mask;
            }while(rval>=(uint32_t)mod);
            count += rval==0;
        }
        boost_check(count<=10000/mod+err);
        boost_check(count>=10000/mod-err);
    }
}

boost_auto_test_case(util_timingresistantequal)
{
    boost_check(timingresistantequal(std::string(""), std::string("")));
    boost_check(!timingresistantequal(std::string("abc"), std::string("")));
    boost_check(!timingresistantequal(std::string(""), std::string("abc")));
    boost_check(!timingresistantequal(std::string("a"), std::string("aa")));
    boost_check(!timingresistantequal(std::string("aa"), std::string("a")));
    boost_check(timingresistantequal(std::string("abc"), std::string("abc")));
    boost_check(!timingresistantequal(std::string("abc"), std::string("aba")));
}

/* test strprintf formatting directives.
 * put a string before and after to ensure sanity of element sizes on stack. */
#define b "check_prefix"
#define e "check_postfix"
boost_auto_test_case(strprintf_numbers)
{
    int64_t s64t = -9223372036854775807ll; /* signed 64 bit test value */
    uint64_t u64t = 18446744073709551615ull; /* unsigned 64 bit test value */
    boost_check(strprintf("%s %d %s", b, s64t, e) == b" -9223372036854775807 " e);
    boost_check(strprintf("%s %u %s", b, u64t, e) == b" 18446744073709551615 " e);
    boost_check(strprintf("%s %x %s", b, u64t, e) == b" ffffffffffffffff " e);

    size_t st = 12345678; /* unsigned size_t test value */
    ssize_t sst = -12345678; /* signed size_t test value */
    boost_check(strprintf("%s %d %s", b, sst, e) == b" -12345678 " e);
    boost_check(strprintf("%s %u %s", b, st, e) == b" 12345678 " e);
    boost_check(strprintf("%s %x %s", b, st, e) == b" bc614e " e);

    ptrdiff_t pt = 87654321; /* positive ptrdiff_t test value */
    ptrdiff_t spt = -87654321; /* negative ptrdiff_t test value */
    boost_check(strprintf("%s %d %s", b, spt, e) == b" -87654321 " e);
    boost_check(strprintf("%s %u %s", b, pt, e) == b" 87654321 " e);
    boost_check(strprintf("%s %x %s", b, pt, e) == b" 5397fb1 " e);
}
#undef b
#undef e

/* check for mingw/wine issue #3494
 * remove this test before time.ctime(0xffffffff) == 'sun feb  7 07:28:15 2106'
 */
boost_auto_test_case(gettime)
{
    boost_check((gettime() & ~0xffffffffll) == 0);
}

boost_auto_test_case(test_parseint32)
{
    int32_t n;
    // valid values
    boost_check(parseint32("1234", null));
    boost_check(parseint32("0", &n) && n == 0);
    boost_check(parseint32("1234", &n) && n == 1234);
    boost_check(parseint32("01234", &n) && n == 1234); // no octal
    boost_check(parseint32("2147483647", &n) && n == 2147483647);
    boost_check(parseint32("-2147483648", &n) && n == -2147483648);
    boost_check(parseint32("-1234", &n) && n == -1234);
    // invalid values
    boost_check(!parseint32("", &n));
    boost_check(!parseint32(" 1", &n)); // no padding inside
    boost_check(!parseint32("1 ", &n));
    boost_check(!parseint32("1a", &n));
    boost_check(!parseint32("aap", &n));
    boost_check(!parseint32("0x1", &n)); // no hex
    boost_check(!parseint32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    boost_check(!parseint32(teststr, &n)); // no embedded nuls
    // overflow and underflow
    boost_check(!parseint32("-2147483649", null));
    boost_check(!parseint32("2147483648", null));
    boost_check(!parseint32("-32482348723847471234", null));
    boost_check(!parseint32("32482348723847471234", null));
}

boost_auto_test_case(test_parseint64)
{
    int64_t n;
    // valid values
    boost_check(parseint64("1234", null));
    boost_check(parseint64("0", &n) && n == 0ll);
    boost_check(parseint64("1234", &n) && n == 1234ll);
    boost_check(parseint64("01234", &n) && n == 1234ll); // no octal
    boost_check(parseint64("2147483647", &n) && n == 2147483647ll);
    boost_check(parseint64("-2147483648", &n) && n == -2147483648ll);
    boost_check(parseint64("9223372036854775807", &n) && n == (int64_t)9223372036854775807);
    boost_check(parseint64("-9223372036854775808", &n) && n == (int64_t)-9223372036854775807-1);
    boost_check(parseint64("-1234", &n) && n == -1234ll);
    // invalid values
    boost_check(!parseint64("", &n));
    boost_check(!parseint64(" 1", &n)); // no padding inside
    boost_check(!parseint64("1 ", &n));
    boost_check(!parseint64("1a", &n));
    boost_check(!parseint64("aap", &n));
    boost_check(!parseint64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    boost_check(!parseint64(teststr, &n)); // no embedded nuls
    // overflow and underflow
    boost_check(!parseint64("-9223372036854775809", null));
    boost_check(!parseint64("9223372036854775808", null));
    boost_check(!parseint64("-32482348723847471234", null));
    boost_check(!parseint64("32482348723847471234", null));
}

boost_auto_test_case(test_parsedouble)
{
    double n;
    // valid values
    boost_check(parsedouble("1234", null));
    boost_check(parsedouble("0", &n) && n == 0.0);
    boost_check(parsedouble("1234", &n) && n == 1234.0);
    boost_check(parsedouble("01234", &n) && n == 1234.0); // no octal
    boost_check(parsedouble("2147483647", &n) && n == 2147483647.0);
    boost_check(parsedouble("-2147483648", &n) && n == -2147483648.0);
    boost_check(parsedouble("-1234", &n) && n == -1234.0);
    boost_check(parsedouble("1e6", &n) && n == 1e6);
    boost_check(parsedouble("-1e6", &n) && n == -1e6);
    // invalid values
    boost_check(!parsedouble("", &n));
    boost_check(!parsedouble(" 1", &n)); // no padding inside
    boost_check(!parsedouble("1 ", &n));
    boost_check(!parsedouble("1a", &n));
    boost_check(!parsedouble("aap", &n));
    boost_check(!parsedouble("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    boost_check(!parsedouble(teststr, &n)); // no embedded nuls
    // overflow and underflow
    boost_check(!parsedouble("-1e10000", null));
    boost_check(!parsedouble("1e10000", null));
}

boost_auto_test_case(test_formatparagraph)
{
    boost_check_equal(formatparagraph("", 79, 0), "");
    boost_check_equal(formatparagraph("test", 79, 0), "test");
    boost_check_equal(formatparagraph(" test", 79, 0), "test");
    boost_check_equal(formatparagraph("test test", 79, 0), "test test");
    boost_check_equal(formatparagraph("test test", 4, 0), "test\ntest");
    boost_check_equal(formatparagraph("testerde test ", 4, 0), "testerde\ntest");
    boost_check_equal(formatparagraph("test test", 4, 4), "test\n    test");
    boost_check_equal(formatparagraph("this is a very long test string. this is a second sentence in the very long test string."), "this is a very long test string. this is a second sentence in the very long\ntest string.");
}

boost_auto_test_case(test_formatsubversion)
{
    std::vector<std::string> comments;
    comments.push_back(std::string("comment1"));
    std::vector<std::string> comments2;
    comments2.push_back(std::string("comment1"));
    comments2.push_back(std::string("comment2"));
    boost_check_equal(formatsubversion("test", 99900, std::vector<std::string>()),std::string("/test:0.9.99/"));
    boost_check_equal(formatsubversion("test", 99900, comments),std::string("/test:0.9.99(comment1)/"));
    boost_check_equal(formatsubversion("test", 99900, comments2),std::string("/test:0.9.99(comment1; comment2)/"));
}
boost_auto_test_suite_end()
