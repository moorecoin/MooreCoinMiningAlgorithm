/**********************************************************************
 * copyright (c) 2013, 2014 pieter wuille                             *
 * distributed under the mit software license, see the accompanying   *
 * file copying or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef _secp256k1_eckey_impl_h_
#define _secp256k1_eckey_impl_h_

#include "eckey.h"

#include "scalar.h"
#include "field.h"
#include "group.h"
#include "ecmult_gen.h"

static int secp256k1_eckey_pubkey_parse(secp256k1_ge_t *elem, const unsigned char *pub, int size) {
    if (size == 33 && (pub[0] == 0x02 || pub[0] == 0x03)) {
        secp256k1_fe_t x;
        return secp256k1_fe_set_b32(&x, pub+1) && secp256k1_ge_set_xo_var(elem, &x, pub[0] == 0x03);
    } else if (size == 65 && (pub[0] == 0x04 || pub[0] == 0x06 || pub[0] == 0x07)) {
        secp256k1_fe_t x, y;
        if (!secp256k1_fe_set_b32(&x, pub+1) || !secp256k1_fe_set_b32(&y, pub+33)) {
            return 0;
        }
        secp256k1_ge_set_xy(elem, &x, &y);
        if ((pub[0] == 0x06 || pub[0] == 0x07) && secp256k1_fe_is_odd(&y) != (pub[0] == 0x07)) {
            return 0;
        }
        return secp256k1_ge_is_valid_var(elem);
    } else {
        return 0;
    }
}

static int secp256k1_eckey_pubkey_serialize(secp256k1_ge_t *elem, unsigned char *pub, int *size, int compressed) {
    if (secp256k1_ge_is_infinity(elem)) {
        return 0;
    }
    secp256k1_fe_normalize_var(&elem->x);
    secp256k1_fe_normalize_var(&elem->y);
    secp256k1_fe_get_b32(&pub[1], &elem->x);
    if (compressed) {
        *size = 33;
        pub[0] = 0x02 | (secp256k1_fe_is_odd(&elem->y) ? 0x01 : 0x00);
    } else {
        *size = 65;
        pub[0] = 0x04;
        secp256k1_fe_get_b32(&pub[33], &elem->y);
    }
    return 1;
}

static int secp256k1_eckey_privkey_parse(secp256k1_scalar_t *key, const unsigned char *privkey, int privkeylen) {
    unsigned char c[32] = {0};
    const unsigned char *end = privkey + privkeylen;
    int lenb = 0;
    int len = 0;
    int overflow = 0;
    /* sequence header */
    if (end < privkey+1 || *privkey != 0x30) {
        return 0;
    }
    privkey++;
    /* sequence length constructor */
    if (end < privkey+1 || !(*privkey & 0x80)) {
        return 0;
    }
    lenb = *privkey & ~0x80; privkey++;
    if (lenb < 1 || lenb > 2) {
        return 0;
    }
    if (end < privkey+lenb) {
        return 0;
    }
    /* sequence length */
    len = privkey[lenb-1] | (lenb > 1 ? privkey[lenb-2] << 8 : 0);
    privkey += lenb;
    if (end < privkey+len) {
        return 0;
    }
    /* sequence element 0: version number (=1) */
    if (end < privkey+3 || privkey[0] != 0x02 || privkey[1] != 0x01 || privkey[2] != 0x01) {
        return 0;
    }
    privkey += 3;
    /* sequence element 1: octet string, up to 32 bytes */
    if (end < privkey+2 || privkey[0] != 0x04 || privkey[1] > 0x20 || end < privkey+2+privkey[1]) {
        return 0;
    }
    memcpy(c + 32 - privkey[1], privkey + 2, privkey[1]);
    secp256k1_scalar_set_b32(key, c, &overflow);
    memset(c, 0, 32);
    return !overflow;
}

static int secp256k1_eckey_privkey_serialize(const secp256k1_ecmult_gen_context_t *ctx, unsigned char *privkey, int *privkeylen, const secp256k1_scalar_t *key, int compressed) {
    secp256k1_gej_t rp;
    secp256k1_ge_t r;
    int pubkeylen = 0;
    secp256k1_ecmult_gen(ctx, &rp, key);
    secp256k1_ge_set_gej(&r, &rp);
    if (compressed) {
        static const unsigned char begin[] = {
            0x30,0x81,0xd3,0x02,0x01,0x01,0x04,0x20
        };
        static const unsigned char middle[] = {
            0xa0,0x81,0x85,0x30,0x81,0x82,0x02,0x01,0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,
            0xce,0x3d,0x01,0x01,0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,
            0x21,0x02,0x79,0xbe,0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
            0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,0x81,0x5b,0x16,0xf8,
            0x17,0x98,0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xff,0xff,0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
            0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x24,0x03,0x22,0x00
        };
        unsigned char *ptr = privkey;
        memcpy(ptr, begin, sizeof(begin)); ptr += sizeof(begin);
        secp256k1_scalar_get_b32(ptr, key); ptr += 32;
        memcpy(ptr, middle, sizeof(middle)); ptr += sizeof(middle);
        if (!secp256k1_eckey_pubkey_serialize(&r, ptr, &pubkeylen, 1)) {
            return 0;
        }
        ptr += pubkeylen;
        *privkeylen = ptr - privkey;
    } else {
        static const unsigned char begin[] = {
            0x30,0x82,0x01,0x13,0x02,0x01,0x01,0x04,0x20
        };
        static const unsigned char middle[] = {
            0xa0,0x81,0xa5,0x30,0x81,0xa2,0x02,0x01,0x01,0x30,0x2c,0x06,0x07,0x2a,0x86,0x48,
            0xce,0x3d,0x01,0x01,0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xfe,0xff,0xff,0xfc,0x2f,0x30,0x06,0x04,0x01,0x00,0x04,0x01,0x07,0x04,
            0x41,0x04,0x79,0xbe,0x66,0x7e,0xf9,0xdc,0xbb,0xac,0x55,0xa0,0x62,0x95,0xce,0x87,
            0x0b,0x07,0x02,0x9b,0xfc,0xdb,0x2d,0xce,0x28,0xd9,0x59,0xf2,0x81,0x5b,0x16,0xf8,
            0x17,0x98,0x48,0x3a,0xda,0x77,0x26,0xa3,0xc4,0x65,0x5d,0xa4,0xfb,0xfc,0x0e,0x11,
            0x08,0xa8,0xfd,0x17,0xb4,0x48,0xa6,0x85,0x54,0x19,0x9c,0x47,0xd0,0x8f,0xfb,0x10,
            0xd4,0xb8,0x02,0x21,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
            0xff,0xff,0xff,0xff,0xfe,0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,0xbf,0xd2,0x5e,
            0x8c,0xd0,0x36,0x41,0x41,0x02,0x01,0x01,0xa1,0x44,0x03,0x42,0x00
        };
        unsigned char *ptr = privkey;
        memcpy(ptr, begin, sizeof(begin)); ptr += sizeof(begin);
        secp256k1_scalar_get_b32(ptr, key); ptr += 32;
        memcpy(ptr, middle, sizeof(middle)); ptr += sizeof(middle);
        if (!secp256k1_eckey_pubkey_serialize(&r, ptr, &pubkeylen, 0)) {
            return 0;
        }
        ptr += pubkeylen;
        *privkeylen = ptr - privkey;
    }
    return 1;
}

static int secp256k1_eckey_privkey_tweak_add(secp256k1_scalar_t *key, const secp256k1_scalar_t *tweak) {
    secp256k1_scalar_add(key, key, tweak);
    if (secp256k1_scalar_is_zero(key)) {
        return 0;
    }
    return 1;
}

static int secp256k1_eckey_pubkey_tweak_add(const secp256k1_ecmult_context_t *ctx, secp256k1_ge_t *key, const secp256k1_scalar_t *tweak) {
    secp256k1_gej_t pt;
    secp256k1_scalar_t one;
    secp256k1_gej_set_ge(&pt, key);
    secp256k1_scalar_set_int(&one, 1);
    secp256k1_ecmult(ctx, &pt, &pt, &one, tweak);

    if (secp256k1_gej_is_infinity(&pt)) {
        return 0;
    }
    secp256k1_ge_set_gej(key, &pt);
    return 1;
}

static int secp256k1_eckey_privkey_tweak_mul(secp256k1_scalar_t *key, const secp256k1_scalar_t *tweak) {
    if (secp256k1_scalar_is_zero(tweak)) {
        return 0;
    }

    secp256k1_scalar_mul(key, key, tweak);
    return 1;
}

static int secp256k1_eckey_pubkey_tweak_mul(const secp256k1_ecmult_context_t *ctx, secp256k1_ge_t *key, const secp256k1_scalar_t *tweak) {
    secp256k1_scalar_t zero;
    secp256k1_gej_t pt;
    if (secp256k1_scalar_is_zero(tweak)) {
        return 0;
    }

    secp256k1_scalar_set_int(&zero, 0);
    secp256k1_gej_set_ge(&pt, key);
    secp256k1_ecmult(ctx, &pt, &pt, tweak, &zero);
    secp256k1_ge_set_gej(key, &pt);
    return 1;
}

#endif
