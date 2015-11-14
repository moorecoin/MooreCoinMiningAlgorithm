/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_ecmult_
#define _secp256k1_ecmult_

#include "num.h"
#include "group.h"

typedef struct {
    /* for accelerating the computation of a*p + b*g: */
    secp256k1_ge_storage_t (*pre_g)[];    /* odd multiples of the generator */
#ifdef use_endomorphism
    secp256k1_ge_storage_t (*pre_g_128)[]; /* odd multiples of 2^128*generator */
#endif
} secp256k1_ecmult_context_t;

static void secp256k1_ecmult_context_init(secp256k1_ecmult_context_t *ctx);
static void secp256k1_ecmult_context_build(secp256k1_ecmult_context_t *ctx);
static void secp256k1_ecmult_context_clone(secp256k1_ecmult_context_t *dst,
                                           const secp256k1_ecmult_context_t *src);
static void secp256k1_ecmult_context_clear(secp256k1_ecmult_context_t *ctx);
static int secp256k1_ecmult_context_is_built(const secp256k1_ecmult_context_t *ctx);

/** double multiply: r = na*a + ng*g */
static void secp256k1_ecmult(const secp256k1_ecmult_context_t *ctx, secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_scalar_t *na, const secp256k1_scalar_t *ng);

#endif
