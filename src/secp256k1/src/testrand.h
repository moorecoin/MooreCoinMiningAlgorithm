/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_testrand_h_
#define _secp256k1_testrand_h_

#if defined have_config_h
#include "libsecp256k1-config.h"
#endif

/* a non-cryptographic rng used only for test infrastructure. */

/** seed the pseudorandom number generator for testing. */
secp256k1_inline static void secp256k1_rand_seed(const unsigned char *seed16);

/** generate a pseudorandom 32-bit number. */
static uint32_t secp256k1_rand32(void);

/** generate a pseudorandom 32-byte array. */
static void secp256k1_rand256(unsigned char *b32);

/** generate a pseudorandom 32-byte array with long sequences of zero and one bits. */
static void secp256k1_rand256_test(unsigned char *b32);

#endif
