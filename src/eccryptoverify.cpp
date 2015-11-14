// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "eccryptoverify.h"

namespace {

int comparebigendian(const unsigned char *c1, size_t c1len, const unsigned char *c2, size_t c2len) {
    while (c1len > c2len) {
        if (*c1)
            return 1;
        c1++;
        c1len--;
    }
    while (c2len > c1len) {
        if (*c2)
            return -1;
        c2++;
        c2len--;
    }
    while (c1len > 0) {
        if (*c1 > *c2)
            return 1;
        if (*c2 > *c1)
            return -1;
        c1++;
        c2++;
        c1len--;
    }
    return 0;
}

/** order of secp256k1's generator minus 1. */
const unsigned char vchmaxmodorder[32] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
    0xba,0xae,0xdc,0xe6,0xaf,0x48,0xa0,0x3b,
    0xbf,0xd2,0x5e,0x8c,0xd0,0x36,0x41,0x40
};

/** half of the order of secp256k1's generator minus 1. */
const unsigned char vchmaxmodhalforder[32] = {
    0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0x5d,0x57,0x6e,0x73,0x57,0xa4,0x50,0x1d,
    0xdf,0xe9,0x2f,0x46,0x68,0x1b,0x20,0xa0
};

const unsigned char vchzero[1] = {0};
} // anon namespace

namespace eccrypto {

bool check(const unsigned char *vch) {
    return vch &&
           comparebigendian(vch, 32, vchzero, 0) > 0 &&
           comparebigendian(vch, 32, vchmaxmodorder, 32) <= 0;
}

bool checksignatureelement(const unsigned char *vch, int len, bool half) {
    return vch &&
           comparebigendian(vch, len, vchzero, 0) > 0 &&
           comparebigendian(vch, len, half ? vchmaxmodhalforder : vchmaxmodorder, 32) <= 0;
}

} // namespace eccrypto
