// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"

uint256 cblockheader::gethash() const
{
    return serializehash(*this);
}

uint256 cblock::buildmerkletree(bool* fmutated) const
{
    /* warning! if you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (cve-2012-2459).

       the reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in merkle trees). this results in certain sequences of
       transactions leading to the same merkle root. for example, these two
       trees:

                    a               a
                  /  \            /   \
                b     c         b       c
               / \    |        / \     / \
              d   e   f       d   e   f   f
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash a (because the hash of both
       of (f) and (f,f) is c).

       the vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. if the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. we defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. assuming no double-sha256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
    */
    vmerkletree.clear();
    vmerkletree.reserve(vtx.size() * 2 + 16); // safe upper bound for the number of total nodes.
    for (std::vector<ctransaction>::const_iterator it(vtx.begin()); it != vtx.end(); ++it)
        vmerkletree.push_back(it->gethash());
    int j = 0;
    bool mutated = false;
    for (int nsize = vtx.size(); nsize > 1; nsize = (nsize + 1) / 2)
    {
        for (int i = 0; i < nsize; i += 2)
        {
            int i2 = std::min(i+1, nsize-1);
            if (i2 == i + 1 && i2 + 1 == nsize && vmerkletree[j+i] == vmerkletree[j+i2]) {
                // two identical hashes at the end of the list at a particular level.
                mutated = true;
            }
            vmerkletree.push_back(hash(begin(vmerkletree[j+i]),  end(vmerkletree[j+i]),
                                       begin(vmerkletree[j+i2]), end(vmerkletree[j+i2])));
        }
        j += nsize;
    }
    if (fmutated) {
        *fmutated = mutated;
    }
    return (vmerkletree.empty() ? uint256() : vmerkletree.back());
}

std::vector<uint256> cblock::getmerklebranch(int nindex) const
{
    if (vmerkletree.empty())
        buildmerkletree();
    std::vector<uint256> vmerklebranch;
    int j = 0;
    for (int nsize = vtx.size(); nsize > 1; nsize = (nsize + 1) / 2)
    {
        int i = std::min(nindex^1, nsize-1);
        vmerklebranch.push_back(vmerkletree[j+i]);
        nindex >>= 1;
        j += nsize;
    }
    return vmerklebranch;
}

uint256 cblock::checkmerklebranch(uint256 hash, const std::vector<uint256>& vmerklebranch, int nindex)
{
    if (nindex == -1)
        return uint256();
    for (std::vector<uint256>::const_iterator it(vmerklebranch.begin()); it != vmerklebranch.end(); ++it)
    {
        if (nindex & 1)
            hash = hash(begin(*it), end(*it), begin(hash), end(hash));
        else
            hash = hash(begin(hash), end(hash), begin(*it), end(*it));
        nindex >>= 1;
    }
    return hash;
}

std::string cblock::tostring() const
{
    std::stringstream s;
    s << strprintf("cblock(hash=%s, ver=%d, hashprevblock=%s, hashmerkleroot=%s, ntime=%u, nbits=%08x, nnonce=%u, vtx=%u)\n",
        gethash().tostring(),
        nversion,
        hashprevblock.tostring(),
        hashmerkleroot.tostring(),
        ntime, nbits, nnonce,
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].tostring() << "\n";
    }
    s << "  vmerkletree: ";
    for (unsigned int i = 0; i < vmerkletree.size(); i++)
        s << " " << vmerkletree[i].tostring();
    s << "\n";
    return s.str();
}
