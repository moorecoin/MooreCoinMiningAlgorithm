/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_num_
#define _secp256k1_num_

#ifndef use_num_none

#if defined have_config_h
#include "libsecp256k1-config.h"
#endif

#if defined(use_num_gmp)
#include "num_gmp.h"
#else
#error "please select num implementation"
#endif

/** copy a number. */
static void secp256k1_num_copy(secp256k1_num_t *r, const secp256k1_num_t *a);

/** convert a number's absolute value to a binary big-endian string.
 *  there must be enough place. */
static void secp256k1_num_get_bin(unsigned char *r, unsigned int rlen, const secp256k1_num_t *a);

/** set a number to the value of a binary big-endian string. */
static void secp256k1_num_set_bin(secp256k1_num_t *r, const unsigned char *a, unsigned int alen);

/** compute a modular inverse. the input must be less than the modulus. */
static void secp256k1_num_mod_inverse(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *m);

/** compare the absolute value of two numbers. */
static int secp256k1_num_cmp(const secp256k1_num_t *a, const secp256k1_num_t *b);

/** test whether two number are equal (including sign). */
static int secp256k1_num_eq(const secp256k1_num_t *a, const secp256k1_num_t *b);

/** add two (signed) numbers. */
static void secp256k1_num_add(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b);

/** subtract two (signed) numbers. */
static void secp256k1_num_sub(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b);

/** multiply two (signed) numbers. */
static void secp256k1_num_mul(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b);

/** replace a number by its remainder modulo m. m's sign is ignored. the result is a number between 0 and m-1,
    even if r was negative. */
static void secp256k1_num_mod(secp256k1_num_t *r, const secp256k1_num_t *m);

/** right-shift the passed number by bits bits. */
static void secp256k1_num_shift(secp256k1_num_t *r, int bits);

/** check whether a number is zero. */
static int secp256k1_num_is_zero(const secp256k1_num_t *a);

/** check whether a number is strictly negative. */
static int secp256k1_num_is_neg(const secp256k1_num_t *a);

/** change a number's sign. */
static void secp256k1_num_negate(secp256k1_num_t *r);

#endif

#endif
