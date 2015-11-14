// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.
#include "arith_uint256.h"
#include "uint256.h"
#include "version.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <string>
#include <stdio.h>

boost_fixture_test_suite(uint256_tests, basictestingsetup)

const unsigned char r1array[] =
    "\x9c\x52\x4a\xdb\xcf\x56\x11\x12\x2b\x29\x12\x5e\x5d\x35\xd2\xd2"
    "\x22\x81\xaa\xb5\x33\xf0\x08\x32\xd5\x56\xb1\xf9\xea\xe5\x1d\x7d";
const char r1arrayhex[] = "7d1de5eaf9b156d53208f033b5aa8122d2d2355d5e12292b121156cfdb4a529c";
const uint256 r1l = uint256(std::vector<unsigned char>(r1array,r1array+32));
const uint160 r1s = uint160(std::vector<unsigned char>(r1array,r1array+20));

const unsigned char r2array[] =
    "\x70\x32\x1d\x7c\x47\xa5\x6b\x40\x26\x7e\x0a\xc3\xa6\x9c\xb6\xbf"
    "\x13\x30\x47\xa3\x19\x2d\xda\x71\x49\x13\x72\xf0\xb4\xca\x81\xd7";
const uint256 r2l = uint256(std::vector<unsigned char>(r2array,r2array+32));
const uint160 r2s = uint160(std::vector<unsigned char>(r2array,r2array+20));

const unsigned char zeroarray[] =
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const uint256 zerol = uint256(std::vector<unsigned char>(zeroarray,zeroarray+32));
const uint160 zeros = uint160(std::vector<unsigned char>(zeroarray,zeroarray+20));

const unsigned char onearray[] =
    "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const uint256 onel = uint256(std::vector<unsigned char>(onearray,onearray+32));
const uint160 ones = uint160(std::vector<unsigned char>(onearray,onearray+20));

const unsigned char maxarray[] =
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
const uint256 maxl = uint256(std::vector<unsigned char>(maxarray,maxarray+32));
const uint160 maxs = uint160(std::vector<unsigned char>(maxarray,maxarray+20));

std::string arraytostring(const unsigned char a[], unsigned int width)
{
    std::stringstream stream;
    stream << std::hex;
    for (unsigned int i = 0; i < width; ++i)
    {
        stream<<std::setw(2)<<std::setfill('0')<<(unsigned int)a[width-i-1];
    }
    return stream.str();
}

inline uint160 uint160s(const char *str)
{
    uint160 rv;
    rv.sethex(str);
    return rv;
}
inline uint160 uint160s(const std::string& str)
{
    uint160 rv;
    rv.sethex(str);
    return rv;
}

boost_auto_test_case( basics ) // constructors, equality, inequality
{
    boost_check(1 == 0+1);
    // constructor uint256(vector<char>):
    boost_check(r1l.tostring() == arraytostring(r1array,32));
    boost_check(r1s.tostring() == arraytostring(r1array,20));
    boost_check(r2l.tostring() == arraytostring(r2array,32));
    boost_check(r2s.tostring() == arraytostring(r2array,20));
    boost_check(zerol.tostring() == arraytostring(zeroarray,32));
    boost_check(zeros.tostring() == arraytostring(zeroarray,20));
    boost_check(onel.tostring() == arraytostring(onearray,32));
    boost_check(ones.tostring() == arraytostring(onearray,20));
    boost_check(maxl.tostring() == arraytostring(maxarray,32));
    boost_check(maxs.tostring() == arraytostring(maxarray,20));
    boost_check(onel.tostring() != arraytostring(zeroarray,32));
    boost_check(ones.tostring() != arraytostring(zeroarray,20));

    // == and !=
    boost_check(r1l != r2l && r1s != r2s);
    boost_check(zerol != onel && zeros != ones);
    boost_check(onel != zerol && ones != zeros);
    boost_check(maxl != zerol && maxs != zeros);

    // string constructor and copy constructor
    boost_check(uint256s("0x"+r1l.tostring()) == r1l);
    boost_check(uint256s("0x"+r2l.tostring()) == r2l);
    boost_check(uint256s("0x"+zerol.tostring()) == zerol);
    boost_check(uint256s("0x"+onel.tostring()) == onel);
    boost_check(uint256s("0x"+maxl.tostring()) == maxl);
    boost_check(uint256s(r1l.tostring()) == r1l);
    boost_check(uint256s("   0x"+r1l.tostring()+"   ") == r1l);
    boost_check(uint256s("") == zerol);
    boost_check(r1l == uint256s(r1arrayhex));
    boost_check(uint256(r1l) == r1l);
    boost_check(uint256(zerol) == zerol);
    boost_check(uint256(onel) == onel);

    boost_check(uint160s("0x"+r1s.tostring()) == r1s);
    boost_check(uint160s("0x"+r2s.tostring()) == r2s);
    boost_check(uint160s("0x"+zeros.tostring()) == zeros);
    boost_check(uint160s("0x"+ones.tostring()) == ones);
    boost_check(uint160s("0x"+maxs.tostring()) == maxs);
    boost_check(uint160s(r1s.tostring()) == r1s);
    boost_check(uint160s("   0x"+r1s.tostring()+"   ") == r1s);
    boost_check(uint160s("") == zeros);
    boost_check(r1s == uint160s(r1arrayhex));

    boost_check(uint160(r1s) == r1s);
    boost_check(uint160(zeros) == zeros);
    boost_check(uint160(ones) == ones);
}

boost_auto_test_case( comparison ) // <= >= < >
{
    uint256 lastl;
    for (int i = 255; i >= 0; --i) {
        uint256 tmpl;
        *(tmpl.begin() + (i>>3)) |= 1<<(7-(i&7));
        boost_check( lastl < tmpl );
        lastl = tmpl;
    }

    boost_check( zerol < r1l );
    boost_check( r2l < r1l );
    boost_check( zerol < onel );
    boost_check( onel < maxl );
    boost_check( r1l < maxl );
    boost_check( r2l < maxl );

    uint160 lasts;
    for (int i = 159; i >= 0; --i) {
        uint160 tmps;
        *(tmps.begin() + (i>>3)) |= 1<<(7-(i&7));
        boost_check( lasts < tmps );
        lasts = tmps;
    }
    boost_check( zeros < r1s );
    boost_check( r2s < r1s );
    boost_check( zeros < ones );
    boost_check( ones < maxs );
    boost_check( r1s < maxs );
    boost_check( r2s < maxs );
}

boost_auto_test_case( methods ) // gethex sethex begin() end() size() getlow64 getserializesize, serialize, unserialize
{
    boost_check(r1l.gethex() == r1l.tostring());
    boost_check(r2l.gethex() == r2l.tostring());
    boost_check(onel.gethex() == onel.tostring());
    boost_check(maxl.gethex() == maxl.tostring());
    uint256 tmpl(r1l);
    boost_check(tmpl == r1l);
    tmpl.sethex(r2l.tostring());   boost_check(tmpl == r2l);
    tmpl.sethex(zerol.tostring()); boost_check(tmpl == uint256());

    tmpl.sethex(r1l.tostring());
    boost_check(memcmp(r1l.begin(), r1array, 32)==0);
    boost_check(memcmp(tmpl.begin(), r1array, 32)==0);
    boost_check(memcmp(r2l.begin(), r2array, 32)==0);
    boost_check(memcmp(zerol.begin(), zeroarray, 32)==0);
    boost_check(memcmp(onel.begin(), onearray, 32)==0);
    boost_check(r1l.size() == sizeof(r1l));
    boost_check(sizeof(r1l) == 32);
    boost_check(r1l.size() == 32);
    boost_check(r2l.size() == 32);
    boost_check(zerol.size() == 32);
    boost_check(maxl.size() == 32);
    boost_check(r1l.begin() + 32 == r1l.end());
    boost_check(r2l.begin() + 32 == r2l.end());
    boost_check(onel.begin() + 32 == onel.end());
    boost_check(maxl.begin() + 32 == maxl.end());
    boost_check(tmpl.begin() + 32 == tmpl.end());
    boost_check(r1l.getserializesize(0,protocol_version) == 32);
    boost_check(zerol.getserializesize(0,protocol_version) == 32);

    std::stringstream ss;
    r1l.serialize(ss,0,protocol_version);
    boost_check(ss.str() == std::string(r1array,r1array+32));
    tmpl.unserialize(ss,0,protocol_version);
    boost_check(r1l == tmpl);
    ss.str("");
    zerol.serialize(ss,0,protocol_version);
    boost_check(ss.str() == std::string(zeroarray,zeroarray+32));
    tmpl.unserialize(ss,0,protocol_version);
    boost_check(zerol == tmpl);
    ss.str("");
    maxl.serialize(ss,0,protocol_version);
    boost_check(ss.str() == std::string(maxarray,maxarray+32));
    tmpl.unserialize(ss,0,protocol_version);
    boost_check(maxl == tmpl);
    ss.str("");

    boost_check(r1s.gethex() == r1s.tostring());
    boost_check(r2s.gethex() == r2s.tostring());
    boost_check(ones.gethex() == ones.tostring());
    boost_check(maxs.gethex() == maxs.tostring());
    uint160 tmps(r1s);
    boost_check(tmps == r1s);
    tmps.sethex(r2s.tostring());   boost_check(tmps == r2s);
    tmps.sethex(zeros.tostring()); boost_check(tmps == uint160());

    tmps.sethex(r1s.tostring());
    boost_check(memcmp(r1s.begin(), r1array, 20)==0);
    boost_check(memcmp(tmps.begin(), r1array, 20)==0);
    boost_check(memcmp(r2s.begin(), r2array, 20)==0);
    boost_check(memcmp(zeros.begin(), zeroarray, 20)==0);
    boost_check(memcmp(ones.begin(), onearray, 20)==0);
    boost_check(r1s.size() == sizeof(r1s));
    boost_check(sizeof(r1s) == 20);
    boost_check(r1s.size() == 20);
    boost_check(r2s.size() == 20);
    boost_check(zeros.size() == 20);
    boost_check(maxs.size() == 20);
    boost_check(r1s.begin() + 20 == r1s.end());
    boost_check(r2s.begin() + 20 == r2s.end());
    boost_check(ones.begin() + 20 == ones.end());
    boost_check(maxs.begin() + 20 == maxs.end());
    boost_check(tmps.begin() + 20 == tmps.end());
    boost_check(r1s.getserializesize(0,protocol_version) == 20);
    boost_check(zeros.getserializesize(0,protocol_version) == 20);

    r1s.serialize(ss,0,protocol_version);
    boost_check(ss.str() == std::string(r1array,r1array+20));
    tmps.unserialize(ss,0,protocol_version);
    boost_check(r1s == tmps);
    ss.str("");
    zeros.serialize(ss,0,protocol_version);
    boost_check(ss.str() == std::string(zeroarray,zeroarray+20));
    tmps.unserialize(ss,0,protocol_version);
    boost_check(zeros == tmps);
    ss.str("");
    maxs.serialize(ss,0,protocol_version);
    boost_check(ss.str() == std::string(maxarray,maxarray+20));
    tmps.unserialize(ss,0,protocol_version);
    boost_check(maxs == tmps);
    ss.str("");
}

boost_auto_test_case( conversion )
{
    boost_check(arithtouint256(uinttoarith256(zerol)) == zerol);
    boost_check(arithtouint256(uinttoarith256(onel)) == onel);
    boost_check(arithtouint256(uinttoarith256(r1l)) == r1l);
    boost_check(arithtouint256(uinttoarith256(r2l)) == r2l);
    boost_check(uinttoarith256(zerol) == 0);
    boost_check(uinttoarith256(onel) == 1);
    boost_check(arithtouint256(0) == zerol);
    boost_check(arithtouint256(1) == onel);
    boost_check(arith_uint256(r1l.gethex()) == uinttoarith256(r1l));
    boost_check(arith_uint256(r2l.gethex()) == uinttoarith256(r2l));
    boost_check(r1l.gethex() == uinttoarith256(r1l).gethex());
    boost_check(r2l.gethex() == uinttoarith256(r2l).gethex());
}

boost_auto_test_suite_end()
