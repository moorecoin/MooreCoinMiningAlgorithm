// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/sha1.h"

#include "crypto/common.h"

#include <string.h>

// internal implementation code.
namespace
{
/// internal sha-1 implementation.
namespace sha1
{
/** one round of sha-1. */
void inline round(uint32_t a, uint32_t& b, uint32_t c, uint32_t d, uint32_t& e, uint32_t f, uint32_t k, uint32_t w)
{
    e += ((a << 5) | (a >> 27)) + f + k + w;
    b = (b << 30) | (b >> 2);
}

uint32_t inline f1(uint32_t b, uint32_t c, uint32_t d) { return d ^ (b & (c ^ d)); }
uint32_t inline f2(uint32_t b, uint32_t c, uint32_t d) { return b ^ c ^ d; }
uint32_t inline f3(uint32_t b, uint32_t c, uint32_t d) { return (b & c) | (d & (b | c)); }

uint32_t inline left(uint32_t x) { return (x << 1) | (x >> 31); }

/** initialize sha-1 state. */
void inline initialize(uint32_t* s)
{
    s[0] = 0x67452301ul;
    s[1] = 0xefcdab89ul;
    s[2] = 0x98badcfeul;
    s[3] = 0x10325476ul;
    s[4] = 0xc3d2e1f0ul;
}

const uint32_t k1 = 0x5a827999ul;
const uint32_t k2 = 0x6ed9eba1ul;
const uint32_t k3 = 0x8f1bbcdcul;
const uint32_t k4 = 0xca62c1d6ul;

/** perform a sha-1 transformation, processing a 64-byte chunk. */
void transform(uint32_t* s, const unsigned char* chunk)
{
    uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
    uint32_t w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;

    round(a, b, c, d, e, f1(b, c, d), k1, w0 = readbe32(chunk + 0));
    round(e, a, b, c, d, f1(a, b, c), k1, w1 = readbe32(chunk + 4));
    round(d, e, a, b, c, f1(e, a, b), k1, w2 = readbe32(chunk + 8));
    round(c, d, e, a, b, f1(d, e, a), k1, w3 = readbe32(chunk + 12));
    round(b, c, d, e, a, f1(c, d, e), k1, w4 = readbe32(chunk + 16));
    round(a, b, c, d, e, f1(b, c, d), k1, w5 = readbe32(chunk + 20));
    round(e, a, b, c, d, f1(a, b, c), k1, w6 = readbe32(chunk + 24));
    round(d, e, a, b, c, f1(e, a, b), k1, w7 = readbe32(chunk + 28));
    round(c, d, e, a, b, f1(d, e, a), k1, w8 = readbe32(chunk + 32));
    round(b, c, d, e, a, f1(c, d, e), k1, w9 = readbe32(chunk + 36));
    round(a, b, c, d, e, f1(b, c, d), k1, w10 = readbe32(chunk + 40));
    round(e, a, b, c, d, f1(a, b, c), k1, w11 = readbe32(chunk + 44));
    round(d, e, a, b, c, f1(e, a, b), k1, w12 = readbe32(chunk + 48));
    round(c, d, e, a, b, f1(d, e, a), k1, w13 = readbe32(chunk + 52));
    round(b, c, d, e, a, f1(c, d, e), k1, w14 = readbe32(chunk + 56));
    round(a, b, c, d, e, f1(b, c, d), k1, w15 = readbe32(chunk + 60));

    round(e, a, b, c, d, f1(a, b, c), k1, w0 = left(w0 ^ w13 ^ w8 ^ w2));
    round(d, e, a, b, c, f1(e, a, b), k1, w1 = left(w1 ^ w14 ^ w9 ^ w3));
    round(c, d, e, a, b, f1(d, e, a), k1, w2 = left(w2 ^ w15 ^ w10 ^ w4));
    round(b, c, d, e, a, f1(c, d, e), k1, w3 = left(w3 ^ w0 ^ w11 ^ w5));
    round(a, b, c, d, e, f2(b, c, d), k2, w4 = left(w4 ^ w1 ^ w12 ^ w6));
    round(e, a, b, c, d, f2(a, b, c), k2, w5 = left(w5 ^ w2 ^ w13 ^ w7));
    round(d, e, a, b, c, f2(e, a, b), k2, w6 = left(w6 ^ w3 ^ w14 ^ w8));
    round(c, d, e, a, b, f2(d, e, a), k2, w7 = left(w7 ^ w4 ^ w15 ^ w9));
    round(b, c, d, e, a, f2(c, d, e), k2, w8 = left(w8 ^ w5 ^ w0 ^ w10));
    round(a, b, c, d, e, f2(b, c, d), k2, w9 = left(w9 ^ w6 ^ w1 ^ w11));
    round(e, a, b, c, d, f2(a, b, c), k2, w10 = left(w10 ^ w7 ^ w2 ^ w12));
    round(d, e, a, b, c, f2(e, a, b), k2, w11 = left(w11 ^ w8 ^ w3 ^ w13));
    round(c, d, e, a, b, f2(d, e, a), k2, w12 = left(w12 ^ w9 ^ w4 ^ w14));
    round(b, c, d, e, a, f2(c, d, e), k2, w13 = left(w13 ^ w10 ^ w5 ^ w15));
    round(a, b, c, d, e, f2(b, c, d), k2, w14 = left(w14 ^ w11 ^ w6 ^ w0));
    round(e, a, b, c, d, f2(a, b, c), k2, w15 = left(w15 ^ w12 ^ w7 ^ w1));

    round(d, e, a, b, c, f2(e, a, b), k2, w0 = left(w0 ^ w13 ^ w8 ^ w2));
    round(c, d, e, a, b, f2(d, e, a), k2, w1 = left(w1 ^ w14 ^ w9 ^ w3));
    round(b, c, d, e, a, f2(c, d, e), k2, w2 = left(w2 ^ w15 ^ w10 ^ w4));
    round(a, b, c, d, e, f2(b, c, d), k2, w3 = left(w3 ^ w0 ^ w11 ^ w5));
    round(e, a, b, c, d, f2(a, b, c), k2, w4 = left(w4 ^ w1 ^ w12 ^ w6));
    round(d, e, a, b, c, f2(e, a, b), k2, w5 = left(w5 ^ w2 ^ w13 ^ w7));
    round(c, d, e, a, b, f2(d, e, a), k2, w6 = left(w6 ^ w3 ^ w14 ^ w8));
    round(b, c, d, e, a, f2(c, d, e), k2, w7 = left(w7 ^ w4 ^ w15 ^ w9));
    round(a, b, c, d, e, f3(b, c, d), k3, w8 = left(w8 ^ w5 ^ w0 ^ w10));
    round(e, a, b, c, d, f3(a, b, c), k3, w9 = left(w9 ^ w6 ^ w1 ^ w11));
    round(d, e, a, b, c, f3(e, a, b), k3, w10 = left(w10 ^ w7 ^ w2 ^ w12));
    round(c, d, e, a, b, f3(d, e, a), k3, w11 = left(w11 ^ w8 ^ w3 ^ w13));
    round(b, c, d, e, a, f3(c, d, e), k3, w12 = left(w12 ^ w9 ^ w4 ^ w14));
    round(a, b, c, d, e, f3(b, c, d), k3, w13 = left(w13 ^ w10 ^ w5 ^ w15));
    round(e, a, b, c, d, f3(a, b, c), k3, w14 = left(w14 ^ w11 ^ w6 ^ w0));
    round(d, e, a, b, c, f3(e, a, b), k3, w15 = left(w15 ^ w12 ^ w7 ^ w1));

    round(c, d, e, a, b, f3(d, e, a), k3, w0 = left(w0 ^ w13 ^ w8 ^ w2));
    round(b, c, d, e, a, f3(c, d, e), k3, w1 = left(w1 ^ w14 ^ w9 ^ w3));
    round(a, b, c, d, e, f3(b, c, d), k3, w2 = left(w2 ^ w15 ^ w10 ^ w4));
    round(e, a, b, c, d, f3(a, b, c), k3, w3 = left(w3 ^ w0 ^ w11 ^ w5));
    round(d, e, a, b, c, f3(e, a, b), k3, w4 = left(w4 ^ w1 ^ w12 ^ w6));
    round(c, d, e, a, b, f3(d, e, a), k3, w5 = left(w5 ^ w2 ^ w13 ^ w7));
    round(b, c, d, e, a, f3(c, d, e), k3, w6 = left(w6 ^ w3 ^ w14 ^ w8));
    round(a, b, c, d, e, f3(b, c, d), k3, w7 = left(w7 ^ w4 ^ w15 ^ w9));
    round(e, a, b, c, d, f3(a, b, c), k3, w8 = left(w8 ^ w5 ^ w0 ^ w10));
    round(d, e, a, b, c, f3(e, a, b), k3, w9 = left(w9 ^ w6 ^ w1 ^ w11));
    round(c, d, e, a, b, f3(d, e, a), k3, w10 = left(w10 ^ w7 ^ w2 ^ w12));
    round(b, c, d, e, a, f3(c, d, e), k3, w11 = left(w11 ^ w8 ^ w3 ^ w13));
    round(a, b, c, d, e, f2(b, c, d), k4, w12 = left(w12 ^ w9 ^ w4 ^ w14));
    round(e, a, b, c, d, f2(a, b, c), k4, w13 = left(w13 ^ w10 ^ w5 ^ w15));
    round(d, e, a, b, c, f2(e, a, b), k4, w14 = left(w14 ^ w11 ^ w6 ^ w0));
    round(c, d, e, a, b, f2(d, e, a), k4, w15 = left(w15 ^ w12 ^ w7 ^ w1));

    round(b, c, d, e, a, f2(c, d, e), k4, w0 = left(w0 ^ w13 ^ w8 ^ w2));
    round(a, b, c, d, e, f2(b, c, d), k4, w1 = left(w1 ^ w14 ^ w9 ^ w3));
    round(e, a, b, c, d, f2(a, b, c), k4, w2 = left(w2 ^ w15 ^ w10 ^ w4));
    round(d, e, a, b, c, f2(e, a, b), k4, w3 = left(w3 ^ w0 ^ w11 ^ w5));
    round(c, d, e, a, b, f2(d, e, a), k4, w4 = left(w4 ^ w1 ^ w12 ^ w6));
    round(b, c, d, e, a, f2(c, d, e), k4, w5 = left(w5 ^ w2 ^ w13 ^ w7));
    round(a, b, c, d, e, f2(b, c, d), k4, w6 = left(w6 ^ w3 ^ w14 ^ w8));
    round(e, a, b, c, d, f2(a, b, c), k4, w7 = left(w7 ^ w4 ^ w15 ^ w9));
    round(d, e, a, b, c, f2(e, a, b), k4, w8 = left(w8 ^ w5 ^ w0 ^ w10));
    round(c, d, e, a, b, f2(d, e, a), k4, w9 = left(w9 ^ w6 ^ w1 ^ w11));
    round(b, c, d, e, a, f2(c, d, e), k4, w10 = left(w10 ^ w7 ^ w2 ^ w12));
    round(a, b, c, d, e, f2(b, c, d), k4, w11 = left(w11 ^ w8 ^ w3 ^ w13));
    round(e, a, b, c, d, f2(a, b, c), k4, w12 = left(w12 ^ w9 ^ w4 ^ w14));
    round(d, e, a, b, c, f2(e, a, b), k4, left(w13 ^ w10 ^ w5 ^ w15));
    round(c, d, e, a, b, f2(d, e, a), k4, left(w14 ^ w11 ^ w6 ^ w0));
    round(b, c, d, e, a, f2(c, d, e), k4, left(w15 ^ w12 ^ w7 ^ w1));

    s[0] += a;
    s[1] += b;
    s[2] += c;
    s[3] += d;
    s[4] += e;
}

} // namespace sha1

} // namespace

////// sha1

csha1::csha1() : bytes(0)
{
    sha1::initialize(s);
}

csha1& csha1::write(const unsigned char* data, size_t len)
{
    const unsigned char* end = data + len;
    size_t bufsize = bytes % 64;
    if (bufsize && bufsize + len >= 64) {
        // fill the buffer, and process it.
        memcpy(buf + bufsize, data, 64 - bufsize);
        bytes += 64 - bufsize;
        data += 64 - bufsize;
        sha1::transform(s, buf);
        bufsize = 0;
    }
    while (end >= data + 64) {
        // process full chunks directly from the source.
        sha1::transform(s, data);
        bytes += 64;
        data += 64;
    }
    if (end > data) {
        // fill the buffer with what remains.
        memcpy(buf + bufsize, data, end - data);
        bytes += end - data;
    }
    return *this;
}

void csha1::finalize(unsigned char hash[output_size])
{
    static const unsigned char pad[64] = {0x80};
    unsigned char sizedesc[8];
    writebe64(sizedesc, bytes << 3);
    write(pad, 1 + ((119 - (bytes % 64)) % 64));
    write(sizedesc, 8);
    writebe32(hash, s[0]);
    writebe32(hash + 4, s[1]);
    writebe32(hash + 8, s[2]);
    writebe32(hash + 12, s[3]);
    writebe32(hash + 16, s[4]);
}

csha1& csha1::reset()
{
    bytes = 0;
    sha1::initialize(s);
    return *this;
}
