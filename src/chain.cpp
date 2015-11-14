// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"

using namespace std;

/**
 * cchain implementation
 */
void cchain::settip(cblockindex *pindex) {
    if (pindex == null) {
        vchain.clear();
        return;
    }
    vchain.resize(pindex->nheight + 1);
    while (pindex && vchain[pindex->nheight] != pindex) {
        vchain[pindex->nheight] = pindex;
        pindex = pindex->pprev;
    }
}

cblocklocator cchain::getlocator(const cblockindex *pindex) const {
    int nstep = 1;
    std::vector<uint256> vhave;
    vhave.reserve(32);

    if (!pindex)
        pindex = tip();
    while (pindex) {
        vhave.push_back(pindex->getblockhash());
        // stop when we have added the genesis block.
        if (pindex->nheight == 0)
            break;
        // exponentially larger steps back, plus the genesis block.
        int nheight = std::max(pindex->nheight - nstep, 0);
        if (contains(pindex)) {
            // use o(1) cchain index if possible.
            pindex = (*this)[nheight];
        } else {
            // otherwise, use o(log n) skiplist.
            pindex = pindex->getancestor(nheight);
        }
        if (vhave.size() > 10)
            nstep *= 2;
    }

    return cblocklocator(vhave);
}

const cblockindex *cchain::findfork(const cblockindex *pindex) const {
    if (pindex->nheight > height())
        pindex = pindex->getancestor(height());
    while (pindex && !contains(pindex))
        pindex = pindex->pprev;
    return pindex;
}

/** turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline invertlowestone(int n) { return n & (n - 1); }

/** compute what height to jump back to with the cblockindex::pskip pointer. */
int static inline getskipheight(int height) {
    if (height < 2)
        return 0;

    // determine which height to jump back to. any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? invertlowestone(invertlowestone(height - 1)) + 1 : invertlowestone(height);
}

cblockindex* cblockindex::getancestor(int height)
{
    if (height > nheight || height < 0)
        return null;

    cblockindex* pindexwalk = this;
    int heightwalk = nheight;
    while (heightwalk > height) {
        int heightskip = getskipheight(heightwalk);
        int heightskipprev = getskipheight(heightwalk - 1);
        if (pindexwalk->pskip != null &&
            (heightskip == height ||
             (heightskip > height && !(heightskipprev < heightskip - 2 &&
                                       heightskipprev >= height)))) {
            // only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexwalk = pindexwalk->pskip;
            heightwalk = heightskip;
        } else {
            pindexwalk = pindexwalk->pprev;
            heightwalk--;
        }
    }
    return pindexwalk;
}

const cblockindex* cblockindex::getancestor(int height) const
{
    return const_cast<cblockindex*>(this)->getancestor(height);
}

void cblockindex::buildskip()
{
    if (pprev)
        pskip = pprev->getancestor(getskipheight(nheight));
}
