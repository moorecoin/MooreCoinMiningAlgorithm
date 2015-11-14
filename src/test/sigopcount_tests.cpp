// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "pubkey.h"
#include "key.h"
#include "script/script.h"
#include "script/standard.h"
#include "uint256.h"
#include "test/test_moorecoin.h"

#include <vector>

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

// helpers:
static std::vector<unsigned char>
serialize(const cscript& s)
{
    std::vector<unsigned char> sserialized(s);
    return sserialized;
}

boost_fixture_test_suite(sigopcount_tests, basictestingsetup)

boost_auto_test_case(getsigopcount)
{
    // test cscript::getsigopcount()
    cscript s1;
    boost_check_equal(s1.getsigopcount(false), 0u);
    boost_check_equal(s1.getsigopcount(true), 0u);

    uint160 dummy;
    s1 << op_1 << tobytevector(dummy) << tobytevector(dummy) << op_2 << op_checkmultisig;
    boost_check_equal(s1.getsigopcount(true), 2u);
    s1 << op_if << op_checksig << op_endif;
    boost_check_equal(s1.getsigopcount(true), 3u);
    boost_check_equal(s1.getsigopcount(false), 21u);

    cscript p2sh = getscriptfordestination(cscriptid(s1));
    cscript scriptsig;
    scriptsig << op_0 << serialize(s1);
    boost_check_equal(p2sh.getsigopcount(scriptsig), 3u);

    std::vector<cpubkey> keys;
    for (int i = 0; i < 3; i++)
    {
        ckey k;
        k.makenewkey(true);
        keys.push_back(k.getpubkey());
    }
    cscript s2 = getscriptformultisig(1, keys);
    boost_check_equal(s2.getsigopcount(true), 3u);
    boost_check_equal(s2.getsigopcount(false), 20u);

    p2sh = getscriptfordestination(cscriptid(s2));
    boost_check_equal(p2sh.getsigopcount(true), 0u);
    boost_check_equal(p2sh.getsigopcount(false), 0u);
    cscript scriptsig2;
    scriptsig2 << op_1 << tobytevector(dummy) << tobytevector(dummy) << serialize(s2);
    boost_check_equal(p2sh.getsigopcount(scriptsig2), 3u);
}

boost_auto_test_suite_end()
