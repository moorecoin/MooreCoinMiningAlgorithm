// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"

unsigned int getnextworkrequired(const cblockindex* pindexlast, const cblockheader *pblock, const consensus::params& params)
{
    unsigned int nproofofworklimit = uinttoarith256(params.powlimit).getcompact();

    // genesis block
    if (pindexlast == null)
        return nproofofworklimit;

    // only change once per difficulty adjustment interval
    if ((pindexlast->nheight+1) % params.difficultyadjustmentinterval() != 0)
    {
        if (params.fpowallowmindifficultyblocks)
        {
            // special difficulty rule for testnet:
            // if the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->getblocktime() > pindexlast->getblocktime() + params.npowtargetspacing*2)
                return nproofofworklimit;
            else
            {
                // return the last non-special-min-difficulty-rules-block
                const cblockindex* pindex = pindexlast;
                while (pindex->pprev && pindex->nheight % params.difficultyadjustmentinterval() != 0 && pindex->nbits == nproofofworklimit)
                    pindex = pindex->pprev;
                return pindex->nbits;
            }
        }
        return pindexlast->nbits;
    }

    // go back by what we want to be 14 days worth of blocks
    int nheightfirst = pindexlast->nheight - (params.difficultyadjustmentinterval()-1);
    assert(nheightfirst >= 0);
    const cblockindex* pindexfirst = pindexlast->getancestor(nheightfirst);
    assert(pindexfirst);

    return calculatenextworkrequired(pindexlast, pindexfirst->getblocktime(), params);
}

unsigned int calculatenextworkrequired(const cblockindex* pindexlast, int64_t nfirstblocktime, const consensus::params& params)
{
    // limit adjustment step
    int64_t nactualtimespan = pindexlast->getblocktime() - nfirstblocktime;
    logprintf("  nactualtimespan = %d  before bounds\n", nactualtimespan);
    if (nactualtimespan < params.npowtargettimespan/4)
        nactualtimespan = params.npowtargettimespan/4;
    if (nactualtimespan > params.npowtargettimespan*4)
        nactualtimespan = params.npowtargettimespan*4;

    // retarget
    const arith_uint256 bnpowlimit = uinttoarith256(params.powlimit);
    arith_uint256 bnnew;
    arith_uint256 bnold;
    bnnew.setcompact(pindexlast->nbits);
    bnold = bnnew;
    bnnew *= nactualtimespan;
    bnnew /= params.npowtargettimespan;

    if (bnnew > bnpowlimit)
        bnnew = bnpowlimit;

    /// debug print
    logprintf("getnextworkrequired retarget\n");
    logprintf("params.npowtargettimespan = %d    nactualtimespan = %d\n", params.npowtargettimespan, nactualtimespan);
    logprintf("before: %08x  %s\n", pindexlast->nbits, bnold.tostring());
    logprintf("after:  %08x  %s\n", bnnew.getcompact(), bnnew.tostring());

    return bnnew.getcompact();
}

bool checkproofofwork(uint256 hash, unsigned int nbits, const consensus::params& params)
{
    bool fnegative;
    bool foverflow;
    arith_uint256 bntarget;

    bntarget.setcompact(nbits, &fnegative, &foverflow);

    // check range
    if (fnegative || bntarget == 0 || foverflow || bntarget > uinttoarith256(params.powlimit))
        return error("checkproofofwork(): nbits below minimum work");

    // check proof of work matches claimed amount
    if (uinttoarith256(hash) > bntarget)
        return error("checkproofofwork(): hash doesn't match nbits");

    return true;
}

arith_uint256 getblockproof(const cblockindex& block)
{
    arith_uint256 bntarget;
    bool fnegative;
    bool foverflow;
    bntarget.setcompact(block.nbits, &fnegative, &foverflow);
    if (fnegative || foverflow || bntarget == 0)
        return 0;
    // we need to compute 2**256 / (bntarget+1), but we can't represent 2**256
    // as it's too large for a arith_uint256. however, as 2**256 is at least as large
    // as bntarget+1, it is equal to ((2**256 - bntarget - 1) / (bntarget+1)) + 1,
    // or ~bntarget / (ntarget+1) + 1.
    return (~bntarget / (bntarget + 1)) + 1;
}

int64_t getblockproofequivalenttime(const cblockindex& to, const cblockindex& from, const cblockindex& tip, const consensus::params& params)
{
    arith_uint256 r;
    int sign = 1;
    if (to.nchainwork > from.nchainwork) {
        r = to.nchainwork - from.nchainwork;
    } else {
        r = from.nchainwork - to.nchainwork;
        sign = -1;
    }
    r = r * arith_uint256(params.npowtargetspacing) / getblockproof(tip);
    if (r.bits() > 63) {
        return sign * std::numeric_limits<int64_t>::max();
    }
    return sign * r.getlow64();
}
