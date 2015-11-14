/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_field_repr_
#define _secp256k1_field_repr_

#include <stdint.h>

typedef struct {
    /* x = sum(i=0..9, elem[i]*2^26) mod n */
    uint32_t n[10];
#ifdef verify
    int magnitude;
    int normalized;
#endif
} secp256k1_fe_t;

/* unpacks a constant into a overlapping multi-limbed fe element. */
#define secp256k1_fe_const_inner(d7, d6, d5, d4, d3, d2, d1, d0) { \
    (d0) & 0x3fffffful, \
    ((d0) >> 26) | ((d1) & 0xffffful) << 6, \
    ((d1) >> 20) | ((d2) & 0x3ffful) << 12, \
    ((d2) >> 14) | ((d3) & 0xfful) << 18, \
    ((d3) >> 8) | ((d4) & 0x3) << 24, \
    ((d4) >> 2) & 0x3fffffful, \
    ((d4) >> 28) | ((d5) & 0x3ffffful) << 4, \
    ((d5) >> 22) | ((d6) & 0xffff) << 10, \
    ((d6) >> 16) | ((d7) & 0x3ff) << 16, \
    ((d7) >> 10) \
}

#ifdef verify
#define secp256k1_fe_const(d7, d6, d5, d4, d3, d2, d1, d0) {secp256k1_fe_const_inner((d7), (d6), (d5), (d4), (d3), (d2), (d1), (d0)), 1, 1}
#else
#define secp256k1_fe_const(d7, d6, d5, d4, d3, d2, d1, d0) {secp256k1_fe_const_inner((d7), (d6), (d5), (d4), (d3), (d2), (d1), (d0))}
#endif

typedef struct {
    uint32_t n[8];
} secp256k1_fe_storage_t;

#define secp256k1_fe_storage_const(d7, d6, d5, d4, d3, d2, d1, d0) {{ (d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7) }}

#endif
