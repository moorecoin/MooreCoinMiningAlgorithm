// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "crypto/hmac_sha256.h"

#include <string.h>

chmac_sha256::chmac_sha256(const unsigned char* key, size_t keylen)
{
    unsigned char rkey[64];
    if (keylen <= 64) {
        memcpy(rkey, key, keylen);
        memset(rkey + keylen, 0, 64 - keylen);
    } else {
        csha256().write(key, keylen).finalize(rkey);
        memset(rkey + 32, 0, 32);
    }

    for (int n = 0; n < 64; n++)
        rkey[n] ^= 0x5c;
    outer.write(rkey, 64);

    for (int n = 0; n < 64; n++)
        rkey[n] ^= 0x5c ^ 0x36;
    inner.write(rkey, 64);
}

void chmac_sha256::finalize(unsigned char hash[output_size])
{
    unsigned char temp[32];
    inner.finalize(temp);
    outer.write(temp, 32).finalize(hash);
}
