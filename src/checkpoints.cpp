// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"

#include "chainparams.h"
#include "main.h"
#include "uint256.h"

#include <stdint.h>

#include <boost/foreach.hpp>

namespace checkpoints {

    /**
     * how many times slower we expect checking transactions after the last
     * checkpoint to be (from checking signatures, which is skipped up to the
     * last checkpoint). this number is a compromise, as it can't be accurate
     * for every system. when reindexing from a fast disk with a slow cpu, it
     * can be up to 20, while when downloading from a slow network with a
     * fast multicore cpu, it won't be much higher than 1.
     */
    static const double sigcheck_verification_factor = 5.0;

    //! guess how far we are in the verification process at the given block index
    double guessverificationprogress(const ccheckpointdata& data, cblockindex *pindex, bool fsigchecks) {
        if (pindex==null)
            return 0.0;

        int64_t nnow = time(null);

        double fsigcheckverificationfactor = fsigchecks ? sigcheck_verification_factor : 1.0;
        double fworkbefore = 0.0; // amount of work done before pindex
        double fworkafter = 0.0;  // amount of work left after pindex (estimated)
        // work is defined as: 1.0 per transaction before the last checkpoint, and
        // fsigcheckverificationfactor per transaction after.

        if (pindex->nchaintx <= data.ntransactionslastcheckpoint) {
            double ncheapbefore = pindex->nchaintx;
            double ncheapafter = data.ntransactionslastcheckpoint - pindex->nchaintx;
            double nexpensiveafter = (nnow - data.ntimelastcheckpoint)/86400.0*data.ftransactionsperday;
            fworkbefore = ncheapbefore;
            fworkafter = ncheapafter + nexpensiveafter*fsigcheckverificationfactor;
        } else {
            double ncheapbefore = data.ntransactionslastcheckpoint;
            double nexpensivebefore = pindex->nchaintx - data.ntransactionslastcheckpoint;
            double nexpensiveafter = (nnow - pindex->getblocktime())/86400.0*data.ftransactionsperday;
            fworkbefore = ncheapbefore + nexpensivebefore*fsigcheckverificationfactor;
            fworkafter = nexpensiveafter*fsigcheckverificationfactor;
        }

        return fworkbefore / (fworkbefore + fworkafter);
    }

    int gettotalblocksestimate(const ccheckpointdata& data)
    {
        const mapcheckpoints& checkpoints = data.mapcheckpoints;

        if (checkpoints.empty())
            return 0;

        return checkpoints.rbegin()->first;
    }

    cblockindex* getlastcheckpoint(const ccheckpointdata& data)
    {
        const mapcheckpoints& checkpoints = data.mapcheckpoints;

        boost_reverse_foreach(const mapcheckpoints::value_type& i, checkpoints)
        {
            const uint256& hash = i.second;
            blockmap::const_iterator t = mapblockindex.find(hash);
            if (t != mapblockindex.end())
                return t->second;
        }
        return null;
    }

} // namespace checkpoints
