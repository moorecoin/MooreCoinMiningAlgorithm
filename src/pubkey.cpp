// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "pubkey.h"

#include "eccryptoverify.h"

#include "ecwrapper.h"

bool cpubkey::verify(const uint256 &hash, const std::vector<unsigned char>& vchsig) const {
    if (!isvalid())
        return false;
    ceckey key;
    if (!key.setpubkey(begin(), size()))
        return false;
    if (!key.verify(hash, vchsig))
        return false;
    return true;
}

bool cpubkey::recovercompact(const uint256 &hash, const std::vector<unsigned char>& vchsig) {
    if (vchsig.size() != 65)
        return false;
    int recid = (vchsig[0] - 27) & 3;
    bool fcomp = ((vchsig[0] - 27) & 4) != 0;
    ceckey key;
    if (!key.recover(hash, &vchsig[1], recid))
        return false;
    std::vector<unsigned char> pubkey;
    key.getpubkey(pubkey, fcomp);
    set(pubkey.begin(), pubkey.end());
    return true;
}

bool cpubkey::isfullyvalid() const {
    if (!isvalid())
        return false;
    ceckey key;
    if (!key.setpubkey(begin(), size()))
        return false;
    return true;
}

bool cpubkey::decompress() {
    if (!isvalid())
        return false;
    ceckey key;
    if (!key.setpubkey(begin(), size()))
        return false;
    std::vector<unsigned char> pubkey;
    key.getpubkey(pubkey, false);
    set(pubkey.begin(), pubkey.end());
    return true;
}

bool cpubkey::derive(cpubkey& pubkeychild, chaincode &ccchild, unsigned int nchild, const chaincode& cc) const {
    assert(isvalid());
    assert((nchild >> 31) == 0);
    assert(begin() + 33 == end());
    unsigned char out[64];
    bip32hash(cc, nchild, *begin(), begin()+1, out);
    memcpy(ccchild.begin(), out+32, 32);
    ceckey key;
    bool ret = key.setpubkey(begin(), size());
    ret &= key.tweakpublic(out);
    std::vector<unsigned char> pubkey;
    key.getpubkey(pubkey, true);
    pubkeychild.set(pubkey.begin(), pubkey.end());
    return ret;
}

void cextpubkey::encode(unsigned char code[74]) const {
    code[0] = ndepth;
    memcpy(code+1, vchfingerprint, 4);
    code[5] = (nchild >> 24) & 0xff; code[6] = (nchild >> 16) & 0xff;
    code[7] = (nchild >>  8) & 0xff; code[8] = (nchild >>  0) & 0xff;
    memcpy(code+9, chaincode.begin(), 32);
    assert(pubkey.size() == 33);
    memcpy(code+41, pubkey.begin(), 33);
}

void cextpubkey::decode(const unsigned char code[74]) {
    ndepth = code[0];
    memcpy(vchfingerprint, code+1, 4);
    nchild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(chaincode.begin(), code+9, 32);
    pubkey.set(code+41, code+74);
}

bool cextpubkey::derive(cextpubkey &out, unsigned int nchild) const {
    out.ndepth = ndepth + 1;
    ckeyid id = pubkey.getid();
    memcpy(&out.vchfingerprint[0], &id, 4);
    out.nchild = nchild;
    return pubkey.derive(out.pubkey, out.chaincode, nchild, chaincode);
}
