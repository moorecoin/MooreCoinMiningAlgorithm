// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

/**
 * why base-58 instead of standard base-64 encoding?
 * - don't want 0oil characters that look the same in some fonts and
 *      could be used to create visually identical looking data.
 * - a string with non-alphanumeric characters is not as easily accepted as input.
 * - e-mail usually won't line-break if there's no punctuation to break at.
 * - double-clicking selects the whole string as one word if it's all alphanumeric.
 */
#ifndef moorecoin_base58_h
#define moorecoin_base58_h

#include "chainparams.h"
#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "support/allocators/zeroafterfree.h"

#include <string>
#include <vector>

/**
 * encode a byte sequence as a base58-encoded string.
 * pbegin and pend cannot be null, unless both are.
 */
std::string encodebase58(const unsigned char* pbegin, const unsigned char* pend);

/**
 * encode a byte vector as a base58-encoded string
 */
std::string encodebase58(const std::vector<unsigned char>& vch);

/**
 * decode a base58-encoded string (psz) into a byte vector (vchret).
 * return true if decoding is successful.
 * psz cannot be null.
 */
bool decodebase58(const char* psz, std::vector<unsigned char>& vchret);

/**
 * decode a base58-encoded string (str) into a byte vector (vchret).
 * return true if decoding is successful.
 */
bool decodebase58(const std::string& str, std::vector<unsigned char>& vchret);

/**
 * encode a byte vector into a base58-encoded string, including checksum
 */
std::string encodebase58check(const std::vector<unsigned char>& vchin);

/**
 * decode a base58-encoded string (psz) that includes a checksum into a byte
 * vector (vchret), return true if decoding is successful
 */
inline bool decodebase58check(const char* psz, std::vector<unsigned char>& vchret);

/**
 * decode a base58-encoded string (str) that includes a checksum into a byte
 * vector (vchret), return true if decoding is successful
 */
inline bool decodebase58check(const std::string& str, std::vector<unsigned char>& vchret);

/**
 * base class for all base58-encoded data
 */
class cbase58data
{
protected:
    //! the version byte(s)
    std::vector<unsigned char> vchversion;

    //! the actually encoded data
    typedef std::vector<unsigned char, zero_after_free_allocator<unsigned char> > vector_uchar;
    vector_uchar vchdata;

    cbase58data();
    void setdata(const std::vector<unsigned char> &vchversionin, const void* pdata, size_t nsize);
    void setdata(const std::vector<unsigned char> &vchversionin, const unsigned char *pbegin, const unsigned char *pend);

public:
    bool setstring(const char* psz, unsigned int nversionbytes = 1);
    bool setstring(const std::string& str);
    std::string tostring() const;
    int compareto(const cbase58data& b58) const;

    bool operator==(const cbase58data& b58) const { return compareto(b58) == 0; }
    bool operator<=(const cbase58data& b58) const { return compareto(b58) <= 0; }
    bool operator>=(const cbase58data& b58) const { return compareto(b58) >= 0; }
    bool operator< (const cbase58data& b58) const { return compareto(b58) <  0; }
    bool operator> (const cbase58data& b58) const { return compareto(b58) >  0; }
};

/** base58-encoded moorecoin addresses.
 * public-key-hash-addresses have version 0 (or 111 testnet).
 * the data vector contains ripemd160(sha256(pubkey)), where pubkey is the serialized public key.
 * script-hash-addresses have version 5 (or 196 testnet).
 * the data vector contains ripemd160(sha256(cscript)), where cscript is the serialized redemption script.
 */
class cmoorecoinaddress : public cbase58data {
public:
    bool set(const ckeyid &id);
    bool set(const cscriptid &id);
    bool set(const ctxdestination &dest);
    bool isvalid() const;
    bool isvalid(const cchainparams &params) const;

    cmoorecoinaddress() {}
    cmoorecoinaddress(const ctxdestination &dest) { set(dest); }
    cmoorecoinaddress(const std::string& straddress) { setstring(straddress); }
    cmoorecoinaddress(const char* pszaddress) { setstring(pszaddress); }

    ctxdestination get() const;
    bool getkeyid(ckeyid &keyid) const;
    bool isscript() const;
};

/**
 * a base58-encoded secret key
 */
class cmoorecoinsecret : public cbase58data
{
public:
    void setkey(const ckey& vchsecret);
    ckey getkey();
    bool isvalid() const;
    bool setstring(const char* pszsecret);
    bool setstring(const std::string& strsecret);

    cmoorecoinsecret(const ckey& vchsecret) { setkey(vchsecret); }
    cmoorecoinsecret() {}
};

template<typename k, int size, cchainparams::base58type type> class cmoorecoinextkeybase : public cbase58data
{
public:
    void setkey(const k &key) {
        unsigned char vch[size];
        key.encode(vch);
        setdata(params().base58prefix(type), vch, vch+size);
    }

    k getkey() {
        k ret;
        ret.decode(&vchdata[0], &vchdata[size]);
        return ret;
    }

    cmoorecoinextkeybase(const k &key) {
        setkey(key);
    }

    cmoorecoinextkeybase() {}
};

typedef cmoorecoinextkeybase<cextkey, 74, cchainparams::ext_secret_key> cmoorecoinextkey;
typedef cmoorecoinextkeybase<cextpubkey, 74, cchainparams::ext_public_key> cmoorecoinextpubkey;

#endif // moorecoin_base58_h
