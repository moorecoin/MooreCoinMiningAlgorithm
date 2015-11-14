// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_crypto_sha512_h
#define moorecoin_crypto_sha512_h

#include <stdint.h>
#include <stdlib.h>

/** a hasher class for sha-512. */
class csha512
{
private:
    uint64_t s[8];
    unsigned char buf[128];
    size_t bytes;

public:
    static const size_t output_size = 64;

    csha512();
    csha512& write(const unsigned char* data, size_t len);
    void finalize(unsigned char hash[output_size]);
    csha512& reset();
};

#endif // moorecoin_crypto_sha512_h
