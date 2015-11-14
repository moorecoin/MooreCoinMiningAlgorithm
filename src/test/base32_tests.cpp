// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "utilstrencodings.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(base32_tests, basictestingsetup)

boost_auto_test_case(base32_testvectors)
{
    static const std::string vstrin[]  = {"","f","fo","foo","foob","fooba","foobar"};
    static const std::string vstrout[] = {"","my======","mzxq====","mzxw6===","mzxw6yq=","mzxw6ytb","mzxw6ytboi======"};
    for (unsigned int i=0; i<sizeof(vstrin)/sizeof(vstrin[0]); i++)
    {
        std::string strenc = encodebase32(vstrin[i]);
        boost_check(strenc == vstrout[i]);
        std::string strdec = decodebase32(vstrout[i]);
        boost_check(strdec == vstrin[i]);
    }
}

boost_auto_test_suite_end()
