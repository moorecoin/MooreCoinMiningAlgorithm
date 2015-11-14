// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"
#include "test/test_moorecoin.h"

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(getarg_tests, basictestingsetup)

static void resetargs(const std::string& strarg)
{
    std::vector<std::string> vecarg;
    if (strarg.size())
      boost::split(vecarg, strarg, boost::is_space(), boost::token_compress_on);

    // insert dummy executable name:
    vecarg.insert(vecarg.begin(), "testmoorecoin");

    // convert to char*:
    std::vector<const char*> vecchar;
    boost_foreach(std::string& s, vecarg)
        vecchar.push_back(s.c_str());

    parseparameters(vecchar.size(), &vecchar[0]);
}

boost_auto_test_case(boolarg)
{
    resetargs("-foo");
    boost_check(getboolarg("-foo", false));
    boost_check(getboolarg("-foo", true));

    boost_check(!getboolarg("-fo", false));
    boost_check(getboolarg("-fo", true));

    boost_check(!getboolarg("-fooo", false));
    boost_check(getboolarg("-fooo", true));

    resetargs("-foo=0");
    boost_check(!getboolarg("-foo", false));
    boost_check(!getboolarg("-foo", true));

    resetargs("-foo=1");
    boost_check(getboolarg("-foo", false));
    boost_check(getboolarg("-foo", true));

    // new 0.6 feature: auto-map -nosomething to !-something:
    resetargs("-nofoo");
    boost_check(!getboolarg("-foo", false));
    boost_check(!getboolarg("-foo", true));

    resetargs("-nofoo=1");
    boost_check(!getboolarg("-foo", false));
    boost_check(!getboolarg("-foo", true));

    resetargs("-foo -nofoo");  // -foo should win
    boost_check(getboolarg("-foo", false));
    boost_check(getboolarg("-foo", true));

    resetargs("-foo=1 -nofoo=1");  // -foo should win
    boost_check(getboolarg("-foo", false));
    boost_check(getboolarg("-foo", true));

    resetargs("-foo=0 -nofoo=0");  // -foo should win
    boost_check(!getboolarg("-foo", false));
    boost_check(!getboolarg("-foo", true));

    // new 0.6 feature: treat -- same as -:
    resetargs("--foo=1");
    boost_check(getboolarg("-foo", false));
    boost_check(getboolarg("-foo", true));

    resetargs("--nofoo=1");
    boost_check(!getboolarg("-foo", false));
    boost_check(!getboolarg("-foo", true));

}

boost_auto_test_case(stringarg)
{
    resetargs("");
    boost_check_equal(getarg("-foo", ""), "");
    boost_check_equal(getarg("-foo", "eleven"), "eleven");

    resetargs("-foo -bar");
    boost_check_equal(getarg("-foo", ""), "");
    boost_check_equal(getarg("-foo", "eleven"), "");

    resetargs("-foo=");
    boost_check_equal(getarg("-foo", ""), "");
    boost_check_equal(getarg("-foo", "eleven"), "");

    resetargs("-foo=11");
    boost_check_equal(getarg("-foo", ""), "11");
    boost_check_equal(getarg("-foo", "eleven"), "11");

    resetargs("-foo=eleven");
    boost_check_equal(getarg("-foo", ""), "eleven");
    boost_check_equal(getarg("-foo", "eleven"), "eleven");

}

boost_auto_test_case(intarg)
{
    resetargs("");
    boost_check_equal(getarg("-foo", 11), 11);
    boost_check_equal(getarg("-foo", 0), 0);

    resetargs("-foo -bar");
    boost_check_equal(getarg("-foo", 11), 0);
    boost_check_equal(getarg("-bar", 11), 0);

    resetargs("-foo=11 -bar=12");
    boost_check_equal(getarg("-foo", 0), 11);
    boost_check_equal(getarg("-bar", 11), 12);

    resetargs("-foo=nan -bar=notanumber");
    boost_check_equal(getarg("-foo", 1), 0);
    boost_check_equal(getarg("-bar", 11), 0);
}

boost_auto_test_case(doubledash)
{
    resetargs("--foo");
    boost_check_equal(getboolarg("-foo", false), true);

    resetargs("--foo=verbose --bar=1");
    boost_check_equal(getarg("-foo", ""), "verbose");
    boost_check_equal(getarg("-bar", 0), 1);
}

boost_auto_test_case(boolargno)
{
    resetargs("-nofoo");
    boost_check(!getboolarg("-foo", true));
    boost_check(!getboolarg("-foo", false));

    resetargs("-nofoo=1");
    boost_check(!getboolarg("-foo", true));
    boost_check(!getboolarg("-foo", false));

    resetargs("-nofoo=0");
    boost_check(getboolarg("-foo", true));
    boost_check(getboolarg("-foo", false));

    resetargs("-foo --nofoo");
    boost_check(getboolarg("-foo", true));
    boost_check(getboolarg("-foo", false));

    resetargs("-nofoo -foo"); // foo always wins:
    boost_check(getboolarg("-foo", true));
    boost_check(getboolarg("-foo", false));
}

boost_auto_test_suite_end()
