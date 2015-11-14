// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"
#include "netbase.h"

#include "test/test_moorecoin.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include "univalue/univalue.h"

using namespace std;

univalue
createargs(int nrequired, const char* address1=null, const char* address2=null)
{
    univalue result(univalue::varr);
    result.push_back(nrequired);
    univalue addresses(univalue::varr);
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

univalue callrpc(string args)
{
    vector<string> vargs;
    boost::split(vargs, args, boost::is_any_of(" \t"));
    string strmethod = vargs[0];
    vargs.erase(vargs.begin());
    univalue params = rpcconvertvalues(strmethod, vargs);

    rpcfn_type method = tablerpc[strmethod]->actor;
    try {
        univalue result = (*method)(params, false);
        return result;
    }
    catch (const univalue& objerror) {
        throw runtime_error(find_value(objerror, "message").get_str());
    }
}


boost_fixture_test_suite(rpc_tests, testingsetup)

boost_auto_test_case(rpc_rawparams)
{
    // test raw transaction api argument handling
    univalue r;

    boost_check_throw(callrpc("getrawtransaction"), runtime_error);
    boost_check_throw(callrpc("getrawtransaction not_hex"), runtime_error);
    boost_check_throw(callrpc("getrawtransaction a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed not_int"), runtime_error);

    boost_check_throw(callrpc("createrawtransaction"), runtime_error);
    boost_check_throw(callrpc("createrawtransaction null null"), runtime_error);
    boost_check_throw(callrpc("createrawtransaction not_array"), runtime_error);
    boost_check_throw(callrpc("createrawtransaction [] []"), runtime_error);
    boost_check_throw(callrpc("createrawtransaction {} {}"), runtime_error);
    boost_check_no_throw(callrpc("createrawtransaction [] {}"));
    boost_check_throw(callrpc("createrawtransaction [] {} extra"), runtime_error);

    boost_check_throw(callrpc("decoderawtransaction"), runtime_error);
    boost_check_throw(callrpc("decoderawtransaction null"), runtime_error);
    boost_check_throw(callrpc("decoderawtransaction deadbeef"), runtime_error);
    string rawtx = "0100000001a15d57094aa7a21a28cb20b59aab8fc7d1149a3bdbcddba9c622e4f5f6a99ece010000006c493046022100f93bb0e7d8db7bd46e40132d1f8242026e045f03a0efe71bbb8e3f475e970d790221009337cd7f1f929f00cc6ff01f03729b069a7c21b59b1736ddfee5db5946c5da8c0121033b9b137ee87d5a812d6f506efdd37f0affa7ffc310711c06c7f3e097c9447c52ffffffff0100e1f505000000001976a9140389035a9225b3839e2bbf32d826a1e222031fd888ac00000000";
    boost_check_no_throw(r = callrpc(string("decoderawtransaction ")+rawtx));
    boost_check_equal(find_value(r.get_obj(), "version").get_int(), 1);
    boost_check_equal(find_value(r.get_obj(), "locktime").get_int(), 0);
    boost_check_throw(r = callrpc(string("decoderawtransaction ")+rawtx+" extra"), runtime_error);

    boost_check_throw(callrpc("signrawtransaction"), runtime_error);
    boost_check_throw(callrpc("signrawtransaction null"), runtime_error);
    boost_check_throw(callrpc("signrawtransaction ff00"), runtime_error);
    boost_check_no_throw(callrpc(string("signrawtransaction ")+rawtx));
    boost_check_no_throw(callrpc(string("signrawtransaction ")+rawtx+" null null none|anyonecanpay"));
    boost_check_no_throw(callrpc(string("signrawtransaction ")+rawtx+" [] [] none|anyonecanpay"));
    boost_check_throw(callrpc(string("signrawtransaction ")+rawtx+" null null badenum"), runtime_error);

    // only check failure cases for sendrawtransaction, there's no network to send to...
    boost_check_throw(callrpc("sendrawtransaction"), runtime_error);
    boost_check_throw(callrpc("sendrawtransaction null"), runtime_error);
    boost_check_throw(callrpc("sendrawtransaction deadbeef"), runtime_error);
    boost_check_throw(callrpc(string("sendrawtransaction ")+rawtx+" extra"), runtime_error);
}

boost_auto_test_case(rpc_rawsign)
{
    univalue r;
    // input is a 1-of-2 multisig (so is output):
    string prevout =
      "[{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\","
      "\"vout\":1,\"scriptpubkey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\","
      "\"redeemscript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}]";
    r = callrpc(string("createrawtransaction ")+prevout+" "+
      "{\"3hqae9ltnbjnsfm4cyyawtnvcauyt7v4oz\":11}");
    string notsigned = r.get_str();
    string privkey1 = "\"kzsxybp9jx64p5ekx1kuxrq79jht9uzw7lorgwe65i5rwacl6lqe\"";
    string privkey2 = "\"kyhdf5luktrx4ge69ybabsiuawjvrk4xgxakk2fqlp2hjgmy87z4\"";
    r = callrpc(string("signrawtransaction ")+notsigned+" "+prevout+" "+"[]");
    boost_check(find_value(r.get_obj(), "complete").get_bool() == false);
    r = callrpc(string("signrawtransaction ")+notsigned+" "+prevout+" "+"["+privkey1+","+privkey2+"]");
    boost_check(find_value(r.get_obj(), "complete").get_bool() == true);
}

boost_auto_test_case(rpc_format_monetary_values)
{
    boost_check(valuefromamount(0ll).write() == "0.00000000");
    boost_check(valuefromamount(1ll).write() == "0.00000001");
    boost_check(valuefromamount(17622195ll).write() == "0.17622195");
    boost_check(valuefromamount(50000000ll).write() == "0.50000000");
    boost_check(valuefromamount(89898989ll).write() == "0.89898989");
    boost_check(valuefromamount(100000000ll).write() == "1.00000000");
    boost_check(valuefromamount(2099999999999990ll).write() == "20999999.99999990");
    boost_check(valuefromamount(2099999999999999ll).write() == "20999999.99999999");
}

static univalue valuefromstring(const std::string &str)
{
    univalue value;
    boost_check(value.setnumstr(str));
    return value;
}

boost_auto_test_case(rpc_parse_monetary_values)
{
    boost_check_throw(amountfromvalue(valuefromstring("-0.00000001")), univalue);
    boost_check_equal(amountfromvalue(valuefromstring("0")), 0ll);
    boost_check_equal(amountfromvalue(valuefromstring("0.00000000")), 0ll);
    boost_check_equal(amountfromvalue(valuefromstring("0.00000001")), 1ll);
    boost_check_equal(amountfromvalue(valuefromstring("0.17622195")), 17622195ll);
    boost_check_equal(amountfromvalue(valuefromstring("0.5")), 50000000ll);
    boost_check_equal(amountfromvalue(valuefromstring("0.50000000")), 50000000ll);
    boost_check_equal(amountfromvalue(valuefromstring("0.89898989")), 89898989ll);
    boost_check_equal(amountfromvalue(valuefromstring("1.00000000")), 100000000ll);
    boost_check_equal(amountfromvalue(valuefromstring("20999999.9999999")), 2099999999999990ll);
    boost_check_equal(amountfromvalue(valuefromstring("20999999.99999999")), 2099999999999999ll);
}

boost_auto_test_case(json_parse_errors)
{
    // valid
    boost_check_equal(parsenonrfcjsonvalue("1.0").get_real(), 1.0);
    // valid, with leading or trailing whitespace
    boost_check_equal(parsenonrfcjsonvalue(" 1.0").get_real(), 1.0);
    boost_check_equal(parsenonrfcjsonvalue("1.0 ").get_real(), 1.0);
    // invalid, initial garbage
    boost_check_throw(parsenonrfcjsonvalue("[1.0"), std::runtime_error);
    boost_check_throw(parsenonrfcjsonvalue("a1.0"), std::runtime_error);
    // invalid, trailing garbage
    boost_check_throw(parsenonrfcjsonvalue("1.0sds"), std::runtime_error);
    boost_check_throw(parsenonrfcjsonvalue("1.0]"), std::runtime_error);
    // btc addresses should fail parsing
    boost_check_throw(parsenonrfcjsonvalue("175twpb8k1s7nmh4zx6rewf9wqrczv245w"), std::runtime_error);
    boost_check_throw(parsenonrfcjsonvalue("3j98t1wpez73cnmqviecrnyiwrnqrhwnl"), std::runtime_error);
}

boost_auto_test_case(rpc_boostasiotocnetaddr)
{
    // check ipv4 addresses
    boost_check_equal(boostasiotocnetaddr(boost::asio::ip::address::from_string("1.2.3.4")).tostring(), "1.2.3.4");
    boost_check_equal(boostasiotocnetaddr(boost::asio::ip::address::from_string("127.0.0.1")).tostring(), "127.0.0.1");
    // check ipv6 addresses
    boost_check_equal(boostasiotocnetaddr(boost::asio::ip::address::from_string("::1")).tostring(), "::1");
    boost_check_equal(boostasiotocnetaddr(boost::asio::ip::address::from_string("123:4567:89ab:cdef:123:4567:89ab:cdef")).tostring(),
                                         "123:4567:89ab:cdef:123:4567:89ab:cdef");
    // v4 compatible must be interpreted as ipv4
    boost_check_equal(boostasiotocnetaddr(boost::asio::ip::address::from_string("::0:127.0.0.1")).tostring(), "127.0.0.1");
    // v4 mapped must be interpreted as ipv4
    boost_check_equal(boostasiotocnetaddr(boost::asio::ip::address::from_string("::ffff:127.0.0.1")).tostring(), "127.0.0.1");
}

boost_auto_test_case(rpc_ban)
{
    boost_check_no_throw(callrpc(string("clearbanned")));
    
    univalue r;
    boost_check_no_throw(r = callrpc(string("setban 127.0.0.0 add")));
    boost_check_throw(r = callrpc(string("setban 127.0.0.0:8334")), runtime_error); //portnumber for setban not allowed
    boost_check_no_throw(r = callrpc(string("listbanned")));
    univalue ar = r.get_array();
    univalue o1 = ar[0].get_obj();
    univalue adr = find_value(o1, "address");
    boost_check_equal(adr.get_str(), "127.0.0.0/255.255.255.255");
    boost_check_no_throw(callrpc(string("setban 127.0.0.0 remove")));;
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    boost_check_equal(ar.size(), 0);

    boost_check_no_throw(r = callrpc(string("setban 127.0.0.0/24 add 1607731200 true")));
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    univalue banned_until = find_value(o1, "banned_untill");
    boost_check_equal(adr.get_str(), "127.0.0.0/255.255.255.0");
    boost_check_equal(banned_until.get_int64(), 1607731200); // absolute time check

    boost_check_no_throw(callrpc(string("clearbanned")));

    boost_check_no_throw(r = callrpc(string("setban 127.0.0.0/24 add 200")));
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    banned_until = find_value(o1, "banned_untill");
    boost_check_equal(adr.get_str(), "127.0.0.0/255.255.255.0");
    int64_t now = gettime();    
    boost_check(banned_until.get_int64() > now);
    boost_check(banned_until.get_int64()-now <= 200);

    // must throw an exception because 127.0.0.1 is in already banned suubnet range
    boost_check_throw(r = callrpc(string("setban 127.0.0.1 add")), runtime_error);

    boost_check_no_throw(callrpc(string("setban 127.0.0.0/24 remove")));;
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    boost_check_equal(ar.size(), 0);

    boost_check_no_throw(r = callrpc(string("setban 127.0.0.0/255.255.0.0 add")));
    boost_check_throw(r = callrpc(string("setban 127.0.1.1 add")), runtime_error);

    boost_check_no_throw(callrpc(string("clearbanned")));
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    boost_check_equal(ar.size(), 0);


    boost_check_throw(r = callrpc(string("setban test add")), runtime_error); //invalid ip

    //ipv6 tests
    boost_check_no_throw(r = callrpc(string("setban fe80:0000:0000:0000:0202:b3ff:fe1e:8329 add")));
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    boost_check_equal(adr.get_str(), "fe80::202:b3ff:fe1e:8329/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");

    boost_check_no_throw(callrpc(string("clearbanned")));
    boost_check_no_throw(r = callrpc(string("setban 2001:db8::/30 add")));
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    boost_check_equal(adr.get_str(), "2001:db8::/ffff:fffc:0:0:0:0:0:0");

    boost_check_no_throw(callrpc(string("clearbanned")));
    boost_check_no_throw(r = callrpc(string("setban 2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128 add")));
    boost_check_no_throw(r = callrpc(string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    boost_check_equal(adr.get_str(), "2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
}

boost_auto_test_suite_end()
