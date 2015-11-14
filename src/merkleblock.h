// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_merkleblock_h
#define moorecoin_merkleblock_h

#include "serialize.h"
#include "uint256.h"
#include "primitives/block.h"
#include "bloom.h"

#include <vector>

/** data structure that represents a partial merkle tree.
 *
 * it represents a subset of the txid's of a known block, in a way that
 * allows recovery of the list of txid's and the merkle root, in an
 * authenticated way.
 *
 * the encoding works as follows: we traverse the tree in depth-first order,
 * storing a bit for each traversed node, signifying whether the node is the
 * parent of at least one matched leaf txid (or a matched txid itself). in
 * case we are at the leaf level, or this bit is 0, its merkle node hash is
 * stored, and its children are not explorer further. otherwise, no hash is
 * stored, but we recurse into both (or the only) child branch. during
 * decoding, the same depth-first traversal is performed, consuming bits and
 * hashes as they written during encoding.
 *
 * the serialization is fixed and provides a hard guarantee about the
 * encoded size:
 *
 *   size <= 10 + ceil(32.25*n)
 *
 * where n represents the number of leaf nodes of the partial tree. n itself
 * is bounded by:
 *
 *   n <= total_transactions
 *   n <= 1 + matched_transactions*tree_height
 *
 * the serialization format:
 *  - uint32     total_transactions (4 bytes)
 *  - varint     number of hashes   (1-3 bytes)
 *  - uint256[]  hashes in depth-first order (<= 32*n bytes)
 *  - varint     number of bytes of flag bits (1-3 bytes)
 *  - byte[]     flag bits, packed per 8 in a byte, least significant bit first (<= 2*n-1 bits)
 * the size constraints follow from this.
 */
class cpartialmerkletree
{
protected:
    /** the total number of transactions in the block */
    unsigned int ntransactions;

    /** node-is-parent-of-matched-txid bits */
    std::vector<bool> vbits;

    /** txids and internal hashes */
    std::vector<uint256> vhash;

    /** flag set when encountering invalid data */
    bool fbad;

    /** helper function to efficiently calculate the number of nodes at given height in the merkle tree */
    unsigned int calctreewidth(int height) {
        return (ntransactions+(1 << height)-1) >> height;
    }

    /** calculate the hash of a node in the merkle tree (at leaf level: the txid's themselves) */
    uint256 calchash(int height, unsigned int pos, const std::vector<uint256> &vtxid);

    /** recursive function that traverses tree nodes, storing the data as bits and hashes */
    void traverseandbuild(int height, unsigned int pos, const std::vector<uint256> &vtxid, const std::vector<bool> &vmatch);

    /**
     * recursive function that traverses tree nodes, consuming the bits and hashes produced by traverseandbuild.
     * it returns the hash of the respective node.
     */
    uint256 traverseandextract(int height, unsigned int pos, unsigned int &nbitsused, unsigned int &nhashused, std::vector<uint256> &vmatch);

public:

    /** serialization implementation */
    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(ntransactions);
        readwrite(vhash);
        std::vector<unsigned char> vbytes;
        if (ser_action.forread()) {
            readwrite(vbytes);
            cpartialmerkletree &us = *(const_cast<cpartialmerkletree*>(this));
            us.vbits.resize(vbytes.size() * 8);
            for (unsigned int p = 0; p < us.vbits.size(); p++)
                us.vbits[p] = (vbytes[p / 8] & (1 << (p % 8))) != 0;
            us.fbad = false;
        } else {
            vbytes.resize((vbits.size()+7)/8);
            for (unsigned int p = 0; p < vbits.size(); p++)
                vbytes[p / 8] |= vbits[p] << (p % 8);
            readwrite(vbytes);
        }
    }

    /** construct a partial merkle tree from a list of transaction ids, and a mask that selects a subset of them */
    cpartialmerkletree(const std::vector<uint256> &vtxid, const std::vector<bool> &vmatch);

    cpartialmerkletree();

    /**
     * extract the matching txid's represented by this partial merkle tree.
     * returns the merkle root, or 0 in case of failure
     */
    uint256 extractmatches(std::vector<uint256> &vmatch);
};


/**
 * used to relay blocks as header + vector<merkle branch>
 * to filtered nodes.
 */
class cmerkleblock
{
public:
    /** public only for unit testing */
    cblockheader header;
    cpartialmerkletree txn;

public:
    /** public only for unit testing and relay testing (not relayed) */
    std::vector<std::pair<unsigned int, uint256> > vmatchedtxn;

    /**
     * create from a cblock, filtering transactions according to filter
     * note that this will call isrelevantandupdate on the filter for each transaction,
     * thus the filter will likely be modified.
     */
    cmerkleblock(const cblock& block, cbloomfilter& filter);

    // create from a cblock, matching the txids in the set
    cmerkleblock(const cblock& block, const std::set<uint256>& txids);

    cmerkleblock() {}

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(header);
        readwrite(txn);
    }
};

#endif // moorecoin_merkleblock_h
