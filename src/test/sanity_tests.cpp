// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "compat/sanity.h"
#include "key.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(sanity_tests, basictestingsetup)

boost_auto_test_case(basic_sanity)
{
  boost_check_message(glibc_sanity_test() == true, "libc sanity test");
  boost_check_message(glibcxx_sanity_test() == true, "stdlib sanity test");
  boost_check_message(ecc_initsanitycheck() == true, "openssl ecc test");
}

boost_auto_test_suite_end()
