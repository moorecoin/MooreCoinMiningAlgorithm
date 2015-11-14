// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_random_h
#define moorecoin_random_h

#include "uint256.h"

#include <stdint.h>

/**
 * seed openssl prng with additional entropy data
 */
void randaddseed();
void randaddseedperfmon();

/**
 * functions to gather random data via the openssl prng
 */
void getrandbytes(unsigned char* buf, int num);
uint64_t getrand(uint64_t nmax);
int getrandint(int nmax);
uint256 getrandhash();

/**
 * seed insecure_rand using the random pool.
 * @param deterministic use a deterministic seed
 */
void seed_insecure_rand(bool fdeterministic = false);

/**
 * mwc rng of george marsaglia
 * this is intended to be fast. it has a period of 2^59.3, though the
 * least significant 16 bits only have a period of about 2^30.1.
 *
 * @return random value
 */
extern uint32_t insecure_rand_rz;
extern uint32_t insecure_rand_rw;
static inline uint32_t insecure_rand(void)
{
    insecure_rand_rz = 36969 * (insecure_rand_rz & 65535) + (insecure_rand_rz >> 16);
    insecure_rand_rw = 18000 * (insecure_rand_rw & 65535) + (insecure_rand_rw >> 16);
    return (insecure_rand_rw << 16) + insecure_rand_rz;
}

#endif // moorecoin_random_h
