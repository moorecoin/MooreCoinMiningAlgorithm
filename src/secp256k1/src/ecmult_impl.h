/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_ecmult_impl_h_
#define _secp256k1_ecmult_impl_h_

#include "group.h"
#include "scalar.h"
#include "ecmult.h"

/* optimal for 128-bit and 256-bit exponents. */
#define window_a 5

/** larger numbers may result in slightly better performance, at the cost of
    exponentially larger precomputed tables. */
#ifdef use_endomorphism
/** two tables for window size 15: 1.375 mib. */
#define window_g 15
#else
/** one table for window size 16: 1.375 mib. */
#define window_g 16
#endif

/** fill a table 'pre' with precomputed odd multiples of a. w determines the size of the table.
 *  pre will contains the values [1*a,3*a,5*a,...,(2^(w-1)-1)*a], so it needs place for
 *  2^(w-2) entries.
 *
 *  there are two versions of this function:
 *  - secp256k1_ecmult_precomp_wnaf_gej, which operates on group elements in jacobian notation,
 *    fast to precompute, but slower to use in later additions.
 *  - secp256k1_ecmult_precomp_wnaf_ge, which operates on group elements in affine notations,
 *    (much) slower to precompute, but a bit faster to use in later additions.
 *  to compute a*p + b*g, we use the jacobian version for p, and the affine version for g, as
 *  g is constant, so it only needs to be done once in advance.
 */
static void secp256k1_ecmult_table_precomp_gej_var(secp256k1_gej_t *pre, const secp256k1_gej_t *a, int w) {
    secp256k1_gej_t d;
    int i;
    pre[0] = *a;
    secp256k1_gej_double_var(&d, &pre[0]);
    for (i = 1; i < (1 << (w-2)); i++) {
        secp256k1_gej_add_var(&pre[i], &d, &pre[i-1]);
    }
}

static void secp256k1_ecmult_table_precomp_ge_storage_var(secp256k1_ge_storage_t *pre, const secp256k1_gej_t *a, int w) {
    secp256k1_gej_t d;
    int i;
    const int table_size = 1 << (w-2);
    secp256k1_gej_t *prej = (secp256k1_gej_t *)checked_malloc(sizeof(secp256k1_gej_t) * table_size);
    secp256k1_ge_t *prea = (secp256k1_ge_t *)checked_malloc(sizeof(secp256k1_ge_t) * table_size);
    prej[0] = *a;
    secp256k1_gej_double_var(&d, a);
    for (i = 1; i < table_size; i++) {
        secp256k1_gej_add_var(&prej[i], &d, &prej[i-1]);
    }
    secp256k1_ge_set_all_gej_var(table_size, prea, prej);
    for (i = 0; i < table_size; i++) {
        secp256k1_ge_to_storage(&pre[i], &prea[i]);
    }
    free(prej);
    free(prea);
}

/** the number of entries a table with precomputed multiples needs to have. */
#define ecmult_table_size(w) (1 << ((w)-2))

/** the following two macro retrieves a particular odd multiple from a table
 *  of precomputed multiples. */
#define ecmult_table_get_gej(r,pre,n,w) do { \
    verify_check(((n) & 1) == 1); \
    verify_check((n) >= -((1 << ((w)-1)) - 1)); \
    verify_check((n) <=  ((1 << ((w)-1)) - 1)); \
    if ((n) > 0) { \
        *(r) = (pre)[((n)-1)/2]; \
    } else { \
        secp256k1_gej_neg((r), &(pre)[(-(n)-1)/2]); \
    } \
} while(0)
#define ecmult_table_get_ge_storage(r,pre,n,w) do { \
    verify_check(((n) & 1) == 1); \
    verify_check((n) >= -((1 << ((w)-1)) - 1)); \
    verify_check((n) <=  ((1 << ((w)-1)) - 1)); \
    if ((n) > 0) { \
        secp256k1_ge_from_storage((r), &(pre)[((n)-1)/2]); \
    } else { \
        secp256k1_ge_from_storage((r), &(pre)[(-(n)-1)/2]); \
        secp256k1_ge_neg((r), (r)); \
    } \
} while(0)

static void secp256k1_ecmult_context_init(secp256k1_ecmult_context_t *ctx) {
    ctx->pre_g = null;
#ifdef use_endomorphism
    ctx->pre_g_128 = null;
#endif
}

static void secp256k1_ecmult_context_build(secp256k1_ecmult_context_t *ctx) {
    secp256k1_gej_t gj;

    if (ctx->pre_g != null) {
        return;
    }

    /* get the generator */
    secp256k1_gej_set_ge(&gj, &secp256k1_ge_const_g);

    ctx->pre_g = (secp256k1_ge_storage_t (*)[])checked_malloc(sizeof((*ctx->pre_g)[0]) * ecmult_table_size(window_g));

    /* precompute the tables with odd multiples */
    secp256k1_ecmult_table_precomp_ge_storage_var(*ctx->pre_g, &gj, window_g);

#ifdef use_endomorphism
    {
        secp256k1_gej_t g_128j;
        int i;

        ctx->pre_g_128 = (secp256k1_ge_storage_t (*)[])checked_malloc(sizeof((*ctx->pre_g_128)[0]) * ecmult_table_size(window_g));

        /* calculate 2^128*generator */
        g_128j = gj;
        for (i = 0; i < 128; i++) {
            secp256k1_gej_double_var(&g_128j, &g_128j);
        }
        secp256k1_ecmult_table_precomp_ge_storage_var(*ctx->pre_g_128, &g_128j, window_g);
    }
#endif
}

static void secp256k1_ecmult_context_clone(secp256k1_ecmult_context_t *dst,
                                           const secp256k1_ecmult_context_t *src) {
    if (src->pre_g == null) {
        dst->pre_g = null;
    } else {
        size_t size = sizeof((*dst->pre_g)[0]) * ecmult_table_size(window_g);
        dst->pre_g = (secp256k1_ge_storage_t (*)[])checked_malloc(size);
        memcpy(dst->pre_g, src->pre_g, size);
    }
#ifdef use_endomorphism
    if (src->pre_g_128 == null) {
        dst->pre_g_128 = null;
    } else {
        size_t size = sizeof((*dst->pre_g_128)[0]) * ecmult_table_size(window_g);
        dst->pre_g_128 = (secp256k1_ge_storage_t (*)[])checked_malloc(size);
        memcpy(dst->pre_g_128, src->pre_g_128, size);
    }
#endif
}

static int secp256k1_ecmult_context_is_built(const secp256k1_ecmult_context_t *ctx) {
    return ctx->pre_g != null;
}

static void secp256k1_ecmult_context_clear(secp256k1_ecmult_context_t *ctx) {
    free(ctx->pre_g);
#ifdef use_endomorphism
    free(ctx->pre_g_128);
#endif
    secp256k1_ecmult_context_init(ctx);
}

/** convert a number to wnaf notation. the number becomes represented by sum(2^i * wnaf[i], i=0..bits),
 *  with the following guarantees:
 *  - each wnaf[i] is either 0, or an odd integer between -(1<<(w-1) - 1) and (1<<(w-1) - 1)
 *  - two non-zero entries in wnaf are separated by at least w-1 zeroes.
 *  - the number of set values in wnaf is returned. this number is at most 256, and at most one more
 *  - than the number of bits in the (absolute value) of the input.
 */
static int secp256k1_ecmult_wnaf(int *wnaf, const secp256k1_scalar_t *a, int w) {
    secp256k1_scalar_t s = *a;
    int set_bits = 0;
    int bit = 0;
    int sign = 1;

    if (secp256k1_scalar_get_bits(&s, 255, 1)) {
        secp256k1_scalar_negate(&s, &s);
        sign = -1;
    }

    while (bit < 256) {
        int now;
        int word;
        if (secp256k1_scalar_get_bits(&s, bit, 1) == 0) {
            bit++;
            continue;
        }
        while (set_bits < bit) {
            wnaf[set_bits++] = 0;
        }
        now = w;
        if (bit + now > 256) {
            now = 256 - bit;
        }
        word = secp256k1_scalar_get_bits_var(&s, bit, now);
        if (word & (1 << (w-1))) {
            secp256k1_scalar_add_bit(&s, bit + w);
            wnaf[set_bits++] = sign * (word - (1 << w));
        } else {
            wnaf[set_bits++] = sign * word;
        }
        bit += now;
    }
    return set_bits;
}

static void secp256k1_ecmult(const secp256k1_ecmult_context_t *ctx, secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_scalar_t *na, const secp256k1_scalar_t *ng) {
    secp256k1_gej_t tmpj;
    secp256k1_gej_t pre_a[ecmult_table_size(window_a)];
    secp256k1_ge_t tmpa;
#ifdef use_endomorphism
    secp256k1_gej_t pre_a_lam[ecmult_table_size(window_a)];
    secp256k1_scalar_t na_1, na_lam;
    /* splitted g factors. */
    secp256k1_scalar_t ng_1, ng_128;
    int wnaf_na_1[130];
    int wnaf_na_lam[130];
    int bits_na_1;
    int bits_na_lam;
    int wnaf_ng_1[129];
    int bits_ng_1;
    int wnaf_ng_128[129];
    int bits_ng_128;
#else
    int wnaf_na[256];
    int bits_na;
    int wnaf_ng[257];
    int bits_ng;
#endif
    int i;
    int bits;

#ifdef use_endomorphism
    /* split na into na_1 and na_lam (where na = na_1 + na_lam*lambda, and na_1 and na_lam are ~128 bit) */
    secp256k1_scalar_split_lambda_var(&na_1, &na_lam, na);

    /* build wnaf representation for na_1 and na_lam. */
    bits_na_1   = secp256k1_ecmult_wnaf(wnaf_na_1,   &na_1,   window_a);
    bits_na_lam = secp256k1_ecmult_wnaf(wnaf_na_lam, &na_lam, window_a);
    verify_check(bits_na_1 <= 130);
    verify_check(bits_na_lam <= 130);
    bits = bits_na_1;
    if (bits_na_lam > bits) {
        bits = bits_na_lam;
    }
#else
    /* build wnaf representation for na. */
    bits_na     = secp256k1_ecmult_wnaf(wnaf_na,     na,      window_a);
    bits = bits_na;
#endif

    /* calculate odd multiples of a */
    secp256k1_ecmult_table_precomp_gej_var(pre_a, a, window_a);

#ifdef use_endomorphism
    for (i = 0; i < ecmult_table_size(window_a); i++) {
        secp256k1_gej_mul_lambda(&pre_a_lam[i], &pre_a[i]);
    }

    /* split ng into ng_1 and ng_128 (where gn = gn_1 + gn_128*2^128, and gn_1 and gn_128 are ~128 bit) */
    secp256k1_scalar_split_128(&ng_1, &ng_128, ng);

    /* build wnaf representation for ng_1 and ng_128 */
    bits_ng_1   = secp256k1_ecmult_wnaf(wnaf_ng_1,   &ng_1,   window_g);
    bits_ng_128 = secp256k1_ecmult_wnaf(wnaf_ng_128, &ng_128, window_g);
    if (bits_ng_1 > bits) {
        bits = bits_ng_1;
    }
    if (bits_ng_128 > bits) {
        bits = bits_ng_128;
    }
#else
    bits_ng     = secp256k1_ecmult_wnaf(wnaf_ng,     ng,      window_g);
    if (bits_ng > bits) {
        bits = bits_ng;
    }
#endif

    secp256k1_gej_set_infinity(r);

    for (i = bits-1; i >= 0; i--) {
        int n;
        secp256k1_gej_double_var(r, r);
#ifdef use_endomorphism
        if (i < bits_na_1 && (n = wnaf_na_1[i])) {
            ecmult_table_get_gej(&tmpj, pre_a, n, window_a);
            secp256k1_gej_add_var(r, r, &tmpj);
        }
        if (i < bits_na_lam && (n = wnaf_na_lam[i])) {
            ecmult_table_get_gej(&tmpj, pre_a_lam, n, window_a);
            secp256k1_gej_add_var(r, r, &tmpj);
        }
        if (i < bits_ng_1 && (n = wnaf_ng_1[i])) {
            ecmult_table_get_ge_storage(&tmpa, *ctx->pre_g, n, window_g);
            secp256k1_gej_add_ge_var(r, r, &tmpa);
        }
        if (i < bits_ng_128 && (n = wnaf_ng_128[i])) {
            ecmult_table_get_ge_storage(&tmpa, *ctx->pre_g_128, n, window_g);
            secp256k1_gej_add_ge_var(r, r, &tmpa);
        }
#else
        if (i < bits_na && (n = wnaf_na[i])) {
            ecmult_table_get_gej(&tmpj, pre_a, n, window_a);
            secp256k1_gej_add_var(r, r, &tmpj);
        }
        if (i < bits_ng && (n = wnaf_ng[i])) {
            ecmult_table_get_ge_storage(&tmpa, *ctx->pre_g, n, window_g);
            secp256k1_gej_add_ge_var(r, r, &tmpa);
        }
#endif
    }
}

#endif
