// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"
#include "test/test_moorecoin.h"

#include <string>

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(netbase_tests, basictestingsetup)

boost_auto_test_case(netbase_networks)
{
    boost_check(cnetaddr("127.0.0.1").getnetwork()                              == net_unroutable);
    boost_check(cnetaddr("::1").getnetwork()                                    == net_unroutable);
    boost_check(cnetaddr("8.8.8.8").getnetwork()                                == net_ipv4);
    boost_check(cnetaddr("2001::8888").getnetwork()                             == net_ipv6);
    boost_check(cnetaddr("fd87:d87e:eb43:edb1:8e4:3588:e546:35ca").getnetwork() == net_tor);
}

boost_auto_test_case(netbase_properties)
{
    boost_check(cnetaddr("127.0.0.1").isipv4());
    boost_check(cnetaddr("::ffff:192.168.1.1").isipv4());
    boost_check(cnetaddr("::1").isipv6());
    boost_check(cnetaddr("10.0.0.1").isrfc1918());
    boost_check(cnetaddr("192.168.1.1").isrfc1918());
    boost_check(cnetaddr("172.31.255.255").isrfc1918());
    boost_check(cnetaddr("2001:0db8::").isrfc3849());
    boost_check(cnetaddr("169.254.1.1").isrfc3927());
    boost_check(cnetaddr("2002::1").isrfc3964());
    boost_check(cnetaddr("fc00::").isrfc4193());
    boost_check(cnetaddr("2001::2").isrfc4380());
    boost_check(cnetaddr("2001:10::").isrfc4843());
    boost_check(cnetaddr("fe80::").isrfc4862());
    boost_check(cnetaddr("64:ff9b::").isrfc6052());
    boost_check(cnetaddr("fd87:d87e:eb43:edb1:8e4:3588:e546:35ca").istor());
    boost_check(cnetaddr("127.0.0.1").islocal());
    boost_check(cnetaddr("::1").islocal());
    boost_check(cnetaddr("8.8.8.8").isroutable());
    boost_check(cnetaddr("2001::1").isroutable());
    boost_check(cnetaddr("127.0.0.1").isvalid());
}

bool static testsplithost(string test, string host, int port)
{
    string hostout;
    int portout = -1;
    splithostport(test, portout, hostout);
    return hostout == host && port == portout;
}

boost_auto_test_case(netbase_splithost)
{
    boost_check(testsplithost("www.moorecoin.org", "www.moorecoin.org", -1));
    boost_check(testsplithost("[www.moorecoin.org]", "www.moorecoin.org", -1));
    boost_check(testsplithost("www.moorecoin.org:80", "www.moorecoin.org", 80));
    boost_check(testsplithost("[www.moorecoin.org]:80", "www.moorecoin.org", 80));
    boost_check(testsplithost("127.0.0.1", "127.0.0.1", -1));
    boost_check(testsplithost("127.0.0.1:8333", "127.0.0.1", 8333));
    boost_check(testsplithost("[127.0.0.1]", "127.0.0.1", -1));
    boost_check(testsplithost("[127.0.0.1]:8333", "127.0.0.1", 8333));
    boost_check(testsplithost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    boost_check(testsplithost("[::ffff:127.0.0.1]:8333", "::ffff:127.0.0.1", 8333));
    boost_check(testsplithost("[::]:8333", "::", 8333));
    boost_check(testsplithost("::8333", "::8333", -1));
    boost_check(testsplithost(":8333", "", 8333));
    boost_check(testsplithost("[]:8333", "", 8333));
    boost_check(testsplithost("", "", -1));
}

bool static testparse(string src, string canon)
{
    cservice addr;
    if (!lookupnumeric(src.c_str(), addr, 65535))
        return canon == "";
    return canon == addr.tostring();
}

boost_auto_test_case(netbase_lookupnumeric)
{
    boost_check(testparse("127.0.0.1", "127.0.0.1:65535"));
    boost_check(testparse("127.0.0.1:8333", "127.0.0.1:8333"));
    boost_check(testparse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    boost_check(testparse("::", "[::]:65535"));
    boost_check(testparse("[::]:8333", "[::]:8333"));
    boost_check(testparse("[127.0.0.1]", "127.0.0.1:65535"));
    boost_check(testparse(":::", ""));
}

boost_auto_test_case(onioncat_test)
{
    // values from https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/onioncat
    cnetaddr addr1("5wyqrzbvrdsumnok.onion");
    cnetaddr addr2("fd87:d87e:eb43:edb1:8e4:3588:e546:35ca");
    boost_check(addr1 == addr2);
    boost_check(addr1.istor());
    boost_check(addr1.tostringip() == "5wyqrzbvrdsumnok.onion");
    boost_check(addr1.isroutable());
}

boost_auto_test_case(subnet_test)
{
    boost_check(csubnet("1.2.3.0/24") == csubnet("1.2.3.0/255.255.255.0"));
    boost_check(csubnet("1.2.3.0/24") != csubnet("1.2.4.0/255.255.255.0"));
    boost_check(csubnet("1.2.3.0/24").match(cnetaddr("1.2.3.4")));
    boost_check(!csubnet("1.2.2.0/24").match(cnetaddr("1.2.3.4")));
    boost_check(csubnet("1.2.3.4").match(cnetaddr("1.2.3.4")));
    boost_check(csubnet("1.2.3.4/32").match(cnetaddr("1.2.3.4")));
    boost_check(!csubnet("1.2.3.4").match(cnetaddr("5.6.7.8")));
    boost_check(!csubnet("1.2.3.4/32").match(cnetaddr("5.6.7.8")));
    boost_check(csubnet("::ffff:127.0.0.1").match(cnetaddr("127.0.0.1")));
    boost_check(csubnet("1:2:3:4:5:6:7:8").match(cnetaddr("1:2:3:4:5:6:7:8")));
    boost_check(!csubnet("1:2:3:4:5:6:7:8").match(cnetaddr("1:2:3:4:5:6:7:9")));
    boost_check(csubnet("1:2:3:4:5:6:7:0/112").match(cnetaddr("1:2:3:4:5:6:7:1234")));
    boost_check(csubnet("192.168.0.1/24").match(cnetaddr("192.168.0.2")));
    boost_check(csubnet("192.168.0.20/29").match(cnetaddr("192.168.0.18")));
    boost_check(csubnet("1.2.2.1/24").match(cnetaddr("1.2.2.4")));
    boost_check(csubnet("1.2.2.110/31").match(cnetaddr("1.2.2.111")));
    boost_check(csubnet("1.2.2.20/26").match(cnetaddr("1.2.2.63")));
    // all-matching ipv6 matches arbitrary ipv4 and ipv6
    boost_check(csubnet("::/0").match(cnetaddr("1:2:3:4:5:6:7:1234")));
    boost_check(csubnet("::/0").match(cnetaddr("1.2.3.4")));
    // all-matching ipv4 does not match ipv6
    boost_check(!csubnet("0.0.0.0/0").match(cnetaddr("1:2:3:4:5:6:7:1234")));
    // invalid subnets match nothing (not even invalid addresses)
    boost_check(!csubnet().match(cnetaddr("1.2.3.4")));
    boost_check(!csubnet("").match(cnetaddr("4.5.6.7")));
    boost_check(!csubnet("bloop").match(cnetaddr("0.0.0.0")));
    boost_check(!csubnet("bloop").match(cnetaddr("hab")));
    // check valid/invalid
    boost_check(csubnet("1.2.3.0/0").isvalid());
    boost_check(!csubnet("1.2.3.0/-1").isvalid());
    boost_check(csubnet("1.2.3.0/32").isvalid());
    boost_check(!csubnet("1.2.3.0/33").isvalid());
    boost_check(csubnet("1:2:3:4:5:6:7:8/0").isvalid());
    boost_check(csubnet("1:2:3:4:5:6:7:8/33").isvalid());
    boost_check(!csubnet("1:2:3:4:5:6:7:8/-1").isvalid());
    boost_check(csubnet("1:2:3:4:5:6:7:8/128").isvalid());
    boost_check(!csubnet("1:2:3:4:5:6:7:8/129").isvalid());
    boost_check(!csubnet("fuzzy").isvalid());
}

boost_auto_test_suite_end()
