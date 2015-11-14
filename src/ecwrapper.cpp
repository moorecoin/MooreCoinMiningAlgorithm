// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "ecwrapper.h"

#include "serialize.h"
#include "uint256.h"

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

namespace {

/**
 * perform ecdsa key recovery (see sec1 4.1.6) for curves over (mod p)-fields
 * recid selects which key is recovered
 * if check is non-zero, additional checks are performed
 */
int ecdsa_sig_recover_key_gfp(ec_key *eckey, ecdsa_sig *ecsig, const unsigned char *msg, int msglen, int recid, int check)
{
    if (!eckey) return 0;

    int ret = 0;
    bn_ctx *ctx = null;

    bignum *x = null;
    bignum *e = null;
    bignum *order = null;
    bignum *sor = null;
    bignum *eor = null;
    bignum *field = null;
    ec_point *r = null;
    ec_point *o = null;
    ec_point *q = null;
    bignum *rr = null;
    bignum *zero = null;
    int n = 0;
    int i = recid / 2;

    const ec_group *group = ec_key_get0_group(eckey);
    if ((ctx = bn_ctx_new()) == null) { ret = -1; goto err; }
    bn_ctx_start(ctx);
    order = bn_ctx_get(ctx);
    if (!ec_group_get_order(group, order, ctx)) { ret = -2; goto err; }
    x = bn_ctx_get(ctx);
    if (!bn_copy(x, order)) { ret=-1; goto err; }
    if (!bn_mul_word(x, i)) { ret=-1; goto err; }
    if (!bn_add(x, x, ecsig->r)) { ret=-1; goto err; }
    field = bn_ctx_get(ctx);
    if (!ec_group_get_curve_gfp(group, field, null, null, ctx)) { ret=-2; goto err; }
    if (bn_cmp(x, field) >= 0) { ret=0; goto err; }
    if ((r = ec_point_new(group)) == null) { ret = -2; goto err; }
    if (!ec_point_set_compressed_coordinates_gfp(group, r, x, recid % 2, ctx)) { ret=0; goto err; }
    if (check)
    {
        if ((o = ec_point_new(group)) == null) { ret = -2; goto err; }
        if (!ec_point_mul(group, o, null, r, order, ctx)) { ret=-2; goto err; }
        if (!ec_point_is_at_infinity(group, o)) { ret = 0; goto err; }
    }
    if ((q = ec_point_new(group)) == null) { ret = -2; goto err; }
    n = ec_group_get_degree(group);
    e = bn_ctx_get(ctx);
    if (!bn_bin2bn(msg, msglen, e)) { ret=-1; goto err; }
    if (8*msglen > n) bn_rshift(e, e, 8-(n & 7));
    zero = bn_ctx_get(ctx);
    if (!bn_zero(zero)) { ret=-1; goto err; }
    if (!bn_mod_sub(e, zero, e, order, ctx)) { ret=-1; goto err; }
    rr = bn_ctx_get(ctx);
    if (!bn_mod_inverse(rr, ecsig->r, order, ctx)) { ret=-1; goto err; }
    sor = bn_ctx_get(ctx);
    if (!bn_mod_mul(sor, ecsig->s, rr, order, ctx)) { ret=-1; goto err; }
    eor = bn_ctx_get(ctx);
    if (!bn_mod_mul(eor, e, rr, order, ctx)) { ret=-1; goto err; }
    if (!ec_point_mul(group, q, eor, r, sor, ctx)) { ret=-2; goto err; }
    if (!ec_key_set_public_key(eckey, q)) { ret=-2; goto err; }

    ret = 1;

err:
    if (ctx) {
        bn_ctx_end(ctx);
        bn_ctx_free(ctx);
    }
    if (r != null) ec_point_free(r);
    if (o != null) ec_point_free(o);
    if (q != null) ec_point_free(q);
    return ret;
}

} // anon namespace

ceckey::ceckey() {
    pkey = ec_key_new_by_curve_name(nid_secp256k1);
    assert(pkey != null);
}

ceckey::~ceckey() {
    ec_key_free(pkey);
}

void ceckey::getpubkey(std::vector<unsigned char> &pubkey, bool fcompressed) {
    ec_key_set_conv_form(pkey, fcompressed ? point_conversion_compressed : point_conversion_uncompressed);
    int nsize = i2o_ecpublickey(pkey, null);
    assert(nsize);
    assert(nsize <= 65);
    pubkey.clear();
    pubkey.resize(nsize);
    unsigned char *pbegin(begin_ptr(pubkey));
    int nsize2 = i2o_ecpublickey(pkey, &pbegin);
    assert(nsize == nsize2);
}

bool ceckey::setpubkey(const unsigned char* pubkey, size_t size) {
    return o2i_ecpublickey(&pkey, &pubkey, size) != null;
}

bool ceckey::verify(const uint256 &hash, const std::vector<unsigned char>& vchsig) {
    if (vchsig.empty())
        return false;

    // new versions of openssl will reject non-canonical der signatures. de/re-serialize first.
    unsigned char *norm_der = null;
    ecdsa_sig *norm_sig = ecdsa_sig_new();
    const unsigned char* sigptr = &vchsig[0];
    assert(norm_sig);
    if (d2i_ecdsa_sig(&norm_sig, &sigptr, vchsig.size()) == null)
    {
        /* as of openssl 1.0.0p d2i_ecdsa_sig frees and nulls the pointer on
         * error. but openssl's own use of this function redundantly frees the
         * result. as ecdsa_sig_free(null) is a no-op, and in the absence of a
         * clear contract for the function behaving the same way is more
         * conservative.
         */
        ecdsa_sig_free(norm_sig);
        return false;
    }
    int derlen = i2d_ecdsa_sig(norm_sig, &norm_der);
    ecdsa_sig_free(norm_sig);
    if (derlen <= 0)
        return false;

    // -1 = error, 0 = bad sig, 1 = good
    bool ret = ecdsa_verify(0, (unsigned char*)&hash, sizeof(hash), norm_der, derlen, pkey) == 1;
    openssl_free(norm_der);
    return ret;
}

bool ceckey::recover(const uint256 &hash, const unsigned char *p64, int rec)
{
    if (rec<0 || rec>=3)
        return false;
    ecdsa_sig *sig = ecdsa_sig_new();
    bn_bin2bn(&p64[0],  32, sig->r);
    bn_bin2bn(&p64[32], 32, sig->s);
    bool ret = ecdsa_sig_recover_key_gfp(pkey, sig, (unsigned char*)&hash, sizeof(hash), rec, 0) == 1;
    ecdsa_sig_free(sig);
    return ret;
}

bool ceckey::tweakpublic(const unsigned char vchtweak[32]) {
    bool ret = true;
    bn_ctx *ctx = bn_ctx_new();
    bn_ctx_start(ctx);
    bignum *bntweak = bn_ctx_get(ctx);
    bignum *bnorder = bn_ctx_get(ctx);
    bignum *bnone = bn_ctx_get(ctx);
    const ec_group *group = ec_key_get0_group(pkey);
    ec_group_get_order(group, bnorder, ctx); // what a grossly inefficient way to get the (constant) group order...
    bn_bin2bn(vchtweak, 32, bntweak);
    if (bn_cmp(bntweak, bnorder) >= 0)
        ret = false; // extremely unlikely
    ec_point *point = ec_point_dup(ec_key_get0_public_key(pkey), group);
    bn_one(bnone);
    ec_point_mul(group, point, bntweak, point, bnone, ctx);
    if (ec_point_is_at_infinity(group, point))
        ret = false; // ridiculously unlikely
    ec_key_set_public_key(pkey, point);
    ec_point_free(point);
    bn_ctx_end(ctx);
    bn_ctx_free(ctx);
    return ret;
}

bool ceckey::sanitycheck()
{
    ec_key *pkey = ec_key_new_by_curve_name(nid_secp256k1);
    if(pkey == null)
        return false;
    ec_key_free(pkey);

    // todo is there more ec functionality that could be missing?
    return true;
}
