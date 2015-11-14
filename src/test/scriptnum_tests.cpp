// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "bignum.h"
#include "script/script.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>
#include <limits.h>
#include <stdint.h>

boost_fixture_test_suite(scriptnum_tests, basictestingsetup)

static const int64_t values[] = \
{ 0, 1, char_min, char_max, uchar_max, shrt_min, ushrt_max, int_min, int_max, uint_max, long_min, long_max };
static const int64_t offsets[] = { 1, 0x79, 0x80, 0x81, 0xff, 0x7fff, 0x8000, 0xffff, 0x10000};

static bool verify(const cbignum& bignum, const cscriptnum& scriptnum)
{
    return bignum.getvch() == scriptnum.getvch() && bignum.getint() == scriptnum.getint();
}

static void checkcreatevch(const int64_t& num)
{
    cbignum bignum(num);
    cscriptnum scriptnum(num);
    boost_check(verify(bignum, scriptnum));

    cbignum bignum2(bignum.getvch());
    cscriptnum scriptnum2(scriptnum.getvch(), false);
    boost_check(verify(bignum2, scriptnum2));

    cbignum bignum3(scriptnum2.getvch());
    cscriptnum scriptnum3(bignum2.getvch(), false);
    boost_check(verify(bignum3, scriptnum3));
}

static void checkcreateint(const int64_t& num)
{
    cbignum bignum(num);
    cscriptnum scriptnum(num);
    boost_check(verify(bignum, scriptnum));
    boost_check(verify(bignum.getint(), cscriptnum(scriptnum.getint())));
    boost_check(verify(scriptnum.getint(), cscriptnum(bignum.getint())));
    boost_check(verify(cbignum(scriptnum.getint()).getint(), cscriptnum(cscriptnum(bignum.getint()).getint())));
}


static void checkadd(const int64_t& num1, const int64_t& num2)
{
    const cbignum bignum1(num1);
    const cbignum bignum2(num2);
    const cscriptnum scriptnum1(num1);
    const cscriptnum scriptnum2(num2);
    cbignum bignum3(num1);
    cbignum bignum4(num1);
    cscriptnum scriptnum3(num1);
    cscriptnum scriptnum4(num1);

    // int64_t overflow is undefined.
    bool invalid = (((num2 > 0) && (num1 > (std::numeric_limits<int64_t>::max() - num2))) ||
                    ((num2 < 0) && (num1 < (std::numeric_limits<int64_t>::min() - num2))));
    if (!invalid)
    {
        boost_check(verify(bignum1 + bignum2, scriptnum1 + scriptnum2));
        boost_check(verify(bignum1 + bignum2, scriptnum1 + num2));
        boost_check(verify(bignum1 + bignum2, scriptnum2 + num1));
    }
}

static void checknegate(const int64_t& num)
{
    const cbignum bignum(num);
    const cscriptnum scriptnum(num);

    // -int64_min is undefined
    if (num != std::numeric_limits<int64_t>::min())
        boost_check(verify(-bignum, -scriptnum));
}

static void checksubtract(const int64_t& num1, const int64_t& num2)
{
    const cbignum bignum1(num1);
    const cbignum bignum2(num2);
    const cscriptnum scriptnum1(num1);
    const cscriptnum scriptnum2(num2);
    bool invalid = false;

    // int64_t overflow is undefined.
    invalid = ((num2 > 0 && num1 < std::numeric_limits<int64_t>::min() + num2) ||
               (num2 < 0 && num1 > std::numeric_limits<int64_t>::max() + num2));
    if (!invalid)
    {
        boost_check(verify(bignum1 - bignum2, scriptnum1 - scriptnum2));
        boost_check(verify(bignum1 - bignum2, scriptnum1 - num2));
    }

    invalid = ((num1 > 0 && num2 < std::numeric_limits<int64_t>::min() + num1) ||
               (num1 < 0 && num2 > std::numeric_limits<int64_t>::max() + num1));
    if (!invalid)
    {
        boost_check(verify(bignum2 - bignum1, scriptnum2 - scriptnum1));
        boost_check(verify(bignum2 - bignum1, scriptnum2 - num1));
    }
}

static void checkcompare(const int64_t& num1, const int64_t& num2)
{
    const cbignum bignum1(num1);
    const cbignum bignum2(num2);
    const cscriptnum scriptnum1(num1);
    const cscriptnum scriptnum2(num2);

    boost_check((bignum1 == bignum1) == (scriptnum1 == scriptnum1));
    boost_check((bignum1 != bignum1) ==  (scriptnum1 != scriptnum1));
    boost_check((bignum1 < bignum1) ==  (scriptnum1 < scriptnum1));
    boost_check((bignum1 > bignum1) ==  (scriptnum1 > scriptnum1));
    boost_check((bignum1 >= bignum1) ==  (scriptnum1 >= scriptnum1));
    boost_check((bignum1 <= bignum1) ==  (scriptnum1 <= scriptnum1));

    boost_check((bignum1 == bignum1) == (scriptnum1 == num1));
    boost_check((bignum1 != bignum1) ==  (scriptnum1 != num1));
    boost_check((bignum1 < bignum1) ==  (scriptnum1 < num1));
    boost_check((bignum1 > bignum1) ==  (scriptnum1 > num1));
    boost_check((bignum1 >= bignum1) ==  (scriptnum1 >= num1));
    boost_check((bignum1 <= bignum1) ==  (scriptnum1 <= num1));

    boost_check((bignum1 == bignum2) ==  (scriptnum1 == scriptnum2));
    boost_check((bignum1 != bignum2) ==  (scriptnum1 != scriptnum2));
    boost_check((bignum1 < bignum2) ==  (scriptnum1 < scriptnum2));
    boost_check((bignum1 > bignum2) ==  (scriptnum1 > scriptnum2));
    boost_check((bignum1 >= bignum2) ==  (scriptnum1 >= scriptnum2));
    boost_check((bignum1 <= bignum2) ==  (scriptnum1 <= scriptnum2));

    boost_check((bignum1 == bignum2) ==  (scriptnum1 == num2));
    boost_check((bignum1 != bignum2) ==  (scriptnum1 != num2));
    boost_check((bignum1 < bignum2) ==  (scriptnum1 < num2));
    boost_check((bignum1 > bignum2) ==  (scriptnum1 > num2));
    boost_check((bignum1 >= bignum2) ==  (scriptnum1 >= num2));
    boost_check((bignum1 <= bignum2) ==  (scriptnum1 <= num2));
}

static void runcreate(const int64_t& num)
{
    checkcreateint(num);
    cscriptnum scriptnum(num);
    if (scriptnum.getvch().size() <= cscriptnum::nmaxnumsize)
        checkcreatevch(num);
    else
    {
        boost_check_throw (checkcreatevch(num), scriptnum_error);
    }
}

static void runoperators(const int64_t& num1, const int64_t& num2)
{
    checkadd(num1, num2);
    checksubtract(num1, num2);
    checknegate(num1);
    checkcompare(num1, num2);
}

boost_auto_test_case(creation)
{
    for(size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        for(size_t j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j)
        {
            runcreate(values[i]);
            runcreate(values[i] + offsets[j]);
            runcreate(values[i] - offsets[j]);
        }
    }
}

boost_auto_test_case(operators)
{
    for(size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        for(size_t j = 0; j < sizeof(offsets) / sizeof(offsets[0]); ++j)
        {
            runoperators(values[i], values[i]);
            runoperators(values[i], -values[i]);
            runoperators(values[i], values[j]);
            runoperators(values[i], -values[j]);
            runoperators(values[i] + values[j], values[j]);
            runoperators(values[i] + values[j], -values[j]);
            runoperators(values[i] - values[j], values[j]);
            runoperators(values[i] - values[j], -values[j]);
            runoperators(values[i] + values[j], values[i] + values[j]);
            runoperators(values[i] + values[j], values[i] - values[j]);
            runoperators(values[i] - values[j], values[i] + values[j]);
            runoperators(values[i] - values[j], values[i] - values[j]);
        }
    }
}

boost_auto_test_suite_end()
