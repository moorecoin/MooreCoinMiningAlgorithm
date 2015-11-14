// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_consensus_params_h
#define moorecoin_consensus_params_h

#include "uint256.h"

namespace consensus {
/**
 * parameters that influence chain consensus.
 */
struct params {
    uint256 hashgenesisblock;
    int nsubsidyhalvinginterval;
    /** used to check majorities for block version upgrade */
    int nmajorityenforceblockupgrade;
    int nmajorityrejectblockoutdated;
    int nmajoritywindow;
    /** proof of work parameters */
    uint256 powlimit;
    bool fpowallowmindifficultyblocks;
    int64_t npowtargetspacing;
    int64_t npowtargettimespan;
    int64_t difficultyadjustmentinterval() const { return npowtargettimespan / npowtargetspacing; }
};
} // namespace consensus

#endif // moorecoin_consensus_params_h
