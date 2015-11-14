// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_crypto_sha1_h
#define moorecoin_crypto_sha1_h

#include <stdint.h>
#include <stdlib.h>

/** a hasher class for sha1. */
class csha1
{
private:
    uint32_t s[5];
    unsigned char buf[64];
    size_t bytes;

public:
    static const size_t output_size = 20;

    csha1();
    csha1& write(const unsigned char* data, size_t len);
    void finalize(unsigned char hash[output_size]);
    csha1& reset();
};

#endif // moorecoin_crypto_sha1_h
