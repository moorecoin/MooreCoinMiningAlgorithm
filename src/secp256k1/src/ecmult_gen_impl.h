/**********************************************************************
 * copyright (c) 2013, 2014, 2015 pieter wuille, gregory maxwell      *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_ecmult_gen_impl_h_
#define _secp256k1_ecmult_gen_impl_h_

#include "scalar.h"
#include "group.h"
#include "ecmult_gen.h"
#include "hash_impl.h"

static void secp256k1_ecmult_gen_context_init(secp256k1_ecmult_gen_context_t *ctx) {
    ctx->prec = null;
}

static void secp256k1_ecmult_gen_context_build(secp256k1_ecmult_gen_context_t *ctx) {
    secp256k1_ge_t prec[1024];
    secp256k1_gej_t gj;
    secp256k1_gej_t nums_gej;
    int i, j;

    if (ctx->prec != null) {
        return;
    }

    ctx->prec = (secp256k1_ge_storage_t (*)[64][16])checked_malloc(sizeof(*ctx->prec));

    /* get the generator */
    secp256k1_gej_set_ge(&gj, &secp256k1_ge_const_g);

    /* construct a group element with no known corresponding scalar (nothing up my sleeve). */
    {
        static const unsigned char nums_b32[33] = "the scalar for this x is unknown";
        secp256k1_fe_t nums_x;
        secp256k1_ge_t nums_ge;
        verify_check(secp256k1_fe_set_b32(&nums_x, nums_b32));
        verify_check(secp256k1_ge_set_xo_var(&nums_ge, &nums_x, 0));
        secp256k1_gej_set_ge(&nums_gej, &nums_ge);
        /* add g to make the bits in x uniformly distributed. */
        secp256k1_gej_add_ge_var(&nums_gej, &nums_gej, &secp256k1_ge_const_g);
    }

    /* compute prec. */
    {
        secp256k1_gej_t precj[1024]; /* jacobian versions of prec. */
        secp256k1_gej_t gbase;
        secp256k1_gej_t numsbase;
        gbase = gj; /* 16^j * g */
        numsbase = nums_gej; /* 2^j * nums. */
        for (j = 0; j < 64; j++) {
            /* set precj[j*16 .. j*16+15] to (numsbase, numsbase + gbase, ..., numsbase + 15*gbase). */
            precj[j*16] = numsbase;
            for (i = 1; i < 16; i++) {
                secp256k1_gej_add_var(&precj[j*16 + i], &precj[j*16 + i - 1], &gbase);
            }
            /* multiply gbase by 16. */
            for (i = 0; i < 4; i++) {
                secp256k1_gej_double_var(&gbase, &gbase);
            }
            /* multiply numbase by 2. */
            secp256k1_gej_double_var(&numsbase, &numsbase);
            if (j == 62) {
                /* in the last iteration, numsbase is (1 - 2^j) * nums instead. */
                secp256k1_gej_neg(&numsbase, &numsbase);
                secp256k1_gej_add_var(&numsbase, &numsbase, &nums_gej);
            }
        }
        secp256k1_ge_set_all_gej_var(1024, prec, precj);
    }
    for (j = 0; j < 64; j++) {
        for (i = 0; i < 16; i++) {
            secp256k1_ge_to_storage(&(*ctx->prec)[j][i], &prec[j*16 + i]);
        }
    }
    secp256k1_ecmult_gen_blind(ctx, null);
}

static int secp256k1_ecmult_gen_context_is_built(const secp256k1_ecmult_gen_context_t* ctx) {
    return ctx->prec != null;
}

static void secp256k1_ecmult_gen_context_clone(secp256k1_ecmult_gen_context_t *dst,
                                               const secp256k1_ecmult_gen_context_t *src) {
    if (src->prec == null) {
        dst->prec = null;
    } else {
        dst->prec = (secp256k1_ge_storage_t (*)[64][16])checked_malloc(sizeof(*dst->prec));
        memcpy(dst->prec, src->prec, sizeof(*dst->prec));
        dst->initial = src->initial;
        dst->blind = src->blind;
    }
}

static void secp256k1_ecmult_gen_context_clear(secp256k1_ecmult_gen_context_t *ctx) {
    free(ctx->prec);
    secp256k1_scalar_clear(&ctx->blind);
    secp256k1_gej_clear(&ctx->initial);
    ctx->prec = null;
}

static void secp256k1_ecmult_gen(const secp256k1_ecmult_gen_context_t *ctx, secp256k1_gej_t *r, const secp256k1_scalar_t *gn) {
    secp256k1_ge_t add;
    secp256k1_ge_storage_t adds;
    secp256k1_scalar_t gnb;
    int bits;
    int i, j;
    memset(&adds, 0, sizeof(adds));
    *r = ctx->initial;
    /* blind scalar/point multiplication by computing (n-b)g + bg instead of ng. */
    secp256k1_scalar_add(&gnb, gn, &ctx->blind);
    add.infinity = 0;
    for (j = 0; j < 64; j++) {
        bits = secp256k1_scalar_get_bits(&gnb, j * 4, 4);
        for (i = 0; i < 16; i++) {
            /** this uses a conditional move to avoid any secret data in array indexes.
             *   _any_ use of secret indexes has been demonstrated to result in timing
             *   sidechannels, even when the cache-line access patterns are uniform.
             *  see also:
             *   "a word of warning", ches 2013 rump session, by daniel j. bernstein and peter schwabe
             *    (https://cryptojedi.org/peter/data/chesrump-20130822.pdf) and
             *   "cache attacks and countermeasures: the case of aes", rsa 2006,
             *    by dag arne osvik, adi shamir, and eran tromer
             *    (http://www.tau.ac.il/~tromer/papers/cache.pdf)
             */
            secp256k1_ge_storage_cmov(&adds, &(*ctx->prec)[j][i], i == bits);
        }
        secp256k1_ge_from_storage(&add, &adds);
        secp256k1_gej_add_ge(r, r, &add);
    }
    bits = 0;
    secp256k1_ge_clear(&add);
    secp256k1_scalar_clear(&gnb);
}

/* setup blinding values for secp256k1_ecmult_gen. */
static void secp256k1_ecmult_gen_blind(secp256k1_ecmult_gen_context_t *ctx, const unsigned char *seed32) {
    secp256k1_scalar_t b;
    secp256k1_gej_t gb;
    secp256k1_fe_t s;
    unsigned char nonce32[32];
    secp256k1_rfc6979_hmac_sha256_t rng;
    int retry;
    if (!seed32) {
        /* when seed is null, reset the initial point and blinding value. */
        secp256k1_gej_set_ge(&ctx->initial, &secp256k1_ge_const_g);
        secp256k1_gej_neg(&ctx->initial, &ctx->initial);
        secp256k1_scalar_set_int(&ctx->blind, 1);
    }
    /* the prior blinding value (if not reset) is chained forward by including it in the hash. */
    secp256k1_scalar_get_b32(nonce32, &ctx->blind);
    /** using a csprng allows a failure free interface, avoids needing large amounts of random data,
     *   and guards against weak or adversarial seeds.  this is a simpler and safer interface than
     *   asking the caller for blinding values directly and expecting them to retry on failure.
     */
    secp256k1_rfc6979_hmac_sha256_initialize(&rng, seed32 ? seed32 : nonce32, 32, nonce32, 32, null, 0);
    /* retry for out of range results to achieve uniformity. */
    do {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, nonce32, 32);
        retry = !secp256k1_fe_set_b32(&s, nonce32);
        retry |= secp256k1_fe_is_zero(&s);
    } while (retry);
    /* randomize the projection to defend against multiplier sidechannels. */
    secp256k1_gej_rescale(&ctx->initial, &s);
    secp256k1_fe_clear(&s);
    do {
        secp256k1_rfc6979_hmac_sha256_generate(&rng, nonce32, 32);
        secp256k1_scalar_set_b32(&b, nonce32, &retry);
        /* a blinding value of 0 works, but would undermine the projection hardening. */
        retry |= secp256k1_scalar_is_zero(&b);
    } while (retry);
    secp256k1_rfc6979_hmac_sha256_finalize(&rng);
    memset(nonce32, 0, 32);
    secp256k1_ecmult_gen(ctx, &gb, &b);
    secp256k1_scalar_negate(&b, &b);
    ctx->blind = b;
    ctx->initial = gb;
    secp256k1_scalar_clear(&b);
    secp256k1_gej_clear(&gb);
}

#endif