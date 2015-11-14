/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_num_repr_impl_h_
#define _secp256k1_num_repr_impl_h_

#include <string.h>
#include <stdlib.h>
#include <gmp.h>

#include "util.h"
#include "num.h"

#ifdef verify
static void secp256k1_num_sanity(const secp256k1_num_t *a) {
    verify_check(a->limbs == 1 || (a->limbs > 1 && a->data[a->limbs-1] != 0));
}
#else
#define secp256k1_num_sanity(a) do { } while(0)
#endif

static void secp256k1_num_copy(secp256k1_num_t *r, const secp256k1_num_t *a) {
    *r = *a;
}

static void secp256k1_num_get_bin(unsigned char *r, unsigned int rlen, const secp256k1_num_t *a) {
    unsigned char tmp[65];
    int len = 0;
    int shift = 0;
    if (a->limbs>1 || a->data[0] != 0) {
        len = mpn_get_str(tmp, 256, (mp_limb_t*)a->data, a->limbs);
    }
    while (shift < len && tmp[shift] == 0) shift++;
    verify_check(len-shift <= (int)rlen);
    memset(r, 0, rlen - len + shift);
    if (len > shift) {
        memcpy(r + rlen - len + shift, tmp + shift, len - shift);
    }
    memset(tmp, 0, sizeof(tmp));
}

static void secp256k1_num_set_bin(secp256k1_num_t *r, const unsigned char *a, unsigned int alen) {
    int len;
    verify_check(alen > 0);
    verify_check(alen <= 64);
    len = mpn_set_str(r->data, a, alen, 256);
    if (len == 0) {
        r->data[0] = 0;
        len = 1;
    }
    verify_check(len <= num_limbs*2);
    r->limbs = len;
    r->neg = 0;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) {
        r->limbs--;
    }
}

static void secp256k1_num_add_abs(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t c = mpn_add(r->data, a->data, a->limbs, b->data, b->limbs);
    r->limbs = a->limbs;
    if (c != 0) {
        verify_check(r->limbs < 2*num_limbs);
        r->data[r->limbs++] = c;
    }
}

static void secp256k1_num_sub_abs(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t c = mpn_sub(r->data, a->data, a->limbs, b->data, b->limbs);
    verify_check(c == 0);
    r->limbs = a->limbs;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) {
        r->limbs--;
    }
}

static void secp256k1_num_mod(secp256k1_num_t *r, const secp256k1_num_t *m) {
    secp256k1_num_sanity(r);
    secp256k1_num_sanity(m);

    if (r->limbs >= m->limbs) {
        mp_limb_t t[2*num_limbs];
        mpn_tdiv_qr(t, r->data, 0, r->data, r->limbs, m->data, m->limbs);
        memset(t, 0, sizeof(t));
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) {
            r->limbs--;
        }
    }

    if (r->neg && (r->limbs > 1 || r->data[0] != 0)) {
        secp256k1_num_sub_abs(r, m, r);
        r->neg = 0;
    }
}

static void secp256k1_num_mod_inverse(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *m) {
    int i;
    mp_limb_t g[num_limbs+1];
    mp_limb_t u[num_limbs+1];
    mp_limb_t v[num_limbs+1];
    mp_size_t sn;
    mp_size_t gn;
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(m);

    /** mpn_gcdext computes: (g,s) = gcdext(u,v), where
     *  * g = gcd(u,v)
     *  * g = u*s + v*t
     *  * u has equal or more limbs than v, and v has no padding
     *  if we set u to be (a padded version of) a, and v = m:
     *    g = a*s + m*t
     *    g = a*s mod m
     *  assuming g=1:
     *    s = 1/a mod m
     */
    verify_check(m->limbs <= num_limbs);
    verify_check(m->data[m->limbs-1] != 0);
    for (i = 0; i < m->limbs; i++) {
        u[i] = (i < a->limbs) ? a->data[i] : 0;
        v[i] = m->data[i];
    }
    sn = num_limbs+1;
    gn = mpn_gcdext(g, r->data, &sn, u, m->limbs, v, m->limbs);
    verify_check(gn == 1);
    verify_check(g[0] == 1);
    r->neg = a->neg ^ m->neg;
    if (sn < 0) {
        mpn_sub(r->data, m->data, m->limbs, r->data, -sn);
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) {
            r->limbs--;
        }
    } else {
        r->limbs = sn;
    }
    memset(g, 0, sizeof(g));
    memset(u, 0, sizeof(u));
    memset(v, 0, sizeof(v));
}

static int secp256k1_num_is_zero(const secp256k1_num_t *a) {
    return (a->limbs == 1 && a->data[0] == 0);
}

static int secp256k1_num_is_neg(const secp256k1_num_t *a) {
    return (a->limbs > 1 || a->data[0] != 0) && a->neg;
}

static int secp256k1_num_cmp(const secp256k1_num_t *a, const secp256k1_num_t *b) {
    if (a->limbs > b->limbs) {
        return 1;
    }
    if (a->limbs < b->limbs) {
        return -1;
    }
    return mpn_cmp(a->data, b->data, a->limbs);
}

static int secp256k1_num_eq(const secp256k1_num_t *a, const secp256k1_num_t *b) {
    if (a->limbs > b->limbs) {
        return 0;
    }
    if (a->limbs < b->limbs) {
        return 0;
    }
    if ((a->neg && !secp256k1_num_is_zero(a)) != (b->neg && !secp256k1_num_is_zero(b))) {
        return 0;
    }
    return mpn_cmp(a->data, b->data, a->limbs) == 0;
}

static void secp256k1_num_subadd(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b, int bneg) {
    if (!(b->neg ^ bneg ^ a->neg)) { /* a and b have the same sign */
        r->neg = a->neg;
        if (a->limbs >= b->limbs) {
            secp256k1_num_add_abs(r, a, b);
        } else {
            secp256k1_num_add_abs(r, b, a);
        }
    } else {
        if (secp256k1_num_cmp(a, b) > 0) {
            r->neg = a->neg;
            secp256k1_num_sub_abs(r, a, b);
        } else {
            r->neg = b->neg ^ bneg;
            secp256k1_num_sub_abs(r, b, a);
        }
    }
}

static void secp256k1_num_add(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    secp256k1_num_subadd(r, a, b, 0);
}

static void secp256k1_num_sub(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    secp256k1_num_subadd(r, a, b, 1);
}

static void secp256k1_num_mul(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t tmp[2*num_limbs+1];
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);

    verify_check(a->limbs + b->limbs <= 2*num_limbs+1);
    if ((a->limbs==1 && a->data[0]==0) || (b->limbs==1 && b->data[0]==0)) {
        r->limbs = 1;
        r->neg = 0;
        r->data[0] = 0;
        return;
    }
    if (a->limbs >= b->limbs) {
        mpn_mul(tmp, a->data, a->limbs, b->data, b->limbs);
    } else {
        mpn_mul(tmp, b->data, b->limbs, a->data, a->limbs);
    }
    r->limbs = a->limbs + b->limbs;
    if (r->limbs > 1 && tmp[r->limbs - 1]==0) {
        r->limbs--;
    }
    verify_check(r->limbs <= 2*num_limbs);
    mpn_copyi(r->data, tmp, r->limbs);
    r->neg = a->neg ^ b->neg;
    memset(tmp, 0, sizeof(tmp));
}

static void secp256k1_num_shift(secp256k1_num_t *r, int bits) {
    int i;
    if (bits % gmp_numb_bits) {
        /* shift within limbs. */
        mpn_rshift(r->data, r->data, r->limbs, bits % gmp_numb_bits);
    }
    if (bits >= gmp_numb_bits) {
        /* shift full limbs. */
        for (i = 0; i < r->limbs; i++) {
            int index = i + (bits / gmp_numb_bits);
            if (index < r->limbs && index < 2*num_limbs) {
                r->data[i] = r->data[index];
            } else {
                r->data[i] = 0;
            }
        }
    }
    while (r->limbs>1 && r->data[r->limbs-1]==0) {
        r->limbs--;
    }
}

static void secp256k1_num_negate(secp256k1_num_t *r) {
    r->neg ^= 1;
}

#endif
