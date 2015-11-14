// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_checkpoints_h
#define moorecoin_checkpoints_h

#include "uint256.h"

#include <map>

class cblockindex;

/**
 * block-chain checkpoints are compiled-in sanity checks.
 * they are updated every release or three.
 */
namespace checkpoints
{
typedef std::map<int, uint256> mapcheckpoints;

struct ccheckpointdata {
    mapcheckpoints mapcheckpoints;
    int64_t ntimelastcheckpoint;
    int64_t ntransactionslastcheckpoint;
    double ftransactionsperday;
};

//! return conservative estimate of total number of blocks, 0 if unknown
int gettotalblocksestimate(const ccheckpointdata& data);

//! returns last cblockindex* in mapblockindex that is a checkpoint
cblockindex* getlastcheckpoint(const ccheckpointdata& data);

double guessverificationprogress(const ccheckpointdata& data, cblockindex* pindex, bool fsigchecks = true);

} //namespace checkpoints

#endif // moorecoin_checkpoints_h
