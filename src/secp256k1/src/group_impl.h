/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_group_impl_h_
#define _secp256k1_group_impl_h_

#include <string.h>

#include "num.h"
#include "field.h"
#include "group.h"

/** generator for secp256k1, value 'g' defined in
 *  "standards for efficient cryptography" (sec2) 2.7.1.
 */
static const secp256k1_ge_t secp256k1_ge_const_g = secp256k1_ge_const(
    0x79be667eul, 0xf9dcbbacul, 0x55a06295ul, 0xce870b07ul,
    0x029bfcdbul, 0x2dce28d9ul, 0x59f2815bul, 0x16f81798ul,
    0x483ada77ul, 0x26a3c465ul, 0x5da4fbfcul, 0x0e1108a8ul,
    0xfd17b448ul, 0xa6855419ul, 0x9c47d08ful, 0xfb10d4b8ul
);

static void secp256k1_ge_set_infinity(secp256k1_ge_t *r) {
    r->infinity = 1;
}

static void secp256k1_ge_set_xy(secp256k1_ge_t *r, const secp256k1_fe_t *x, const secp256k1_fe_t *y) {
    r->infinity = 0;
    r->x = *x;
    r->y = *y;
}

static int secp256k1_ge_is_infinity(const secp256k1_ge_t *a) {
    return a->infinity;
}

static void secp256k1_ge_neg(secp256k1_ge_t *r, const secp256k1_ge_t *a) {
    *r = *a;
    secp256k1_fe_normalize_weak(&r->y);
    secp256k1_fe_negate(&r->y, &r->y, 1);
}

static void secp256k1_ge_set_gej(secp256k1_ge_t *r, secp256k1_gej_t *a) {
    secp256k1_fe_t z2, z3;
    r->infinity = a->infinity;
    secp256k1_fe_inv(&a->z, &a->z);
    secp256k1_fe_sqr(&z2, &a->z);
    secp256k1_fe_mul(&z3, &a->z, &z2);
    secp256k1_fe_mul(&a->x, &a->x, &z2);
    secp256k1_fe_mul(&a->y, &a->y, &z3);
    secp256k1_fe_set_int(&a->z, 1);
    r->x = a->x;
    r->y = a->y;
}

static void secp256k1_ge_set_gej_var(secp256k1_ge_t *r, secp256k1_gej_t *a) {
    secp256k1_fe_t z2, z3;
    r->infinity = a->infinity;
    if (a->infinity) {
        return;
    }
    secp256k1_fe_inv_var(&a->z, &a->z);
    secp256k1_fe_sqr(&z2, &a->z);
    secp256k1_fe_mul(&z3, &a->z, &z2);
    secp256k1_fe_mul(&a->x, &a->x, &z2);
    secp256k1_fe_mul(&a->y, &a->y, &z3);
    secp256k1_fe_set_int(&a->z, 1);
    r->x = a->x;
    r->y = a->y;
}

static void secp256k1_ge_set_all_gej_var(size_t len, secp256k1_ge_t *r, const secp256k1_gej_t *a) {
    secp256k1_fe_t *az;
    secp256k1_fe_t *azi;
    size_t i;
    size_t count = 0;
    az = (secp256k1_fe_t *)checked_malloc(sizeof(secp256k1_fe_t) * len);
    for (i = 0; i < len; i++) {
        if (!a[i].infinity) {
            az[count++] = a[i].z;
        }
    }

    azi = (secp256k1_fe_t *)checked_malloc(sizeof(secp256k1_fe_t) * count);
    secp256k1_fe_inv_all_var(count, azi, az);
    free(az);

    count = 0;
    for (i = 0; i < len; i++) {
        r[i].infinity = a[i].infinity;
        if (!a[i].infinity) {
            secp256k1_fe_t zi2, zi3;
            secp256k1_fe_t *zi = &azi[count++];
            secp256k1_fe_sqr(&zi2, zi);
            secp256k1_fe_mul(&zi3, &zi2, zi);
            secp256k1_fe_mul(&r[i].x, &a[i].x, &zi2);
            secp256k1_fe_mul(&r[i].y, &a[i].y, &zi3);
        }
    }
    free(azi);
}

static void secp256k1_gej_set_infinity(secp256k1_gej_t *r) {
    r->infinity = 1;
    secp256k1_fe_set_int(&r->x, 0);
    secp256k1_fe_set_int(&r->y, 0);
    secp256k1_fe_set_int(&r->z, 0);
}

static void secp256k1_gej_set_xy(secp256k1_gej_t *r, const secp256k1_fe_t *x, const secp256k1_fe_t *y) {
    r->infinity = 0;
    r->x = *x;
    r->y = *y;
    secp256k1_fe_set_int(&r->z, 1);
}

static void secp256k1_gej_clear(secp256k1_gej_t *r) {
    r->infinity = 0;
    secp256k1_fe_clear(&r->x);
    secp256k1_fe_clear(&r->y);
    secp256k1_fe_clear(&r->z);
}

static void secp256k1_ge_clear(secp256k1_ge_t *r) {
    r->infinity = 0;
    secp256k1_fe_clear(&r->x);
    secp256k1_fe_clear(&r->y);
}

static int secp256k1_ge_set_xo_var(secp256k1_ge_t *r, const secp256k1_fe_t *x, int odd) {
    secp256k1_fe_t x2, x3, c;
    r->x = *x;
    secp256k1_fe_sqr(&x2, x);
    secp256k1_fe_mul(&x3, x, &x2);
    r->infinity = 0;
    secp256k1_fe_set_int(&c, 7);
    secp256k1_fe_add(&c, &x3);
    if (!secp256k1_fe_sqrt_var(&r->y, &c)) {
        return 0;
    }
    secp256k1_fe_normalize_var(&r->y);
    if (secp256k1_fe_is_odd(&r->y) != odd) {
        secp256k1_fe_negate(&r->y, &r->y, 1);
    }
    return 1;
}

static void secp256k1_gej_set_ge(secp256k1_gej_t *r, const secp256k1_ge_t *a) {
   r->infinity = a->infinity;
   r->x = a->x;
   r->y = a->y;
   secp256k1_fe_set_int(&r->z, 1);
}

static int secp256k1_gej_eq_x_var(const secp256k1_fe_t *x, const secp256k1_gej_t *a) {
    secp256k1_fe_t r, r2;
    verify_check(!a->infinity);
    secp256k1_fe_sqr(&r, &a->z); secp256k1_fe_mul(&r, &r, x);
    r2 = a->x; secp256k1_fe_normalize_weak(&r2);
    return secp256k1_fe_equal_var(&r, &r2);
}

static void secp256k1_gej_neg(secp256k1_gej_t *r, const secp256k1_gej_t *a) {
    r->infinity = a->infinity;
    r->x = a->x;
    r->y = a->y;
    r->z = a->z;
    secp256k1_fe_normalize_weak(&r->y);
    secp256k1_fe_negate(&r->y, &r->y, 1);
}

static int secp256k1_gej_is_infinity(const secp256k1_gej_t *a) {
    return a->infinity;
}

static int secp256k1_gej_is_valid_var(const secp256k1_gej_t *a) {
    secp256k1_fe_t y2, x3, z2, z6;
    if (a->infinity) {
        return 0;
    }
    /** y^2 = x^3 + 7
     *  (y/z^3)^2 = (x/z^2)^3 + 7
     *  y^2 / z^6 = x^3 / z^6 + 7
     *  y^2 = x^3 + 7*z^6
     */
    secp256k1_fe_sqr(&y2, &a->y);
    secp256k1_fe_sqr(&x3, &a->x); secp256k1_fe_mul(&x3, &x3, &a->x);
    secp256k1_fe_sqr(&z2, &a->z);
    secp256k1_fe_sqr(&z6, &z2); secp256k1_fe_mul(&z6, &z6, &z2);
    secp256k1_fe_mul_int(&z6, 7);
    secp256k1_fe_add(&x3, &z6);
    secp256k1_fe_normalize_weak(&x3);
    return secp256k1_fe_equal_var(&y2, &x3);
}

static int secp256k1_ge_is_valid_var(const secp256k1_ge_t *a) {
    secp256k1_fe_t y2, x3, c;
    if (a->infinity) {
        return 0;
    }
    /* y^2 = x^3 + 7 */
    secp256k1_fe_sqr(&y2, &a->y);
    secp256k1_fe_sqr(&x3, &a->x); secp256k1_fe_mul(&x3, &x3, &a->x);
    secp256k1_fe_set_int(&c, 7);
    secp256k1_fe_add(&x3, &c);
    secp256k1_fe_normalize_weak(&x3);
    return secp256k1_fe_equal_var(&y2, &x3);
}

static void secp256k1_gej_double_var(secp256k1_gej_t *r, const secp256k1_gej_t *a) {
    /* operations: 3 mul, 4 sqr, 0 normalize, 12 mul_int/add/negate */
    secp256k1_fe_t t1,t2,t3,t4;
    /** for secp256k1, 2q is infinity if and only if q is infinity. this is because if 2q = infinity,
     *  q must equal -q, or that q.y == -(q.y), or q.y is 0. for a point on y^2 = x^3 + 7 to have
     *  y=0, x^3 must be -7 mod p. however, -7 has no cube root mod p.
     */
    r->infinity = a->infinity;
    if (r->infinity) {
        return;
    }

    secp256k1_fe_mul(&r->z, &a->z, &a->y);
    secp256k1_fe_mul_int(&r->z, 2);       /* z' = 2*y*z (2) */
    secp256k1_fe_sqr(&t1, &a->x);
    secp256k1_fe_mul_int(&t1, 3);         /* t1 = 3*x^2 (3) */
    secp256k1_fe_sqr(&t2, &t1);           /* t2 = 9*x^4 (1) */
    secp256k1_fe_sqr(&t3, &a->y);
    secp256k1_fe_mul_int(&t3, 2);         /* t3 = 2*y^2 (2) */
    secp256k1_fe_sqr(&t4, &t3);
    secp256k1_fe_mul_int(&t4, 2);         /* t4 = 8*y^4 (2) */
    secp256k1_fe_mul(&t3, &t3, &a->x);    /* t3 = 2*x*y^2 (1) */
    r->x = t3;
    secp256k1_fe_mul_int(&r->x, 4);       /* x' = 8*x*y^2 (4) */
    secp256k1_fe_negate(&r->x, &r->x, 4); /* x' = -8*x*y^2 (5) */
    secp256k1_fe_add(&r->x, &t2);         /* x' = 9*x^4 - 8*x*y^2 (6) */
    secp256k1_fe_negate(&t2, &t2, 1);     /* t2 = -9*x^4 (2) */
    secp256k1_fe_mul_int(&t3, 6);         /* t3 = 12*x*y^2 (6) */
    secp256k1_fe_add(&t3, &t2);           /* t3 = 12*x*y^2 - 9*x^4 (8) */
    secp256k1_fe_mul(&r->y, &t1, &t3);    /* y' = 36*x^3*y^2 - 27*x^6 (1) */
    secp256k1_fe_negate(&t2, &t4, 2);     /* t2 = -8*y^4 (3) */
    secp256k1_fe_add(&r->y, &t2);         /* y' = 36*x^3*y^2 - 27*x^6 - 8*y^4 (4) */
}

static void secp256k1_gej_add_var(secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_gej_t *b) {
    /* operations: 12 mul, 4 sqr, 2 normalize, 12 mul_int/add/negate */
    secp256k1_fe_t z22, z12, u1, u2, s1, s2, h, i, i2, h2, h3, t;
    if (a->infinity) {
        *r = *b;
        return;
    }
    if (b->infinity) {
        *r = *a;
        return;
    }
    r->infinity = 0;
    secp256k1_fe_sqr(&z22, &b->z);
    secp256k1_fe_sqr(&z12, &a->z);
    secp256k1_fe_mul(&u1, &a->x, &z22);
    secp256k1_fe_mul(&u2, &b->x, &z12);
    secp256k1_fe_mul(&s1, &a->y, &z22); secp256k1_fe_mul(&s1, &s1, &b->z);
    secp256k1_fe_mul(&s2, &b->y, &z12); secp256k1_fe_mul(&s2, &s2, &a->z);
    secp256k1_fe_negate(&h, &u1, 1); secp256k1_fe_add(&h, &u2);
    secp256k1_fe_negate(&i, &s1, 1); secp256k1_fe_add(&i, &s2);
    if (secp256k1_fe_normalizes_to_zero_var(&h)) {
        if (secp256k1_fe_normalizes_to_zero_var(&i)) {
            secp256k1_gej_double_var(r, a);
        } else {
            r->infinity = 1;
        }
        return;
    }
    secp256k1_fe_sqr(&i2, &i);
    secp256k1_fe_sqr(&h2, &h);
    secp256k1_fe_mul(&h3, &h, &h2);
    secp256k1_fe_mul(&r->z, &a->z, &b->z); secp256k1_fe_mul(&r->z, &r->z, &h);
    secp256k1_fe_mul(&t, &u1, &h2);
    r->x = t; secp256k1_fe_mul_int(&r->x, 2); secp256k1_fe_add(&r->x, &h3); secp256k1_fe_negate(&r->x, &r->x, 3); secp256k1_fe_add(&r->x, &i2);
    secp256k1_fe_negate(&r->y, &r->x, 5); secp256k1_fe_add(&r->y, &t); secp256k1_fe_mul(&r->y, &r->y, &i);
    secp256k1_fe_mul(&h3, &h3, &s1); secp256k1_fe_negate(&h3, &h3, 1);
    secp256k1_fe_add(&r->y, &h3);
}

static void secp256k1_gej_add_ge_var(secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_ge_t *b) {
    /* 8 mul, 3 sqr, 4 normalize, 12 mul_int/add/negate */
    secp256k1_fe_t z12, u1, u2, s1, s2, h, i, i2, h2, h3, t;
    if (a->infinity) {
        r->infinity = b->infinity;
        r->x = b->x;
        r->y = b->y;
        secp256k1_fe_set_int(&r->z, 1);
        return;
    }
    if (b->infinity) {
        *r = *a;
        return;
    }
    r->infinity = 0;
    secp256k1_fe_sqr(&z12, &a->z);
    u1 = a->x; secp256k1_fe_normalize_weak(&u1);
    secp256k1_fe_mul(&u2, &b->x, &z12);
    s1 = a->y; secp256k1_fe_normalize_weak(&s1);
    secp256k1_fe_mul(&s2, &b->y, &z12); secp256k1_fe_mul(&s2, &s2, &a->z);
    secp256k1_fe_negate(&h, &u1, 1); secp256k1_fe_add(&h, &u2);
    secp256k1_fe_negate(&i, &s1, 1); secp256k1_fe_add(&i, &s2);
    if (secp256k1_fe_normalizes_to_zero_var(&h)) {
        if (secp256k1_fe_normalizes_to_zero_var(&i)) {
            secp256k1_gej_double_var(r, a);
        } else {
            r->infinity = 1;
        }
        return;
    }
    secp256k1_fe_sqr(&i2, &i);
    secp256k1_fe_sqr(&h2, &h);
    secp256k1_fe_mul(&h3, &h, &h2);
    r->z = a->z; secp256k1_fe_mul(&r->z, &r->z, &h);
    secp256k1_fe_mul(&t, &u1, &h2);
    r->x = t; secp256k1_fe_mul_int(&r->x, 2); secp256k1_fe_add(&r->x, &h3); secp256k1_fe_negate(&r->x, &r->x, 3); secp256k1_fe_add(&r->x, &i2);
    secp256k1_fe_negate(&r->y, &r->x, 5); secp256k1_fe_add(&r->y, &t); secp256k1_fe_mul(&r->y, &r->y, &i);
    secp256k1_fe_mul(&h3, &h3, &s1); secp256k1_fe_negate(&h3, &h3, 1);
    secp256k1_fe_add(&r->y, &h3);
}

static void secp256k1_gej_add_ge(secp256k1_gej_t *r, const secp256k1_gej_t *a, const secp256k1_ge_t *b) {
    /* operations: 7 mul, 5 sqr, 5 normalize, 17 mul_int/add/negate/cmov */
    static const secp256k1_fe_t fe_1 = secp256k1_fe_const(0, 0, 0, 0, 0, 0, 0, 1);
    secp256k1_fe_t zz, u1, u2, s1, s2, z, t, m, n, q, rr;
    int infinity;
    verify_check(!b->infinity);
    verify_check(a->infinity == 0 || a->infinity == 1);

    /** in:
     *    eric brier and marc joye, weierstrass elliptic curves and side-channel attacks.
     *    in d. naccache and p. paillier, eds., public key cryptography, vol. 2274 of lecture notes in computer science, pages 335-345. springer-verlag, 2002.
     *  we find as solution for a unified addition/doubling formula:
     *    lambda = ((x1 + x2)^2 - x1 * x2 + a) / (y1 + y2), with a = 0 for secp256k1's curve equation.
     *    x3 = lambda^2 - (x1 + x2)
     *    2*y3 = lambda * (x1 + x2 - 2 * x3) - (y1 + y2).
     *
     *  substituting x_i = xi / zi^2 and yi = yi / zi^3, for i=1,2,3, gives:
     *    u1 = x1*z2^2, u2 = x2*z1^2
     *    s1 = y1*z2^3, s2 = y2*z1^3
     *    z = z1*z2
     *    t = u1+u2
     *    m = s1+s2
     *    q = t*m^2
     *    r = t^2-u1*u2
     *    x3 = 4*(r^2-q)
     *    y3 = 4*(r*(3*q-2*r^2)-m^4)
     *    z3 = 2*m*z
     *  (note that the paper uses xi = xi / zi and yi = yi / zi instead.)
     */

    secp256k1_fe_sqr(&zz, &a->z);                       /* z = z1^2 */
    u1 = a->x; secp256k1_fe_normalize_weak(&u1);        /* u1 = u1 = x1*z2^2 (1) */
    secp256k1_fe_mul(&u2, &b->x, &zz);                  /* u2 = u2 = x2*z1^2 (1) */
    s1 = a->y; secp256k1_fe_normalize_weak(&s1);        /* s1 = s1 = y1*z2^3 (1) */
    secp256k1_fe_mul(&s2, &b->y, &zz);                  /* s2 = y2*z2^2 (1) */
    secp256k1_fe_mul(&s2, &s2, &a->z);                  /* s2 = s2 = y2*z1^3 (1) */
    z = a->z;                                           /* z = z = z1*z2 (8) */
    t = u1; secp256k1_fe_add(&t, &u2);                  /* t = t = u1+u2 (2) */
    m = s1; secp256k1_fe_add(&m, &s2);                  /* m = m = s1+s2 (2) */
    secp256k1_fe_sqr(&n, &m);                           /* n = m^2 (1) */
    secp256k1_fe_mul(&q, &n, &t);                       /* q = q = t*m^2 (1) */
    secp256k1_fe_sqr(&n, &n);                           /* n = m^4 (1) */
    secp256k1_fe_sqr(&rr, &t);                          /* rr = t^2 (1) */
    secp256k1_fe_mul(&t, &u1, &u2); secp256k1_fe_negate(&t, &t, 1); /* t = -u1*u2 (2) */
    secp256k1_fe_add(&rr, &t);                                      /* rr = r = t^2-u1*u2 (3) */
    secp256k1_fe_sqr(&t, &rr);                                      /* t = r^2 (1) */
    secp256k1_fe_mul(&r->z, &m, &z);                                /* r->z = m*z (1) */
    infinity = secp256k1_fe_normalizes_to_zero(&r->z) * (1 - a->infinity);
    secp256k1_fe_mul_int(&r->z, 2 * (1 - a->infinity)); /* r->z = z3 = 2*m*z (2) */
    r->x = t;                                           /* r->x = r^2 (1) */
    secp256k1_fe_negate(&q, &q, 1);                     /* q = -q (2) */
    secp256k1_fe_add(&r->x, &q);                        /* r->x = r^2-q (3) */
    secp256k1_fe_normalize(&r->x);
    secp256k1_fe_mul_int(&q, 3);                        /* q = -3*q (6) */
    secp256k1_fe_mul_int(&t, 2);                        /* t = 2*r^2 (2) */
    secp256k1_fe_add(&t, &q);                           /* t = 2*r^2-3*q (8) */
    secp256k1_fe_mul(&t, &t, &rr);                      /* t = r*(2*r^2-3*q) (1) */
    secp256k1_fe_add(&t, &n);                           /* t = r*(2*r^2-3*q)+m^4 (2) */
    secp256k1_fe_negate(&r->y, &t, 2);                  /* r->y = r*(3*q-2*r^2)-m^4 (3) */
    secp256k1_fe_normalize_weak(&r->y);
    secp256k1_fe_mul_int(&r->x, 4 * (1 - a->infinity)); /* r->x = x3 = 4*(r^2-q) */
    secp256k1_fe_mul_int(&r->y, 4 * (1 - a->infinity)); /* r->y = y3 = 4*r*(3*q-2*r^2)-4*m^4 (4) */

    /** in case a->infinity == 1, the above code results in r->x, r->y, and r->z all equal to 0.
     *  replace r with b->x, b->y, 1 in that case.
     */
    secp256k1_fe_cmov(&r->x, &b->x, a->infinity);
    secp256k1_fe_cmov(&r->y, &b->y, a->infinity);
    secp256k1_fe_cmov(&r->z, &fe_1, a->infinity);
    r->infinity = infinity;
}

static void secp256k1_gej_rescale(secp256k1_gej_t *r, const secp256k1_fe_t *s) {
    /* operations: 4 mul, 1 sqr */
    secp256k1_fe_t zz;
    verify_check(!secp256k1_fe_is_zero(s));
    secp256k1_fe_sqr(&zz, s);
    secp256k1_fe_mul(&r->x, &r->x, &zz);                /* r->x *= s^2 */
    secp256k1_fe_mul(&r->y, &r->y, &zz);
    secp256k1_fe_mul(&r->y, &r->y, s);                  /* r->y *= s^3 */
    secp256k1_fe_mul(&r->z, &r->z, s);                  /* r->z *= s   */
}

static void secp256k1_ge_to_storage(secp256k1_ge_storage_t *r, const secp256k1_ge_t *a) {
    secp256k1_fe_t x, y;
    verify_check(!a->infinity);
    x = a->x;
    secp256k1_fe_normalize(&x);
    y = a->y;
    secp256k1_fe_normalize(&y);
    secp256k1_fe_to_storage(&r->x, &x);
    secp256k1_fe_to_storage(&r->y, &y);
}

static void secp256k1_ge_from_storage(secp256k1_ge_t *r, const secp256k1_ge_storage_t *a) {
    secp256k1_fe_from_storage(&r->x, &a->x);
    secp256k1_fe_from_storage(&r->y, &a->y);
    r->infinity = 0;
}

static secp256k1_inline void secp256k1_ge_storage_cmov(secp256k1_ge_storage_t *r, const secp256k1_ge_storage_t *a, int flag) {
    secp256k1_fe_storage_cmov(&r->x, &a->x, flag);
    secp256k1_fe_storage_cmov(&r->y, &a->y, flag);
}

#ifdef use_endomorphism
static void secp256k1_gej_mul_lambda(secp256k1_gej_t *r, const secp256k1_gej_t *a) {
    static const secp256k1_fe_t beta = secp256k1_fe_const(
        0x7ae96a2bul, 0x657c0710ul, 0x6e64479eul, 0xac3434e9ul,
        0x9cf04975ul, 0x12f58995ul, 0xc1396c28ul, 0x719501eeul
    );
    *r = *a;
    secp256k1_fe_mul(&r->x, &r->x, &beta);
}
#endif

#endif
