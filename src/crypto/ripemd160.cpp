// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/ripemd160.h"

#include "crypto/common.h"

#include <string.h>

// internal implementation code.
namespace
{
/// internal ripemd-160 implementation.
namespace ripemd160
{
uint32_t inline f1(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
uint32_t inline f2(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
uint32_t inline f3(uint32_t x, uint32_t y, uint32_t z) { return (x | ~y) ^ z; }
uint32_t inline f4(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
uint32_t inline f5(uint32_t x, uint32_t y, uint32_t z) { return x ^ (y | ~z); }

/** initialize ripemd-160 state. */
void inline initialize(uint32_t* s)
{
    s[0] = 0x67452301ul;
    s[1] = 0xefcdab89ul;
    s[2] = 0x98badcfeul;
    s[3] = 0x10325476ul;
    s[4] = 0xc3d2e1f0ul;
}

uint32_t inline rol(uint32_t x, int i) { return (x << i) | (x >> (32 - i)); }

void inline round(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t f, uint32_t x, uint32_t k, int r)
{
    a = rol(a + f + x + k, r) + e;
    c = rol(c, 10);
}

void inline r11(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f1(b, c, d), x, 0, r); }
void inline r21(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f2(b, c, d), x, 0x5a827999ul, r); }
void inline r31(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f3(b, c, d), x, 0x6ed9eba1ul, r); }
void inline r41(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f4(b, c, d), x, 0x8f1bbcdcul, r); }
void inline r51(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f5(b, c, d), x, 0xa953fd4eul, r); }

void inline r12(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f5(b, c, d), x, 0x50a28be6ul, r); }
void inline r22(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f4(b, c, d), x, 0x5c4dd124ul, r); }
void inline r32(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f3(b, c, d), x, 0x6d703ef3ul, r); }
void inline r42(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f2(b, c, d), x, 0x7a6d76e9ul, r); }
void inline r52(uint32_t& a, uint32_t b, uint32_t& c, uint32_t d, uint32_t e, uint32_t x, int r) { round(a, b, c, d, e, f1(b, c, d), x, 0, r); }

/** perform a ripemd-160 transformation, processing a 64-byte chunk. */
void transform(uint32_t* s, const unsigned char* chunk)
{
    uint32_t a1 = s[0], b1 = s[1], c1 = s[2], d1 = s[3], e1 = s[4];
    uint32_t a2 = a1, b2 = b1, c2 = c1, d2 = d1, e2 = e1;
    uint32_t w0 = readle32(chunk + 0), w1 = readle32(chunk + 4), w2 = readle32(chunk + 8), w3 = readle32(chunk + 12);
    uint32_t w4 = readle32(chunk + 16), w5 = readle32(chunk + 20), w6 = readle32(chunk + 24), w7 = readle32(chunk + 28);
    uint32_t w8 = readle32(chunk + 32), w9 = readle32(chunk + 36), w10 = readle32(chunk + 40), w11 = readle32(chunk + 44);
    uint32_t w12 = readle32(chunk + 48), w13 = readle32(chunk + 52), w14 = readle32(chunk + 56), w15 = readle32(chunk + 60);

    r11(a1, b1, c1, d1, e1, w0, 11);
    r12(a2, b2, c2, d2, e2, w5, 8);
    r11(e1, a1, b1, c1, d1, w1, 14);
    r12(e2, a2, b2, c2, d2, w14, 9);
    r11(d1, e1, a1, b1, c1, w2, 15);
    r12(d2, e2, a2, b2, c2, w7, 9);
    r11(c1, d1, e1, a1, b1, w3, 12);
    r12(c2, d2, e2, a2, b2, w0, 11);
    r11(b1, c1, d1, e1, a1, w4, 5);
    r12(b2, c2, d2, e2, a2, w9, 13);
    r11(a1, b1, c1, d1, e1, w5, 8);
    r12(a2, b2, c2, d2, e2, w2, 15);
    r11(e1, a1, b1, c1, d1, w6, 7);
    r12(e2, a2, b2, c2, d2, w11, 15);
    r11(d1, e1, a1, b1, c1, w7, 9);
    r12(d2, e2, a2, b2, c2, w4, 5);
    r11(c1, d1, e1, a1, b1, w8, 11);
    r12(c2, d2, e2, a2, b2, w13, 7);
    r11(b1, c1, d1, e1, a1, w9, 13);
    r12(b2, c2, d2, e2, a2, w6, 7);
    r11(a1, b1, c1, d1, e1, w10, 14);
    r12(a2, b2, c2, d2, e2, w15, 8);
    r11(e1, a1, b1, c1, d1, w11, 15);
    r12(e2, a2, b2, c2, d2, w8, 11);
    r11(d1, e1, a1, b1, c1, w12, 6);
    r12(d2, e2, a2, b2, c2, w1, 14);
    r11(c1, d1, e1, a1, b1, w13, 7);
    r12(c2, d2, e2, a2, b2, w10, 14);
    r11(b1, c1, d1, e1, a1, w14, 9);
    r12(b2, c2, d2, e2, a2, w3, 12);
    r11(a1, b1, c1, d1, e1, w15, 8);
    r12(a2, b2, c2, d2, e2, w12, 6);

    r21(e1, a1, b1, c1, d1, w7, 7);
    r22(e2, a2, b2, c2, d2, w6, 9);
    r21(d1, e1, a1, b1, c1, w4, 6);
    r22(d2, e2, a2, b2, c2, w11, 13);
    r21(c1, d1, e1, a1, b1, w13, 8);
    r22(c2, d2, e2, a2, b2, w3, 15);
    r21(b1, c1, d1, e1, a1, w1, 13);
    r22(b2, c2, d2, e2, a2, w7, 7);
    r21(a1, b1, c1, d1, e1, w10, 11);
    r22(a2, b2, c2, d2, e2, w0, 12);
    r21(e1, a1, b1, c1, d1, w6, 9);
    r22(e2, a2, b2, c2, d2, w13, 8);
    r21(d1, e1, a1, b1, c1, w15, 7);
    r22(d2, e2, a2, b2, c2, w5, 9);
    r21(c1, d1, e1, a1, b1, w3, 15);
    r22(c2, d2, e2, a2, b2, w10, 11);
    r21(b1, c1, d1, e1, a1, w12, 7);
    r22(b2, c2, d2, e2, a2, w14, 7);
    r21(a1, b1, c1, d1, e1, w0, 12);
    r22(a2, b2, c2, d2, e2, w15, 7);
    r21(e1, a1, b1, c1, d1, w9, 15);
    r22(e2, a2, b2, c2, d2, w8, 12);
    r21(d1, e1, a1, b1, c1, w5, 9);
    r22(d2, e2, a2, b2, c2, w12, 7);
    r21(c1, d1, e1, a1, b1, w2, 11);
    r22(c2, d2, e2, a2, b2, w4, 6);
    r21(b1, c1, d1, e1, a1, w14, 7);
    r22(b2, c2, d2, e2, a2, w9, 15);
    r21(a1, b1, c1, d1, e1, w11, 13);
    r22(a2, b2, c2, d2, e2, w1, 13);
    r21(e1, a1, b1, c1, d1, w8, 12);
    r22(e2, a2, b2, c2, d2, w2, 11);

    r31(d1, e1, a1, b1, c1, w3, 11);
    r32(d2, e2, a2, b2, c2, w15, 9);
    r31(c1, d1, e1, a1, b1, w10, 13);
    r32(c2, d2, e2, a2, b2, w5, 7);
    r31(b1, c1, d1, e1, a1, w14, 6);
    r32(b2, c2, d2, e2, a2, w1, 15);
    r31(a1, b1, c1, d1, e1, w4, 7);
    r32(a2, b2, c2, d2, e2, w3, 11);
    r31(e1, a1, b1, c1, d1, w9, 14);
    r32(e2, a2, b2, c2, d2, w7, 8);
    r31(d1, e1, a1, b1, c1, w15, 9);
    r32(d2, e2, a2, b2, c2, w14, 6);
    r31(c1, d1, e1, a1, b1, w8, 13);
    r32(c2, d2, e2, a2, b2, w6, 6);
    r31(b1, c1, d1, e1, a1, w1, 15);
    r32(b2, c2, d2, e2, a2, w9, 14);
    r31(a1, b1, c1, d1, e1, w2, 14);
    r32(a2, b2, c2, d2, e2, w11, 12);
    r31(e1, a1, b1, c1, d1, w7, 8);
    r32(e2, a2, b2, c2, d2, w8, 13);
    r31(d1, e1, a1, b1, c1, w0, 13);
    r32(d2, e2, a2, b2, c2, w12, 5);
    r31(c1, d1, e1, a1, b1, w6, 6);
    r32(c2, d2, e2, a2, b2, w2, 14);
    r31(b1, c1, d1, e1, a1, w13, 5);
    r32(b2, c2, d2, e2, a2, w10, 13);
    r31(a1, b1, c1, d1, e1, w11, 12);
    r32(a2, b2, c2, d2, e2, w0, 13);
    r31(e1, a1, b1, c1, d1, w5, 7);
    r32(e2, a2, b2, c2, d2, w4, 7);
    r31(d1, e1, a1, b1, c1, w12, 5);
    r32(d2, e2, a2, b2, c2, w13, 5);

    r41(c1, d1, e1, a1, b1, w1, 11);
    r42(c2, d2, e2, a2, b2, w8, 15);
    r41(b1, c1, d1, e1, a1, w9, 12);
    r42(b2, c2, d2, e2, a2, w6, 5);
    r41(a1, b1, c1, d1, e1, w11, 14);
    r42(a2, b2, c2, d2, e2, w4, 8);
    r41(e1, a1, b1, c1, d1, w10, 15);
    r42(e2, a2, b2, c2, d2, w1, 11);
    r41(d1, e1, a1, b1, c1, w0, 14);
    r42(d2, e2, a2, b2, c2, w3, 14);
    r41(c1, d1, e1, a1, b1, w8, 15);
    r42(c2, d2, e2, a2, b2, w11, 14);
    r41(b1, c1, d1, e1, a1, w12, 9);
    r42(b2, c2, d2, e2, a2, w15, 6);
    r41(a1, b1, c1, d1, e1, w4, 8);
    r42(a2, b2, c2, d2, e2, w0, 14);
    r41(e1, a1, b1, c1, d1, w13, 9);
    r42(e2, a2, b2, c2, d2, w5, 6);
    r41(d1, e1, a1, b1, c1, w3, 14);
    r42(d2, e2, a2, b2, c2, w12, 9);
    r41(c1, d1, e1, a1, b1, w7, 5);
    r42(c2, d2, e2, a2, b2, w2, 12);
    r41(b1, c1, d1, e1, a1, w15, 6);
    r42(b2, c2, d2, e2, a2, w13, 9);
    r41(a1, b1, c1, d1, e1, w14, 8);
    r42(a2, b2, c2, d2, e2, w9, 12);
    r41(e1, a1, b1, c1, d1, w5, 6);
    r42(e2, a2, b2, c2, d2, w7, 5);
    r41(d1, e1, a1, b1, c1, w6, 5);
    r42(d2, e2, a2, b2, c2, w10, 15);
    r41(c1, d1, e1, a1, b1, w2, 12);
    r42(c2, d2, e2, a2, b2, w14, 8);

    r51(b1, c1, d1, e1, a1, w4, 9);
    r52(b2, c2, d2, e2, a2, w12, 8);
    r51(a1, b1, c1, d1, e1, w0, 15);
    r52(a2, b2, c2, d2, e2, w15, 5);
    r51(e1, a1, b1, c1, d1, w5, 5);
    r52(e2, a2, b2, c2, d2, w10, 12);
    r51(d1, e1, a1, b1, c1, w9, 11);
    r52(d2, e2, a2, b2, c2, w4, 9);
    r51(c1, d1, e1, a1, b1, w7, 6);
    r52(c2, d2, e2, a2, b2, w1, 12);
    r51(b1, c1, d1, e1, a1, w12, 8);
    r52(b2, c2, d2, e2, a2, w5, 5);
    r51(a1, b1, c1, d1, e1, w2, 13);
    r52(a2, b2, c2, d2, e2, w8, 14);
    r51(e1, a1, b1, c1, d1, w10, 12);
    r52(e2, a2, b2, c2, d2, w7, 6);
    r51(d1, e1, a1, b1, c1, w14, 5);
    r52(d2, e2, a2, b2, c2, w6, 8);
    r51(c1, d1, e1, a1, b1, w1, 12);
    r52(c2, d2, e2, a2, b2, w2, 13);
    r51(b1, c1, d1, e1, a1, w3, 13);
    r52(b2, c2, d2, e2, a2, w13, 6);
    r51(a1, b1, c1, d1, e1, w8, 14);
    r52(a2, b2, c2, d2, e2, w14, 5);
    r51(e1, a1, b1, c1, d1, w11, 11);
    r52(e2, a2, b2, c2, d2, w0, 15);
    r51(d1, e1, a1, b1, c1, w6, 8);
    r52(d2, e2, a2, b2, c2, w3, 13);
    r51(c1, d1, e1, a1, b1, w15, 5);
    r52(c2, d2, e2, a2, b2, w9, 11);
    r51(b1, c1, d1, e1, a1, w13, 6);
    r52(b2, c2, d2, e2, a2, w11, 11);

    uint32_t t = s[0];
    s[0] = s[1] + c1 + d2;
    s[1] = s[2] + d1 + e2;
    s[2] = s[3] + e1 + a2;
    s[3] = s[4] + a1 + b2;
    s[4] = t + b1 + c2;
}

} // namespace ripemd160

} // namespace

////// ripemd160

cripemd160::cripemd160() : bytes(0)
{
    ripemd160::initialize(s);
}

cripemd160& cripemd160::write(const unsigned char* data, size_t len)
{
    const unsigned char* end = data + len;
    size_t bufsize = bytes % 64;
    if (bufsize && bufsize + len >= 64) {
        // fill the buffer, and process it.
        memcpy(buf + bufsize, data, 64 - bufsize);
        bytes += 64 - bufsize;
        data += 64 - bufsize;
        ripemd160::transform(s, buf);
        bufsize = 0;
    }
    while (end >= data + 64) {
        // process full chunks directly from the source.
        ripemd160::transform(s, data);
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

void cripemd160::finalize(unsigned char hash[output_size])
{
    static const unsigned char pad[64] = {0x80};
    unsigned char sizedesc[8];
    writele64(sizedesc, bytes << 3);
    write(pad, 1 + ((119 - (bytes % 64)) % 64));
    write(sizedesc, 8);
    writele32(hash, s[0]);
    writele32(hash + 4, s[1]);
    writele32(hash + 8, s[2]);
    writele32(hash + 12, s[3]);
    writele32(hash + 16, s[4]);
}

cripemd160& cripemd160::reset()
{
    bytes = 0;
    ripemd160::initialize(s);
    return *this;
}
