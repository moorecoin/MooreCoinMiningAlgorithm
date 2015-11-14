/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_field_repr_impl_h_
#define _secp256k1_field_repr_impl_h_

#if defined have_config_h
#include "libsecp256k1-config.h"
#endif

#include <string.h>
#include "util.h"
#include "num.h"
#include "field.h"

#if defined(use_asm_x86_64)
#include "field_5x52_asm_impl.h"
#else
#include "field_5x52_int128_impl.h"
#endif

/** implements arithmetic modulo ffffffff ffffffff ffffffff ffffffff ffffffff ffffffff fffffffe fffffc2f,
 *  represented as 5 uint64_t's in base 2^52. the values are allowed to contain >52 each. in particular,
 *  each fieldelem has a 'magnitude' associated with it. internally, a magnitude m means each element
 *  is at most m*(2^53-1), except the most significant one, which is limited to m*(2^49-1). all operations
 *  accept any input with magnitude at most m, and have different rules for propagating magnitude to their
 *  output.
 */

#ifdef verify
static void secp256k1_fe_verify(const secp256k1_fe_t *a) {
    const uint64_t *d = a->n;
    int m = a->normalized ? 1 : 2 * a->magnitude, r = 1;
   /* secp256k1 'p' value defined in "standards for efficient cryptography" (sec2) 2.7.1. */
    r &= (d[0] <= 0xfffffffffffffull * m);
    r &= (d[1] <= 0xfffffffffffffull * m);
    r &= (d[2] <= 0xfffffffffffffull * m);
    r &= (d[3] <= 0xfffffffffffffull * m);
    r &= (d[4] <= 0x0ffffffffffffull * m);
    r &= (a->magnitude >= 0);
    r &= (a->magnitude <= 2048);
    if (a->normalized) {
        r &= (a->magnitude <= 1);
        if (r && (d[4] == 0x0ffffffffffffull) && ((d[3] & d[2] & d[1]) == 0xfffffffffffffull)) {
            r &= (d[0] < 0xffffefffffc2full);
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
    uint64_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4];

    /* reduce t4 at the start so there will be at most a single carry from the first pass */
    uint64_t m;
    uint64_t x = t4 >> 48; t4 &= 0x0ffffffffffffull;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x1000003d1ull;
    t1 += (t0 >> 52); t0 &= 0xfffffffffffffull;
    t2 += (t1 >> 52); t1 &= 0xfffffffffffffull; m = t1;
    t3 += (t2 >> 52); t2 &= 0xfffffffffffffull; m &= t2;
    t4 += (t3 >> 52); t3 &= 0xfffffffffffffull; m &= t3;

    /* ... except for a possible carry at bit 48 of t4 (i.e. bit 256 of the field element) */
    verify_check(t4 >> 49 == 0);

    /* at most a single final reduction is needed; check if the value is >= the field characteristic */
    x = (t4 >> 48) | ((t4 == 0x0ffffffffffffull) & (m == 0xfffffffffffffull)
        & (t0 >= 0xffffefffffc2full));

    /* apply the final reduction (for constant-time behaviour, we do it always) */
    t0 += x * 0x1000003d1ull;
    t1 += (t0 >> 52); t0 &= 0xfffffffffffffull;
    t2 += (t1 >> 52); t1 &= 0xfffffffffffffull;
    t3 += (t2 >> 52); t2 &= 0xfffffffffffffull;
    t4 += (t3 >> 52); t3 &= 0xfffffffffffffull;

    /* if t4 didn't carry to bit 48 already, then it should have after any final reduction */
    verify_check(t4 >> 48 == x);

    /* mask off the possible multiple of 2^256 from the final reduction */
    t4 &= 0x0ffffffffffffull;

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;

#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_normalize_weak(secp256k1_fe_t *r) {
    uint64_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4];

    /* reduce t4 at the start so there will be at most a single carry from the first pass */
    uint64_t x = t4 >> 48; t4 &= 0x0ffffffffffffull;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x1000003d1ull;
    t1 += (t0 >> 52); t0 &= 0xfffffffffffffull;
    t2 += (t1 >> 52); t1 &= 0xfffffffffffffull;
    t3 += (t2 >> 52); t2 &= 0xfffffffffffffull;
    t4 += (t3 >> 52); t3 &= 0xfffffffffffffull;

    /* ... except for a possible carry at bit 48 of t4 (i.e. bit 256 of the field element) */
    verify_check(t4 >> 49 == 0);

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;

#ifdef verify
    r->magnitude = 1;
    secp256k1_fe_verify(r);
#endif
}

static void secp256k1_fe_normalize_var(secp256k1_fe_t *r) {
    uint64_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4];

    /* reduce t4 at the start so there will be at most a single carry from the first pass */
    uint64_t m;
    uint64_t x = t4 >> 48; t4 &= 0x0ffffffffffffull;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x1000003d1ull;
    t1 += (t0 >> 52); t0 &= 0xfffffffffffffull;
    t2 += (t1 >> 52); t1 &= 0xfffffffffffffull; m = t1;
    t3 += (t2 >> 52); t2 &= 0xfffffffffffffull; m &= t2;
    t4 += (t3 >> 52); t3 &= 0xfffffffffffffull; m &= t3;

    /* ... except for a possible carry at bit 48 of t4 (i.e. bit 256 of the field element) */
    verify_check(t4 >> 49 == 0);

    /* at most a single final reduction is needed; check if the value is >= the field characteristic */
    x = (t4 >> 48) | ((t4 == 0x0ffffffffffffull) & (m == 0xfffffffffffffull)
        & (t0 >= 0xffffefffffc2full));

    if (x) {
        t0 += 0x1000003d1ull;
        t1 += (t0 >> 52); t0 &= 0xfffffffffffffull;
        t2 += (t1 >> 52); t1 &= 0xfffffffffffffull;
        t3 += (t2 >> 52); t2 &= 0xfffffffffffffull;
        t4 += (t3 >> 52); t3 &= 0xfffffffffffffull;

        /* if t4 didn't carry to bit 48 already, then it should have after any final reduction */
        verify_check(t4 >> 48 == x);

        /* mask off the possible multiple of 2^256 from the final reduction */
        t4 &= 0x0ffffffffffffull;
    }

    r->n[0] = t0; r->n[1] = t1; r->n[2] = t2; r->n[3] = t3; r->n[4] = t4;

#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

static int secp256k1_fe_normalizes_to_zero(secp256k1_fe_t *r) {
    uint64_t t0 = r->n[0], t1 = r->n[1], t2 = r->n[2], t3 = r->n[3], t4 = r->n[4];

    /* z0 tracks a possible raw value of 0, z1 tracks a possible raw value of p */
    uint64_t z0, z1;

    /* reduce t4 at the start so there will be at most a single carry from the first pass */
    uint64_t x = t4 >> 48; t4 &= 0x0ffffffffffffull;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x1000003d1ull;
    t1 += (t0 >> 52); t0 &= 0xfffffffffffffull; z0  = t0; z1  = t0 ^ 0x1000003d0ull;
    t2 += (t1 >> 52); t1 &= 0xfffffffffffffull; z0 |= t1; z1 &= t1;
    t3 += (t2 >> 52); t2 &= 0xfffffffffffffull; z0 |= t2; z1 &= t2;
    t4 += (t3 >> 52); t3 &= 0xfffffffffffffull; z0 |= t3; z1 &= t3;
                                                z0 |= t4; z1 &= t4 ^ 0xf000000000000ull;

    /* ... except for a possible carry at bit 48 of t4 (i.e. bit 256 of the field element) */
    verify_check(t4 >> 49 == 0);

    return (z0 == 0) | (z1 == 0xfffffffffffffull);
}

static int secp256k1_fe_normalizes_to_zero_var(secp256k1_fe_t *r) {
    uint64_t t0, t1, t2, t3, t4;
    uint64_t z0, z1;
    uint64_t x;

    t0 = r->n[0];
    t4 = r->n[4];

    /* reduce t4 at the start so there will be at most a single carry from the first pass */
    x = t4 >> 48;

    /* the first pass ensures the magnitude is 1, ... */
    t0 += x * 0x1000003d1ull;

    /* z0 tracks a possible raw value of 0, z1 tracks a possible raw value of p */
    z0 = t0 & 0xfffffffffffffull;
    z1 = z0 ^ 0x1000003d0ull;

    /* fast return path should catch the majority of cases */
    if ((z0 != 0ull) & (z1 != 0xfffffffffffffull)) {
        return 0;
    }

    t1 = r->n[1];
    t2 = r->n[2];
    t3 = r->n[3];

    t4 &= 0x0ffffffffffffull;

    t1 += (t0 >> 52); t0  = z0;
    t2 += (t1 >> 52); t1 &= 0xfffffffffffffull; z0 |= t1; z1 &= t1;
    t3 += (t2 >> 52); t2 &= 0xfffffffffffffull; z0 |= t2; z1 &= t2;
    t4 += (t3 >> 52); t3 &= 0xfffffffffffffull; z0 |= t3; z1 &= t3;
                                                z0 |= t4; z1 &= t4 ^ 0xf000000000000ull;

    /* ... except for a possible carry at bit 48 of t4 (i.e. bit 256 of the field element) */
    verify_check(t4 >> 49 == 0);

    return (z0 == 0) | (z1 == 0xfffffffffffffull);
}

secp256k1_inline static void secp256k1_fe_set_int(secp256k1_fe_t *r, int a) {
    r->n[0] = a;
    r->n[1] = r->n[2] = r->n[3] = r->n[4] = 0;
#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
    secp256k1_fe_verify(r);
#endif
}

secp256k1_inline static int secp256k1_fe_is_zero(const secp256k1_fe_t *a) {
    const uint64_t *t = a->n;
#ifdef verify
    verify_check(a->normalized);
    secp256k1_fe_verify(a);
#endif
    return (t[0] | t[1] | t[2] | t[3] | t[4]) == 0;
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
    for (i=0; i<5; i++) {
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
    for (i = 4; i >= 0; i--) {
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
    for (i=0; i<32; i++) {
        int j;
        for (j=0; j<2; j++) {
            int limb = (8*i+4*j)/52;
            int shift = (8*i+4*j)%52;
            r->n[limb] |= (uint64_t)((a[31-i] >> (4*j)) & 0xf) << shift;
        }
    }
    if (r->n[4] == 0x0ffffffffffffull && (r->n[3] & r->n[2] & r->n[1]) == 0xfffffffffffffull && r->n[0] >= 0xffffefffffc2full) {
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
        for (j=0; j<2; j++) {
            int limb = (8*i+4*j)/52;
            int shift = (8*i+4*j)%52;
            c |= ((a->n[limb] >> shift) & 0xf) << (4 * j);
        }
        r[31-i] = c;
    }
}

secp256k1_inline static void secp256k1_fe_negate(secp256k1_fe_t *r, const secp256k1_fe_t *a, int m) {
#ifdef verify
    verify_check(a->magnitude <= m);
    secp256k1_fe_verify(a);
#endif
    r->n[0] = 0xffffefffffc2full * 2 * (m + 1) - a->n[0];
    r->n[1] = 0xfffffffffffffull * 2 * (m + 1) - a->n[1];
    r->n[2] = 0xfffffffffffffull * 2 * (m + 1) - a->n[2];
    r->n[3] = 0xfffffffffffffull * 2 * (m + 1) - a->n[3];
    r->n[4] = 0x0ffffffffffffull * 2 * (m + 1) - a->n[4];
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
#ifdef verify
    r->magnitude += a->magnitude;
    r->normalized = 0;
    secp256k1_fe_verify(r);
#endif
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
    uint64_t mask0, mask1;
    mask0 = flag + ~((uint64_t)0);
    mask1 = ~mask0;
    r->n[0] = (r->n[0] & mask0) | (a->n[0] & mask1);
    r->n[1] = (r->n[1] & mask0) | (a->n[1] & mask1);
    r->n[2] = (r->n[2] & mask0) | (a->n[2] & mask1);
    r->n[3] = (r->n[3] & mask0) | (a->n[3] & mask1);
    r->n[4] = (r->n[4] & mask0) | (a->n[4] & mask1);
#ifdef verify
    r->magnitude = (r->magnitude & mask0) | (a->magnitude & mask1);
    r->normalized = (r->normalized & mask0) | (a->normalized & mask1);
#endif
}

static secp256k1_inline void secp256k1_fe_storage_cmov(secp256k1_fe_storage_t *r, const secp256k1_fe_storage_t *a, int flag) {
    uint64_t mask0, mask1;
    mask0 = flag + ~((uint64_t)0);
    mask1 = ~mask0;
    r->n[0] = (r->n[0] & mask0) | (a->n[0] & mask1);
    r->n[1] = (r->n[1] & mask0) | (a->n[1] & mask1);
    r->n[2] = (r->n[2] & mask0) | (a->n[2] & mask1);
    r->n[3] = (r->n[3] & mask0) | (a->n[3] & mask1);
}

static void secp256k1_fe_to_storage(secp256k1_fe_storage_t *r, const secp256k1_fe_t *a) {
#ifdef verify
    verify_check(a->normalized);
#endif
    r->n[0] = a->n[0] | a->n[1] << 52;
    r->n[1] = a->n[1] >> 12 | a->n[2] << 40;
    r->n[2] = a->n[2] >> 24 | a->n[3] << 28;
    r->n[3] = a->n[3] >> 36 | a->n[4] << 16;
}

static secp256k1_inline void secp256k1_fe_from_storage(secp256k1_fe_t *r, const secp256k1_fe_storage_t *a) {
    r->n[0] = a->n[0] & 0xfffffffffffffull;
    r->n[1] = a->n[0] >> 52 | ((a->n[1] << 12) & 0xfffffffffffffull);
    r->n[2] = a->n[1] >> 40 | ((a->n[2] << 24) & 0xfffffffffffffull);
    r->n[3] = a->n[2] >> 28 | ((a->n[3] << 36) & 0xfffffffffffffull);
    r->n[4] = a->n[3] >> 16;
#ifdef verify
    r->magnitude = 1;
    r->normalized = 1;
#endif
}

#endif
