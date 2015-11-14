// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_key_h
#define moorecoin_key_h

#include "pubkey.h"
#include "serialize.h"
#include "support/allocators/secure.h"
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

/**
 * secure_allocator is defined in allocators.h
 * cprivkey is a serialized private key, with all parameters included (279 bytes)
 */
typedef std::vector<unsigned char, secure_allocator<unsigned char> > cprivkey;

/** an encapsulated private key. */
class ckey
{
private:
    //! whether this private key is valid. we check for correctness when modifying the key
    //! data, so fvalid should always correspond to the actual state.
    bool fvalid;

    //! whether the public key corresponding to this private key is (to be) compressed.
    bool fcompressed;

    //! the actual byte data
    unsigned char vch[32];

    //! check whether the 32-byte array pointed to be vch is valid keydata.
    bool static check(const unsigned char* vch);

public:
    //! construct an invalid private key.
    ckey() : fvalid(false), fcompressed(false)
    {
        lockobject(vch);
    }

    //! copy constructor. this is necessary because of memlocking.
    ckey(const ckey& secret) : fvalid(secret.fvalid), fcompressed(secret.fcompressed)
    {
        lockobject(vch);
        memcpy(vch, secret.vch, sizeof(vch));
    }

    //! destructor (again necessary because of memlocking).
    ~ckey()
    {
        unlockobject(vch);
    }

    friend bool operator==(const ckey& a, const ckey& b)
    {
        return a.fcompressed == b.fcompressed && a.size() == b.size() &&
               memcmp(&a.vch[0], &b.vch[0], a.size()) == 0;
    }

    //! initialize using begin and end iterators to byte data.
    template <typename t>
    void set(const t pbegin, const t pend, bool fcompressedin)
    {
        if (pend - pbegin != 32) {
            fvalid = false;
            return;
        }
        if (check(&pbegin[0])) {
            memcpy(vch, (unsigned char*)&pbegin[0], 32);
            fvalid = true;
            fcompressed = fcompressedin;
        } else {
            fvalid = false;
        }
    }

    //! simple read-only vector-like interface.
    unsigned int size() const { return (fvalid ? 32 : 0); }
    const unsigned char* begin() const { return vch; }
    const unsigned char* end() const { return vch + size(); }

    //! check whether this private key is valid.
    bool isvalid() const { return fvalid; }

    //! check whether the public key corresponding to this private key is (to be) compressed.
    bool iscompressed() const { return fcompressed; }

    //! initialize from a cprivkey (serialized openssl private key data).
    bool setprivkey(const cprivkey& vchprivkey, bool fcompressed);

    //! generate a new private key using a cryptographic prng.
    void makenewkey(bool fcompressed);

    /**
     * convert the private key to a cprivkey (serialized openssl private key data).
     * this is expensive. 
     */
    cprivkey getprivkey() const;

    /**
     * compute the public key from a private key.
     * this is expensive.
     */
    cpubkey getpubkey() const;

    /**
     * create a der-serialized signature.
     * the test_case parameter tweaks the deterministic nonce.
     */
    bool sign(const uint256& hash, std::vector<unsigned char>& vchsig, uint32_t test_case = 0) const;

    /**
     * create a compact signature (65 bytes), which allows reconstructing the used public key.
     * the format is one header byte, followed by two times 32 bytes for the serialized r and s values.
     * the header byte: 0x1b = first key with even y, 0x1c = first key with odd y,
     *                  0x1d = second key with even y, 0x1e = second key with odd y,
     *                  add 0x04 for compressed keys.
     */
    bool signcompact(const uint256& hash, std::vector<unsigned char>& vchsig) const;

    //! derive bip32 child key.
    bool derive(ckey& keychild, chaincode &ccchild, unsigned int nchild, const chaincode& cc) const;

    /**
     * verify thoroughly whether a private key and a public key match.
     * this is done using a different mechanism than just regenerating it.
     */
    bool verifypubkey(const cpubkey& vchpubkey) const;

    //! load private key and check that public key matches.
    bool load(cprivkey& privkey, cpubkey& vchpubkey, bool fskipcheck);

    //! check whether an element of a signature (r or s) is valid.
    static bool checksignatureelement(const unsigned char* vch, int len, bool half);
};

struct cextkey {
    unsigned char ndepth;
    unsigned char vchfingerprint[4];
    unsigned int nchild;
    chaincode chaincode;
    ckey key;

    friend bool operator==(const cextkey& a, const cextkey& b)
    {
        return a.ndepth == b.ndepth && memcmp(&a.vchfingerprint[0], &b.vchfingerprint[0], 4) == 0 && a.nchild == b.nchild &&
               a.chaincode == b.chaincode && a.key == b.key;
    }

    void encode(unsigned char code[74]) const;
    void decode(const unsigned char code[74]);
    bool derive(cextkey& out, unsigned int nchild) const;
    cextpubkey neuter() const;
    void setmaster(const unsigned char* seed, unsigned int nseedlen);
};

/** initialize the elliptic curve support. may not be called twice without calling ecc_stop first. */
void ecc_start(void);

/** deinitialize the elliptic curve support. no-op if ecc_start wasn't called first. */
void ecc_stop(void);

/** check that required ec support is available at runtime. */
bool ecc_initsanitycheck(void);

#endif // moorecoin_key_h
