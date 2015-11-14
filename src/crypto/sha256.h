// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_crypto_sha256_h
#define moorecoin_crypto_sha256_h

#include <stdint.h>
#include <stdlib.h>

/** a hasher class for sha-256. */
class csha256
{
private:
    uint32_t s[8];
    unsigned char buf[64];
    size_t bytes;

public:
    static const size_t output_size = 32;

    csha256();
    csha256& write(const unsigned char* data, size_t len);
    void finalize(unsigned char hash[output_size]);
    csha256& reset();
};

#endif // moorecoin_crypto_sha256_h
