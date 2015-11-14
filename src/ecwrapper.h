// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_ecwrapper_h
#define moorecoin_ecwrapper_h

#include <cstddef>
#include <vector>

#include <openssl/ec.h>

class uint256;

/** raii wrapper around openssl's ec_key */
class ceckey {
private:
    ec_key *pkey;

public:
    ceckey();
    ~ceckey();

    void getpubkey(std::vector<unsigned char>& pubkey, bool fcompressed);
    bool setpubkey(const unsigned char* pubkey, size_t size);
    bool verify(const uint256 &hash, const std::vector<unsigned char>& vchsig);

    /**
     * reconstruct public key from a compact signature
     * this is only slightly more cpu intensive than just verifying it.
     * if this function succeeds, the recovered public key is guaranteed to be valid
     * (the signature is a valid signature of the given data for that key)
     */
    bool recover(const uint256 &hash, const unsigned char *p64, int rec);

    bool tweakpublic(const unsigned char vchtweak[32]);
    static bool sanitycheck();
};

#endif // moorecoin_ecwrapper_h
