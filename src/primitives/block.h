// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_primitives_block_h
#define moorecoin_primitives_block_h

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"

/** nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  when they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  the first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class cblockheader
{
public:
    // header
    static const int32_t current_version=3;
    int32_t nversion;
    uint256 hashprevblock;
    uint256 hashmerkleroot;
    uint32_t ntime;
    uint32_t nbits;
    uint32_t nnonce;

    cblockheader()
    {
        setnull();
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(this->nversion);
        nversion = this->nversion;
        readwrite(hashprevblock);
        readwrite(hashmerkleroot);
        readwrite(ntime);
        readwrite(nbits);
        readwrite(nnonce);
    }

    void setnull()
    {
        nversion = cblockheader::current_version;
        hashprevblock.setnull();
        hashmerkleroot.setnull();
        ntime = 0;
        nbits = 0;
        nnonce = 0;
    }

    bool isnull() const
    {
        return (nbits == 0);
    }

    uint256 gethash() const;

    int64_t getblocktime() const
    {
        return (int64_t)ntime;
    }
};


class cblock : public cblockheader
{
public:
    // network and disk
    std::vector<ctransaction> vtx;

    // memory only
    mutable std::vector<uint256> vmerkletree;

    cblock()
    {
        setnull();
    }

    cblock(const cblockheader &header)
    {
        setnull();
        *((cblockheader*)this) = header;
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(*(cblockheader*)this);
        readwrite(vtx);
    }

    void setnull()
    {
        cblockheader::setnull();
        vtx.clear();
        vmerkletree.clear();
    }

    cblockheader getblockheader() const
    {
        cblockheader block;
        block.nversion       = nversion;
        block.hashprevblock  = hashprevblock;
        block.hashmerkleroot = hashmerkleroot;
        block.ntime          = ntime;
        block.nbits          = nbits;
        block.nnonce         = nnonce;
        return block;
    }

    // build the in-memory merkle tree for this block and return the merkle root.
    // if non-null, *mutated is set to whether mutation was detected in the merkle
    // tree (a duplication of transactions in the block leading to an identical
    // merkle root).
    uint256 buildmerkletree(bool* mutated = null) const;

    std::vector<uint256> getmerklebranch(int nindex) const;
    static uint256 checkmerklebranch(uint256 hash, const std::vector<uint256>& vmerklebranch, int nindex);
    std::string tostring() const;
};


/** describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * the further back it is, the further before the fork it may be.
 */
struct cblocklocator
{
    std::vector<uint256> vhave;

    cblocklocator() {}

    cblocklocator(const std::vector<uint256>& vhavein)
    {
        vhave = vhavein;
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!(ntype & ser_gethash))
            readwrite(nversion);
        readwrite(vhave);
    }

    void setnull()
    {
        vhave.clear();
    }

    bool isnull() const
    {
        return vhave.empty();
    }
};

#endif // moorecoin_primitives_block_h
