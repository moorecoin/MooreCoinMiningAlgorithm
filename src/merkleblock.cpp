// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "merkleblock.h"

#include "hash.h"
#include "consensus/consensus.h"
#include "utilstrencodings.h"

using namespace std;

cmerkleblock::cmerkleblock(const cblock& block, cbloomfilter& filter)
{
    header = block.getblockheader();

    vector<bool> vmatch;
    vector<uint256> vhashes;

    vmatch.reserve(block.vtx.size());
    vhashes.reserve(block.vtx.size());

    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const uint256& hash = block.vtx[i].gethash();
        if (filter.isrelevantandupdate(block.vtx[i]))
        {
            vmatch.push_back(true);
            vmatchedtxn.push_back(make_pair(i, hash));
        }
        else
            vmatch.push_back(false);
        vhashes.push_back(hash);
    }

    txn = cpartialmerkletree(vhashes, vmatch);
}

cmerkleblock::cmerkleblock(const cblock& block, const std::set<uint256>& txids)
{
    header = block.getblockheader();

    vector<bool> vmatch;
    vector<uint256> vhashes;

    vmatch.reserve(block.vtx.size());
    vhashes.reserve(block.vtx.size());

    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const uint256& hash = block.vtx[i].gethash();
        if (txids.count(hash))
            vmatch.push_back(true);
        else
            vmatch.push_back(false);
        vhashes.push_back(hash);
    }

    txn = cpartialmerkletree(vhashes, vmatch);
}

uint256 cpartialmerkletree::calchash(int height, unsigned int pos, const std::vector<uint256> &vtxid) {
    if (height == 0) {
        // hash at height 0 is the txids themself
        return vtxid[pos];
    } else {
        // calculate left hash
        uint256 left = calchash(height-1, pos*2, vtxid), right;
        // calculate right hash if not beyond the end of the array - copy left hash otherwise1
        if (pos*2+1 < calctreewidth(height-1))
            right = calchash(height-1, pos*2+1, vtxid);
        else
            right = left;
        // combine subhashes
        return hash(begin(left), end(left), begin(right), end(right));
    }
}

void cpartialmerkletree::traverseandbuild(int height, unsigned int pos, const std::vector<uint256> &vtxid, const std::vector<bool> &vmatch) {
    // determine whether this node is the parent of at least one matched txid
    bool fparentofmatch = false;
    for (unsigned int p = pos << height; p < (pos+1) << height && p < ntransactions; p++)
        fparentofmatch |= vmatch[p];
    // store as flag bit
    vbits.push_back(fparentofmatch);
    if (height==0 || !fparentofmatch) {
        // if at height 0, or nothing interesting below, store hash and stop
        vhash.push_back(calchash(height, pos, vtxid));
    } else {
        // otherwise, don't store any hash, but descend into the subtrees
        traverseandbuild(height-1, pos*2, vtxid, vmatch);
        if (pos*2+1 < calctreewidth(height-1))
            traverseandbuild(height-1, pos*2+1, vtxid, vmatch);
    }
}

uint256 cpartialmerkletree::traverseandextract(int height, unsigned int pos, unsigned int &nbitsused, unsigned int &nhashused, std::vector<uint256> &vmatch) {
    if (nbitsused >= vbits.size()) {
        // overflowed the bits array - failure
        fbad = true;
        return uint256();
    }
    bool fparentofmatch = vbits[nbitsused++];
    if (height==0 || !fparentofmatch) {
        // if at height 0, or nothing interesting below, use stored hash and do not descend
        if (nhashused >= vhash.size()) {
            // overflowed the hash array - failure
            fbad = true;
            return uint256();
        }
        const uint256 &hash = vhash[nhashused++];
        if (height==0 && fparentofmatch) // in case of height 0, we have a matched txid
            vmatch.push_back(hash);
        return hash;
    } else {
        // otherwise, descend into the subtrees to extract matched txids and hashes
        uint256 left = traverseandextract(height-1, pos*2, nbitsused, nhashused, vmatch), right;
        if (pos*2+1 < calctreewidth(height-1)) {
            right = traverseandextract(height-1, pos*2+1, nbitsused, nhashused, vmatch);
            if (right == left) {
                // the left and right branches should never be identical, as the transaction
                // hashes covered by them must each be unique.
                fbad = true;
            }
        } else {
            right = left;
        }
        // and combine them before returning
        return hash(begin(left), end(left), begin(right), end(right));
    }
}

cpartialmerkletree::cpartialmerkletree(const std::vector<uint256> &vtxid, const std::vector<bool> &vmatch) : ntransactions(vtxid.size()), fbad(false) {
    // reset state
    vbits.clear();
    vhash.clear();

    // calculate height of tree
    int nheight = 0;
    while (calctreewidth(nheight) > 1)
        nheight++;

    // traverse the partial tree
    traverseandbuild(nheight, 0, vtxid, vmatch);
}

cpartialmerkletree::cpartialmerkletree() : ntransactions(0), fbad(true) {}

uint256 cpartialmerkletree::extractmatches(std::vector<uint256> &vmatch) {
    vmatch.clear();
    // an empty set will not work
    if (ntransactions == 0)
        return uint256();
    // check for excessively high numbers of transactions
    if (ntransactions > max_block_size / 60) // 60 is the lower bound for the size of a serialized ctransaction
        return uint256();
    // there can never be more hashes provided than one for every txid
    if (vhash.size() > ntransactions)
        return uint256();
    // there must be at least one bit per node in the partial tree, and at least one node per hash
    if (vbits.size() < vhash.size())
        return uint256();
    // calculate height of tree
    int nheight = 0;
    while (calctreewidth(nheight) > 1)
        nheight++;
    // traverse the partial tree
    unsigned int nbitsused = 0, nhashused = 0;
    uint256 hashmerkleroot = traverseandextract(nheight, 0, nbitsused, nhashused, vmatch);
    // verify that no problems occured during the tree traversal
    if (fbad)
        return uint256();
    // verify that all bits were consumed (except for the padding caused by serializing it as a byte sequence)
    if ((nbitsused+7)/8 != (vbits.size()+7)/8)
        return uint256();
    // verify that all hashes were consumed
    if (nhashused != vhash.size())
        return uint256();
    return hashmerkleroot;
}
