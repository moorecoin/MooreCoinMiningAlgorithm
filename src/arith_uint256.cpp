// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"

#include "uint256.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

#include <stdio.h>
#include <string.h>

template <unsigned int bits>
base_uint<bits>::base_uint(const std::string& str)
{
    sethex(str);
}

template <unsigned int bits>
base_uint<bits>& base_uint<bits>::operator<<=(unsigned int shift)
{
    base_uint<bits> a(*this);
    for (int i = 0; i < width; i++)
        pn[i] = 0;
    int k = shift / 32;
    shift = shift % 32;
    for (int i = 0; i < width; i++) {
        if (i + k + 1 < width && shift != 0)
            pn[i + k + 1] |= (a.pn[i] >> (32 - shift));
        if (i + k < width)
            pn[i + k] |= (a.pn[i] << shift);
    }
    return *this;
}

template <unsigned int bits>
base_uint<bits>& base_uint<bits>::operator>>=(unsigned int shift)
{
    base_uint<bits> a(*this);
    for (int i = 0; i < width; i++)
        pn[i] = 0;
    int k = shift / 32;
    shift = shift % 32;
    for (int i = 0; i < width; i++) {
        if (i - k - 1 >= 0 && shift != 0)
            pn[i - k - 1] |= (a.pn[i] << (32 - shift));
        if (i - k >= 0)
            pn[i - k] |= (a.pn[i] >> shift);
    }
    return *this;
}

template <unsigned int bits>
base_uint<bits>& base_uint<bits>::operator*=(uint32_t b32)
{
    uint64_t carry = 0;
    for (int i = 0; i < width; i++) {
        uint64_t n = carry + (uint64_t)b32 * pn[i];
        pn[i] = n & 0xffffffff;
        carry = n >> 32;
    }
    return *this;
}

template <unsigned int bits>
base_uint<bits>& base_uint<bits>::operator*=(const base_uint& b)
{
    base_uint<bits> a = *this;
    *this = 0;
    for (int j = 0; j < width; j++) {
        uint64_t carry = 0;
        for (int i = 0; i + j < width; i++) {
            uint64_t n = carry + pn[i + j] + (uint64_t)a.pn[j] * b.pn[i];
            pn[i + j] = n & 0xffffffff;
            carry = n >> 32;
        }
    }
    return *this;
}

template <unsigned int bits>
base_uint<bits>& base_uint<bits>::operator/=(const base_uint& b)
{
    base_uint<bits> div = b;     // make a copy, so we can shift.
    base_uint<bits> num = *this; // make a copy, so we can subtract.
    *this = 0;                   // the quotient.
    int num_bits = num.bits();
    int div_bits = div.bits();
    if (div_bits == 0)
        throw uint_error("division by zero");
    if (div_bits > num_bits) // the result is certainly 0.
        return *this;
    int shift = num_bits - div_bits;
    div <<= shift; // shift so that div and num align.
    while (shift >= 0) {
        if (num >= div) {
            num -= div;
            pn[shift / 32] |= (1 << (shift & 31)); // set a bit of the result.
        }
        div >>= 1; // shift back.
        shift--;
    }
    // num now contains the remainder of the division.
    return *this;
}

template <unsigned int bits>
int base_uint<bits>::compareto(const base_uint<bits>& b) const
{
    for (int i = width - 1; i >= 0; i--) {
        if (pn[i] < b.pn[i])
            return -1;
        if (pn[i] > b.pn[i])
            return 1;
    }
    return 0;
}

template <unsigned int bits>
bool base_uint<bits>::equalto(uint64_t b) const
{
    for (int i = width - 1; i >= 2; i--) {
        if (pn[i])
            return false;
    }
    if (pn[1] != (b >> 32))
        return false;
    if (pn[0] != (b & 0xfffffffful))
        return false;
    return true;
}

template <unsigned int bits>
double base_uint<bits>::getdouble() const
{
    double ret = 0.0;
    double fact = 1.0;
    for (int i = 0; i < width; i++) {
        ret += fact * pn[i];
        fact *= 4294967296.0;
    }
    return ret;
}

template <unsigned int bits>
std::string base_uint<bits>::gethex() const
{
    return arithtouint256(*this).gethex();
}

template <unsigned int bits>
void base_uint<bits>::sethex(const char* psz)
{
    *this = uinttoarith256(uint256s(psz));
}

template <unsigned int bits>
void base_uint<bits>::sethex(const std::string& str)
{
    sethex(str.c_str());
}

template <unsigned int bits>
std::string base_uint<bits>::tostring() const
{
    return (gethex());
}

template <unsigned int bits>
unsigned int base_uint<bits>::bits() const
{
    for (int pos = width - 1; pos >= 0; pos--) {
        if (pn[pos]) {
            for (int bits = 31; bits > 0; bits--) {
                if (pn[pos] & 1 << bits)
                    return 32 * pos + bits + 1;
            }
            return 32 * pos + 1;
        }
    }
    return 0;
}

// explicit instantiations for base_uint<256>
template base_uint<256>::base_uint(const std::string&);
template base_uint<256>& base_uint<256>::operator<<=(unsigned int);
template base_uint<256>& base_uint<256>::operator>>=(unsigned int);
template base_uint<256>& base_uint<256>::operator*=(uint32_t b32);
template base_uint<256>& base_uint<256>::operator*=(const base_uint<256>& b);
template base_uint<256>& base_uint<256>::operator/=(const base_uint<256>& b);
template int base_uint<256>::compareto(const base_uint<256>&) const;
template bool base_uint<256>::equalto(uint64_t) const;
template double base_uint<256>::getdouble() const;
template std::string base_uint<256>::gethex() const;
template std::string base_uint<256>::tostring() const;
template void base_uint<256>::sethex(const char*);
template void base_uint<256>::sethex(const std::string&);
template unsigned int base_uint<256>::bits() const;

// this implementation directly uses shifts instead of going
// through an intermediate mpi representation.
arith_uint256& arith_uint256::setcompact(uint32_t ncompact, bool* pfnegative, bool* pfoverflow)
{
    int nsize = ncompact >> 24;
    uint32_t nword = ncompact & 0x007fffff;
    if (nsize <= 3) {
        nword >>= 8 * (3 - nsize);
        *this = nword;
    } else {
        *this = nword;
        *this <<= 8 * (nsize - 3);
    }
    if (pfnegative)
        *pfnegative = nword != 0 && (ncompact & 0x00800000) != 0;
    if (pfoverflow)
        *pfoverflow = nword != 0 && ((nsize > 34) ||
                                     (nword > 0xff && nsize > 33) ||
                                     (nword > 0xffff && nsize > 32));
    return *this;
}

uint32_t arith_uint256::getcompact(bool fnegative) const
{
    int nsize = (bits() + 7) / 8;
    uint32_t ncompact = 0;
    if (nsize <= 3) {
        ncompact = getlow64() << 8 * (3 - nsize);
    } else {
        arith_uint256 bn = *this >> 8 * (nsize - 3);
        ncompact = bn.getlow64();
    }
    // the 0x00800000 bit denotes the sign.
    // thus, if it is already set, divide the mantissa by 256 and increase the exponent.
    if (ncompact & 0x00800000) {
        ncompact >>= 8;
        nsize++;
    }
    assert((ncompact & ~0x007fffff) == 0);
    assert(nsize < 256);
    ncompact |= nsize << 24;
    ncompact |= (fnegative && (ncompact & 0x007fffff) ? 0x00800000 : 0);
    return ncompact;
}

uint256 arithtouint256(const arith_uint256 &a)
{
    uint256 b;
    for(int x=0; x<a.width; ++x)
        writele32(b.begin() + x*4, a.pn[x]);
    return b;
}
arith_uint256 uinttoarith256(const uint256 &a)
{
    arith_uint256 b;
    for(int x=0; x<b.width; ++x)
        b.pn[x] = readle32(a.begin() + x*4);
    return b;
}
