// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include "uint256.h"
#include "arith_uint256.h"
#include <string>
#include "version.h"
#include "test/test_moorecoin.h"

boost_fixture_test_suite(arith_uint256_tests, basictestingsetup)

/// convert vector to arith_uint256, via uint256 blob
inline arith_uint256 arith_uint256v(const std::vector<unsigned char>& vch)
{
    return uinttoarith256(uint256(vch));
}

const unsigned char r1array[] =
    "\x9c\x52\x4a\xdb\xcf\x56\x11\x12\x2b\x29\x12\x5e\x5d\x35\xd2\xd2"
    "\x22\x81\xaa\xb5\x33\xf0\x08\x32\xd5\x56\xb1\xf9\xea\xe5\x1d\x7d";
const char r1arrayhex[] = "7d1de5eaf9b156d53208f033b5aa8122d2d2355d5e12292b121156cfdb4a529c";
const double r1ldouble = 0.4887374590559308955; // r1l equals roughly r1ldouble * 2^256
const arith_uint256 r1l = arith_uint256v(std::vector<unsigned char>(r1array,r1array+32));
const uint64_t r1llow64 = 0x121156cfdb4a529cull;

const unsigned char r2array[] =
    "\x70\x32\x1d\x7c\x47\xa5\x6b\x40\x26\x7e\x0a\xc3\xa6\x9c\xb6\xbf"
    "\x13\x30\x47\xa3\x19\x2d\xda\x71\x49\x13\x72\xf0\xb4\xca\x81\xd7";
const arith_uint256 r2l = arith_uint256v(std::vector<unsigned char>(r2array,r2array+32));

const char r1lplusr2l[] = "549fb09fea236a1ea3e31d4d58f1b1369288d204211ca751527cfc175767850c";

const unsigned char zeroarray[] =
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const arith_uint256 zerol = arith_uint256v(std::vector<unsigned char>(zeroarray,zeroarray+32));

const unsigned char onearray[] =
    "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
const arith_uint256 onel = arith_uint256v(std::vector<unsigned char>(onearray,onearray+32));

const unsigned char maxarray[] =
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
const arith_uint256 maxl = arith_uint256v(std::vector<unsigned char>(maxarray,maxarray+32));

const arith_uint256 halfl = (onel << 255);
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

boost_auto_test_case( basics ) // constructors, equality, inequality
{
    boost_check(1 == 0+1);
    // constructor arith_uint256(vector<char>):
    boost_check(r1l.tostring() == arraytostring(r1array,32));
    boost_check(r2l.tostring() == arraytostring(r2array,32));
    boost_check(zerol.tostring() == arraytostring(zeroarray,32));
    boost_check(onel.tostring() == arraytostring(onearray,32));
    boost_check(maxl.tostring() == arraytostring(maxarray,32));
    boost_check(onel.tostring() != arraytostring(zeroarray,32));

    // == and !=
    boost_check(r1l != r2l);
    boost_check(zerol != onel);
    boost_check(onel != zerol);
    boost_check(maxl != zerol);
    boost_check(~maxl == zerol);
    boost_check( ((r1l ^ r2l) ^ r1l) == r2l);

    uint64_t tmp64 = 0xc4dab720d9c7acaaull;
    for (unsigned int i = 0; i < 256; ++i)
    {
        boost_check(zerol != (onel << i));
        boost_check((onel << i) != zerol);
        boost_check(r1l != (r1l ^ (onel << i)));
        boost_check(((arith_uint256(tmp64) ^ (onel << i) ) != tmp64 ));
    }
    boost_check(zerol == (onel << 256));

    // string constructor and copy constructor
    boost_check(arith_uint256("0x"+r1l.tostring()) == r1l);
    boost_check(arith_uint256("0x"+r2l.tostring()) == r2l);
    boost_check(arith_uint256("0x"+zerol.tostring()) == zerol);
    boost_check(arith_uint256("0x"+onel.tostring()) == onel);
    boost_check(arith_uint256("0x"+maxl.tostring()) == maxl);
    boost_check(arith_uint256(r1l.tostring()) == r1l);
    boost_check(arith_uint256("   0x"+r1l.tostring()+"   ") == r1l);
    boost_check(arith_uint256("") == zerol);
    boost_check(r1l == arith_uint256(r1arrayhex));
    boost_check(arith_uint256(r1l) == r1l);
    boost_check((arith_uint256(r1l^r2l)^r2l) == r1l);
    boost_check(arith_uint256(zerol) == zerol);
    boost_check(arith_uint256(onel) == onel);

    // uint64_t constructor
    boost_check( (r1l & arith_uint256("0xffffffffffffffff")) == arith_uint256(r1llow64));
    boost_check(zerol == arith_uint256(0));
    boost_check(onel == arith_uint256(1));
    boost_check(arith_uint256("0xffffffffffffffff") = arith_uint256(0xffffffffffffffffull));

    // assignment (from base_uint)
    arith_uint256 tmpl = ~zerol; boost_check(tmpl == ~zerol);
    tmpl = ~onel; boost_check(tmpl == ~onel);
    tmpl = ~r1l; boost_check(tmpl == ~r1l);
    tmpl = ~r2l; boost_check(tmpl == ~r2l);
    tmpl = ~maxl; boost_check(tmpl == ~maxl);
}

void shiftarrayright(unsigned char* to, const unsigned char* from, unsigned int arraylength, unsigned int bitstoshift)
{
    for (unsigned int t=0; t < arraylength; ++t)
    {
        unsigned int f = (t+bitstoshift/8);
        if (f < arraylength)
            to[t]  = from[f] >> (bitstoshift%8);
        else
            to[t] = 0;
        if (f + 1 < arraylength)
            to[t] |= from[(f+1)] << (8-bitstoshift%8);
    }
}

void shiftarrayleft(unsigned char* to, const unsigned char* from, unsigned int arraylength, unsigned int bitstoshift)
{
    for (unsigned int t=0; t < arraylength; ++t)
    {
        if (t >= bitstoshift/8)
        {
            unsigned int f = t-bitstoshift/8;
            to[t]  = from[f] << (bitstoshift%8);
            if (t >= bitstoshift/8+1)
                to[t] |= from[f-1] >> (8-bitstoshift%8);
        }
        else {
            to[t] = 0;
        }
    }
}

boost_auto_test_case( shifts ) { // "<<"  ">>"  "<<="  ">>="
    unsigned char tmparray[32];
    arith_uint256 tmpl;
    for (unsigned int i = 0; i < 256; ++i)
    {
        shiftarrayleft(tmparray, onearray, 32, i);
        boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (onel << i));
        tmpl = onel; tmpl <<= i;
        boost_check(tmpl == (onel << i));
        boost_check((halfl >> (255-i)) == (onel << i));
        tmpl = halfl; tmpl >>= (255-i);
        boost_check(tmpl == (onel << i));

        shiftarrayleft(tmparray, r1array, 32, i);
        boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (r1l << i));
        tmpl = r1l; tmpl <<= i;
        boost_check(tmpl == (r1l << i));

        shiftarrayright(tmparray, r1array, 32, i);
        boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (r1l >> i));
        tmpl = r1l; tmpl >>= i;
        boost_check(tmpl == (r1l >> i));

        shiftarrayleft(tmparray, maxarray, 32, i);
        boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (maxl << i));
        tmpl = maxl; tmpl <<= i;
        boost_check(tmpl == (maxl << i));

        shiftarrayright(tmparray, maxarray, 32, i);
        boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (maxl >> i));
        tmpl = maxl; tmpl >>= i;
        boost_check(tmpl == (maxl >> i));
    }
    arith_uint256 c1l = arith_uint256(0x0123456789abcdefull);
    arith_uint256 c2l = c1l << 128;
    for (unsigned int i = 0; i < 128; ++i) {
        boost_check((c1l << i) == (c2l >> (128-i)));
    }
    for (unsigned int i = 128; i < 256; ++i) {
        boost_check((c1l << i) == (c2l << (i-128)));
    }
}

boost_auto_test_case( unaryoperators ) // !    ~    -
{
    boost_check(!zerol);
    boost_check(!(!onel));
    for (unsigned int i = 0; i < 256; ++i)
        boost_check(!(!(onel<<i)));
    boost_check(!(!r1l));
    boost_check(!(!maxl));

    boost_check(~zerol == maxl);

    unsigned char tmparray[32];
    for (unsigned int i = 0; i < 32; ++i) { tmparray[i] = ~r1array[i]; }
    boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (~r1l));

    boost_check(-zerol == zerol);
    boost_check(-r1l == (~r1l)+1);
    for (unsigned int i = 0; i < 256; ++i)
        boost_check(-(onel<<i) == (maxl << i));
}


// check if doing _a_ _op_ _b_ results in the same as applying _op_ onto each
// element of aarray and barray, and then converting the result into a arith_uint256.
#define checkbitwiseoperator(_a_,_b_,_op_)                              \
    for (unsigned int i = 0; i < 32; ++i) { tmparray[i] = _a_##array[i] _op_ _b_##array[i]; } \
    boost_check(arith_uint256v(std::vector<unsigned char>(tmparray,tmparray+32)) == (_a_##l _op_ _b_##l));

#define checkassignmentoperator(_a_,_b_,_op_)                           \
    tmpl = _a_##l; tmpl _op_##= _b_##l; boost_check(tmpl == (_a_##l _op_ _b_##l));

boost_auto_test_case( bitwiseoperators )
{
    unsigned char tmparray[32];

    checkbitwiseoperator(r1,r2,|)
    checkbitwiseoperator(r1,r2,^)
    checkbitwiseoperator(r1,r2,&)
    checkbitwiseoperator(r1,zero,|)
    checkbitwiseoperator(r1,zero,^)
    checkbitwiseoperator(r1,zero,&)
    checkbitwiseoperator(r1,max,|)
    checkbitwiseoperator(r1,max,^)
    checkbitwiseoperator(r1,max,&)
    checkbitwiseoperator(zero,r1,|)
    checkbitwiseoperator(zero,r1,^)
    checkbitwiseoperator(zero,r1,&)
    checkbitwiseoperator(max,r1,|)
    checkbitwiseoperator(max,r1,^)
    checkbitwiseoperator(max,r1,&)

    arith_uint256 tmpl;
    checkassignmentoperator(r1,r2,|)
    checkassignmentoperator(r1,r2,^)
    checkassignmentoperator(r1,r2,&)
    checkassignmentoperator(r1,zero,|)
    checkassignmentoperator(r1,zero,^)
    checkassignmentoperator(r1,zero,&)
    checkassignmentoperator(r1,max,|)
    checkassignmentoperator(r1,max,^)
    checkassignmentoperator(r1,max,&)
    checkassignmentoperator(zero,r1,|)
    checkassignmentoperator(zero,r1,^)
    checkassignmentoperator(zero,r1,&)
    checkassignmentoperator(max,r1,|)
    checkassignmentoperator(max,r1,^)
    checkassignmentoperator(max,r1,&)

    uint64_t tmp64 = 0xe1db685c9a0b47a2ull;
    tmpl = r1l; tmpl |= tmp64;  boost_check(tmpl == (r1l | arith_uint256(tmp64)));
    tmpl = r1l; tmpl |= 0; boost_check(tmpl == r1l);
    tmpl ^= 0; boost_check(tmpl == r1l);
    tmpl ^= tmp64;  boost_check(tmpl == (r1l ^ arith_uint256(tmp64)));
}

boost_auto_test_case( comparison ) // <= >= < >
{
    arith_uint256 tmpl;
    for (unsigned int i = 0; i < 256; ++i) {
        tmpl= onel<< i;
        boost_check( tmpl >= zerol && tmpl > zerol && zerol < tmpl && zerol <= tmpl);
        boost_check( tmpl >= 0 && tmpl > 0 && 0 < tmpl && 0 <= tmpl);
        tmpl |= r1l;
        boost_check( tmpl >= r1l ); boost_check( (tmpl == r1l) != (tmpl > r1l)); boost_check( (tmpl == r1l) || !( tmpl <= r1l));
        boost_check( r1l <= tmpl ); boost_check( (r1l == tmpl) != (r1l < tmpl)); boost_check( (tmpl == r1l) || !( r1l >= tmpl));
        boost_check(! (tmpl < r1l)); boost_check(! (r1l > tmpl));
    }
}

boost_auto_test_case( plusminus )
{
    arith_uint256 tmpl = 0;
    boost_check(r1l+r2l == arith_uint256(r1lplusr2l));
    tmpl += r1l;
    boost_check(tmpl == r1l);
    tmpl += r2l;
    boost_check(tmpl == r1l + r2l);
    boost_check(onel+maxl == zerol);
    boost_check(maxl+onel == zerol);
    for (unsigned int i = 1; i < 256; ++i) {
        boost_check( (maxl >> i) + onel == (halfl >> (i-1)) );
        boost_check( onel + (maxl >> i) == (halfl >> (i-1)) );
        tmpl = (maxl>>i); tmpl += onel;
        boost_check( tmpl == (halfl >> (i-1)) );
        tmpl = (maxl>>i); tmpl += 1;
        boost_check( tmpl == (halfl >> (i-1)) );
        tmpl = (maxl>>i);
        boost_check( tmpl++ == (maxl>>i) );
        boost_check( tmpl == (halfl >> (i-1)));
    }
    boost_check(arith_uint256(0xbedc77e27940a7ull) + 0xee8d836fce66fbull == arith_uint256(0xbedc77e27940a7ull + 0xee8d836fce66fbull));
    tmpl = arith_uint256(0xbedc77e27940a7ull); tmpl += 0xee8d836fce66fbull;
    boost_check(tmpl == arith_uint256(0xbedc77e27940a7ull+0xee8d836fce66fbull));
    tmpl -= 0xee8d836fce66fbull;  boost_check(tmpl == 0xbedc77e27940a7ull);
    tmpl = r1l;
    boost_check(++tmpl == r1l+1);

    boost_check(r1l -(-r2l) == r1l+r2l);
    boost_check(r1l -(-onel) == r1l+onel);
    boost_check(r1l - onel == r1l+(-onel));
    for (unsigned int i = 1; i < 256; ++i) {
        boost_check((maxl>>i) - (-onel)  == (halfl >> (i-1)));
        boost_check((halfl >> (i-1)) - onel == (maxl>>i));
        tmpl = (halfl >> (i-1));
        boost_check(tmpl-- == (halfl >> (i-1)));
        boost_check(tmpl == (maxl >> i));
        tmpl = (halfl >> (i-1));
        boost_check(--tmpl == (maxl >> i));
    }
    tmpl = r1l;
    boost_check(--tmpl == r1l-1);
}

boost_auto_test_case( multiply )
{
    boost_check((r1l * r1l).tostring() == "62a38c0486f01e45879d7910a7761bf30d5237e9873f9bff3642a732c4d84f10");
    boost_check((r1l * r2l).tostring() == "de37805e9986996cfba76ff6ba51c008df851987d9dd323f0e5de07760529c40");
    boost_check((r1l * zerol) == zerol);
    boost_check((r1l * onel) == r1l);
    boost_check((r1l * maxl) == -r1l);
    boost_check((r2l * r1l) == (r1l * r2l));
    boost_check((r2l * r2l).tostring() == "ac8c010096767d3cae5005dec28bb2b45a1d85ab7996ccd3e102a650f74ff100");
    boost_check((r2l * zerol) == zerol);
    boost_check((r2l * onel) == r2l);
    boost_check((r2l * maxl) == -r2l);

    boost_check(maxl * maxl == onel);

    boost_check((r1l * 0) == 0);
    boost_check((r1l * 1) == r1l);
    boost_check((r1l * 3).tostring() == "7759b1c0ed14047f961ad09b20ff83687876a0181a367b813634046f91def7d4");
    boost_check((r2l * 0x87654321ul).tostring() == "23f7816e30c4ae2017257b7a0fa64d60402f5234d46e746b61c960d09a26d070");
}

boost_auto_test_case( divide )
{
    arith_uint256 d1l("ad7133ac1977fa2b7");
    arith_uint256 d2l("ecd751716");
    boost_check((r1l / d1l).tostring() == "00000000000000000b8ac01106981635d9ed112290f8895545a7654dde28fb3a");
    boost_check((r1l / d2l).tostring() == "000000000873ce8efec5b67150bad3aa8c5fcb70e947586153bf2cec7c37c57a");
    boost_check(r1l / onel == r1l);
    boost_check(r1l / maxl == zerol);
    boost_check(maxl / r1l == 2);
    boost_check_throw(r1l / zerol, uint_error);
    boost_check((r2l / d1l).tostring() == "000000000000000013e1665895a1cc981de6d93670105a6b3ec3b73141b3a3c5");
    boost_check((r2l / d2l).tostring() == "000000000e8f0abe753bb0afe2e9437ee85d280be60882cf0bd1aaf7fa3cc2c4");
    boost_check(r2l / onel == r2l);
    boost_check(r2l / maxl == zerol);
    boost_check(maxl / r2l == 1);
    boost_check_throw(r2l / zerol, uint_error);
}


bool almostequal(double d1, double d2)
{
    return fabs(d1-d2) <= 4*fabs(d1)*std::numeric_limits<double>::epsilon();
}

boost_auto_test_case( methods ) // gethex sethex size() getlow64 getserializesize, serialize, unserialize
{
    boost_check(r1l.gethex() == r1l.tostring());
    boost_check(r2l.gethex() == r2l.tostring());
    boost_check(onel.gethex() == onel.tostring());
    boost_check(maxl.gethex() == maxl.tostring());
    arith_uint256 tmpl(r1l);
    boost_check(tmpl == r1l);
    tmpl.sethex(r2l.tostring());   boost_check(tmpl == r2l);
    tmpl.sethex(zerol.tostring()); boost_check(tmpl == 0);
    tmpl.sethex(halfl.tostring()); boost_check(tmpl == halfl);

    tmpl.sethex(r1l.tostring());
    boost_check(r1l.size() == 32);
    boost_check(r2l.size() == 32);
    boost_check(zerol.size() == 32);
    boost_check(maxl.size() == 32);
    boost_check(r1l.getlow64()  == r1llow64);
    boost_check(halfl.getlow64() ==0x0000000000000000ull);
    boost_check(onel.getlow64() ==0x0000000000000001ull);

    for (unsigned int i = 0; i < 255; ++i)
    {
        boost_check((onel << i).getdouble() == ldexp(1.0,i));
    }
    boost_check(zerol.getdouble() == 0.0);
    for (int i = 256; i > 53; --i)
        boost_check(almostequal((r1l>>(256-i)).getdouble(), ldexp(r1ldouble,i)));
    uint64_t r1l64part = (r1l>>192).getlow64();
    for (int i = 53; i > 0; --i) // doubles can store all integers in {0,...,2^54-1} exactly
    {
        boost_check((r1l>>(256-i)).getdouble() == (double)(r1l64part >> (64-i)));
    }
}

boost_auto_test_case(bignum_setcompact)
{
    arith_uint256 num;
    bool fnegative;
    bool foverflow;
    num.setcompact(0, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x00123456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x01003456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x02000056, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x03000000, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x04000000, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x00923456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x01803456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x02800056, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x03800000, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x04800000, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x01123456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000000012");
    boost_check_equal(num.getcompact(), 0x01120000u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    // make sure that we don't generate compacts with the 0x00800000 bit set
    num = 0x80;
    boost_check_equal(num.getcompact(), 0x02008000u);

    num.setcompact(0x01fedcba, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "000000000000000000000000000000000000000000000000000000000000007e");
    boost_check_equal(num.getcompact(true), 0x01fe0000u);
    boost_check_equal(fnegative, true);
    boost_check_equal(foverflow, false);

    num.setcompact(0x02123456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000001234");
    boost_check_equal(num.getcompact(), 0x02123400u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x03123456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000000123456");
    boost_check_equal(num.getcompact(), 0x03123456u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x04123456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000012345600");
    boost_check_equal(num.getcompact(), 0x04123456u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x04923456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000012345600");
    boost_check_equal(num.getcompact(true), 0x04923456u);
    boost_check_equal(fnegative, true);
    boost_check_equal(foverflow, false);

    num.setcompact(0x05009234, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "0000000000000000000000000000000000000000000000000000000092340000");
    boost_check_equal(num.getcompact(), 0x05009234u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0x20123456, &fnegative, &foverflow);
    boost_check_equal(num.gethex(), "1234560000000000000000000000000000000000000000000000000000000000");
    boost_check_equal(num.getcompact(), 0x20123456u);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, false);

    num.setcompact(0xff123456, &fnegative, &foverflow);
    boost_check_equal(fnegative, false);
    boost_check_equal(foverflow, true);
}


boost_auto_test_case( getmaxcoverage ) // some more tests just to get 100% coverage
{
    // ~r1l give a base_uint<256>
    boost_check((~~r1l >> 10) == (r1l >> 10));
    boost_check((~~r1l << 10) == (r1l << 10));
    boost_check(!(~~r1l < r1l));
    boost_check(~~r1l <= r1l);
    boost_check(!(~~r1l > r1l));
    boost_check(~~r1l >= r1l);
    boost_check(!(r1l < ~~r1l));
    boost_check(r1l <= ~~r1l);
    boost_check(!(r1l > ~~r1l));
    boost_check(r1l >= ~~r1l);

    boost_check(~~r1l + r2l == r1l + ~~r2l);
    boost_check(~~r1l - r2l == r1l - ~~r2l);
    boost_check(~r1l != r1l); boost_check(r1l != ~r1l);
    unsigned char tmparray[32];
    checkbitwiseoperator(~r1,r2,|)
    checkbitwiseoperator(~r1,r2,^)
    checkbitwiseoperator(~r1,r2,&)
    checkbitwiseoperator(r1,~r2,|)
    checkbitwiseoperator(r1,~r2,^)
    checkbitwiseoperator(r1,~r2,&)
}

boost_auto_test_suite_end()
