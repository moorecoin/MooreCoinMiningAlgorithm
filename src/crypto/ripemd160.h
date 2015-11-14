// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_crypto_ripemd160_h
#define moorecoin_crypto_ripemd160_h

#include <stdint.h>
#include <stdlib.h>

/** a hasher class for ripemd-160. */
class cripemd160
{
private:
    uint32_t s[5];
    unsigned char buf[64];
    size_t bytes;

public:
    static const size_t output_size = 20;

    cripemd160();
    cripemd160& write(const unsigned char* data, size_t len);
    void finalize(unsigned char hash[output_size]);
    cripemd160& reset();
};

#endif // moorecoin_crypto_ripemd160_h
