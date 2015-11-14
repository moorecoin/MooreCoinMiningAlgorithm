// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "utilstrencodings.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(base64_tests, basictestingsetup)

boost_auto_test_case(base64_testvectors)
{
    static const std::string vstrin[]  = {"","f","fo","foo","foob","fooba","foobar"};
    static const std::string vstrout[] = {"","zg==","zm8=","zm9v","zm9vyg==","zm9vyme=","zm9vymfy"};
    for (unsigned int i=0; i<sizeof(vstrin)/sizeof(vstrin[0]); i++)
    {
        std::string strenc = encodebase64(vstrin[i]);
        boost_check(strenc == vstrout[i]);
        std::string strdec = decodebase64(strenc);
        boost_check(strdec == vstrin[i]);
    }
}

boost_auto_test_suite_end()
