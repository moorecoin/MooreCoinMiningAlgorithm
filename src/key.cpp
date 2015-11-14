// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "arith_uint256.h"
#include "crypto/common.h"
#include "crypto/hmac_sha512.h"
#include "eccryptoverify.h"
#include "pubkey.h"
#include "random.h"

#include <secp256k1.h>
#include "ecwrapper.h"

static secp256k1_context_t* secp256k1_context = null;

bool ckey::check(const unsigned char *vch) {
    return eccrypto::check(vch);
}

void ckey::makenewkey(bool fcompressedin) {
    randaddseedperfmon();
    do {
        getrandbytes(vch, sizeof(vch));
    } while (!check(vch));
    fvalid = true;
    fcompressed = fcompressedin;
}

bool ckey::setprivkey(const cprivkey &privkey, bool fcompressedin) {
    if (!secp256k1_ec_privkey_import(secp256k1_context, (unsigned char*)begin(), &privkey[0], privkey.size()))
        return false;
    fcompressed = fcompressedin;
    fvalid = true;
    return true;
}

cprivkey ckey::getprivkey() const {
    assert(fvalid);
    cprivkey privkey;
    int privkeylen, ret;
    privkey.resize(279);
    privkeylen = 279;
    ret = secp256k1_ec_privkey_export(secp256k1_context, begin(), (unsigned char*)&privkey[0], &privkeylen, fcompressed);
    assert(ret);
    privkey.resize(privkeylen);
    return privkey;
}

cpubkey ckey::getpubkey() const {
    assert(fvalid);
    cpubkey result;
    int clen = 65;
    int ret = secp256k1_ec_pubkey_create(secp256k1_context, (unsigned char*)result.begin(), &clen, begin(), fcompressed);
    assert((int)result.size() == clen);
    assert(ret);
    assert(result.isvalid());
    return result;
}

bool ckey::sign(const uint256 &hash, std::vector<unsigned char>& vchsig, uint32_t test_case) const {
    if (!fvalid)
        return false;
    vchsig.resize(72);
    int nsiglen = 72;
    unsigned char extra_entropy[32] = {0};
    writele32(extra_entropy, test_case);
    int ret = secp256k1_ecdsa_sign(secp256k1_context, hash.begin(), (unsigned char*)&vchsig[0], &nsiglen, begin(), secp256k1_nonce_function_rfc6979, test_case ? extra_entropy : null);
    assert(ret);
    vchsig.resize(nsiglen);
    return true;
}

bool ckey::verifypubkey(const cpubkey& pubkey) const {
    if (pubkey.iscompressed() != fcompressed) {
        return false;
    }
    unsigned char rnd[8];
    std::string str = "moorecoin key verification\n";
    getrandbytes(rnd, sizeof(rnd));
    uint256 hash;
    chash256().write((unsigned char*)str.data(), str.size()).write(rnd, sizeof(rnd)).finalize(hash.begin());
    std::vector<unsigned char> vchsig;
    sign(hash, vchsig);
    return pubkey.verify(hash, vchsig);
}

bool ckey::signcompact(const uint256 &hash, std::vector<unsigned char>& vchsig) const {
    if (!fvalid)
        return false;
    vchsig.resize(65);
    int rec = -1;
    int ret = secp256k1_ecdsa_sign_compact(secp256k1_context, hash.begin(), &vchsig[1], begin(), secp256k1_nonce_function_rfc6979, null, &rec);
    assert(ret);
    assert(rec != -1);
    vchsig[0] = 27 + rec + (fcompressed ? 4 : 0);
    return true;
}

bool ckey::load(cprivkey &privkey, cpubkey &vchpubkey, bool fskipcheck=false) {
    if (!secp256k1_ec_privkey_import(secp256k1_context, (unsigned char*)begin(), &privkey[0], privkey.size()))
        return false;
    fcompressed = vchpubkey.iscompressed();
    fvalid = true;

    if (fskipcheck)
        return true;

    return verifypubkey(vchpubkey);
}

bool ckey::derive(ckey& keychild, chaincode &ccchild, unsigned int nchild, const chaincode& cc) const {
    assert(isvalid());
    assert(iscompressed());
    unsigned char out[64];
    lockobject(out);
    if ((nchild >> 31) == 0) {
        cpubkey pubkey = getpubkey();
        assert(pubkey.begin() + 33 == pubkey.end());
        bip32hash(cc, nchild, *pubkey.begin(), pubkey.begin()+1, out);
    } else {
        assert(begin() + 32 == end());
        bip32hash(cc, nchild, 0, begin(), out);
    }
    memcpy(ccchild.begin(), out+32, 32);
    memcpy((unsigned char*)keychild.begin(), begin(), 32);
    bool ret = secp256k1_ec_privkey_tweak_add(secp256k1_context, (unsigned char*)keychild.begin(), out);
    unlockobject(out);
    keychild.fcompressed = true;
    keychild.fvalid = ret;
    return ret;
}

bool cextkey::derive(cextkey &out, unsigned int nchild) const {
    out.ndepth = ndepth + 1;
    ckeyid id = key.getpubkey().getid();
    memcpy(&out.vchfingerprint[0], &id, 4);
    out.nchild = nchild;
    return key.derive(out.key, out.chaincode, nchild, chaincode);
}

void cextkey::setmaster(const unsigned char *seed, unsigned int nseedlen) {
    static const unsigned char hashkey[] = {'b','i','t','c','o','i','n',' ','s','e','e','d'};
    unsigned char out[64];
    lockobject(out);
    chmac_sha512(hashkey, sizeof(hashkey)).write(seed, nseedlen).finalize(out);
    key.set(&out[0], &out[32], true);
    memcpy(chaincode.begin(), &out[32], 32);
    unlockobject(out);
    ndepth = 0;
    nchild = 0;
    memset(vchfingerprint, 0, sizeof(vchfingerprint));
}

cextpubkey cextkey::neuter() const {
    cextpubkey ret;
    ret.ndepth = ndepth;
    memcpy(&ret.vchfingerprint[0], &vchfingerprint[0], 4);
    ret.nchild = nchild;
    ret.pubkey = key.getpubkey();
    ret.chaincode = chaincode;
    return ret;
}

void cextkey::encode(unsigned char code[74]) const {
    code[0] = ndepth;
    memcpy(code+1, vchfingerprint, 4);
    code[5] = (nchild >> 24) & 0xff; code[6] = (nchild >> 16) & 0xff;
    code[7] = (nchild >>  8) & 0xff; code[8] = (nchild >>  0) & 0xff;
    memcpy(code+9, chaincode.begin(), 32);
    code[41] = 0;
    assert(key.size() == 32);
    memcpy(code+42, key.begin(), 32);
}

void cextkey::decode(const unsigned char code[74]) {
    ndepth = code[0];
    memcpy(vchfingerprint, code+1, 4);
    nchild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(chaincode.begin(), code+9, 32);
    key.set(code+42, code+74, true);
}

bool ecc_initsanitycheck() {
    if (!ceckey::sanitycheck()) {
        return false;
    }
    ckey key;
    key.makenewkey(true);
    cpubkey pubkey = key.getpubkey();
    return key.verifypubkey(pubkey);
}


void ecc_start() {
    assert(secp256k1_context == null);

    secp256k1_context_t *ctx = secp256k1_context_create(secp256k1_context_sign);
    assert(ctx != null);

    {
        // pass in a random blinding seed to the secp256k1 context.
        unsigned char seed[32];
        lockobject(seed);
        getrandbytes(seed, 32);
        bool ret = secp256k1_context_randomize(ctx, seed);
        assert(ret);
        unlockobject(seed);
    }

    secp256k1_context = ctx;
}

void ecc_stop() {
    secp256k1_context_t *ctx = secp256k1_context;
    secp256k1_context = null;

    if (ctx) {
        secp256k1_context_destroy(ctx);
    }
}
