/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_field_inner5x52_impl_h_
#define _secp256k1_field_inner5x52_impl_h_

#include <stdint.h>

#ifdef verify
#define verify_bits(x, n) verify_check(((x) >> (n)) == 0)
#else
#define verify_bits(x, n) do { } while(0)
#endif

secp256k1_inline static void secp256k1_fe_mul_inner(uint64_t *r, const uint64_t *a, const uint64_t * secp256k1_restrict b) {
    uint128_t c, d;
    uint64_t t3, t4, tx, u0;
    uint64_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], a4 = a[4];
    const uint64_t m = 0xfffffffffffffull, r = 0x1000003d10ull;

    verify_bits(a[0], 56);
    verify_bits(a[1], 56);
    verify_bits(a[2], 56);
    verify_bits(a[3], 56);
    verify_bits(a[4], 52);
    verify_bits(b[0], 56);
    verify_bits(b[1], 56);
    verify_bits(b[2], 56);
    verify_bits(b[3], 56);
    verify_bits(b[4], 52);
    verify_check(r != b);

    /*  [... a b c] is a shorthand for ... + a<<104 + b<<52 + c<<0 mod n.
     *  px is a shorthand for sum(a[i]*b[x-i], i=0..x).
     *  note that [x 0 0 0 0 0] = [x*r].
     */

    d  = (uint128_t)a0 * b[3]
       + (uint128_t)a1 * b[2]
       + (uint128_t)a2 * b[1]
       + (uint128_t)a3 * b[0];
    verify_bits(d, 114);
    /* [d 0 0 0] = [p3 0 0 0] */
    c  = (uint128_t)a4 * b[4];
    verify_bits(c, 112);
    /* [c 0 0 0 0 d 0 0 0] = [p8 0 0 0 0 p3 0 0 0] */
    d += (c & m) * r; c >>= 52;
    verify_bits(d, 115);
    verify_bits(c, 60);
    /* [c 0 0 0 0 0 d 0 0 0] = [p8 0 0 0 0 p3 0 0 0] */
    t3 = d & m; d >>= 52;
    verify_bits(t3, 52);
    verify_bits(d, 63);
    /* [c 0 0 0 0 d t3 0 0 0] = [p8 0 0 0 0 p3 0 0 0] */

    d += (uint128_t)a0 * b[4]
       + (uint128_t)a1 * b[3]
       + (uint128_t)a2 * b[2]
       + (uint128_t)a3 * b[1]
       + (uint128_t)a4 * b[0];
    verify_bits(d, 115);
    /* [c 0 0 0 0 d t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */
    d += c * r;
    verify_bits(d, 116);
    /* [d t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */
    t4 = d & m; d >>= 52;
    verify_bits(t4, 52);
    verify_bits(d, 64);
    /* [d t4 t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */
    tx = (t4 >> 48); t4 &= (m >> 4);
    verify_bits(tx, 4);
    verify_bits(t4, 48);
    /* [d t4+(tx<<48) t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */

    c  = (uint128_t)a0 * b[0];
    verify_bits(c, 112);
    /* [d t4+(tx<<48) t3 0 0 c] = [p8 0 0 0 p4 p3 0 0 p0] */
    d += (uint128_t)a1 * b[4]
       + (uint128_t)a2 * b[3]
       + (uint128_t)a3 * b[2]
       + (uint128_t)a4 * b[1];
    verify_bits(d, 115);
    /* [d t4+(tx<<48) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    u0 = d & m; d >>= 52;
    verify_bits(u0, 52);
    verify_bits(d, 63);
    /* [d u0 t4+(tx<<48) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    /* [d 0 t4+(tx<<48)+(u0<<52) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    u0 = (u0 << 4) | tx;
    verify_bits(u0, 56);
    /* [d 0 t4+(u0<<48) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    c += (uint128_t)u0 * (r >> 4);
    verify_bits(c, 115);
    /* [d 0 t4 t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    r[0] = c & m; c >>= 52;
    verify_bits(r[0], 52);
    verify_bits(c, 61);
    /* [d 0 t4 t3 0 c r0] = [p8 0 0 p5 p4 p3 0 0 p0] */

    c += (uint128_t)a0 * b[1]
       + (uint128_t)a1 * b[0];
    verify_bits(c, 114);
    /* [d 0 t4 t3 0 c r0] = [p8 0 0 p5 p4 p3 0 p1 p0] */
    d += (uint128_t)a2 * b[4]
       + (uint128_t)a3 * b[3]
       + (uint128_t)a4 * b[2];
    verify_bits(d, 114);
    /* [d 0 t4 t3 0 c r0] = [p8 0 p6 p5 p4 p3 0 p1 p0] */
    c += (d & m) * r; d >>= 52;
    verify_bits(c, 115);
    verify_bits(d, 62);
    /* [d 0 0 t4 t3 0 c r0] = [p8 0 p6 p5 p4 p3 0 p1 p0] */
    r[1] = c & m; c >>= 52;
    verify_bits(r[1], 52);
    verify_bits(c, 63);
    /* [d 0 0 t4 t3 c r1 r0] = [p8 0 p6 p5 p4 p3 0 p1 p0] */

    c += (uint128_t)a0 * b[2]
       + (uint128_t)a1 * b[1]
       + (uint128_t)a2 * b[0];
    verify_bits(c, 114);
    /* [d 0 0 t4 t3 c r1 r0] = [p8 0 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint128_t)a3 * b[4]
       + (uint128_t)a4 * b[3];
    verify_bits(d, 114);
    /* [d 0 0 t4 t3 c t1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c += (d & m) * r; d >>= 52;
    verify_bits(c, 115);
    verify_bits(d, 62);
    /* [d 0 0 0 t4 t3 c r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    /* [d 0 0 0 t4 t3 c r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[2] = c & m; c >>= 52;
    verify_bits(r[2], 52);
    verify_bits(c, 63);
    /* [d 0 0 0 t4 t3+c r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c   += d * r + t3;;
    verify_bits(c, 100);
    /* [t4 c r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[3] = c & m; c >>= 52;
    verify_bits(r[3], 52);
    verify_bits(c, 48);
    /* [t4+c r3 r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c   += t4;
    verify_bits(c, 49);
    /* [c r3 r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[4] = c;
    verify_bits(r[4], 49);
    /* [r4 r3 r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
}

secp256k1_inline static void secp256k1_fe_sqr_inner(uint64_t *r, const uint64_t *a) {
    uint128_t c, d;
    uint64_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3], a4 = a[4];
    int64_t t3, t4, tx, u0;
    const uint64_t m = 0xfffffffffffffull, r = 0x1000003d10ull;

    verify_bits(a[0], 56);
    verify_bits(a[1], 56);
    verify_bits(a[2], 56);
    verify_bits(a[3], 56);
    verify_bits(a[4], 52);

    /**  [... a b c] is a shorthand for ... + a<<104 + b<<52 + c<<0 mod n.
     *  px is a shorthand for sum(a[i]*a[x-i], i=0..x).
     *  note that [x 0 0 0 0 0] = [x*r].
     */

    d  = (uint128_t)(a0*2) * a3
       + (uint128_t)(a1*2) * a2;
    verify_bits(d, 114);
    /* [d 0 0 0] = [p3 0 0 0] */
    c  = (uint128_t)a4 * a4;
    verify_bits(c, 112);
    /* [c 0 0 0 0 d 0 0 0] = [p8 0 0 0 0 p3 0 0 0] */
    d += (c & m) * r; c >>= 52;
    verify_bits(d, 115);
    verify_bits(c, 60);
    /* [c 0 0 0 0 0 d 0 0 0] = [p8 0 0 0 0 p3 0 0 0] */
    t3 = d & m; d >>= 52;
    verify_bits(t3, 52);
    verify_bits(d, 63);
    /* [c 0 0 0 0 d t3 0 0 0] = [p8 0 0 0 0 p3 0 0 0] */

    a4 *= 2;
    d += (uint128_t)a0 * a4
       + (uint128_t)(a1*2) * a3
       + (uint128_t)a2 * a2;
    verify_bits(d, 115);
    /* [c 0 0 0 0 d t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */
    d += c * r;
    verify_bits(d, 116);
    /* [d t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */
    t4 = d & m; d >>= 52;
    verify_bits(t4, 52);
    verify_bits(d, 64);
    /* [d t4 t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */
    tx = (t4 >> 48); t4 &= (m >> 4);
    verify_bits(tx, 4);
    verify_bits(t4, 48);
    /* [d t4+(tx<<48) t3 0 0 0] = [p8 0 0 0 p4 p3 0 0 0] */

    c  = (uint128_t)a0 * a0;
    verify_bits(c, 112);
    /* [d t4+(tx<<48) t3 0 0 c] = [p8 0 0 0 p4 p3 0 0 p0] */
    d += (uint128_t)a1 * a4
       + (uint128_t)(a2*2) * a3;
    verify_bits(d, 114);
    /* [d t4+(tx<<48) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    u0 = d & m; d >>= 52;
    verify_bits(u0, 52);
    verify_bits(d, 62);
    /* [d u0 t4+(tx<<48) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    /* [d 0 t4+(tx<<48)+(u0<<52) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    u0 = (u0 << 4) | tx;
    verify_bits(u0, 56);
    /* [d 0 t4+(u0<<48) t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    c += (uint128_t)u0 * (r >> 4);
    verify_bits(c, 113);
    /* [d 0 t4 t3 0 0 c] = [p8 0 0 p5 p4 p3 0 0 p0] */
    r[0] = c & m; c >>= 52;
    verify_bits(r[0], 52);
    verify_bits(c, 61);
    /* [d 0 t4 t3 0 c r0] = [p8 0 0 p5 p4 p3 0 0 p0] */

    a0 *= 2;
    c += (uint128_t)a0 * a1;
    verify_bits(c, 114);
    /* [d 0 t4 t3 0 c r0] = [p8 0 0 p5 p4 p3 0 p1 p0] */
    d += (uint128_t)a2 * a4
       + (uint128_t)a3 * a3;
    verify_bits(d, 114);
    /* [d 0 t4 t3 0 c r0] = [p8 0 p6 p5 p4 p3 0 p1 p0] */
    c += (d & m) * r; d >>= 52;
    verify_bits(c, 115);
    verify_bits(d, 62);
    /* [d 0 0 t4 t3 0 c r0] = [p8 0 p6 p5 p4 p3 0 p1 p0] */
    r[1] = c & m; c >>= 52;
    verify_bits(r[1], 52);
    verify_bits(c, 63);
    /* [d 0 0 t4 t3 c r1 r0] = [p8 0 p6 p5 p4 p3 0 p1 p0] */

    c += (uint128_t)a0 * a2
       + (uint128_t)a1 * a1;
    verify_bits(c, 114);
    /* [d 0 0 t4 t3 c r1 r0] = [p8 0 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint128_t)a3 * a4;
    verify_bits(d, 114);
    /* [d 0 0 t4 t3 c r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c += (d & m) * r; d >>= 52;
    verify_bits(c, 115);
    verify_bits(d, 62);
    /* [d 0 0 0 t4 t3 c r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[2] = c & m; c >>= 52;
    verify_bits(r[2], 52);
    verify_bits(c, 63);
    /* [d 0 0 0 t4 t3+c r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    c   += d * r + t3;;
    verify_bits(c, 100);
    /* [t4 c r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[3] = c & m; c >>= 52;
    verify_bits(r[3], 52);
    verify_bits(c, 48);
    /* [t4+c r3 r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c   += t4;
    verify_bits(c, 49);
    /* [c r3 r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[4] = c;
    verify_bits(r[4], 49);
    /* [r4 r3 r2 r1 r0] = [p8 p7 p6 p5 p4 p3 p2 p1 p0] */
}

#endif
