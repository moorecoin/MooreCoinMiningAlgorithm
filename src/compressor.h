// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_compressor_h
#define moorecoin_compressor_h

#include "primitives/transaction.h"
#include "script/script.h"
#include "serialize.h"

class ckeyid;
class cpubkey;
class cscriptid;

/** compact serializer for scripts.
 *
 *  it detects common cases and encodes them much more efficiently.
 *  3 special cases are defined:
 *  * pay to pubkey hash (encoded as 21 bytes)
 *  * pay to script hash (encoded as 21 bytes)
 *  * pay to pubkey starting with 0x02, 0x03 or 0x04 (encoded as 33 bytes)
 *
 *  other scripts up to 121 bytes require 1 byte + script length. above
 *  that, scripts up to 16505 bytes require 2 bytes + script length.
 */
class cscriptcompressor
{
private:
    /**
     * make this static for now (there are only 6 special scripts defined)
     * this can potentially be extended together with a new nversion for
     * transactions, in which case this value becomes dependent on nversion
     * and nheight of the enclosing transaction.
     */
    static const unsigned int nspecialscripts = 6;

    cscript &script;
protected:
    /**
     * these check for scripts for which a special case with a shorter encoding is defined.
     * they are implemented separately from the cscript test, as these test for exact byte
     * sequence correspondences, and are more strict. for example, istopubkey also verifies
     * whether the public key is valid (as invalid ones cannot be represented in compressed
     * form).
     */
    bool istokeyid(ckeyid &hash) const;
    bool istoscriptid(cscriptid &hash) const;
    bool istopubkey(cpubkey &pubkey) const;

    bool compress(std::vector<unsigned char> &out) const;
    unsigned int getspecialsize(unsigned int nsize) const;
    bool decompress(unsigned int nsize, const std::vector<unsigned char> &out);
public:
    cscriptcompressor(cscript &scriptin) : script(scriptin) { }

    unsigned int getserializesize(int ntype, int nversion) const {
        std::vector<unsigned char> compr;
        if (compress(compr))
            return compr.size();
        unsigned int nsize = script.size() + nspecialscripts;
        return script.size() + varint(nsize).getserializesize(ntype, nversion);
    }

    template<typename stream>
    void serialize(stream &s, int ntype, int nversion) const {
        std::vector<unsigned char> compr;
        if (compress(compr)) {
            s << cflatdata(compr);
            return;
        }
        unsigned int nsize = script.size() + nspecialscripts;
        s << varint(nsize);
        s << cflatdata(script);
    }

    template<typename stream>
    void unserialize(stream &s, int ntype, int nversion) {
        unsigned int nsize = 0;
        s >> varint(nsize);
        if (nsize < nspecialscripts) {
            std::vector<unsigned char> vch(getspecialsize(nsize), 0x00);
            s >> ref(cflatdata(vch));
            decompress(nsize, vch);
            return;
        }
        nsize -= nspecialscripts;
        script.resize(nsize);
        s >> ref(cflatdata(script));
    }
};

/** wrapper for ctxout that provides a more compact serialization */
class ctxoutcompressor
{
private:
    ctxout &txout;

public:
    static uint64_t compressamount(uint64_t namount);
    static uint64_t decompressamount(uint64_t namount);

    ctxoutcompressor(ctxout &txoutin) : txout(txoutin) { }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        if (!ser_action.forread()) {
            uint64_t nval = compressamount(txout.nvalue);
            readwrite(varint(nval));
        } else {
            uint64_t nval = 0;
            readwrite(varint(nval));
            txout.nvalue = decompressamount(nval);
        }
        cscriptcompressor cscript(ref(txout.scriptpubkey));
        readwrite(cscript);
    }
};

#endif // moorecoin_compressor_h
