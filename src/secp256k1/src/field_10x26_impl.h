/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_field_repr_impl_h_
#define _secp256k1_field_repr_impl_h_

#include <stdio.h>
#include <string.h>
#include "util.h"
#include "num.h"
#include "field.h"

#ifdef verify
static void secp256k1_fe_verify(const secp256k1_fe_t *a) {
    const uint32_t *d = a->n;
    int m = a->normalized ? 1 : 2 * a->magnitude, r = 1;
    r &= (d[0] <= 0x3fffffful * m);
    r &= (d[1] <= 0x3fffffful * m);
    r &= (d[2] <= 0x3fffffful * m);
    r &= (d[3] <= 0x3fffffful * m);
    r &= (d[4] <= 0x3fffffful * m);
    r &= (d[5] <= 0x3fffffful * m);
    r &= (d[6] <= 0x3fffffful * m);
    r &= (d[7] <= 0x3fffffful * m);
    r &= (d[8] <= 0x3fffffful * m);
    r &= (d[9] <= 0x03ffffful * m);
    r &= (a->magnitude >= 0);
    r &= (a->magnitude <= 32);
    if (a->normalized) {
        r &= (a->magnitude <= 1);
        if (r && (d[9] == 0x03ffffful)) {
            uint32_t mid = d[8] & d[7] & d[6] & d[5] & d[4] & d[3] & d[2];
            if (mid == 0x3fffffful) {
                r &= ((d[1] + 0x40ul + ((d[0] + 0x3d1ul) >> 26)) <= 0x3fffffful);
            }
        }
    }
    verify_check(r == 1);
}
#else
static void secp256k1_fe_verify(const secp256k1_fe_t *a) {
    (void)a;
}
#endif

static void secp256k1_fe_normalize(secp256k1_fe_t *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /* reduce t9 at the start so there will be at most a single carry from the first pass */
    uint32_t m;
    uint32_t x = t9 >> 22; t9 &= 0x03ffffful;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x3d1ul; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3fffffful;
    t2 += (t1 >> 26); t1 &= 0x3fffffful;
    t3 += (t2 >> 26); t2 &= 0x3fffffful; m = t2;
    t4 += (t3 >> 26); t3 &= 0x3fffffful; m &= t3;
    t5 += (t4 >> 26); t4 &= 0x3fffffful; m &= t4;
    t6 += (t5 >> 26); t5 &= 0x3fffffful; m &= t5;
    t7 += (t6 >> 26); t6 &= 0x3fffffful; m &= t6;
    t8 += (t7 >> 26); t7 &= 0x3fffffful; m &= t7;
    t9 += (t8 >> 26); t8 &= 0x3fffffful; m &= t8;

    /* ... except for a possible carry at bit 22 of t9 (i.e. bit 256 of the field element) */
    verify_check(t9 >> 23 == 0);

    /* at most a single final reduction is needed; check if the value is >= the field characteristic */
    x = (t9 >> 22) | ((t9 == 0x03ffffful) & (m == 0x3fffffful)
        & ((t1 + 0x40ul + ((t0 + 0x3d1ul) >> 26)) > 0x3fffffful));

    /* apply the final reduction (for constant-time behaviour, we do it always) */
    t0 += x * 0x3d1ul; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3fffffful;
    t2 += (t1 >> 26); t1 &= 0x3fffffful;
    t3 += (t2 >> 26); t2 &= 0x3fffffful;
    t4 += (t3 >> 26); t3 &= 0x3fffffful;
    t5 += (t4 >> 26); t4 &= 0x3fffffful;
    t6 += (t5 >> 26); t5 &= 0x3fffffful;
    t7 += (t6 >> 26); t6 &= 0x3fffffful;
    t8 += (t7 >> 26); t7 &= 0x3fffffful;
    t9 += (t8 >> 26); t8 &= 0x3fffffful;

    /* if t9 didn't carry to bit 22 already, then it should have after any final reduction */
    verify_check(t9 >> 22 == x);

    /* mask off the possible multiple of 2^256 from the final reduction */
    t9 &= 0x03ffffful;

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;
    r->n[5] = t5; r->n[6] = t6; r->n[7] = t7; r->n[8] = t8; r->n[9] = t9;

#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_normalize_weak(secp256k1_fe_t *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /* reduce t9 at the start so there will be at most a single carry from the first pass */
    uint32_t x = t9 >> 22; t9 &= 0x03ffffful;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x3d1ul; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3fffffful;
    t2 += (t1 >> 26); t1 &= 0x3fffffful;
    t3 += (t2 >> 26); t2 &= 0x3fffffful;
    t4 += (t3 >> 26); t3 &= 0x3fffffful;
    t5 += (t4 >> 26); t4 &= 0x3fffffful;
    t6 += (t5 >> 26); t5 &= 0x3fffffful;
    t7 += (t6 >> 26); t6 &= 0x3fffffful;
    t8 += (t7 >> 26); t7 &= 0x3fffffful;
    t9 += (t8 >> 26); t8 &= 0x3fffffful;

    /* ... except for a possible carry at bit 22 of t9 (i.e. bit 256 of the field element) */
    verify_check(t9 >> 23 == 0);

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;
    r->n[5] = t5; r->n[6] = t6; r->n[7] = t7; r->n[8] = t8; r->n[9] = t9;

#ifdef verify
    r->magnitude = 1;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_normalize_var(secp256k1_fe_t *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /* reduce t9 at the start so there will be at most a single carry from the first pass */
    uint32_t m;
    uint32_t x = t9 >> 22; t9 &= 0x03ffffful;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x3d1ul; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3fffffful;
    t2 += (t1 >> 26); t1 &= 0x3fffffful;
    t3 += (t2 >> 26); t2 &= 0x3fffffful; m = t2;
    t4 += (t3 >> 26); t3 &= 0x3fffffful; m &= t3;
    t5 += (t4 >> 26); t4 &= 0x3fffffful; m &= t4;
    t6 += (t5 >> 26); t5 &= 0x3fffffful; m &= t5;
    t7 += (t6 >> 26); t6 &= 0x3fffffful; m &= t6;
    t8 += (t7 >> 26); t7 &= 0x3fffffful; m &= t7;
    t9 += (t8 >> 26); t8 &= 0x3fffffful; m &= t8;

    /* ... except for a possible carry at bit 22 of t9 (i.e. bit 256 of the field element) */
    verify_check(t9 >> 23 == 0);

    /* at most a single final reduction is needed; check if the value is >= the field characteristic */
    x = (t9 >> 22) | ((t9 == 0x03ffffful) & (m == 0x3fffffful)
        & ((t1 + 0x40ul + ((t0 + 0x3d1ul) >> 26)) > 0x3fffffful));

    if (x) {
        t0 += 0x3d1ul; t1 += (x << 6);
        t1 += (t0 >> 26); t0 &= 0x3fffffful;
        t2 += (t1 >> 26); t1 &= 0x3fffffful;
        t3 += (t2 >> 26); t2 &= 0x3fffffful;
        t4 += (t3 >> 26); t3 &= 0x3fffffful;
        t5 += (t4 >> 26); t4 &= 0x3fffffful;
        t6 += (t5 >> 26); t5 &= 0x3fffffful;
        t7 += (t6 >> 26); t6 &= 0x3fffffful;
        t8 += (t7 >> 26); t7 &= 0x3fffffful;
        t9 += (t8 >> 26); t8 &= 0x3fffffful;

        /* if t9 didn't carry to bit 22 already, then it should have after any final reduction */
        verify_check(t9 >> 22 == x);

        /* mask off the possible multiple of 2^256 from the final reduction */
        t9 &= 0x03ffffful;
    }

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;
    r->n[5] = t5; r->n[6] = t6; r->n[7] = t7; r->n[8] = t8; r->n[9] = t9;

#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

static int secp256k1_fe_normalizes_to_zero(secp256k1_fe_t *r) {
    uint32_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4],
             t5 = r->n[5], t6 = r->n[6], t7 = r->n[7], t8 = r->n[8], t9 = r->n[9];

    /* z0 tracks a possible raw value of 0, z1 tracks a possible raw value of p */
    uint32_t z0, z1;

    /* reduce t9 at the start so there will be at most a single carry from the first pass */
    uint32_t x = t9 >> 22; t9 &= 0x03ffffful;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x3d1ul; t1 += (x << 6);
    t1 += (t0 >> 26); t0 &= 0x3fffffful; z0  = t0; z1  = t0 ^ 0x3d0ul;
    t2 += (t1 >> 26); t1 &= 0x3fffffful; z0 |= t1; z1 &= t1 ^ 0x40ul;
    t3 += (t2 >> 26); t2 &= 0x3fffffful; z0 |= t2; z1 &= t2;
    t4 += (t3 >> 26); t3 &= 0x3fffffful; z0 |= t3; z1 &= t3;
    t5 += (t4 >> 26); t4 &= 0x3fffffful; z0 |= t4; z1 &= t4;
    t6 += (t5 >> 26); t5 &= 0x3fffffful; z0 |= t5; z1 &= t5;
    t7 += (t6 >> 26); t6 &= 0x3fffffful; z0 |= t6; z1 &= t6;
    t8 += (t7 >> 26); t7 &= 0x3fffffful; z0 |= t7; z1 &= t7;
    t9 += (t8 >> 26); t8 &= 0x3fffffful; z0 |= t8; z1 &= t8;
                                         z0 |= t9; z1 &= t9 ^ 0x3c00000ul;

    /* ... except for a possible carry at bit 22 of t9 (i.e. bit 256 of the field element) */
    verify_check(t9 >> 23 == 0);

    return (z0 == 0) | (z1 == 0x3fffffful);
}

static int secp256k1_fe_normalizes_to_zero_var(secp256k1_fe_t *r) {
    uint32_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    uint32_t z0, z1;
    uint32_t x;

    t0 = r->n[0];
    t9 = r->n[9];

    /* reduce t9 at the start so there will be at most a single carry from the first pass */
    x = t9 >> 22;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x3d1ul;

    /* z0 tracks a possible raw value of 0, z1 tracks a possible raw value of p */
    z0 = t0 & 0x3fffffful;
    z1 = z0 ^ 0x3d0ul;

    /* fast return path should catch the majority of cases */
    if ((z0 != 0ul) & (z1 != 0x3fffffful)) {
        return 0;
    }

    t1 = r->n[1];
    t2 = r->n[2];
    t3 = r->n[3];
    t4 = r->n[4];
    t5 = r->n[5];
    t6 = r->n[6];
    t7 = r->n[7];
    t8 = r->n[8];

    t9 &= 0x03ffffful;
    t1 += (x << 6);

    t1 += (t0 >> 26); t0  = z0;
    t2 += (t1 >> 26); t1 &= 0x3fffffful; z0 |= t1; z1 &= t1 ^ 0x40ul;
    t3 += (t2 >> 26); t2 &= 0x3fffffful; z0 |= t2; z1 &= t2;
    t4 += (t3 >> 26); t3 &= 0x3fffffful; z0 |= t3; z1 &= t3;
    t5 += (t4 >> 26); t4 &= 0x3fffffful; z0 |= t4; z1 &= t4;
    t6 += (t5 >> 26); t5 &= 0x3fffffful; z0 |= t5; z1 &= t5;
    t7 += (t6 >> 26); t6 &= 0x3fffffful; z0 |= t6; z1 &= t6;
    t8 += (t7 >> 26); t7 &= 0x3fffffful; z0 |= t7; z1 &= t7;
    t9 += (t8 >> 26); t8 &= 0x3fffffful; z0 |= t8; z1 &= t8;
                                         z0 |= t9; z1 &= t9 ^ 0x3c00000ul;

    /* ... except for a possible carry at bit 22 of t9 (i.e. bit 256 of the field element) */
    verify_check(t9 >> 23 == 0);

    return (z0 == 0) | (z1 == 0x3fffffful);
}

secp256k1_inline static void secp256k1_fe_set_int(secp256k1_fe_t *r, int a) {
    r->n[0] = a;
    r->n[1] = r->n[2] = r->n[3] = r->n[4] = r->n[5] = r->n[6] = r->n[7] = r->n[8] = r->n[9] = 0;
#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

secp256k1_inline static int secp256k1_fe_is_zero(const secp256k1_fe_t *a) {
    const uint32_t *t = a->n;
#ifdef verify
    verify_check(a->normalized);
    secp256k1_fe_verify(a);
#endif
    return (t[0] | t[1] | t[2] | t[3] | t[4] | t[5] | t[6] | t[7] | t[8] | t[9]) == 0;
}

secp256k1_inline static int secp256k1_fe_is_odd(const secp256k1_fe_t *a) {
#ifdef verify
    verify_check(a->normalized);
    secp256k1_fe_verify(a);
#endif
    return a->n[0] & 1;
}

secp256k1_inline static void secp256k1_fe_clear(secp256k1_fe_t *a) {
    int i;
#ifdef verify
    a->magnitude = 0;
    a->normalized = 1;
#endif
    for (i=0; i<10; i++) {
        a->n[i] = 0;
    }
}

static int secp256k1_fe_cmp_var(const secp256k1_fe_t *a, const secp256k1_fe_t *b) {
    int i;
#ifdef verify
    verify_check(a->normalized);
    verify_check(b->normalized);
    secp256k1_fe_verify(a);
    secp256k1_fe_verify(b);
#endif
    for (i = 9; i >= 0; i--) {
        if (a->n[i] > b->n[i]) {
            return 1;
        }
        if (a->n[i] < b->n[i]) {
            return -1;
        }
    }
    return 0;
}

static int secp256k1_fe_set_b32(secp256k1_fe_t *r, const unsigned char *a) {
    int i;
    r->n[0] = r->n[1] = r->n[2] = r->n[3] = r->n[4] = 0;
    r->n[5] = r->n[6] = r->n[7] = r->n[8] = r->n[9] = 0;
    for (i=0; i<32; i++) {
        int j;
        for (j=0; j<4; j++) {
            int limb = (8*i+2*j)/26;
            int shift = (8*i+2*j)%26;
            r->n[limb] |= (uint32_t)((a[31-i] >> (2*j)) & 0x3) << shift;
        }
    }
    if (r->n[9] == 0x3ffffful && (r->n[8] & r->n[7] & r->n[6] & r->n[5] & r->n[4] & r->n[3] & r->n[2]) == 0x3fffffful && (r->n[1] + 0x40ul + ((r->n[0] + 0x3d1ul) >> 26)) > 0x3fffffful) {
        return 0;
    }
#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
    return 1;
}

/** convert a field element to a 32-byte big endian value. requires the input to be normalized */
static void secp256k1_fe_get_b32(unsigned char *r, const secp256k1_fe_t *a) {
    int i;
#ifdef verify
    verify_check(a->normalized);
    secp256k1_fe_verify(a);
#endif
    for (i=0; i<32; i++) {
        int j;
        int c = 0;
        for (j=0; j<4; j++) {
            int limb = (8*i+2*j)/26;
            int shift = (8*i+2*j)%26;
            c |= ((a->n[limb] >> shift) & 0x3) << (2 * j);
        }
        r[31-i] = c;
    }
}

secp256k1_inline static void secp256k1_fe_negate(secp256k1_fe_t *r, const secp256k1_fe_t *a, int m) {
#ifdef verify
    verify_check(a->magnitude <= m);
    secp256k1_fe_verify(a);
#endif
    r->n[0] = 0x3fffc2ful * 2 * (m + 1) - a->n[0];
    r->n[1] = 0x3ffffbful * 2 * (m + 1) - a->n[1];
    r->n[2] = 0x3fffffful * 2 * (m + 1) - a->n[2];
    r->n[3] = 0x3fffffful * 2 * (m + 1) - a->n[3];
    r->n[4] = 0x3fffffful * 2 * (m + 1) - a->n[4];
    r->n[5] = 0x3fffffful * 2 * (m + 1) - a->n[5];
    r->n[6] = 0x3fffffful * 2 * (m + 1) - a->n[6];
    r->n[7] = 0x3fffffful * 2 * (m + 1) - a->n[7];
    r->n[8] = 0x3fffffful * 2 * (m + 1) - a->n[8];
    r->n[9] = 0x03ffffful * 2 * (m + 1) - a->n[9];
#ifdef verify
    r->magnitude = m + 1;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

secp256k1_inline static void secp256k1_fe_mul_int(secp256k1_fe_t *r, int a) {
    r->n[0] *= a;
    r->n[1] *= a;
    r->n[2] *= a;
    r->n[3] *= a;
    r->n[4] *= a;
    r->n[5] *= a;
    r->n[6] *= a;
    r->n[7] *= a;
    r->n[8] *= a;
    r->n[9] *= a;
#ifdef verify
    r->magnitude *= a;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

secp256k1_inline static void secp256k1_fe_add(secp256k1_fe_t *r, const secp256k1_fe_t *a) {
#ifdef verify
    secp256k1_fe_verify(a);
#endif
    r->n[0] += a->n[0];
    r->n[1] += a->n[1];
    r->n[2] += a->n[2];
    r->n[3] += a->n[3];
    r->n[4] += a->n[4];
    r->n[5] += a->n[5];
    r->n[6] += a->n[6];
    r->n[7] += a->n[7];
    r->n[8] += a->n[8];
    r->n[9] += a->n[9];
#ifdef verify
    r->magnitude += a->magnitude;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

#ifdef verify
#define verify_bits(x, n) verify_check(((x) >> (n)) == 0)
#else
#define verify_bits(x, n) do { } while(0)
#endif

secp256k1_inline static void secp256k1_fe_mul_inner(uint32_t *r, const uint32_t *a, const uint32_t * secp256k1_restrict b) {
    uint64_t c, d;
    uint64_t u0, u1, u2, u3, u4, u5, u6, u7, u8;
    uint32_t t9, t1, t0, t2, t3, t4, t5, t6, t7;
    const uint32_t m = 0x3fffffful, r0 = 0x3d10ul, r1 = 0x400ul;

    verify_bits(a[0], 30);
    verify_bits(a[1], 30);
    verify_bits(a[2], 30);
    verify_bits(a[3], 30);
    verify_bits(a[4], 30);
    verify_bits(a[5], 30);
    verify_bits(a[6], 30);
    verify_bits(a[7], 30);
    verify_bits(a[8], 30);
    verify_bits(a[9], 26);
    verify_bits(b[0], 30);
    verify_bits(b[1], 30);
    verify_bits(b[2], 30);
    verify_bits(b[3], 30);
    verify_bits(b[4], 30);
    verify_bits(b[5], 30);
    verify_bits(b[6], 30);
    verify_bits(b[7], 30);
    verify_bits(b[8], 30);
    verify_bits(b[9], 26);

    /** [... a b c] is a shorthand for ... + a<<52 + b<<26 + c<<0 mod n.
     *  px is a shorthand for sum(a[i]*b[x-i], i=0..x).
     *  note that [x 0 0 0 0 0 0 0 0 0 0] = [x*r1 x*r0].
     */

    d  = (uint64_t)a[0] * b[9]
       + (uint64_t)a[1] * b[8]
       + (uint64_t)a[2] * b[7]
       + (uint64_t)a[3] * b[6]
       + (uint64_t)a[4] * b[5]
       + (uint64_t)a[5] * b[4]
       + (uint64_t)a[6] * b[3]
       + (uint64_t)a[7] * b[2]
       + (uint64_t)a[8] * b[1]
       + (uint64_t)a[9] * b[0];
    /* verify_bits(d, 64); */
    /* [d 0 0 0 0 0 0 0 0 0] = [p9 0 0 0 0 0 0 0 0 0] */
    t9 = d & m; d >>= 26;
    verify_bits(t9, 26);
    verify_bits(d, 38);
    /* [d t9 0 0 0 0 0 0 0 0 0] = [p9 0 0 0 0 0 0 0 0 0] */

    c  = (uint64_t)a[0] * b[0];
    verify_bits(c, 60);
    /* [d t9 0 0 0 0 0 0 0 0 c] = [p9 0 0 0 0 0 0 0 0 p0] */
    d += (uint64_t)a[1] * b[9]
       + (uint64_t)a[2] * b[8]
       + (uint64_t)a[3] * b[7]
       + (uint64_t)a[4] * b[6]
       + (uint64_t)a[5] * b[5]
       + (uint64_t)a[6] * b[4]
       + (uint64_t)a[7] * b[3]
       + (uint64_t)a[8] * b[2]
       + (uint64_t)a[9] * b[1];
    verify_bits(d, 63);
    /* [d t9 0 0 0 0 0 0 0 0 c] = [p10 p9 0 0 0 0 0 0 0 0 p0] */
    u0 = d & m; d >>= 26; c += u0 * r0;
    verify_bits(u0, 26);
    verify_bits(d, 37);
    verify_bits(c, 61);
    /* [d u0 t9 0 0 0 0 0 0 0 0 c-u0*r0] = [p10 p9 0 0 0 0 0 0 0 0 p0] */
    t0 = c & m; c >>= 26; c += u0 * r1;
    verify_bits(t0, 26);
    verify_bits(c, 37);
    /* [d u0 t9 0 0 0 0 0 0 0 c-u0*r1 t0-u0*r0] = [p10 p9 0 0 0 0 0 0 0 0 p0] */
    /* [d 0 t9 0 0 0 0 0 0 0 c t0] = [p10 p9 0 0 0 0 0 0 0 0 p0] */

    c += (uint64_t)a[0] * b[1]
       + (uint64_t)a[1] * b[0];
    verify_bits(c, 62);
    /* [d 0 t9 0 0 0 0 0 0 0 c t0] = [p10 p9 0 0 0 0 0 0 0 p1 p0] */
    d += (uint64_t)a[2] * b[9]
       + (uint64_t)a[3] * b[8]
       + (uint64_t)a[4] * b[7]
       + (uint64_t)a[5] * b[6]
       + (uint64_t)a[6] * b[5]
       + (uint64_t)a[7] * b[4]
       + (uint64_t)a[8] * b[3]
       + (uint64_t)a[9] * b[2];
    verify_bits(d, 63);
    /* [d 0 t9 0 0 0 0 0 0 0 c t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */
    u1 = d & m; d >>= 26; c += u1 * r0;
    verify_bits(u1, 26);
    verify_bits(d, 37);
    verify_bits(c, 63);
    /* [d u1 0 t9 0 0 0 0 0 0 0 c-u1*r0 t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */
    t1 = c & m; c >>= 26; c += u1 * r1;
    verify_bits(t1, 26);
    verify_bits(c, 38);
    /* [d u1 0 t9 0 0 0 0 0 0 c-u1*r1 t1-u1*r0 t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */
    /* [d 0 0 t9 0 0 0 0 0 0 c t1 t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */

    c += (uint64_t)a[0] * b[2]
       + (uint64_t)a[1] * b[1]
       + (uint64_t)a[2] * b[0];
    verify_bits(c, 62);
    /* [d 0 0 t9 0 0 0 0 0 0 c t1 t0] = [p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    d += (uint64_t)a[3] * b[9]
       + (uint64_t)a[4] * b[8]
       + (uint64_t)a[5] * b[7]
       + (uint64_t)a[6] * b[6]
       + (uint64_t)a[7] * b[5]
       + (uint64_t)a[8] * b[4]
       + (uint64_t)a[9] * b[3];
    verify_bits(d, 63);
    /* [d 0 0 t9 0 0 0 0 0 0 c t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    u2 = d & m; d >>= 26; c += u2 * r0;
    verify_bits(u2, 26);
    verify_bits(d, 37);
    verify_bits(c, 63);
    /* [d u2 0 0 t9 0 0 0 0 0 0 c-u2*r0 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    t2 = c & m; c >>= 26; c += u2 * r1;
    verify_bits(t2, 26);
    verify_bits(c, 38);
    /* [d u2 0 0 t9 0 0 0 0 0 c-u2*r1 t2-u2*r0 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    /* [d 0 0 0 t9 0 0 0 0 0 c t2 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */

    c += (uint64_t)a[0] * b[3]
       + (uint64_t)a[1] * b[2]
       + (uint64_t)a[2] * b[1]
       + (uint64_t)a[3] * b[0];
    verify_bits(c, 63);
    /* [d 0 0 0 t9 0 0 0 0 0 c t2 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    d += (uint64_t)a[4] * b[9]
       + (uint64_t)a[5] * b[8]
       + (uint64_t)a[6] * b[7]
       + (uint64_t)a[7] * b[6]
       + (uint64_t)a[8] * b[5]
       + (uint64_t)a[9] * b[4];
    verify_bits(d, 63);
    /* [d 0 0 0 t9 0 0 0 0 0 c t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    u3 = d & m; d >>= 26; c += u3 * r0;
    verify_bits(u3, 26);
    verify_bits(d, 37);
    /* verify_bits(c, 64); */
    /* [d u3 0 0 0 t9 0 0 0 0 0 c-u3*r0 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    t3 = c & m; c >>= 26; c += u3 * r1;
    verify_bits(t3, 26);
    verify_bits(c, 39);
    /* [d u3 0 0 0 t9 0 0 0 0 c-u3*r1 t3-u3*r0 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    /* [d 0 0 0 0 t9 0 0 0 0 c t3 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */

    c += (uint64_t)a[0] * b[4]
       + (uint64_t)a[1] * b[3]
       + (uint64_t)a[2] * b[2]
       + (uint64_t)a[3] * b[1]
       + (uint64_t)a[4] * b[0];
    verify_bits(c, 63);
    /* [d 0 0 0 0 t9 0 0 0 0 c t3 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    d += (uint64_t)a[5] * b[9]
       + (uint64_t)a[6] * b[8]
       + (uint64_t)a[7] * b[7]
       + (uint64_t)a[8] * b[6]
       + (uint64_t)a[9] * b[5];
    verify_bits(d, 62);
    /* [d 0 0 0 0 t9 0 0 0 0 c t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    u4 = d & m; d >>= 26; c += u4 * r0;
    verify_bits(u4, 26);
    verify_bits(d, 36);
    /* verify_bits(c, 64); */
    /* [d u4 0 0 0 0 t9 0 0 0 0 c-u4*r0 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    t4 = c & m; c >>= 26; c += u4 * r1;
    verify_bits(t4, 26);
    verify_bits(c, 39);
    /* [d u4 0 0 0 0 t9 0 0 0 c-u4*r1 t4-u4*r0 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 t9 0 0 0 c t4 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */

    c += (uint64_t)a[0] * b[5]
       + (uint64_t)a[1] * b[4]
       + (uint64_t)a[2] * b[3]
       + (uint64_t)a[3] * b[2]
       + (uint64_t)a[4] * b[1]
       + (uint64_t)a[5] * b[0];
    verify_bits(c, 63);
    /* [d 0 0 0 0 0 t9 0 0 0 c t4 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)a[6] * b[9]
       + (uint64_t)a[7] * b[8]
       + (uint64_t)a[8] * b[7]
       + (uint64_t)a[9] * b[6];
    verify_bits(d, 62);
    /* [d 0 0 0 0 0 t9 0 0 0 c t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    u5 = d & m; d >>= 26; c += u5 * r0;
    verify_bits(u5, 26);
    verify_bits(d, 36);
    /* verify_bits(c, 64); */
    /* [d u5 0 0 0 0 0 t9 0 0 0 c-u5*r0 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    t5 = c & m; c >>= 26; c += u5 * r1;
    verify_bits(t5, 26);
    verify_bits(c, 39);
    /* [d u5 0 0 0 0 0 t9 0 0 c-u5*r1 t5-u5*r0 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 t9 0 0 c t5 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */

    c += (uint64_t)a[0] * b[6]
       + (uint64_t)a[1] * b[5]
       + (uint64_t)a[2] * b[4]
       + (uint64_t)a[3] * b[3]
       + (uint64_t)a[4] * b[2]
       + (uint64_t)a[5] * b[1]
       + (uint64_t)a[6] * b[0];
    verify_bits(c, 63);
    /* [d 0 0 0 0 0 0 t9 0 0 c t5 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)a[7] * b[9]
       + (uint64_t)a[8] * b[8]
       + (uint64_t)a[9] * b[7];
    verify_bits(d, 61);
    /* [d 0 0 0 0 0 0 t9 0 0 c t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    u6 = d & m; d >>= 26; c += u6 * r0;
    verify_bits(u6, 26);
    verify_bits(d, 35);
    /* verify_bits(c, 64); */
    /* [d u6 0 0 0 0 0 0 t9 0 0 c-u6*r0 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    t6 = c & m; c >>= 26; c += u6 * r1;
    verify_bits(t6, 26);
    verify_bits(c, 39);
    /* [d u6 0 0 0 0 0 0 t9 0 c-u6*r1 t6-u6*r0 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */

    c += (uint64_t)a[0] * b[7]
       + (uint64_t)a[1] * b[6]
       + (uint64_t)a[2] * b[5]
       + (uint64_t)a[3] * b[4]
       + (uint64_t)a[4] * b[3]
       + (uint64_t)a[5] * b[2]
       + (uint64_t)a[6] * b[1]
       + (uint64_t)a[7] * b[0];
    /* verify_bits(c, 64); */
    verify_check(c <= 0x8000007c00000007ull);
    /* [d 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)a[8] * b[9]
       + (uint64_t)a[9] * b[8];
    verify_bits(d, 58);
    /* [d 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    u7 = d & m; d >>= 26; c += u7 * r0;
    verify_bits(u7, 26);
    verify_bits(d, 32);
    /* verify_bits(c, 64); */
    verify_check(c <= 0x800001703fffc2f7ull);
    /* [d u7 0 0 0 0 0 0 0 t9 0 c-u7*r0 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    t7 = c & m; c >>= 26; c += u7 * r1;
    verify_bits(t7, 26);
    verify_bits(c, 38);
    /* [d u7 0 0 0 0 0 0 0 t9 c-u7*r1 t7-u7*r0 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 0 t9 c t7 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */

    c += (uint64_t)a[0] * b[8]
       + (uint64_t)a[1] * b[7]
       + (uint64_t)a[2] * b[6]
       + (uint64_t)a[3] * b[5]
       + (uint64_t)a[4] * b[4]
       + (uint64_t)a[5] * b[3]
       + (uint64_t)a[6] * b[2]
       + (uint64_t)a[7] * b[1]
       + (uint64_t)a[8] * b[0];
    /* verify_bits(c, 64); */
    verify_check(c <= 0x9000007b80000008ull);
    /* [d 0 0 0 0 0 0 0 0 t9 c t7 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)a[9] * b[9];
    verify_bits(d, 57);
    /* [d 0 0 0 0 0 0 0 0 t9 c t7 t6 t5 t4 t3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    u8 = d & m; d >>= 26; c += u8 * r0;
    verify_bits(u8, 26);
    verify_bits(d, 31);
    /* verify_bits(c, 64); */
    verify_check(c <= 0x9000016fbfffc2f8ull);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 t5 t4 t3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    r[3] = t3;
    verify_bits(r[3], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 t5 t4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[4] = t4;
    verify_bits(r[4], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 t5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[5] = t5;
    verify_bits(r[5], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[6] = t6;
    verify_bits(r[6], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[7] = t7;
    verify_bits(r[7], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    r[8] = c & m; c >>= 26; c += u8 * r1;
    verify_bits(r[8], 26);
    verify_bits(c, 39);
    /* [d u8 0 0 0 0 0 0 0 0 t9+c-u8*r1 r8-u8*r0 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 0 0 t9+c r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c   += d * r0 + t9;
    verify_bits(c, 45);
    /* [d 0 0 0 0 0 0 0 0 0 c-d*r0 r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[9] = c & (m >> 4); c >>= 22; c += d * (r1 << 4);
    verify_bits(r[9], 22);
    verify_bits(c, 46);
    /* [d 0 0 0 0 0 0 0 0 r9+((c-d*r1<<4)<<22)-d*r0 r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 -d*r1 r9+(c<<22)-d*r0 r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    d    = c * (r0 >> 4) + t0;
    verify_bits(d, 56);
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 t1 d-c*r0>>4] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[0] = d & m; d >>= 26;
    verify_bits(r[0], 26);
    verify_bits(d, 30);
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 t1+d r0-c*r0>>4] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    d   += c * (r1 >> 4) + t1;
    verify_bits(d, 53);
    verify_check(d <= 0x10000003ffffbfull);
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 d-c*r1>>4 r0-c*r0>>4] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [r9 r8 r7 r6 r5 r4 r3 t2 d r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[1] = d & m; d >>= 26;
    verify_bits(r[1], 26);
    verify_bits(d, 27);
    verify_check(d <= 0x4000000ull);
    /* [r9 r8 r7 r6 r5 r4 r3 t2+d r1 r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    d   += t2;
    verify_bits(d, 27);
    /* [r9 r8 r7 r6 r5 r4 r3 d r1 r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[2] = d;
    verify_bits(r[2], 27);
    /* [r9 r8 r7 r6 r5 r4 r3 r2 r1 r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
}

secp256k1_inline static void secp256k1_fe_sqr_inner(uint32_t *r, const uint32_t *a) {
    uint64_t c, d;
    uint64_t u0, u1, u2, u3, u4, u5, u6, u7, u8;
    uint32_t t9, t0, t1, t2, t3, t4, t5, t6, t7;
    const uint32_t m = 0x3fffffful, r0 = 0x3d10ul, r1 = 0x400ul;

    verify_bits(a[0], 30);
    verify_bits(a[1], 30);
    verify_bits(a[2], 30);
    verify_bits(a[3], 30);
    verify_bits(a[4], 30);
    verify_bits(a[5], 30);
    verify_bits(a[6], 30);
    verify_bits(a[7], 30);
    verify_bits(a[8], 30);
    verify_bits(a[9], 26);

    /** [... a b c] is a shorthand for ... + a<<52 + b<<26 + c<<0 mod n.
     *  px is a shorthand for sum(a[i]*a[x-i], i=0..x).
     *  note that [x 0 0 0 0 0 0 0 0 0 0] = [x*r1 x*r0].
     */

    d  = (uint64_t)(a[0]*2) * a[9]
       + (uint64_t)(a[1]*2) * a[8]
       + (uint64_t)(a[2]*2) * a[7]
       + (uint64_t)(a[3]*2) * a[6]
       + (uint64_t)(a[4]*2) * a[5];
    /* verify_bits(d, 64); */
    /* [d 0 0 0 0 0 0 0 0 0] = [p9 0 0 0 0 0 0 0 0 0] */
    t9 = d & m; d >>= 26;
    verify_bits(t9, 26);
    verify_bits(d, 38);
    /* [d t9 0 0 0 0 0 0 0 0 0] = [p9 0 0 0 0 0 0 0 0 0] */

    c  = (uint64_t)a[0] * a[0];
    verify_bits(c, 60);
    /* [d t9 0 0 0 0 0 0 0 0 c] = [p9 0 0 0 0 0 0 0 0 p0] */
    d += (uint64_t)(a[1]*2) * a[9]
       + (uint64_t)(a[2]*2) * a[8]
       + (uint64_t)(a[3]*2) * a[7]
       + (uint64_t)(a[4]*2) * a[6]
       + (uint64_t)a[5] * a[5];
    verify_bits(d, 63);
    /* [d t9 0 0 0 0 0 0 0 0 c] = [p10 p9 0 0 0 0 0 0 0 0 p0] */
    u0 = d & m; d >>= 26; c += u0 * r0;
    verify_bits(u0, 26);
    verify_bits(d, 37);
    verify_bits(c, 61);
    /* [d u0 t9 0 0 0 0 0 0 0 0 c-u0*r0] = [p10 p9 0 0 0 0 0 0 0 0 p0] */
    t0 = c & m; c >>= 26; c += u0 * r1;
    verify_bits(t0, 26);
    verify_bits(c, 37);
    /* [d u0 t9 0 0 0 0 0 0 0 c-u0*r1 t0-u0*r0] = [p10 p9 0 0 0 0 0 0 0 0 p0] */
    /* [d 0 t9 0 0 0 0 0 0 0 c t0] = [p10 p9 0 0 0 0 0 0 0 0 p0] */

    c += (uint64_t)(a[0]*2) * a[1];
    verify_bits(c, 62);
    /* [d 0 t9 0 0 0 0 0 0 0 c t0] = [p10 p9 0 0 0 0 0 0 0 p1 p0] */
    d += (uint64_t)(a[2]*2) * a[9]
       + (uint64_t)(a[3]*2) * a[8]
       + (uint64_t)(a[4]*2) * a[7]
       + (uint64_t)(a[5]*2) * a[6];
    verify_bits(d, 63);
    /* [d 0 t9 0 0 0 0 0 0 0 c t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */
    u1 = d & m; d >>= 26; c += u1 * r0;
    verify_bits(u1, 26);
    verify_bits(d, 37);
    verify_bits(c, 63);
    /* [d u1 0 t9 0 0 0 0 0 0 0 c-u1*r0 t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */
    t1 = c & m; c >>= 26; c += u1 * r1;
    verify_bits(t1, 26);
    verify_bits(c, 38);
    /* [d u1 0 t9 0 0 0 0 0 0 c-u1*r1 t1-u1*r0 t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */
    /* [d 0 0 t9 0 0 0 0 0 0 c t1 t0] = [p11 p10 p9 0 0 0 0 0 0 0 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[2]
       + (uint64_t)a[1] * a[1];
    verify_bits(c, 62);
    /* [d 0 0 t9 0 0 0 0 0 0 c t1 t0] = [p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    d += (uint64_t)(a[3]*2) * a[9]
       + (uint64_t)(a[4]*2) * a[8]
       + (uint64_t)(a[5]*2) * a[7]
       + (uint64_t)a[6] * a[6];
    verify_bits(d, 63);
    /* [d 0 0 t9 0 0 0 0 0 0 c t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    u2 = d & m; d >>= 26; c += u2 * r0;
    verify_bits(u2, 26);
    verify_bits(d, 37);
    verify_bits(c, 63);
    /* [d u2 0 0 t9 0 0 0 0 0 0 c-u2*r0 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    t2 = c & m; c >>= 26; c += u2 * r1;
    verify_bits(t2, 26);
    verify_bits(c, 38);
    /* [d u2 0 0 t9 0 0 0 0 0 c-u2*r1 t2-u2*r0 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */
    /* [d 0 0 0 t9 0 0 0 0 0 c t2 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 0 p2 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[3]
       + (uint64_t)(a[1]*2) * a[2];
    verify_bits(c, 63);
    /* [d 0 0 0 t9 0 0 0 0 0 c t2 t1 t0] = [p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    d += (uint64_t)(a[4]*2) * a[9]
       + (uint64_t)(a[5]*2) * a[8]
       + (uint64_t)(a[6]*2) * a[7];
    verify_bits(d, 63);
    /* [d 0 0 0 t9 0 0 0 0 0 c t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    u3 = d & m; d >>= 26; c += u3 * r0;
    verify_bits(u3, 26);
    verify_bits(d, 37);
    /* verify_bits(c, 64); */
    /* [d u3 0 0 0 t9 0 0 0 0 0 c-u3*r0 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    t3 = c & m; c >>= 26; c += u3 * r1;
    verify_bits(t3, 26);
    verify_bits(c, 39);
    /* [d u3 0 0 0 t9 0 0 0 0 c-u3*r1 t3-u3*r0 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */
    /* [d 0 0 0 0 t9 0 0 0 0 c t3 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 0 p3 p2 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[4]
       + (uint64_t)(a[1]*2) * a[3]
       + (uint64_t)a[2] * a[2];
    verify_bits(c, 63);
    /* [d 0 0 0 0 t9 0 0 0 0 c t3 t2 t1 t0] = [p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    d += (uint64_t)(a[5]*2) * a[9]
       + (uint64_t)(a[6]*2) * a[8]
       + (uint64_t)a[7] * a[7];
    verify_bits(d, 62);
    /* [d 0 0 0 0 t9 0 0 0 0 c t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    u4 = d & m; d >>= 26; c += u4 * r0;
    verify_bits(u4, 26);
    verify_bits(d, 36);
    /* verify_bits(c, 64); */
    /* [d u4 0 0 0 0 t9 0 0 0 0 c-u4*r0 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    t4 = c & m; c >>= 26; c += u4 * r1;
    verify_bits(t4, 26);
    verify_bits(c, 39);
    /* [d u4 0 0 0 0 t9 0 0 0 c-u4*r1 t4-u4*r0 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 t9 0 0 0 c t4 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 0 p4 p3 p2 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[5]
       + (uint64_t)(a[1]*2) * a[4]
       + (uint64_t)(a[2]*2) * a[3];
    verify_bits(c, 63);
    /* [d 0 0 0 0 0 t9 0 0 0 c t4 t3 t2 t1 t0] = [p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)(a[6]*2) * a[9]
       + (uint64_t)(a[7]*2) * a[8];
    verify_bits(d, 62);
    /* [d 0 0 0 0 0 t9 0 0 0 c t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    u5 = d & m; d >>= 26; c += u5 * r0;
    verify_bits(u5, 26);
    verify_bits(d, 36);
    /* verify_bits(c, 64); */
    /* [d u5 0 0 0 0 0 t9 0 0 0 c-u5*r0 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    t5 = c & m; c >>= 26; c += u5 * r1;
    verify_bits(t5, 26);
    verify_bits(c, 39);
    /* [d u5 0 0 0 0 0 t9 0 0 c-u5*r1 t5-u5*r0 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 t9 0 0 c t5 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 0 p5 p4 p3 p2 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[6]
       + (uint64_t)(a[1]*2) * a[5]
       + (uint64_t)(a[2]*2) * a[4]
       + (uint64_t)a[3] * a[3];
    verify_bits(c, 63);
    /* [d 0 0 0 0 0 0 t9 0 0 c t5 t4 t3 t2 t1 t0] = [p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)(a[7]*2) * a[9]
       + (uint64_t)a[8] * a[8];
    verify_bits(d, 61);
    /* [d 0 0 0 0 0 0 t9 0 0 c t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    u6 = d & m; d >>= 26; c += u6 * r0;
    verify_bits(u6, 26);
    verify_bits(d, 35);
    /* verify_bits(c, 64); */
    /* [d u6 0 0 0 0 0 0 t9 0 0 c-u6*r0 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    t6 = c & m; c >>= 26; c += u6 * r1;
    verify_bits(t6, 26);
    verify_bits(c, 39);
    /* [d u6 0 0 0 0 0 0 t9 0 c-u6*r1 t6-u6*r0 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 0 p6 p5 p4 p3 p2 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[7]
       + (uint64_t)(a[1]*2) * a[6]
       + (uint64_t)(a[2]*2) * a[5]
       + (uint64_t)(a[3]*2) * a[4];
    /* verify_bits(c, 64); */
    verify_check(c <= 0x8000007c00000007ull);
    /* [d 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0] = [p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)(a[8]*2) * a[9];
    verify_bits(d, 58);
    /* [d 0 0 0 0 0 0 0 t9 0 c t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    u7 = d & m; d >>= 26; c += u7 * r0;
    verify_bits(u7, 26);
    verify_bits(d, 32);
    /* verify_bits(c, 64); */
    verify_check(c <= 0x800001703fffc2f7ull);
    /* [d u7 0 0 0 0 0 0 0 t9 0 c-u7*r0 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    t7 = c & m; c >>= 26; c += u7 * r1;
    verify_bits(t7, 26);
    verify_bits(c, 38);
    /* [d u7 0 0 0 0 0 0 0 t9 c-u7*r1 t7-u7*r0 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 0 t9 c t7 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 0 p7 p6 p5 p4 p3 p2 p1 p0] */

    c += (uint64_t)(a[0]*2) * a[8]
       + (uint64_t)(a[1]*2) * a[7]
       + (uint64_t)(a[2]*2) * a[6]
       + (uint64_t)(a[3]*2) * a[5]
       + (uint64_t)a[4] * a[4];
    /* verify_bits(c, 64); */
    verify_check(c <= 0x9000007b80000008ull);
    /* [d 0 0 0 0 0 0 0 0 t9 c t7 t6 t5 t4 t3 t2 t1 t0] = [p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    d += (uint64_t)a[9] * a[9];
    verify_bits(d, 57);
    /* [d 0 0 0 0 0 0 0 0 t9 c t7 t6 t5 t4 t3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    u8 = d & m; d >>= 26; c += u8 * r0;
    verify_bits(u8, 26);
    verify_bits(d, 31);
    /* verify_bits(c, 64); */
    verify_check(c <= 0x9000016fbfffc2f8ull);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 t5 t4 t3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    r[3] = t3;
    verify_bits(r[3], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 t5 t4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[4] = t4;
    verify_bits(r[4], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 t5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[5] = t5;
    verify_bits(r[5], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 t6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[6] = t6;
    verify_bits(r[6], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 t7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[7] = t7;
    verify_bits(r[7], 26);
    /* [d u8 0 0 0 0 0 0 0 0 t9 c-u8*r0 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    r[8] = c & m; c >>= 26; c += u8 * r1;
    verify_bits(r[8], 26);
    verify_bits(c, 39);
    /* [d u8 0 0 0 0 0 0 0 0 t9+c-u8*r1 r8-u8*r0 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 0 0 t9+c r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    c   += d * r0 + t9;
    verify_bits(c, 45);
    /* [d 0 0 0 0 0 0 0 0 0 c-d*r0 r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[9] = c & (m >> 4); c >>= 22; c += d * (r1 << 4);
    verify_bits(r[9], 22);
    verify_bits(c, 46);
    /* [d 0 0 0 0 0 0 0 0 r9+((c-d*r1<<4)<<22)-d*r0 r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [d 0 0 0 0 0 0 0 -d*r1 r9+(c<<22)-d*r0 r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 t1 t0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */

    d    = c * (r0 >> 4) + t0;
    verify_bits(d, 56);
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 t1 d-c*r0>>4] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[0] = d & m; d >>= 26;
    verify_bits(r[0], 26);
    verify_bits(d, 30);
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 t1+d r0-c*r0>>4] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    d   += c * (r1 >> 4) + t1;
    verify_bits(d, 53);
    verify_check(d <= 0x10000003ffffbfull);
    /* [r9+(c<<22) r8 r7 r6 r5 r4 r3 t2 d-c*r1>>4 r0-c*r0>>4] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    /* [r9 r8 r7 r6 r5 r4 r3 t2 d r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[1] = d & m; d >>= 26;
    verify_bits(r[1], 26);
    verify_bits(d, 27);
    verify_check(d <= 0x4000000ull);
    /* [r9 r8 r7 r6 r5 r4 r3 t2+d r1 r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    d   += t2;
    verify_bits(d, 27);
    /* [r9 r8 r7 r6 r5 r4 r3 d r1 r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
    r[2] = d;
    verify_bits(r[2], 27);
    /* [r9 r8 r7 r6 r5 r4 r3 r2 r1 r0] = [p18 p17 p16 p15 p14 p13 p12 p11 p10 p9 p8 p7 p6 p5 p4 p3 p2 p1 p0] */
}


static void secp256k1_fe_mul(secp256k1_fe_t *r, const secp256k1_fe_t *a, const secp256k1_fe_t * secp256k1_restrict b) {
#ifdef verify
    verify_check(a->magnitude <= 8);
    verify_check(b->magnitude <= 8);
    secp256k1_fe_verify(a);
    secp256k1_fe_verify(b);
    verify_check(r != b);
#endif
    secp256k1_fe_mul_inner(r->n, a->n, b->n);
#ifdef verify
    r->magnitude = 1;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_sqr(secp256k1_fe_t *r, const secp256k1_fe_t *a) {
#ifdef verify
    verify_check(a->magnitude <= 8);
    secp256k1_fe_verify(a);
#endif
    secp256k1_fe_sqr_inner(r->n, a->n);
#ifdef verify
    r->magnitude = 1;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
}

static secp256k1_inline void secp256k1_fe_cmov(secp256k1_fe_t *r, const secp256k1_fe_t *a, int flag) {
    uint32_t mask0, mask1;
    mask0 = flag + ~((uint32_t)0);
    mask1 = ~mask0;
    r->n[0] = (r->n[0] & mask0) | (a->n[0] & mask1);
    r->n[1] = (r->n[1] & mask0) | (a->n[1] & mask1);
    r->n[2] = (r->n[2] & mask0) | (a->n[2] & mask1);
    r->n[3] = (r->n[3] & mask0) | (a->n[3] & mask1);
    r->n[4] = (r->n[4] & mask0) | (a->n[4] & mask1);
    r->n[5] = (r->n[5] & mask0) | (a->n[5] & mask1);
    r->n[6] = (r->n[6] & mask0) | (a->n[6] & mask1);
    r->n[7] = (r->n[7] & mask0) | (a->n[7] & mask1);
    r->n[8] = (r->n[8] & mask0) | (a->n[8] & mask1);
    r->n[9] = (r->n[9] & mask0) | (a->n[9] & mask1);
#ifdef verify
    r->magnitude = (r->magnitude & mask0) | (a->magnitude & mask1);
    r->normalized = (r->normalized & mask0) | (a->normalized & mask1);
#endif
}

static secp256k1_inline void secp256k1_fe_storage_cmov(secp256k1_fe_storage_t *r, const secp256k1_fe_storage_t *a, int flag) {
    uint32_t mask0, mask1;
    mask0 = flag + ~((uint32_t)0);
    mask1 = ~mask0;
    r->n[0] = (r->n[0] & mask0) | (a->n[0] & mask1);
    r->n[1] = (r->n[1] & mask0) | (a->n[1] & mask1);
    r->n[2] = (r->n[2] & mask0) | (a->n[2] & mask1);
    r->n[3] = (r->n[3] & mask0) | (a->n[3] & mask1);
    r->n[4] = (r->n[4] & mask0) | (a->n[4] & mask1);
    r->n[5] = (r->n[5] & mask0) | (a->n[5] & mask1);
    r->n[6] = (r->n[6] & mask0) | (a->n[6] & mask1);
    r->n[7] = (r->n[7] & mask0) | (a->n[7] & mask1);
}

static void secp256k1_fe_to_storage(secp256k1_fe_storage_t *r, const secp256k1_fe_t *a) {
#ifdef verify
    verify_check(a->normalized);
#endif
    r->n[0] = a->n[0] | a->n[1] << 26;
    r->n[1] = a->n[1] >> 6 | a->n[2] << 20;
    r->n[2] = a->n[2] >> 12 | a->n[3] << 14;
    r->n[3] = a->n[3] >> 18 | a->n[4] << 8;
    r->n[4] = a->n[4] >> 24 | a->n[5] << 2 | a->n[6] << 28;
    r->n[5] = a->n[6] >> 4 | a->n[7] << 22;
    r->n[6] = a->n[7] >> 10 | a->n[8] << 16;
    r->n[7] = a->n[8] >> 16 | a->n[9] << 10;
}

static secp256k1_inline void secp256k1_fe_from_storage(secp256k1_fe_t *r, const secp256k1_fe_storage_t *a) {
    r->n[0] = a->n[0] & 0x3fffffful;
    r->n[1] = a->n[0] >> 26 | ((a->n[1] << 6) & 0x3fffffful);
    r->n[2] = a->n[1] >> 20 | ((a->n[2] << 12) & 0x3fffffful);
    r->n[3] = a->n[2] >> 14 | ((a->n[3] << 18) & 0x3fffffful);
    r->n[4] = a->n[3] >> 8 | ((a->n[4] << 24) & 0x3fffffful);
    r->n[5] = (a->n[4] >> 2) & 0x3fffffful;
    r->n[6] = a->n[4] >> 28 | ((a->n[5] << 4) & 0x3fffffful);
    r->n[7] = a->n[5] >> 22 | ((a->n[6] << 10) & 0x3fffffful);
    r->n[8] = a->n[6] >> 16 | ((a->n[7] << 16) & 0x3fffffful);
    r->n[9] = a->n[7] >> 10;
#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
#endif
}

#endif
