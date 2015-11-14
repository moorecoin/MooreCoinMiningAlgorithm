/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_num_repr_
#define _secp256k1_num_repr_

#include <gmp.h>

#define num_limbs ((256+gmp_numb_bits-1)/gmp_numb_bits)

typedef struct {
    mp_limb_t data[2*num_limbs];
    int neg;
    int limbs;
} secp256k1_num_t;

#endif
