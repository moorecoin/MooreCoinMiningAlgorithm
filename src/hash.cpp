// copyright (c) 2013-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "crypto/common.h"
#include "crypto/hmac_sha512.h"
#include "pubkey.h"


inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

unsigned int murmurhash3(unsigned int nhashseed, const std::vector<unsigned char>& vdatatohash)
{
    // the following is murmurhash3 (x86_32), see http://code.google.com/p/smhasher/source/browse/trunk/murmurhash3.cpp
    uint32_t h1 = nhashseed;
    if (vdatatohash.size() > 0)
    {
        const uint32_t c1 = 0xcc9e2d51;
        const uint32_t c2 = 0x1b873593;

        const int nblocks = vdatatohash.size() / 4;

        //----------
        // body
        const uint8_t* blocks = &vdatatohash[0] + nblocks * 4;

        for (int i = -nblocks; i; i++) {
            uint32_t k1 = readle32(blocks + i*4);

            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;

            h1 ^= k1;
            h1 = rotl32(h1, 13);
            h1 = h1 * 5 + 0xe6546b64;
        }

        //----------
        // tail
        const uint8_t* tail = (const uint8_t*)(&vdatatohash[0] + nblocks * 4);

        uint32_t k1 = 0;

        switch (vdatatohash.size() & 3) {
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
        };
    }

    //----------
    // finalization
    h1 ^= vdatatohash.size();
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

void bip32hash(const chaincode &chaincode, unsigned int nchild, unsigned char header, const unsigned char data[32], unsigned char output[64])
{
    unsigned char num[4];
    num[0] = (nchild >> 24) & 0xff;
    num[1] = (nchild >> 16) & 0xff;
    num[2] = (nchild >>  8) & 0xff;
    num[3] = (nchild >>  0) & 0xff;
    chmac_sha512(chaincode.begin(), chaincode.size()).write(&header, 1).write(data, 32).write(num, 4).finalize(output);
}
