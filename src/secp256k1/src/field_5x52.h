/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_field_repr_
#define _secp256k1_field_repr_

#include <stdint.h>

typedef struct {
    /* x = sum(i=0..4, elem[i]*2^52) mod n */
    uint64_t n[5];
#ifdef verify
    int magnitude;
    int normalized;
#endif
} secp256k1_fe_t;

/* unpacks a constant into a overlapping multi-limbed fe element. */
#define secp256k1_fe_const_inner(d7, d6, d5, d4, d3, d2, d1, d0) { \
    (d0) | ((uint64_t)(d1) & 0xffffful) << 32, \
    ((d1) >> 20) | ((uint64_t)(d2)) << 12 | ((uint64_t)(d3) & 0xfful) << 44, \
    ((d3) >> 8) | ((uint64_t)(d4) & 0xffffffful) << 24, \
    ((d4) >> 28) | ((uint64_t)(d5)) << 4 | ((uint64_t)(d6) & 0xfffful) << 36, \
    ((d6) >> 16) | ((uint64_t)(d7)) << 16 \
}

#ifdef verify
#define secp256k1_fe_const(d7, d6, d5, d4, d3, d2, d1, d0) {secp256k1_fe_const_inner((d7), (d6), (d5), (d4), (d3), (d2), (d1), (d0)), 1, 1}
#else
#define secp256k1_fe_const(d7, d6, d5, d4, d3, d2, d1, d0) {secp256k1_fe_const_inner((d7), (d6), (d5), (d4), (d3), (d2), (d1), (d0))}
#endif

typedef struct {
    uint64_t n[4];
} secp256k1_fe_storage_t;

#define secp256k1_fe_storage_const(d7, d6, d5, d4, d3, d2, d1, d0) {{ \
    (d0) | ((uint64_t)(d1)) << 32, \
    (d2) | ((uint64_t)(d3)) << 32, \
    (d4) | ((uint64_t)(d5)) << 32, \
    (d6) | ((uint64_t)(d7)) << 32 \
}}

#endif
