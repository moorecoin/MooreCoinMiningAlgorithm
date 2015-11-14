// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/hmac_sha512.h"

#include <string.h>

chmac_sha512::chmac_sha512(const unsigned char* key, size_t keylen)
{
    unsigned char rkey[128];
    if (keylen <= 128) {
        memcpy(rkey, key, keylen);
        memset(rkey + keylen, 0, 128 - keylen);
    } else {
        csha512().write(key, keylen).finalize(rkey);
        memset(rkey + 64, 0, 64);
    }

    for (int n = 0; n < 128; n++)
        rkey[n] ^= 0x5c;
    outer.write(rkey, 128);

    for (int n = 0; n < 128; n++)
        rkey[n] ^= 0x5c ^ 0x36;
    inner.write(rkey, 128);
}

void chmac_sha512::finalize(unsigned char hash[output_size])
{
    unsigned char temp[64];
    inner.finalize(temp);
    outer.write(temp, 64).finalize(hash);
}
