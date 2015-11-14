/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_field_
#define _secp256k1_field_

/** field element module.
 *
 *  field elements can be represented in several ways, but code accessing
 *  it (and implementations) need to take certain properaties into account:
 *  - each field element can be normalized or not.
 *  - each field element has a magnitude, which represents how far away
 *    its representation is away from normalization. normalized elements
 *    always have a magnitude of 1, but a magnitude of 1 doesn't imply
 *    normality.
 */

#if defined have_config_h
#include "libsecp256k1-config.h"
#endif

#if defined(use_field_10x26)
#include "field_10x26.h"
#elif defined(use_field_5x52)
#include "field_5x52.h"
#else
#error "please select field implementation"
#endif

/** normalize a field element. */
static void secp256k1_fe_normalize(secp256k1_fe_t *r);

/** weakly normalize a field element: reduce it magnitude to 1, but don't fully normalize. */
static void secp256k1_fe_normalize_weak(secp256k1_fe_t *r);

/** normalize a field element, without constant-time guarantee. */
static void secp256k1_fe_normalize_var(secp256k1_fe_t *r);

/** verify whether a field element represents zero i.e. would normalize to a zero value. the field
 *  implementation may optionally normalize the input, but this should not be relied upon. */
static int secp256k1_fe_normalizes_to_zero(secp256k1_fe_t *r);

/** verify whether a field element represents zero i.e. would normalize to a zero value. the field
 *  implementation may optionally normalize the input, but this should not be relied upon. */
static int secp256k1_fe_normalizes_to_zero_var(secp256k1_fe_t *r);

/** set a field element equal to a small integer. resulting field element is normalized. */
static void secp256k1_fe_set_int(secp256k1_fe_t *r, int a);

/** verify whether a field element is zero. requires the input to be normalized. */
static int secp256k1_fe_is_zero(const secp256k1_fe_t *a);

/** check the "oddness" of a field element. requires the input to be normalized. */
static int secp256k1_fe_is_odd(const secp256k1_fe_t *a);

/** compare two field elements. requires magnitude-1 inputs. */
static int secp256k1_fe_equal_var(const secp256k1_fe_t *a, const secp256k1_fe_t *b);

/** compare two field elements. requires both inputs to be normalized */
static int secp256k1_fe_cmp_var(const secp256k1_fe_t *a, const secp256k1_fe_t *b);

/** set a field element equal to 32-byte big endian value. if succesful, the resulting field element is normalized. */
static int secp256k1_fe_set_b32(secp256k1_fe_t *r, const unsigned char *a);

/** convert a field element to a 32-byte big endian value. requires the input to be normalized */
static void secp256k1_fe_get_b32(unsigned char *r, const secp256k1_fe_t *a);

/** set a field element equal to the additive inverse of another. takes a maximum magnitude of the input
 *  as an argument. the magnitude of the output is one higher. */
static void secp256k1_fe_negate(secp256k1_fe_t *r, const secp256k1_fe_t *a, int m);

/** multiplies the passed field element with a small integer constant. multiplies the magnitude by that
 *  small integer. */
static void secp256k1_fe_mul_int(secp256k1_fe_t *r, int a);

/** adds a field element to another. the result has the sum of the inputs' magnitudes as magnitude. */
static void secp256k1_fe_add(secp256k1_fe_t *r, const secp256k1_fe_t *a);

/** sets a field element to be the product of two others. requires the inputs' magnitudes to be at most 8.
 *  the output magnitude is 1 (but not guaranteed to be normalized). */
static void secp256k1_fe_mul(secp256k1_fe_t *r, const secp256k1_fe_t *a, const secp256k1_fe_t * secp256k1_restrict b);

/** sets a field element to be the square of another. requires the input's magnitude to be at most 8.
 *  the output magnitude is 1 (but not guaranteed to be normalized). */
static void secp256k1_fe_sqr(secp256k1_fe_t *r, const secp256k1_fe_t *a);

/** sets a field element to be the (modular) square root (if any exist) of another. requires the
 *  input's magnitude to be at most 8. the output magnitude is 1 (but not guaranteed to be
 *  normalized). return value indicates whether a square root was found. */
static int secp256k1_fe_sqrt_var(secp256k1_fe_t *r, const secp256k1_fe_t *a);

/** sets a field element to be the (modular) inverse of another. requires the input's magnitude to be
 *  at most 8. the output magnitude is 1 (but not guaranteed to be normalized). */
static void secp256k1_fe_inv(secp256k1_fe_t *r, const secp256k1_fe_t *a);

/** potentially faster version of secp256k1_fe_inv, without constant-time guarantee. */
static void secp256k1_fe_inv_var(secp256k1_fe_t *r, const secp256k1_fe_t *a);

/** calculate the (modular) inverses of a batch of field elements. requires the inputs' magnitudes to be
 *  at most 8. the output magnitudes are 1 (but not guaranteed to be normalized). the inputs and
 *  outputs must not overlap in memory. */
static void secp256k1_fe_inv_all_var(size_t len, secp256k1_fe_t *r, const secp256k1_fe_t *a);

/** convert a field element to the storage type. */
static void secp256k1_fe_to_storage(secp256k1_fe_storage_t *r, const secp256k1_fe_t*);

/** convert a field element back from the storage type. */
static void secp256k1_fe_from_storage(secp256k1_fe_t *r, const secp256k1_fe_storage_t*);

/** if flag is true, set *r equal to *a; otherwise leave it. constant-time. */
static void secp256k1_fe_storage_cmov(secp256k1_fe_storage_t *r, const secp256k1_fe_storage_t *a, int flag);

/** if flag is true, set *r equal to *a; otherwise leave it. constant-time. */
static void secp256k1_fe_cmov(secp256k1_fe_t *r, const secp256k1_fe_t *a, int flag);

#endif
