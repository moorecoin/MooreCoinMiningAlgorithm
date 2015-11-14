// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_undo_h
#define moorecoin_undo_h

#include "compressor.h" 
#include "primitives/transaction.h"
#include "serialize.h"

/** undo information for a ctxin
 *
 *  contains the prevout's ctxout being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class ctxinundo
{
public:
    ctxout txout;         // the txout data before being spent
    bool fcoinbase;       // if the outpoint was the last unspent: whether it belonged to a coinbase
    unsigned int nheight; // if the outpoint was the last unspent: its height
    int nversion;         // if the outpoint was the last unspent: its version

    ctxinundo() : txout(), fcoinbase(false), nheight(0), nversion(0) {}
    ctxinundo(const ctxout &txoutin, bool fcoinbasein = false, unsigned int nheightin = 0, int nversionin = 0) : txout(txoutin), fcoinbase(fcoinbasein), nheight(nheightin), nversion(nversionin) { }

    unsigned int getserializesize(int ntype, int nversion) const {
        return ::getserializesize(varint(nheight*2+(fcoinbase ? 1 : 0)), ntype, nversion) +
               (nheight > 0 ? ::getserializesize(varint(this->nversion), ntype, nversion) : 0) +
               ::getserializesize(ctxoutcompressor(ref(txout)), ntype, nversion);
    }

    template<typename stream>
    void serialize(stream &s, int ntype, int nversion) const {
        ::serialize(s, varint(nheight*2+(fcoinbase ? 1 : 0)), ntype, nversion);
        if (nheight > 0)
            ::serialize(s, varint(this->nversion), ntype, nversion);
        ::serialize(s, ctxoutcompressor(ref(txout)), ntype, nversion);
    }

    template<typename stream>
    void unserialize(stream &s, int ntype, int nversion) {
        unsigned int ncode = 0;
        ::unserialize(s, varint(ncode), ntype, nversion);
        nheight = ncode / 2;
        fcoinbase = ncode & 1;
        if (nheight > 0)
            ::unserialize(s, varint(this->nversion), ntype, nversion);
        ::unserialize(s, ref(ctxoutcompressor(ref(txout))), ntype, nversion);
    }
};

/** undo information for a ctransaction */
class ctxundo
{
public:
    // undo information for all txins
    std::vector<ctxinundo> vprevout;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(vprevout);
    }
};

/** undo information for a cblock */
class cblockundo
{
public:
    std::vector<ctxundo> vtxundo; // for all but the coinbase

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(vtxundo);
    }
};

#endif // moorecoin_undo_h
