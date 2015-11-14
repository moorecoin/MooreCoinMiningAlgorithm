// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "uint256.h"

#include "utilstrencodings.h"

#include <stdio.h>
#include <string.h>

template <unsigned int bits>
base_blob<bits>::base_blob(const std::vector<unsigned char>& vch)
{
    assert(vch.size() == sizeof(data));
    memcpy(data, &vch[0], sizeof(data));
}

template <unsigned int bits>
std::string base_blob<bits>::gethex() const
{
    char psz[sizeof(data) * 2 + 1];
    for (unsigned int i = 0; i < sizeof(data); i++)
        sprintf(psz + i * 2, "%02x", data[sizeof(data) - i - 1]);
    return std::string(psz, psz + sizeof(data) * 2);
}

template <unsigned int bits>
void base_blob<bits>::sethex(const char* psz)
{
    memset(data, 0, sizeof(data));

    // skip leading spaces
    while (isspace(*psz))
        psz++;

    // skip 0x
    if (psz[0] == '0' && tolower(psz[1]) == 'x')
        psz += 2;

    // hex string to uint
    const char* pbegin = psz;
    while (::hexdigit(*psz) != -1)
        psz++;
    psz--;
    unsigned char* p1 = (unsigned char*)data;
    unsigned char* pend = p1 + width;
    while (psz >= pbegin && p1 < pend) {
        *p1 = ::hexdigit(*psz--);
        if (psz >= pbegin) {
            *p1 |= ((unsigned char)::hexdigit(*psz--) << 4);
            p1++;
        }
    }
}

template <unsigned int bits>
void base_blob<bits>::sethex(const std::string& str)
{
    sethex(str.c_str());
}

template <unsigned int bits>
std::string base_blob<bits>::tostring() const
{
    return (gethex());
}

// explicit instantiations for base_blob<160>
template base_blob<160>::base_blob(const std::vector<unsigned char>&);
template std::string base_blob<160>::gethex() const;
template std::string base_blob<160>::tostring() const;
template void base_blob<160>::sethex(const char*);
template void base_blob<160>::sethex(const std::string&);

// explicit instantiations for base_blob<256>
template base_blob<256>::base_blob(const std::vector<unsigned char>&);
template std::string base_blob<256>::gethex() const;
template std::string base_blob<256>::tostring() const;
template void base_blob<256>::sethex(const char*);
template void base_blob<256>::sethex(const std::string&);

static void inline hashmix(uint32_t& a, uint32_t& b, uint32_t& c)
{
    // taken from lookup3, by bob jenkins.
    a -= c;
    a ^= ((c << 4) | (c >> 28));
    c += b;
    b -= a;
    b ^= ((a << 6) | (a >> 26));
    a += c;
    c -= b;
    c ^= ((b << 8) | (b >> 24));
    b += a;
    a -= c;
    a ^= ((c << 16) | (c >> 16));
    c += b;
    b -= a;
    b ^= ((a << 19) | (a >> 13));
    a += c;
    c -= b;
    c ^= ((b << 4) | (b >> 28));
    b += a;
}

static void inline hashfinal(uint32_t& a, uint32_t& b, uint32_t& c)
{
    // taken from lookup3, by bob jenkins.
    c ^= b;
    c -= ((b << 14) | (b >> 18));
    a ^= c;
    a -= ((c << 11) | (c >> 21));
    b ^= a;
    b -= ((a << 25) | (a >> 7));
    c ^= b;
    c -= ((b << 16) | (b >> 16));
    a ^= c;
    a -= ((c << 4) | (c >> 28));
    b ^= a;
    b -= ((a << 14) | (a >> 18));
    c ^= b;
    c -= ((b << 24) | (b >> 8));
}

uint64_t uint256::gethash(const uint256& salt) const
{
    uint32_t a, b, c;
    const uint32_t *pn = (const uint32_t*)data;
    const uint32_t *salt_pn = (const uint32_t*)salt.data;
    a = b = c = 0xdeadbeef + width;

    a += pn[0] ^ salt_pn[0];
    b += pn[1] ^ salt_pn[1];
    c += pn[2] ^ salt_pn[2];
    hashmix(a, b, c);
    a += pn[3] ^ salt_pn[3];
    b += pn[4] ^ salt_pn[4];
    c += pn[5] ^ salt_pn[5];
    hashmix(a, b, c);
    a += pn[6] ^ salt_pn[6];
    b += pn[7] ^ salt_pn[7];
    hashfinal(a, b, c);

    return ((((uint64_t)b) << 32) | c);
}
