/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_num_impl_h_
#define _secp256k1_num_impl_h_

#if defined have_config_h
#include "libsecp256k1-config.h"
#endif

#include "num.h"

#if defined(use_num_gmp)
#include "num_gmp_impl.h"
#elif defined(use_num_none)
/* nothing. */
#else
#error "please select num implementation"
#endif

#endif
