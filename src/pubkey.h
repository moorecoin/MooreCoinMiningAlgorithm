// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_pubkey_h
#define moorecoin_pubkey_h

#include "hash.h"
#include "serialize.h"
#include "uint256.h"

#include <stdexcept>
#include <vector>

/** 
 * secp256k1:
 * const unsigned int private_key_size = 279;
 * const unsigned int public_key_size  = 65;
 * const unsigned int signature_size   = 72;
 *
 * see www.keylength.com
 * script supports up to 75 for single byte push
 */

/** a reference to a ckey: the hash160 of its serialized public key */
class ckeyid : public uint160
{
public:
    ckeyid() : uint160() {}
    ckeyid(const uint160& in) : uint160(in) {}
};

typedef uint256 chaincode;

/** an encapsulated public key. */
class cpubkey
{
private:

    /**
     * just store the serialized data.
     * its length can very cheaply be computed from the first byte.
     */
    unsigned char vch[65];

    //! compute the length of a pubkey with a given first byte.
    unsigned int static getlen(unsigned char chheader)
    {
        if (chheader == 2 || chheader == 3)
            return 33;
        if (chheader == 4 || chheader == 6 || chheader == 7)
            return 65;
        return 0;
    }

    //! set this key data to be invalid
    void invalidate()
    {
        vch[0] = 0xff;
    }

public:
    //! construct an invalid public key.
    cpubkey()
    {
        invalidate();
    }

    //! initialize a public key using begin/end iterators to byte data.
    template <typename t>
    void set(const t pbegin, const t pend)
    {
        int len = pend == pbegin ? 0 : getlen(pbegin[0]);
        if (len && len == (pend - pbegin))
            memcpy(vch, (unsigned char*)&pbegin[0], len);
        else
            invalidate();
    }

    //! construct a public key using begin/end iterators to byte data.
    template <typename t>
    cpubkey(const t pbegin, const t pend)
    {
        set(pbegin, pend);
    }

    //! construct a public key from a byte vector.
    cpubkey(const std::vector<unsigned char>& vch)
    {
        set(vch.begin(), vch.end());
    }

    //! simple read-only vector-like interface to the pubkey data.
    unsigned int size() const { return getlen(vch[0]); }
    const unsigned char* begin() const { return vch; }
    const unsigned char* end() const { return vch + size(); }
    const unsigned char& operator[](unsigned int pos) const { return vch[pos]; }

    //! comparator implementation.
    friend bool operator==(const cpubkey& a, const cpubkey& b)
    {
        return a.vch[0] == b.vch[0] &&
               memcmp(a.vch, b.vch, a.size()) == 0;
    }
    friend bool operator!=(const cpubkey& a, const cpubkey& b)
    {
        return !(a == b);
    }
    friend bool operator<(const cpubkey& a, const cpubkey& b)
    {
        return a.vch[0] < b.vch[0] ||
               (a.vch[0] == b.vch[0] && memcmp(a.vch, b.vch, a.size()) < 0);
    }

    //! implement serialization, as if this was a byte vector.
    unsigned int getserializesize(int ntype, int nversion) const
    {
        return size() + 1;
    }
    template <typename stream>
    void serialize(stream& s, int ntype, int nversion) const
    {
        unsigned int len = size();
        ::writecompactsize(s, len);
        s.write((char*)vch, len);
    }
    template <typename stream>
    void unserialize(stream& s, int ntype, int nversion)
    {
        unsigned int len = ::readcompactsize(s);
        if (len <= 65) {
            s.read((char*)vch, len);
        } else {
            // invalid pubkey, skip available data
            char dummy;
            while (len--)
                s.read(&dummy, 1);
            invalidate();
        }
    }

    //! get the keyid of this public key (hash of its serialization)
    ckeyid getid() const
    {
        return ckeyid(hash160(vch, vch + size()));
    }

    //! get the 256-bit hash of this public key.
    uint256 gethash() const
    {
        return hash(vch, vch + size());
    }

    /*
     * check syntactic correctness.
     * 
     * note that this is consensus critical as checksig() calls it!
     */
    bool isvalid() const
    {
        return size() > 0;
    }

    //! fully validate whether this is a valid public key (more expensive than isvalid())
    bool isfullyvalid() const;

    //! check whether this is a compressed public key.
    bool iscompressed() const
    {
        return size() == 33;
    }

    /**
     * verify a der signature (~72 bytes).
     * if this public key is not fully valid, the return value will be false.
     */
    bool verify(const uint256& hash, const std::vector<unsigned char>& vchsig) const;

    //! recover a public key from a compact signature.
    bool recovercompact(const uint256& hash, const std::vector<unsigned char>& vchsig);

    //! turn this public key into an uncompressed public key.
    bool decompress();

    //! derive bip32 child pubkey.
    bool derive(cpubkey& pubkeychild, chaincode &ccchild, unsigned int nchild, const chaincode& cc) const;
};

struct cextpubkey {
    unsigned char ndepth;
    unsigned char vchfingerprint[4];
    unsigned int nchild;
    chaincode chaincode;
    cpubkey pubkey;

    friend bool operator==(const cextpubkey &a, const cextpubkey &b)
    {
        return a.ndepth == b.ndepth && memcmp(&a.vchfingerprint[0], &b.vchfingerprint[0], 4) == 0 && a.nchild == b.nchild &&
               a.chaincode == b.chaincode && a.pubkey == b.pubkey;
    }

    void encode(unsigned char code[74]) const;
    void decode(const unsigned char code[74]);
    bool derive(cextpubkey& out, unsigned int nchild) const;
};

#endif // moorecoin_pubkey_h
