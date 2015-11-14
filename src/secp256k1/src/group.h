/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_group_
#define _secp256k1_group_

#include "num.h"
#include "field.h"

/** a group element of the secp256k1 curve, in affine coordinates. */
typedef struct {
    secp256k1_fe_t x;
    secp256k1_fe_t y;
    int infinity; /* whether this represents the point at infinity */
} secp256k1_ge_t;

#define secp256k1_ge_const(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {secp256k1_fe_const((a),(b),(c),(d),(e),(f),(g),(h)), secp256k1_fe_const((i),(j),(k),(l),(m),(n),(o),(p)), 0}
#define secp256k1_ge_const_infinity {secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 0), secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 0), 1}

/** a group element of the secp256k1 curve, in jacobian coordinates. */
typedef struct {
    secp256k1_fe_t x; /* actual x: x/z^2 */
    secp256k1_fe_t y; /* actual y: y/z^3 */
    secp256k1_fe_t z;
    int infinity; /* whether this represents the point at infinity */
} secp256k1_gej_t;

#define secp256k1_gej_const(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {secp256k1_fe_const((a),(b),(c),(d),(e),(f),(g),(h)), secp256k1_fe_const((i),(j),(k),(l),(m),(n),(o),(p)), secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 1), 0}
#define secp256k1_gej_const_infinity {secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 0), secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 0), secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 0), 1}

typedef struct {
    secp256k1_fe_storage_t x;
    secp256k1_fe_storage_t y;
} secp256k1_ge_storage_t;

#define secp256k1_ge_storage_const(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {secp256k1_fe_storage_const((a),(b),(c),(d),(e),(f),(g),(h)), secp256k1_fe_storage_const((i),(j),(k),(l),(m),(n),(o),(p))}

/** set a group element equal to the point at infinity */
static void secp256k1_ge_set_infinity(secp256k1_ge_t *r);

/** set a group element equal to the point with given x and y coordinates */
static void secp256k1_ge_set_xy(secp256k1_ge_t *r, const secp256k1_fe_t *x, const secp256k1_fe_t *y);

/** set a group element (affine) equal to the point with the given x coordinate, and given oddness
 *  for y. return value indicates whether the result is valid. */
static int secp256k1_ge_set_xo_var(secp256k1_ge_t *r, const secp256k1_fe_t *x, int odd);

/** check whether a group element is the point at infinity. */
static int secp256k1_ge_is_infinity(const secp256k1_ge_t *a);

/** check whether a group element is valid (i.e., on the curve). */
static int secp256k1_ge_is_valid_var(const secp256k1_ge_t *a);

static void secp256k1_ge_neg(secp256k1_ge_t *r, const secp256k1_ge_t *a);

/** set a group element equal to another which is given in jacobian coordinates */
static void secp256k1_ge_set_gej(secp256k1_ge_t *r, secp256k1_gej_t *a);

/** set a batch of group elements equal to the inputs given in jacobian coordinates */
static void secp256k1_ge_set_all_gej_var(size_t len, secp256k1_ge_t *r, const secp256k1_gej_t *a);


/** set a group element (jacobian) equal to the point at infinity. */
static void secp256k1_gej_set_infinity(secp256k1_gej_t *r);

/** set a group element (jacobian) equal to the point with given x and y coordinates. */
static void secp256k1_gej_set_xy(secp256k1_gej_t *r, const secp256k1_fe_t *x, const secp256k1_fe_t *y);

/** set a group element (jacobian) equal to another which is given in affine coordinates. */
static void secp256k1_gej_set_ge(secp256k1_gej_t *r, const secp256k1_ge_t *a);

/** compare the x coordinate of a group element (jacobian). */
static int secp256k1_gej_eq_x_var(const secp256k1_fe_t *x, const secp256k1_gej_t *a);

/** set r equal to the inverse of a (i.e., mirrored around the x axis) */
static void secp256k1_gej_neg(secp256k1_gej_t *r, const secp256k1_gej_t *a);

/** check whether a group element is the point at infinity. */
static int secp256k1_gej_is_infinity(const secp256k1_gej_t *a);

/** set r equal to the double of a. */
static void secp256k1_gej_double_var(secp256k1_gej_t *r, const secp256k1_gej_t *a);

/** set r equal to the sum of a and b. */
static void secp256k1_gej_add_var(secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_gej_t *b);

/** set r equal to the sum of a and b (with b given in affine coordinates, and not infinity). */
static void secp256k1_gej_add_ge(secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_ge_t *b);

/** set r equal to the sum of a and b (with b given in affine coordinates). this is more efficient
    than secp256k1_gej_add_var. it is identical to secp256k1_gej_add_ge but without constant-time
    guarantee, and b is allowed to be infinity. */
static void secp256k1_gej_add_ge_var(secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_ge_t *b);

#ifdef use_endomorphism
/** set r to be equal to lambda times a, where lambda is chosen in a way such that this is very fast. */
static void secp256k1_gej_mul_lambda(secp256k1_gej_t *r, const secp256k1_gej_t *a);
#endif

/** clear a secp256k1_gej_t to prevent leaking sensitive information. */
static void secp256k1_gej_clear(secp256k1_gej_t *r);

/** clear a secp256k1_ge_t to prevent leaking sensitive information. */
static void secp256k1_ge_clear(secp256k1_ge_t *r);

/** convert a group element to the storage type. */
static void secp256k1_ge_to_storage(secp256k1_ge_storage_t *r, const secp256k1_ge_t*);

/** convert a group element back from the storage type. */
static void secp256k1_ge_from_storage(secp256k1_ge_t *r, const secp256k1_ge_storage_t*);

/** if flag is true, set *r equal to *a; otherwise leave it. constant-time. */
static void secp256k1_ge_storage_cmov(secp256k1_ge_storage_t *r, const secp256k1_ge_storage_t *a, int flag);

/** rescale a jacobian point by b which must be non-zero. constant-time. */
static void secp256k1_gej_rescale(secp256k1_gej_t *r, const secp256k1_fe_t *b);

#endif
