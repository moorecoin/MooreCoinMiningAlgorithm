// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_chain_h
#define moorecoin_chain_h

#include "arith_uint256.h"
#include "primitives/block.h"
#include "pow.h"
#include "tinyformat.h"
#include "uint256.h"

#include <vector>

#include <boost/foreach.hpp>

struct cdiskblockpos
{
    int nfile;
    unsigned int npos;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(varint(nfile));
        readwrite(varint(npos));
    }

    cdiskblockpos() {
        setnull();
    }

    cdiskblockpos(int nfilein, unsigned int nposin) {
        nfile = nfilein;
        npos = nposin;
    }

    friend bool operator==(const cdiskblockpos &a, const cdiskblockpos &b) {
        return (a.nfile == b.nfile && a.npos == b.npos);
    }

    friend bool operator!=(const cdiskblockpos &a, const cdiskblockpos &b) {
        return !(a == b);
    }

    void setnull() { nfile = -1; npos = 0; }
    bool isnull() const { return (nfile == -1); }

    std::string tostring() const
    {
        return strprintf("cblockdiskpos(nfile=%i, npos=%i)", nfile, npos);
    }

};

enum blockstatus {
    //! unused.
    block_valid_unknown      =    0,

    //! parsed, version ok, hash satisfies claimed pow, 1 <= vtx count <= max, timestamp not in future
    block_valid_header       =    1,

    //! all parent headers found, difficulty matches, timestamp >= median previous, checkpoint. implies all parents
    //! are also at least tree.
    block_valid_tree         =    2,

    /**
     * only first tx is coinbase, 2 <= coinbase input script length <= 100, transactions valid, no duplicate txids,
     * sigops, size, merkle root. implies all parents are at least tree but not necessarily transactions. when all
     * parent blocks also have transactions, cblockindex::nchaintx will be set.
     */
    block_valid_transactions =    3,

    //! outputs do not overspend inputs, no double spends, coinbase output ok, no immature coinbase spends, bip30.
    //! implies all parents are also at least chain.
    block_valid_chain        =    4,

    //! scripts & signatures ok. implies all parents are also at least scripts.
    block_valid_scripts      =    5,

    //! all validity bits.
    block_valid_mask         =   block_valid_header | block_valid_tree | block_valid_transactions |
                                 block_valid_chain | block_valid_scripts,

    block_have_data          =    8, //! full block available in blk*.dat
    block_have_undo          =   16, //! undo data available in rev*.dat
    block_have_mask          =   block_have_data | block_have_undo,

    block_failed_valid       =   32, //! stage after last reached validness failed
    block_failed_child       =   64, //! descends from failed block
    block_failed_mask        =   block_failed_valid | block_failed_child,
};

/** the block chain is a tree shaped structure starting with the
 * genesis block at the root, with each block potentially having multiple
 * candidates to be the next block. a blockindex may have multiple pprev pointing
 * to it, but at most one of them can be part of the currently active branch.
 */
class cblockindex
{
public:
    //! pointer to the hash of the block, if any. memory is owned by this cblockindex
    const uint256* phashblock;

    //! pointer to the index of the predecessor of this block
    cblockindex* pprev;

    //! pointer to the index of some further predecessor of this block
    cblockindex* pskip;

    //! height of the entry in the chain. the genesis block has height 0
    int nheight;

    //! which # file this block is stored in (blk?????.dat)
    int nfile;

    //! byte offset within blk?????.dat where this block's data is stored
    unsigned int ndatapos;

    //! byte offset within rev?????.dat where this block's undo data is stored
    unsigned int nundopos;

    //! (memory only) total amount of work (expected number of hashes) in the chain up to and including this block
    arith_uint256 nchainwork;

    //! number of transactions in this block.
    //! note: in a potential headers-first mode, this number cannot be relied upon
    unsigned int ntx;

    //! (memory only) number of transactions in the chain up to and including this block.
    //! this value will be non-zero only if and only if transactions for this block and all its parents are available.
    //! change to 64-bit type when necessary; won't happen before 2030
    unsigned int nchaintx;

    //! verification status of this block. see enum blockstatus
    unsigned int nstatus;

    //! block header
    int nversion;
    uint256 hashmerkleroot;
    unsigned int ntime;
    unsigned int nbits;
    unsigned int nnonce;

    //! (memory only) sequential id assigned to distinguish order in which blocks are received.
    uint32_t nsequenceid;

    void setnull()
    {
        phashblock = null;
        pprev = null;
        pskip = null;
        nheight = 0;
        nfile = 0;
        ndatapos = 0;
        nundopos = 0;
        nchainwork = arith_uint256();
        ntx = 0;
        nchaintx = 0;
        nstatus = 0;
        nsequenceid = 0;

        nversion       = 0;
        hashmerkleroot = uint256();
        ntime          = 0;
        nbits          = 0;
        nnonce         = 0;
    }

    cblockindex()
    {
        setnull();
    }

    cblockindex(const cblockheader& block)
    {
        setnull();

        nversion       = block.nversion;
        hashmerkleroot = block.hashmerkleroot;
        ntime          = block.ntime;
        nbits          = block.nbits;
        nnonce         = block.nnonce;
    }

    cdiskblockpos getblockpos() const {
        cdiskblockpos ret;
        if (nstatus & block_have_data) {
            ret.nfile = nfile;
            ret.npos  = ndatapos;
        }
        return ret;
    }

    cdiskblockpos getundopos() const {
        cdiskblockpos ret;
        if (nstatus & block_have_undo) {
            ret.nfile = nfile;
            ret.npos  = nundopos;
        }
        return ret;
    }

    cblockheader getblockheader() const
    {
        cblockheader block;
        block.nversion       = nversion;
        if (pprev)
            block.hashprevblock = pprev->getblockhash();
        block.hashmerkleroot = hashmerkleroot;
        block.ntime          = ntime;
        block.nbits          = nbits;
        block.nnonce         = nnonce;
        return block;
    }

    uint256 getblockhash() const
    {
        return *phashblock;
    }

    int64_t getblocktime() const
    {
        return (int64_t)ntime;
    }

    enum { nmediantimespan=11 };

    int64_t getmediantimepast() const
    {
        int64_t pmedian[nmediantimespan];
        int64_t* pbegin = &pmedian[nmediantimespan];
        int64_t* pend = &pmedian[nmediantimespan];

        const cblockindex* pindex = this;
        for (int i = 0; i < nmediantimespan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->getblocktime();

        std::sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    std::string tostring() const
    {
        return strprintf("cblockindex(pprev=%p, nheight=%d, merkle=%s, hashblock=%s)",
            pprev, nheight,
            hashmerkleroot.tostring(),
            getblockhash().tostring());
    }

    //! check whether this block index entry is valid up to the passed validity level.
    bool isvalid(enum blockstatus nupto = block_valid_transactions) const
    {
        assert(!(nupto & ~block_valid_mask)); // only validity flags allowed.
        if (nstatus & block_failed_mask)
            return false;
        return ((nstatus & block_valid_mask) >= nupto);
    }

    //! raise the validity level of this block index entry.
    //! returns true if the validity was changed.
    bool raisevalidity(enum blockstatus nupto)
    {
        assert(!(nupto & ~block_valid_mask)); // only validity flags allowed.
        if (nstatus & block_failed_mask)
            return false;
        if ((nstatus & block_valid_mask) < nupto) {
            nstatus = (nstatus & ~block_valid_mask) | nupto;
            return true;
        }
        return false;
    }

    //! build the skiplist pointer for this entry.
    void buildskip();

    //! efficiently find an ancestor of this block.
    cblockindex* getancestor(int height);
    const cblockindex* getancestor(int height) const;
};

/** used to marshal pointers into hashes for db storage. */
class cdiskblockindex : public cblockindex
{
public:
    uint256 hashprev;

    cdiskblockindex() {
        hashprev = uint256();
    }

    explicit cdiskblockindex(const cblockindex* pindex) : cblockindex(*pindex) {
        hashprev = (pprev ? pprev->getblockhash() : uint256());
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!(ntype & ser_gethash))
            readwrite(varint(nversion));

        readwrite(varint(nheight));
        readwrite(varint(nstatus));
        readwrite(varint(ntx));
        if (nstatus & (block_have_data | block_have_undo))
            readwrite(varint(nfile));
        if (nstatus & block_have_data)
            readwrite(varint(ndatapos));
        if (nstatus & block_have_undo)
            readwrite(varint(nundopos));

        // block header
        readwrite(this->nversion);
        readwrite(hashprev);
        readwrite(hashmerkleroot);
        readwrite(ntime);
        readwrite(nbits);
        readwrite(nnonce);
    }

    uint256 getblockhash() const
    {
        cblockheader block;
        block.nversion        = nversion;
        block.hashprevblock   = hashprev;
        block.hashmerkleroot  = hashmerkleroot;
        block.ntime           = ntime;
        block.nbits           = nbits;
        block.nnonce          = nnonce;
        return block.gethash();
    }


    std::string tostring() const
    {
        std::string str = "cdiskblockindex(";
        str += cblockindex::tostring();
        str += strprintf("\n                hashblock=%s, hashprev=%s)",
            getblockhash().tostring(),
            hashprev.tostring());
        return str;
    }
};

/** an in-memory indexed chain of blocks. */
class cchain {
private:
    std::vector<cblockindex*> vchain;

public:
    /** returns the index entry for the genesis block of this chain, or null if none. */
    cblockindex *genesis() const {
        return vchain.size() > 0 ? vchain[0] : null;
    }

    /** returns the index entry for the tip of this chain, or null if none. */
    cblockindex *tip() const {
        return vchain.size() > 0 ? vchain[vchain.size() - 1] : null;
    }

    /** returns the index entry at a particular height in this chain, or null if no such height exists. */
    cblockindex *operator[](int nheight) const {
        if (nheight < 0 || nheight >= (int)vchain.size())
            return null;
        return vchain[nheight];
    }

    /** compare two chains efficiently. */
    friend bool operator==(const cchain &a, const cchain &b) {
        return a.vchain.size() == b.vchain.size() &&
               a.vchain[a.vchain.size() - 1] == b.vchain[b.vchain.size() - 1];
    }

    /** efficiently check whether a block is present in this chain. */
    bool contains(const cblockindex *pindex) const {
        return (*this)[pindex->nheight] == pindex;
    }

    /** find the successor of a block in this chain, or null if the given index is not found or is the tip. */
    cblockindex *next(const cblockindex *pindex) const {
        if (contains(pindex))
            return (*this)[pindex->nheight + 1];
        else
            return null;
    }

    /** return the maximal height in the chain. is equal to chain.tip() ? chain.tip()->nheight : -1. */
    int height() const {
        return vchain.size() - 1;
    }

    /** set/initialize a chain with a given tip. */
    void settip(cblockindex *pindex);

    /** return a cblocklocator that refers to a block in this chain (by default the tip). */
    cblocklocator getlocator(const cblockindex *pindex = null) const;

    /** find the last common block between this chain and a block index entry. */
    const cblockindex *findfork(const cblockindex *pindex) const;
};

#endif // moorecoin_chain_h
