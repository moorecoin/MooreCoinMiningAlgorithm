// copyright 2014 bitpay, inc.
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include <stdint.h>
#include <vector>
#include <string>
#include <map>
#include "univalue/univalue.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(univalue_tests, basictestingsetup)

boost_auto_test_case(univalue_constructor)
{
    univalue v1;
    boost_check(v1.isnull());

    univalue v2(univalue::vstr);
    boost_check(v2.isstr());

    univalue v3(univalue::vstr, "foo");
    boost_check(v3.isstr());
    boost_check_equal(v3.getvalstr(), "foo");

    univalue numtest;
    boost_check(numtest.setnumstr("82"));
    boost_check(numtest.isnum());
    boost_check_equal(numtest.getvalstr(), "82");

    uint64_t vu64 = 82;
    univalue v4(vu64);
    boost_check(v4.isnum());
    boost_check_equal(v4.getvalstr(), "82");

    int64_t vi64 = -82;
    univalue v5(vi64);
    boost_check(v5.isnum());
    boost_check_equal(v5.getvalstr(), "-82");

    int vi = -688;
    univalue v6(vi);
    boost_check(v6.isnum());
    boost_check_equal(v6.getvalstr(), "-688");

    double vd = -7.21;
    univalue v7(vd);
    boost_check(v7.isreal());
    boost_check_equal(v7.getvalstr(), "-7.21");

    string vs("yawn");
    univalue v8(vs);
    boost_check(v8.isstr());
    boost_check_equal(v8.getvalstr(), "yawn");

    const char *vcs = "zappa";
    univalue v9(vcs);
    boost_check(v9.isstr());
    boost_check_equal(v9.getvalstr(), "zappa");
}

boost_auto_test_case(univalue_typecheck)
{
    univalue v1;
    boost_check(v1.setnumstr("1"));
    boost_check(v1.isnum());
    boost_check_throw(v1.get_bool(), runtime_error);

    univalue v2;
    boost_check(v2.setbool(true));
    boost_check_equal(v2.get_bool(), true);
    boost_check_throw(v2.get_int(), runtime_error);

    univalue v3;
    boost_check(v3.setnumstr("32482348723847471234"));
    boost_check_throw(v3.get_int64(), runtime_error);
    boost_check(v3.setnumstr("1000"));
    boost_check_equal(v3.get_int64(), 1000);

    univalue v4;
    boost_check(v4.setnumstr("2147483648"));
    boost_check_equal(v4.get_int64(), 2147483648);
    boost_check_throw(v4.get_int(), runtime_error);
    boost_check(v4.setnumstr("1000"));
    boost_check_equal(v4.get_int(), 1000);
    boost_check_throw(v4.get_str(), runtime_error);
    boost_check_equal(v4.get_real(), 1000);
    boost_check_throw(v4.get_array(), runtime_error);
    boost_check_throw(v4.getkeys(), runtime_error);
    boost_check_throw(v4.getvalues(), runtime_error);
    boost_check_throw(v4.get_obj(), runtime_error);

    univalue v5;
    boost_check(v5.read("[true, 10]"));
    boost_check_no_throw(v5.get_array());
    std::vector<univalue> vals = v5.getvalues();
    boost_check_throw(vals[0].get_int(), runtime_error);
    boost_check_equal(vals[0].get_bool(), true);

    boost_check_equal(vals[1].get_int(), 10);
    boost_check_throw(vals[1].get_bool(), runtime_error);
}

boost_auto_test_case(univalue_set)
{
    univalue v(univalue::vstr, "foo");
    v.clear();
    boost_check(v.isnull());
    boost_check_equal(v.getvalstr(), "");

    boost_check(v.setobject());
    boost_check(v.isobject());
    boost_check_equal(v.size(), 0);
    boost_check_equal(v.gettype(), univalue::vobj);
    boost_check(v.empty());

    boost_check(v.setarray());
    boost_check(v.isarray());
    boost_check_equal(v.size(), 0);

    boost_check(v.setstr("zum"));
    boost_check(v.isstr());
    boost_check_equal(v.getvalstr(), "zum");

    boost_check(v.setfloat(-1.01));
    boost_check(v.isreal());
    boost_check_equal(v.getvalstr(), "-1.01");

    boost_check(v.setint((int)1023));
    boost_check(v.isnum());
    boost_check_equal(v.getvalstr(), "1023");

    boost_check(v.setint((int64_t)-1023ll));
    boost_check(v.isnum());
    boost_check_equal(v.getvalstr(), "-1023");

    boost_check(v.setint((uint64_t)1023ull));
    boost_check(v.isnum());
    boost_check_equal(v.getvalstr(), "1023");

    boost_check(v.setnumstr("-688"));
    boost_check(v.isnum());
    boost_check_equal(v.getvalstr(), "-688");

    boost_check(v.setbool(false));
    boost_check_equal(v.isbool(), true);
    boost_check_equal(v.istrue(), false);
    boost_check_equal(v.isfalse(), true);
    boost_check_equal(v.getbool(), false);

    boost_check(v.setbool(true));
    boost_check_equal(v.isbool(), true);
    boost_check_equal(v.istrue(), true);
    boost_check_equal(v.isfalse(), false);
    boost_check_equal(v.getbool(), true);

    boost_check(!v.setnumstr("zombocom"));

    boost_check(v.setnull());
    boost_check(v.isnull());
}

boost_auto_test_case(univalue_array)
{
    univalue arr(univalue::varr);

    univalue v((int64_t)1023ll);
    boost_check(arr.push_back(v));

    string vstr("zippy");
    boost_check(arr.push_back(vstr));

    const char *s = "pippy";
    boost_check(arr.push_back(s));

    vector<univalue> vec;
    v.setstr("boing");
    vec.push_back(v);

    v.setstr("going");
    vec.push_back(v);

    boost_check(arr.push_backv(vec));

    boost_check_equal(arr.empty(), false);
    boost_check_equal(arr.size(), 5);

    boost_check_equal(arr[0].getvalstr(), "1023");
    boost_check_equal(arr[1].getvalstr(), "zippy");
    boost_check_equal(arr[2].getvalstr(), "pippy");
    boost_check_equal(arr[3].getvalstr(), "boing");
    boost_check_equal(arr[4].getvalstr(), "going");

    boost_check_equal(arr[999].getvalstr(), "");

    arr.clear();
    boost_check(arr.empty());
    boost_check_equal(arr.size(), 0);
}

boost_auto_test_case(univalue_object)
{
    univalue obj(univalue::vobj);
    string strkey, strval;
    univalue v;

    strkey = "age";
    v.setint(100);
    boost_check(obj.pushkv(strkey, v));

    strkey = "first";
    strval = "john";
    boost_check(obj.pushkv(strkey, strval));

    strkey = "last";
    const char *cval = "smith";
    boost_check(obj.pushkv(strkey, cval));

    strkey = "distance";
    boost_check(obj.pushkv(strkey, (int64_t) 25));

    strkey = "time";
    boost_check(obj.pushkv(strkey, (uint64_t) 3600));

    strkey = "calories";
    boost_check(obj.pushkv(strkey, (int) 12));

    strkey = "temperature";
    boost_check(obj.pushkv(strkey, (double) 90.012));

    univalue obj2(univalue::vobj);
    boost_check(obj2.pushkv("cat1", 9000));
    boost_check(obj2.pushkv("cat2", 12345));

    boost_check(obj.pushkvs(obj2));

    boost_check_equal(obj.empty(), false);
    boost_check_equal(obj.size(), 9);

    boost_check_equal(obj["age"].getvalstr(), "100");
    boost_check_equal(obj["first"].getvalstr(), "john");
    boost_check_equal(obj["last"].getvalstr(), "smith");
    boost_check_equal(obj["distance"].getvalstr(), "25");
    boost_check_equal(obj["time"].getvalstr(), "3600");
    boost_check_equal(obj["calories"].getvalstr(), "12");
    boost_check_equal(obj["temperature"].getvalstr(), "90.012");
    boost_check_equal(obj["cat1"].getvalstr(), "9000");
    boost_check_equal(obj["cat2"].getvalstr(), "12345");

    boost_check_equal(obj["nyuknyuknyuk"].getvalstr(), "");

    boost_check(obj.exists("age"));
    boost_check(obj.exists("first"));
    boost_check(obj.exists("last"));
    boost_check(obj.exists("distance"));
    boost_check(obj.exists("time"));
    boost_check(obj.exists("calories"));
    boost_check(obj.exists("temperature"));
    boost_check(obj.exists("cat1"));
    boost_check(obj.exists("cat2"));

    boost_check(!obj.exists("nyuknyuknyuk"));

    map<string, univalue::vtype> objtypes;
    objtypes["age"] = univalue::vnum;
    objtypes["first"] = univalue::vstr;
    objtypes["last"] = univalue::vstr;
    objtypes["distance"] = univalue::vnum;
    objtypes["time"] = univalue::vnum;
    objtypes["calories"] = univalue::vnum;
    objtypes["temperature"] = univalue::vreal;
    objtypes["cat1"] = univalue::vnum;
    objtypes["cat2"] = univalue::vnum;
    boost_check(obj.checkobject(objtypes));

    objtypes["cat2"] = univalue::vstr;
    boost_check(!obj.checkobject(objtypes));

    obj.clear();
    boost_check(obj.empty());
    boost_check_equal(obj.size(), 0);
}

static const char *json1 =
"[1.10000000,{\"key1\":\"str\\u0000\",\"key2\":800,\"key3\":{\"name\":\"martian\"}}]";

boost_auto_test_case(univalue_readwrite)
{
    univalue v;
    boost_check(v.read(json1));

    string strjson1(json1);
    boost_check(v.read(strjson1));

    boost_check(v.isarray());
    boost_check_equal(v.size(), 2);

    boost_check_equal(v[0].getvalstr(), "1.10000000");

    univalue obj = v[1];
    boost_check(obj.isobject());
    boost_check_equal(obj.size(), 3);

    boost_check(obj["key1"].isstr());
    std::string correctvalue("str");
    correctvalue.push_back('\0');
    boost_check_equal(obj["key1"].getvalstr(), correctvalue);
    boost_check(obj["key2"].isnum());
    boost_check_equal(obj["key2"].getvalstr(), "800");
    boost_check(obj["key3"].isobject());

    boost_check_equal(strjson1, v.write());
}

boost_auto_test_suite_end()

