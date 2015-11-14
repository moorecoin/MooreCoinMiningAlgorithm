/**********************************************************************
 * copyright (c) 2014 pieter wuille                                   *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_scalar_repr_
#define _secp256k1_scalar_repr_

#include <stdint.h>

/** a scalar modulo the group order of the secp256k1 curve. */
typedef struct {
    uint32_t d[8];
} secp256k1_scalar_t;

#define secp256k1_scalar_const(d7, d6, d5, d4, d3, d2, d1, d0) {{(d0), (d1), (d2), (d3), (d4), (d5), (d6), (d7)}}

#endif
