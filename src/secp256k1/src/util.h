/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_util_h_
#define _secp256k1_util_h_

#if defined have_config_h
#include "libsecp256k1-config.h"
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifdef deterministic
#define test_failure(msg) do { \
    fprintf(stderr, "%s\n", msg); \
    abort(); \
} while(0);
#else
#define test_failure(msg) do { \
    fprintf(stderr, "%s:%d: %s\n", __file__, __line__, msg); \
    abort(); \
} while(0)
#endif

#ifdef have_builtin_expect
#define expect(x,c) __builtin_expect((x),(c))
#else
#define expect(x,c) (x)
#endif

#ifdef deterministic
#define check(cond) do { \
    if (expect(!(cond), 0)) { \
        test_failure("test condition failed"); \
    } \
} while(0)
#else
#define check(cond) do { \
    if (expect(!(cond), 0)) { \
        test_failure("test condition failed: " #cond); \
    } \
} while(0)
#endif

/* like assert(), but safe to use on expressions with side effects. */
#ifndef ndebug
#define debug_check check
#else
#define debug_check(cond) do { (void)(cond); } while(0)
#endif

/* like debug_check(), but when verify is defined instead of ndebug not defined. */
#ifdef verify
#define verify_check check
#else
#define verify_check(cond) do { (void)(cond); } while(0)
#endif

static secp256k1_inline void *checked_malloc(size_t size) {
    void *ret = malloc(size);
    check(ret != null);
    return ret;
}

/* macro for restrict, when available and not in a verify build. */
#if defined(secp256k1_build) && defined(verify)
# define secp256k1_restrict
#else
# if (!defined(__stdc_version__) || (__stdc_version__ < 199901l) )
#  if secp256k1_gnuc_prereq(3,0)
#   define secp256k1_restrict __restrict__
#  elif (defined(_msc_ver) && _msc_ver >= 1400)
#   define secp256k1_restrict __restrict
#  else
#   define secp256k1_restrict
#  endif
# else
#  define secp256k1_restrict restrict
# endif
#endif

#if defined(_win32)
# define i64format "i64d"
# define i64uformat "i64u"
#else
# define i64format "lld"
# define i64uformat "llu"
#endif

#if defined(have___int128)
# if defined(__gnuc__)
#  define secp256k1_gnuc_ext __extension__
# else
#  define secp256k1_gnuc_ext
# endif
secp256k1_gnuc_ext typedef unsigned __int128 uint128_t;
#endif

#endif
