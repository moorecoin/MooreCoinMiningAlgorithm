// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_pow_h
#define moorecoin_pow_h

#include "consensus/params.h"

#include <stdint.h>

class cblockheader;
class cblockindex;
class uint256;
class arith_uint256;

unsigned int getnextworkrequired(const cblockindex* pindexlast, const cblockheader *pblock, const consensus::params&);
unsigned int calculatenextworkrequired(const cblockindex* pindexlast, int64_t nfirstblocktime, const consensus::params&);

/** check whether a block hash satisfies the proof-of-work requirement specified by nbits */
bool checkproofofwork(uint256 hash, unsigned int nbits, const consensus::params&);
arith_uint256 getblockproof(const cblockindex& block);

/** return the time it would take to redo the work difference between from and to, assuming the current hashrate corresponds to the difficulty at tip, in seconds. */
int64_t getblockproofequivalenttime(const cblockindex& to, const cblockindex& from, const cblockindex& tip, const consensus::params&);

#endif // moorecoin_pow_h
