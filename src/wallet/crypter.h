// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_wallet_crypter_h
#define moorecoin_wallet_crypter_h

#include "keystore.h"
#include "serialize.h"
#include "support/allocators/secure.h"

class uint256;

const unsigned int wallet_crypto_key_size = 32;
const unsigned int wallet_crypto_salt_size = 8;

/**
 * private key encryption is done based on a cmasterkey,
 * which holds a salt and random encryption key.
 * 
 * cmasterkeys are encrypted using aes-256-cbc using a key
 * derived using derivation method nderivationmethod
 * (0 == evp_sha512()) and derivation iterations nderiveiterations.
 * vchotherderivationparameters is provided for alternative algorithms
 * which may require more parameters (such as scrypt).
 * 
 * wallet private keys are then encrypted using aes-256-cbc
 * with the double-sha256 of the public key as the iv, and the
 * master key's key as the encryption key (see keystore.[ch]).
 */

/** master key for wallet encryption */
class cmasterkey
{
public:
    std::vector<unsigned char> vchcryptedkey;
    std::vector<unsigned char> vchsalt;
    //! 0 = evp_sha512()
    //! 1 = scrypt()
    unsigned int nderivationmethod;
    unsigned int nderiveiterations;
    //! use this for more parameters to key derivation,
    //! such as the various parameters to scrypt
    std::vector<unsigned char> vchotherderivationparameters;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(vchcryptedkey);
        readwrite(vchsalt);
        readwrite(nderivationmethod);
        readwrite(nderiveiterations);
        readwrite(vchotherderivationparameters);
    }

    cmasterkey()
    {
        // 25000 rounds is just under 0.1 seconds on a 1.86 ghz pentium m
        // ie slightly lower than the lowest hardware we need bother supporting
        nderiveiterations = 25000;
        nderivationmethod = 0;
        vchotherderivationparameters = std::vector<unsigned char>(0);
    }
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > ckeyingmaterial;

/** encryption/decryption context with key information */
class ccrypter
{
private:
    unsigned char chkey[wallet_crypto_key_size];
    unsigned char chiv[wallet_crypto_key_size];
    bool fkeyset;

public:
    bool setkeyfrompassphrase(const securestring &strkeydata, const std::vector<unsigned char>& chsalt, const unsigned int nrounds, const unsigned int nderivationmethod);
    bool encrypt(const ckeyingmaterial& vchplaintext, std::vector<unsigned char> &vchciphertext);
    bool decrypt(const std::vector<unsigned char>& vchciphertext, ckeyingmaterial& vchplaintext);
    bool setkey(const ckeyingmaterial& chnewkey, const std::vector<unsigned char>& chnewiv);

    void cleankey()
    {
        memory_cleanse(chkey, sizeof(chkey));
        memory_cleanse(chiv, sizeof(chiv));
        fkeyset = false;
    }

    ccrypter()
    {
        fkeyset = false;

        // try to keep the key data out of swap (and be a bit over-careful to keep the iv that we don't even use out of swap)
        // note that this does nothing about suspend-to-disk (which will put all our key data on disk)
        // note as well that at no point in this program is any attempt made to prevent stealing of keys by reading the memory of the running process.
        lockedpagemanager::instance().lockrange(&chkey[0], sizeof chkey);
        lockedpagemanager::instance().lockrange(&chiv[0], sizeof chiv);
    }

    ~ccrypter()
    {
        cleankey();

        lockedpagemanager::instance().unlockrange(&chkey[0], sizeof chkey);
        lockedpagemanager::instance().unlockrange(&chiv[0], sizeof chiv);
    }
};

/** keystore which keeps the private keys encrypted.
 * it derives from the basic key store, which is used if no encryption is active.
 */
class ccryptokeystore : public cbasickeystore
{
private:
    cryptedkeymap mapcryptedkeys;

    ckeyingmaterial vmasterkey;

    //! if fusecrypto is true, mapkeys must be empty
    //! if fusecrypto is false, vmasterkey must be empty
    bool fusecrypto;

    //! keeps track of whether unlock has run a thorough check before
    bool fdecryptionthoroughlychecked;

protected:
    bool setcrypted();

    //! will encrypt previously unencrypted keys
    bool encryptkeys(ckeyingmaterial& vmasterkeyin);

    bool unlock(const ckeyingmaterial& vmasterkeyin);

public:
    ccryptokeystore() : fusecrypto(false), fdecryptionthoroughlychecked(false)
    {
    }

    bool iscrypted() const
    {
        return fusecrypto;
    }

    bool islocked() const
    {
        if (!iscrypted())
            return false;
        bool result;
        {
            lock(cs_keystore);
            result = vmasterkey.empty();
        }
        return result;
    }

    bool lock();

    virtual bool addcryptedkey(const cpubkey &vchpubkey, const std::vector<unsigned char> &vchcryptedsecret);
    bool addkeypubkey(const ckey& key, const cpubkey &pubkey);
    bool havekey(const ckeyid &address) const
    {
        {
            lock(cs_keystore);
            if (!iscrypted())
                return cbasickeystore::havekey(address);
            return mapcryptedkeys.count(address) > 0;
        }
        return false;
    }
    bool getkey(const ckeyid &address, ckey& keyout) const;
    bool getpubkey(const ckeyid &address, cpubkey& vchpubkeyout) const;
    void getkeys(std::set<ckeyid> &setaddress) const
    {
        if (!iscrypted())
        {
            cbasickeystore::getkeys(setaddress);
            return;
        }
        setaddress.clear();
        cryptedkeymap::const_iterator mi = mapcryptedkeys.begin();
        while (mi != mapcryptedkeys.end())
        {
            setaddress.insert((*mi).first);
            mi++;
        }
    }

    /**
     * wallet status (encrypted, locked) changed.
     * note: called without locks held.
     */
    boost::signals2::signal<void (ccryptokeystore* wallet)> notifystatuschanged;
};

#endif // moorecoin_wallet_crypter_h
