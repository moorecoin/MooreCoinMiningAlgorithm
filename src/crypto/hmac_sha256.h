// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_crypto_hmac_sha256_h
#define moorecoin_crypto_hmac_sha256_h

#include "crypto/sha256.h"

#include <stdint.h>
#include <stdlib.h>

/** a hasher class for hmac-sha-512. */
class chmac_sha256
{
private:
    csha256 outer;
    csha256 inner;

public:
    static const size_t output_size = 32;

    chmac_sha256(const unsigned char* key, size_t keylen);
    chmac_sha256& write(const unsigned char* data, size_t len)
    {
        inner.write(data, len);
        return *this;
    }
    void finalize(unsigned char hash[output_size]);
};

#endif // moorecoin_crypto_hmac_sha256_h
