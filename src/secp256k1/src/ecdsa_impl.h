/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                               *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/


#ifndef _secp256k1_ecdsa_impl_h_
#define _secp256k1_ecdsa_impl_h_

#include "scalar.h"
#include "field.h"
#include "group.h"
#include "ecmult.h"
#include "ecmult_gen.h"
#include "ecdsa.h"

/** group order for secp256k1 defined as 'n' in "standards for efficient cryptography" (sec2) 2.7.1
 *  sage: for t in xrange(1023, -1, -1):
 *     ..   p = 2**256 - 2**32 - t
 *     ..   if p.is_prime():
 *     ..     print '%x'%p
 *     ..     break
 *   'fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f'
 *  sage: a = 0
 *  sage: b = 7
 *  sage: f = finitefield (p)
 *  sage: '%x' % (ellipticcurve ([f (a), f (b)]).order())
 *   'fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141'
 */
static const secp256k1_fe_t secp256k1_ecdsa_const_order_as_fe = secp256k1_fe_const(
    0xfffffffful, 0xfffffffful, 0xfffffffful, 0xfffffffeul,
    0xbaaedce6ul, 0xaf48a03bul, 0xbfd25e8cul, 0xd0364141ul
);

/** difference between field and order, values 'p' and 'n' values defined in
 *  "standards for efficient cryptography" (sec2) 2.7.1.
 *  sage: p = 0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f
 *  sage: a = 0
 *  sage: b = 7
 *  sage: f = finitefield (p)
 *  sage: '%x' % (p - ellipticcurve ([f (a), f (b)]).order())
 *   '14551231950b75fc4402da1722fc9baee'
 */
static const secp256k1_fe_t secp256k1_ecdsa_const_p_minus_order = secp256k1_fe_const(
    0, 0, 0, 1, 0x45512319ul, 0x50b75fc4ul, 0x402da172ul, 0x2fc9baeeul
);

static int secp256k1_ecdsa_sig_parse(secp256k1_ecdsa_sig_t *r, const unsigned char *sig, int size) {
    unsigned char ra[32] = {0}, sa[32] = {0};
    const unsigned char *rp;
    const unsigned char *sp;
    int lenr;
    int lens;
    int overflow;
    if (sig[0] != 0x30) {
        return 0;
    }
    lenr = sig[3];
    if (5+lenr >= size) {
        return 0;
    }
    lens = sig[lenr+5];
    if (sig[1] != lenr+lens+4) {
        return 0;
    }
    if (lenr+lens+6 > size) {
        return 0;
    }
    if (sig[2] != 0x02) {
        return 0;
    }
    if (lenr == 0) {
        return 0;
    }
    if (sig[lenr+4] != 0x02) {
        return 0;
    }
    if (lens == 0) {
        return 0;
    }
    sp = sig + 6 + lenr;
    while (lens > 0 && sp[0] == 0) {
        lens--;
        sp++;
    }
    if (lens > 32) {
        return 0;
    }
    rp = sig + 4;
    while (lenr > 0 && rp[0] == 0) {
        lenr--;
        rp++;
    }
    if (lenr > 32) {
        return 0;
    }
    memcpy(ra + 32 - lenr, rp, lenr);
    memcpy(sa + 32 - lens, sp, lens);
    overflow = 0;
    secp256k1_scalar_set_b32(&r->r, ra, &overflow);
    if (overflow) {
        return 0;
    }
    secp256k1_scalar_set_b32(&r->s, sa, &overflow);
    if (overflow) {
        return 0;
    }
    return 1;
}

static int secp256k1_ecdsa_sig_serialize(unsigned char *sig, int *size, const secp256k1_ecdsa_sig_t *a) {
    unsigned char r[33] = {0}, s[33] = {0};
    unsigned char *rp = r, *sp = s;
    int lenr = 33, lens = 33;
    secp256k1_scalar_get_b32(&r[1], &a->r);
    secp256k1_scalar_get_b32(&s[1], &a->s);
    while (lenr > 1 && rp[0] == 0 && rp[1] < 0x80) { lenr--; rp++; }
    while (lens > 1 && sp[0] == 0 && sp[1] < 0x80) { lens--; sp++; }
    if (*size < 6+lens+lenr) {
        return 0;
    }
    *size = 6 + lens + lenr;
    sig[0] = 0x30;
    sig[1] = 4 + lens + lenr;
    sig[2] = 0x02;
    sig[3] = lenr;
    memcpy(sig+4, rp, lenr);
    sig[4+lenr] = 0x02;
    sig[5+lenr] = lens;
    memcpy(sig+lenr+6, sp, lens);
    return 1;
}

static int secp256k1_ecdsa_sig_verify(const secp256k1_ecmult_context_t *ctx, const secp256k1_ecdsa_sig_t *sig, const secp256k1_ge_t *pubkey, const secp256k1_scalar_t *message) {
    unsigned char c[32];
    secp256k1_scalar_t sn, u1, u2;
    secp256k1_fe_t xr;
    secp256k1_gej_t pubkeyj;
    secp256k1_gej_t pr;

    if (secp256k1_scalar_is_zero(&sig->r) || secp256k1_scalar_is_zero(&sig->s)) {
        return 0;
    }

    secp256k1_scalar_inverse_var(&sn, &sig->s);
    secp256k1_scalar_mul(&u1, &sn, message);
    secp256k1_scalar_mul(&u2, &sn, &sig->r);
    secp256k1_gej_set_ge(&pubkeyj, pubkey);
    secp256k1_ecmult(ctx, &pr, &pubkeyj, &u2, &u1);
    if (secp256k1_gej_is_infinity(&pr)) {
        return 0;
    }
    secp256k1_scalar_get_b32(c, &sig->r);
    secp256k1_fe_set_b32(&xr, c);

    /** we now have the recomputed r point in pr, and its claimed x coordinate (modulo n)
     *  in xr. naively, we would extract the x coordinate from pr (requiring a inversion modulo p),
     *  compute the remainder modulo n, and compare it to xr. however:
     *
     *        xr == x(pr) mod n
     *    <=> exists h. (xr + h * n < p && xr + h * n == x(pr))
     *    [since 2 * n > p, h can only be 0 or 1]
     *    <=> (xr == x(pr)) || (xr + n < p && xr + n == x(pr))
     *    [in jacobian coordinates, x(pr) is pr.x / pr.z^2 mod p]
     *    <=> (xr == pr.x / pr.z^2 mod p) || (xr + n < p && xr + n == pr.x / pr.z^2 mod p)
     *    [multiplying both sides of the equations by pr.z^2 mod p]
     *    <=> (xr * pr.z^2 mod p == pr.x) || (xr + n < p && (xr + n) * pr.z^2 mod p == pr.x)
     *
     *  thus, we can avoid the inversion, but we have to check both cases separately.
     *  secp256k1_gej_eq_x implements the (xr * pr.z^2 mod p == pr.x) test.
     */
    if (secp256k1_gej_eq_x_var(&xr, &pr)) {
        /* xr.x == xr * xr.z^2 mod p, so the signature is valid. */
        return 1;
    }
    if (secp256k1_fe_cmp_var(&xr, &secp256k1_ecdsa_const_p_minus_order) >= 0) {
        /* xr + p >= n, so we can skip testing the second case. */
        return 0;
    }
    secp256k1_fe_add(&xr, &secp256k1_ecdsa_const_order_as_fe);
    if (secp256k1_gej_eq_x_var(&xr, &pr)) {
        /* (xr + n) * pr.z^2 mod p == pr.x, so the signature is valid. */
        return 1;
    }
    return 0;
}

static int secp256k1_ecdsa_sig_recover(const secp256k1_ecmult_context_t *ctx, const secp256k1_ecdsa_sig_t *sig, secp256k1_ge_t *pubkey, const secp256k1_scalar_t *message, int recid) {
    unsigned char brx[32];
    secp256k1_fe_t fx;
    secp256k1_ge_t x;
    secp256k1_gej_t xj;
    secp256k1_scalar_t rn, u1, u2;
    secp256k1_gej_t qj;

    if (secp256k1_scalar_is_zero(&sig->r) || secp256k1_scalar_is_zero(&sig->s)) {
        return 0;
    }

    secp256k1_scalar_get_b32(brx, &sig->r);
    verify_check(secp256k1_fe_set_b32(&fx, brx)); /* brx comes from a scalar, so is less than the order; certainly less than p */
    if (recid & 2) {
        if (secp256k1_fe_cmp_var(&fx, &secp256k1_ecdsa_const_p_minus_order) >= 0) {
            return 0;
        }
        secp256k1_fe_add(&fx, &secp256k1_ecdsa_const_order_as_fe);
    }
    if (!secp256k1_ge_set_xo_var(&x, &fx, recid & 1)) {
        return 0;
    }
    secp256k1_gej_set_ge(&xj, &x);
    secp256k1_scalar_inverse_var(&rn, &sig->r);
    secp256k1_scalar_mul(&u1, &rn, message);
    secp256k1_scalar_negate(&u1, &u1);
    secp256k1_scalar_mul(&u2, &rn, &sig->s);
    secp256k1_ecmult(ctx, &qj, &xj, &u2, &u1);
    secp256k1_ge_set_gej_var(pubkey, &qj);
    return !secp256k1_gej_is_infinity(&qj);
}

static int secp256k1_ecdsa_sig_sign(const secp256k1_ecmult_gen_context_t *ctx, secp256k1_ecdsa_sig_t *sig, const secp256k1_scalar_t *seckey, const secp256k1_scalar_t *message, const secp256k1_scalar_t *nonce, int *recid) {
    unsigned char b[32];
    secp256k1_gej_t rp;
    secp256k1_ge_t r;
    secp256k1_scalar_t n;
    int overflow = 0;

    secp256k1_ecmult_gen(ctx, &rp, nonce);
    secp256k1_ge_set_gej(&r, &rp);
    secp256k1_fe_normalize(&r.x);
    secp256k1_fe_normalize(&r.y);
    secp256k1_fe_get_b32(b, &r.x);
    secp256k1_scalar_set_b32(&sig->r, b, &overflow);
    if (secp256k1_scalar_is_zero(&sig->r)) {
        /* p.x = order is on the curve, so technically sig->r could end up zero, which would be an invalid signature. */
        secp256k1_gej_clear(&rp);
        secp256k1_ge_clear(&r);
        return 0;
    }
    if (recid) {
        *recid = (overflow ? 2 : 0) | (secp256k1_fe_is_odd(&r.y) ? 1 : 0);
    }
    secp256k1_scalar_mul(&n, &sig->r, seckey);
    secp256k1_scalar_add(&n, &n, message);
    secp256k1_scalar_inverse(&sig->s, nonce);
    secp256k1_scalar_mul(&sig->s, &sig->s, &n);
    secp256k1_scalar_clear(&n);
    secp256k1_gej_clear(&rp);
    secp256k1_ge_clear(&r);
    if (secp256k1_scalar_is_zero(&sig->s)) {
        return 0;
    }
    if (secp256k1_scalar_is_high(&sig->s)) {
        secp256k1_scalar_negate(&sig->s, &sig->s);
        if (recid) {
            *recid ^= 1;
        }
    }
    return 1;
}

#endif
