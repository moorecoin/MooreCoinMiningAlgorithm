// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"

#include "script/script.h"
#include "script/standard.h"
#include "util.h"

#include <string>
#include <vector>
#include <boost/foreach.hpp>
#include <openssl/aes.h>
#include <openssl/evp.h>

bool ccrypter::setkeyfrompassphrase(const securestring& strkeydata, const std::vector<unsigned char>& chsalt, const unsigned int nrounds, const unsigned int nderivationmethod)
{
    if (nrounds < 1 || chsalt.size() != wallet_crypto_salt_size)
        return false;

    int i = 0;
    if (nderivationmethod == 0)
        i = evp_bytestokey(evp_aes_256_cbc(), evp_sha512(), &chsalt[0],
                          (unsigned char *)&strkeydata[0], strkeydata.size(), nrounds, chkey, chiv);

    if (i != (int)wallet_crypto_key_size)
    {
        memory_cleanse(chkey, sizeof(chkey));
        memory_cleanse(chiv, sizeof(chiv));
        return false;
    }

    fkeyset = true;
    return true;
}

bool ccrypter::setkey(const ckeyingmaterial& chnewkey, const std::vector<unsigned char>& chnewiv)
{
    if (chnewkey.size() != wallet_crypto_key_size || chnewiv.size() != wallet_crypto_key_size)
        return false;

    memcpy(&chkey[0], &chnewkey[0], sizeof chkey);
    memcpy(&chiv[0], &chnewiv[0], sizeof chiv);

    fkeyset = true;
    return true;
}

bool ccrypter::encrypt(const ckeyingmaterial& vchplaintext, std::vector<unsigned char> &vchciphertext)
{
    if (!fkeyset)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + aes_block_size - 1 bytes
    int nlen = vchplaintext.size();
    int nclen = nlen + aes_block_size, nflen = 0;
    vchciphertext = std::vector<unsigned char> (nclen);

    evp_cipher_ctx ctx;

    bool fok = true;

    evp_cipher_ctx_init(&ctx);
    if (fok) fok = evp_encryptinit_ex(&ctx, evp_aes_256_cbc(), null, chkey, chiv) != 0;
    if (fok) fok = evp_encryptupdate(&ctx, &vchciphertext[0], &nclen, &vchplaintext[0], nlen) != 0;
    if (fok) fok = evp_encryptfinal_ex(&ctx, (&vchciphertext[0]) + nclen, &nflen) != 0;
    evp_cipher_ctx_cleanup(&ctx);

    if (!fok) return false;

    vchciphertext.resize(nclen + nflen);
    return true;
}

bool ccrypter::decrypt(const std::vector<unsigned char>& vchciphertext, ckeyingmaterial& vchplaintext)
{
    if (!fkeyset)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nlen = vchciphertext.size();
    int nplen = nlen, nflen = 0;

    vchplaintext = ckeyingmaterial(nplen);

    evp_cipher_ctx ctx;

    bool fok = true;

    evp_cipher_ctx_init(&ctx);
    if (fok) fok = evp_decryptinit_ex(&ctx, evp_aes_256_cbc(), null, chkey, chiv) != 0;
    if (fok) fok = evp_decryptupdate(&ctx, &vchplaintext[0], &nplen, &vchciphertext[0], nlen) != 0;
    if (fok) fok = evp_decryptfinal_ex(&ctx, (&vchplaintext[0]) + nplen, &nflen) != 0;
    evp_cipher_ctx_cleanup(&ctx);

    if (!fok) return false;

    vchplaintext.resize(nplen + nflen);
    return true;
}


static bool encryptsecret(const ckeyingmaterial& vmasterkey, const ckeyingmaterial &vchplaintext, const uint256& niv, std::vector<unsigned char> &vchciphertext)
{
    ccrypter ckeycrypter;
    std::vector<unsigned char> chiv(wallet_crypto_key_size);
    memcpy(&chiv[0], &niv, wallet_crypto_key_size);
    if(!ckeycrypter.setkey(vmasterkey, chiv))
        return false;
    return ckeycrypter.encrypt(*((const ckeyingmaterial*)&vchplaintext), vchciphertext);
}

static bool decryptsecret(const ckeyingmaterial& vmasterkey, const std::vector<unsigned char>& vchciphertext, const uint256& niv, ckeyingmaterial& vchplaintext)
{
    ccrypter ckeycrypter;
    std::vector<unsigned char> chiv(wallet_crypto_key_size);
    memcpy(&chiv[0], &niv, wallet_crypto_key_size);
    if(!ckeycrypter.setkey(vmasterkey, chiv))
        return false;
    return ckeycrypter.decrypt(vchciphertext, *((ckeyingmaterial*)&vchplaintext));
}

static bool decryptkey(const ckeyingmaterial& vmasterkey, const std::vector<unsigned char>& vchcryptedsecret, const cpubkey& vchpubkey, ckey& key)
{
    ckeyingmaterial vchsecret;
    if(!decryptsecret(vmasterkey, vchcryptedsecret, vchpubkey.gethash(), vchsecret))
        return false;

    if (vchsecret.size() != 32)
        return false;

    key.set(vchsecret.begin(), vchsecret.end(), vchpubkey.iscompressed());
    return key.verifypubkey(vchpubkey);
}

bool ccryptokeystore::setcrypted()
{
    lock(cs_keystore);
    if (fusecrypto)
        return true;
    if (!mapkeys.empty())
        return false;
    fusecrypto = true;
    return true;
}

bool ccryptokeystore::lock()
{
    if (!setcrypted())
        return false;

    {
        lock(cs_keystore);
        vmasterkey.clear();
    }

    notifystatuschanged(this);
    return true;
}

bool ccryptokeystore::unlock(const ckeyingmaterial& vmasterkeyin)
{
    {
        lock(cs_keystore);
        if (!setcrypted())
            return false;

        bool keypass = false;
        bool keyfail = false;
        cryptedkeymap::const_iterator mi = mapcryptedkeys.begin();
        for (; mi != mapcryptedkeys.end(); ++mi)
        {
            const cpubkey &vchpubkey = (*mi).second.first;
            const std::vector<unsigned char> &vchcryptedsecret = (*mi).second.second;
            ckey key;
            if (!decryptkey(vmasterkeyin, vchcryptedsecret, vchpubkey, key))
            {
                keyfail = true;
                break;
            }
            keypass = true;
            if (fdecryptionthoroughlychecked)
                break;
        }
        if (keypass && keyfail)
        {
            logprintf("the wallet is probably corrupted: some keys decrypt but not all.");
            assert(false);
        }
        if (keyfail || !keypass)
            return false;
        vmasterkey = vmasterkeyin;
        fdecryptionthoroughlychecked = true;
    }
    notifystatuschanged(this);
    return true;
}

bool ccryptokeystore::addkeypubkey(const ckey& key, const cpubkey &pubkey)
{
    {
        lock(cs_keystore);
        if (!iscrypted())
            return cbasickeystore::addkeypubkey(key, pubkey);

        if (islocked())
            return false;

        std::vector<unsigned char> vchcryptedsecret;
        ckeyingmaterial vchsecret(key.begin(), key.end());
        if (!encryptsecret(vmasterkey, vchsecret, pubkey.gethash(), vchcryptedsecret))
            return false;

        if (!addcryptedkey(pubkey, vchcryptedsecret))
            return false;
    }
    return true;
}


bool ccryptokeystore::addcryptedkey(const cpubkey &vchpubkey, const std::vector<unsigned char> &vchcryptedsecret)
{
    {
        lock(cs_keystore);
        if (!setcrypted())
            return false;

        mapcryptedkeys[vchpubkey.getid()] = make_pair(vchpubkey, vchcryptedsecret);
    }
    return true;
}

bool ccryptokeystore::getkey(const ckeyid &address, ckey& keyout) const
{
    {
        lock(cs_keystore);
        if (!iscrypted())
            return cbasickeystore::getkey(address, keyout);

        cryptedkeymap::const_iterator mi = mapcryptedkeys.find(address);
        if (mi != mapcryptedkeys.end())
        {
            const cpubkey &vchpubkey = (*mi).second.first;
            const std::vector<unsigned char> &vchcryptedsecret = (*mi).second.second;
            return decryptkey(vmasterkey, vchcryptedsecret, vchpubkey, keyout);
        }
    }
    return false;
}

bool ccryptokeystore::getpubkey(const ckeyid &address, cpubkey& vchpubkeyout) const
{
    {
        lock(cs_keystore);
        if (!iscrypted())
            return ckeystore::getpubkey(address, vchpubkeyout);

        cryptedkeymap::const_iterator mi = mapcryptedkeys.find(address);
        if (mi != mapcryptedkeys.end())
        {
            vchpubkeyout = (*mi).second.first;
            return true;
        }
    }
    return false;
}

bool ccryptokeystore::encryptkeys(ckeyingmaterial& vmasterkeyin)
{
    {
        lock(cs_keystore);
        if (!mapcryptedkeys.empty() || iscrypted())
            return false;

        fusecrypto = true;
        boost_foreach(keymap::value_type& mkey, mapkeys)
        {
            const ckey &key = mkey.second;
            cpubkey vchpubkey = key.getpubkey();
            ckeyingmaterial vchsecret(key.begin(), key.end());
            std::vector<unsigned char> vchcryptedsecret;
            if (!encryptsecret(vmasterkeyin, vchsecret, vchpubkey.gethash(), vchcryptedsecret))
                return false;
            if (!addcryptedkey(vchpubkey, vchcryptedsecret))
                return false;
        }
        mapkeys.clear();
    }
    return true;
}
