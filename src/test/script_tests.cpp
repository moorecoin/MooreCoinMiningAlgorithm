// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "data/script_invalid.json.h"
#include "data/script_valid.json.h"

#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "util.h"
#include "test/test_moorecoin.h"

#if defined(have_consensus_lib)
#include "script/moorecoinconsensus.h"
#endif

#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include "univalue/univalue.h"

using namespace std;

// uncomment if you want to output updated json tests.
// #define update_json_tests

static const unsigned int flags = script_verify_p2sh | script_verify_strictenc;

unsigned int parsescriptflags(string strflags);
string formatscriptflags(unsigned int flags);

univalue
read_json(const std::string& jsondata)
{
    univalue v;

    if (!v.read(jsondata) || !v.isarray())
    {
        boost_error("parse error.");
        return univalue(univalue::varr);
    }
    return v.get_array();
}

boost_fixture_test_suite(script_tests, basictestingsetup)

cmutabletransaction buildcreditingtransaction(const cscript& scriptpubkey)
{
    cmutabletransaction txcredit;
    txcredit.nversion = 1;
    txcredit.nlocktime = 0;
    txcredit.vin.resize(1);
    txcredit.vout.resize(1);
    txcredit.vin[0].prevout.setnull();
    txcredit.vin[0].scriptsig = cscript() << cscriptnum(0) << cscriptnum(0);
    txcredit.vin[0].nsequence = std::numeric_limits<unsigned int>::max();
    txcredit.vout[0].scriptpubkey = scriptpubkey;
    txcredit.vout[0].nvalue = 0;

    return txcredit;
}

cmutabletransaction buildspendingtransaction(const cscript& scriptsig, const cmutabletransaction& txcredit)
{
    cmutabletransaction txspend;
    txspend.nversion = 1;
    txspend.nlocktime = 0;
    txspend.vin.resize(1);
    txspend.vout.resize(1);
    txspend.vin[0].prevout.hash = txcredit.gethash();
    txspend.vin[0].prevout.n = 0;
    txspend.vin[0].scriptsig = scriptsig;
    txspend.vin[0].nsequence = std::numeric_limits<unsigned int>::max();
    txspend.vout[0].scriptpubkey = cscript();
    txspend.vout[0].nvalue = 0;

    return txspend;
}

void dotest(const cscript& scriptpubkey, const cscript& scriptsig, int flags, bool expect, const std::string& message)
{
    scripterror err;
    cmutabletransaction tx = buildspendingtransaction(scriptsig, buildcreditingtransaction(scriptpubkey));
    cmutabletransaction tx2 = tx;
    boost_check_message(verifyscript(scriptsig, scriptpubkey, flags, mutabletransactionsignaturechecker(&tx, 0), &err) == expect, message);
    boost_check_message(expect == (err == script_err_ok), std::string(scripterrorstring(err)) + ": " + message);
#if defined(have_consensus_lib)
    cdatastream stream(ser_network, protocol_version);
    stream << tx2;
    boost_check_message(moorecoinconsensus_verify_script(begin_ptr(scriptpubkey), scriptpubkey.size(), (const unsigned char*)&stream[0], stream.size(), 0, flags, null) == expect,message);
#endif
}

void static negatesignatures(std::vector<unsigned char>& vchsig) {
    // parse the signature.
    std::vector<unsigned char> r, s;
    r = std::vector<unsigned char>(vchsig.begin() + 4, vchsig.begin() + 4 + vchsig[3]);
    s = std::vector<unsigned char>(vchsig.begin() + 6 + vchsig[3], vchsig.begin() + 6 + vchsig[3] + vchsig[5 + vchsig[3]]);

    // really ugly to implement mod-n negation here, but it would be feature creep to expose such functionality from libsecp256k1.
    static const unsigned char order[33] = {
        0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        0xba, 0xae, 0xdc, 0xe6, 0xaf, 0x48, 0xa0, 0x3b,
        0xbf, 0xd2, 0x5e, 0x8c, 0xd0, 0x36, 0x41, 0x41
    };
    while (s.size() < 33) {
        s.insert(s.begin(), 0x00);
    }
    int carry = 0;
    for (int p = 32; p >= 1; p--) {
        int n = (int)order[p] - s[p] - carry;
        s[p] = (n + 256) & 0xff;
        carry = (n < 0);
    }
    assert(carry == 0);
    if (s.size() > 1 && s[0] == 0 && s[1] < 0x80) {
        s.erase(s.begin());
    }

    // reconstruct the signature.
    vchsig.clear();
    vchsig.push_back(0x30);
    vchsig.push_back(4 + r.size() + s.size());
    vchsig.push_back(0x02);
    vchsig.push_back(r.size());
    vchsig.insert(vchsig.end(), r.begin(), r.end());
    vchsig.push_back(0x02);
    vchsig.push_back(s.size());
    vchsig.insert(vchsig.end(), s.begin(), s.end());
}

namespace
{
const unsigned char vchkey0[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
const unsigned char vchkey1[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0};
const unsigned char vchkey2[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0};

struct keydata
{
    ckey key0, key0c, key1, key1c, key2, key2c;
    cpubkey pubkey0, pubkey0c, pubkey0h;
    cpubkey pubkey1, pubkey1c;
    cpubkey pubkey2, pubkey2c;

    keydata()
    {

        key0.set(vchkey0, vchkey0 + 32, false);
        key0c.set(vchkey0, vchkey0 + 32, true);
        pubkey0 = key0.getpubkey();
        pubkey0h = key0.getpubkey();
        pubkey0c = key0c.getpubkey();
        *const_cast<unsigned char*>(&pubkey0h[0]) = 0x06 | (pubkey0h[64] & 1);

        key1.set(vchkey1, vchkey1 + 32, false);
        key1c.set(vchkey1, vchkey1 + 32, true);
        pubkey1 = key1.getpubkey();
        pubkey1c = key1c.getpubkey();

        key2.set(vchkey2, vchkey2 + 32, false);
        key2c.set(vchkey2, vchkey2 + 32, true);
        pubkey2 = key2.getpubkey();
        pubkey2c = key2c.getpubkey();
    }
};


class testbuilder
{
private:
    cscript scriptpubkey;
    ctransaction credittx;
    cmutabletransaction spendtx;
    bool havepush;
    std::vector<unsigned char> push;
    std::string comment;
    int flags;

    void dopush()
    {
        if (havepush) {
            spendtx.vin[0].scriptsig << push;
            havepush = false;
        }
    }

    void dopush(const std::vector<unsigned char>& data)
    {
         dopush();
         push = data;
         havepush = true;
    }

public:
    testbuilder(const cscript& redeemscript, const std::string& comment_, int flags_, bool p2sh = false) : scriptpubkey(redeemscript), havepush(false), comment(comment_), flags(flags_)
    {
        if (p2sh) {
            credittx = buildcreditingtransaction(cscript() << op_hash160 << tobytevector(cscriptid(redeemscript)) << op_equal);
        } else {
            credittx = buildcreditingtransaction(redeemscript);
        }
        spendtx = buildspendingtransaction(cscript(), credittx);
    }

    testbuilder& add(const cscript& script)
    {
        dopush();
        spendtx.vin[0].scriptsig += script;
        return *this;
    }

    testbuilder& num(int num)
    {
        dopush();
        spendtx.vin[0].scriptsig << num;
        return *this;
    }

    testbuilder& push(const std::string& hex)
    {
        dopush(parsehex(hex));
        return *this;
    }

    testbuilder& pushsig(const ckey& key, int nhashtype = sighash_all, unsigned int lenr = 32, unsigned int lens = 32)
    {
        uint256 hash = signaturehash(scriptpubkey, spendtx, 0, nhashtype);
        std::vector<unsigned char> vchsig, r, s;
        uint32_t iter = 0;
        do {
            key.sign(hash, vchsig, iter++);
            if ((lens == 33) != (vchsig[5 + vchsig[3]] == 33)) {
                negatesignatures(vchsig);
            }
            r = std::vector<unsigned char>(vchsig.begin() + 4, vchsig.begin() + 4 + vchsig[3]);
            s = std::vector<unsigned char>(vchsig.begin() + 6 + vchsig[3], vchsig.begin() + 6 + vchsig[3] + vchsig[5 + vchsig[3]]);
        } while (lenr != r.size() || lens != s.size());
        vchsig.push_back(static_cast<unsigned char>(nhashtype));
        dopush(vchsig);
        return *this;
    }

    testbuilder& push(const cpubkey& pubkey)
    {
        dopush(std::vector<unsigned char>(pubkey.begin(), pubkey.end()));
        return *this;
    }

    testbuilder& pushredeem()
    {
        dopush(static_cast<std::vector<unsigned char> >(scriptpubkey));
        return *this;
    }

    testbuilder& editpush(unsigned int pos, const std::string& hexin, const std::string& hexout)
    {
        assert(havepush);
        std::vector<unsigned char> datain = parsehex(hexin);
        std::vector<unsigned char> dataout = parsehex(hexout);
        assert(pos + datain.size() <= push.size());
        boost_check_message(std::vector<unsigned char>(push.begin() + pos, push.begin() + pos + datain.size()) == datain, comment);
        push.erase(push.begin() + pos, push.begin() + pos + datain.size());
        push.insert(push.begin() + pos, dataout.begin(), dataout.end());
        return *this;
    }

    testbuilder& damagepush(unsigned int pos)
    {
        assert(havepush);
        assert(pos < push.size());
        push[pos] ^= 1;
        return *this;
    }

    testbuilder& test(bool expect)
    {
        testbuilder copy = *this; // make a copy so we can rollback the push.
        dopush();
        dotest(credittx.vout[0].scriptpubkey, spendtx.vin[0].scriptsig, flags, expect, comment);
        *this = copy;
        return *this;
    }

    univalue getjson()
    {
        dopush();
        univalue array(univalue::varr);
        array.push_back(formatscript(spendtx.vin[0].scriptsig));
        array.push_back(formatscript(credittx.vout[0].scriptpubkey));
        array.push_back(formatscriptflags(flags));
        array.push_back(comment);
        return array;
    }

    std::string getcomment()
    {
        return comment;
    }

    const cscript& getscriptpubkey()
    {
        return credittx.vout[0].scriptpubkey;
    }
};
}

boost_auto_test_case(script_build)
{
    const keydata keys;

    std::vector<testbuilder> good;
    std::vector<testbuilder> bad;

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                               "p2pk", 0
                              ).pushsig(keys.key0));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                              "p2pk, bad sig", 0
                             ).pushsig(keys.key0).damagepush(10));

    good.push_back(testbuilder(cscript() << op_dup << op_hash160 << tobytevector(keys.pubkey1c.getid()) << op_equalverify << op_checksig,
                               "p2pkh", 0
                              ).pushsig(keys.key1).push(keys.pubkey1c));
    bad.push_back(testbuilder(cscript() << op_dup << op_hash160 << tobytevector(keys.pubkey2c.getid()) << op_equalverify << op_checksig,
                              "p2pkh, bad pubkey", 0
                             ).pushsig(keys.key2).push(keys.pubkey2c).damagepush(5));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1) << op_checksig,
                               "p2pk anyonecanpay", 0
                              ).pushsig(keys.key1, sighash_all | sighash_anyonecanpay));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1) << op_checksig,
                              "p2pk anyonecanpay marked with normal hashtype", 0
                             ).pushsig(keys.key1, sighash_all | sighash_anyonecanpay).editpush(70, "81", "01"));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0c) << op_checksig,
                               "p2sh(p2pk)", script_verify_p2sh, true
                              ).pushsig(keys.key0).pushredeem());
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0c) << op_checksig,
                              "p2sh(p2pk), bad redeemscript", script_verify_p2sh, true
                             ).pushsig(keys.key0).pushredeem().damagepush(10));

    good.push_back(testbuilder(cscript() << op_dup << op_hash160 << tobytevector(keys.pubkey1.getid()) << op_equalverify << op_checksig,
                               "p2sh(p2pkh), bad sig but no verify_p2sh", 0, true
                              ).pushsig(keys.key0).damagepush(10).pushredeem());
    bad.push_back(testbuilder(cscript() << op_dup << op_hash160 << tobytevector(keys.pubkey1.getid()) << op_equalverify << op_checksig,
                              "p2sh(p2pkh), bad sig", script_verify_p2sh, true
                             ).pushsig(keys.key0).damagepush(10).pushredeem());

    good.push_back(testbuilder(cscript() << op_3 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig,
                               "3-of-3", 0
                              ).num(0).pushsig(keys.key0).pushsig(keys.key1).pushsig(keys.key2));
    bad.push_back(testbuilder(cscript() << op_3 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig,
                              "3-of-3, 2 sigs", 0
                             ).num(0).pushsig(keys.key0).pushsig(keys.key1).num(0));

    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig,
                               "p2sh(2-of-3)", script_verify_p2sh, true
                              ).num(0).pushsig(keys.key1).pushsig(keys.key2).pushredeem());
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig,
                              "p2sh(2-of-3), 1 sig", script_verify_p2sh, true
                             ).num(0).pushsig(keys.key1).num(0).pushredeem());

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                               "p2pk with too much r padding but no dersig", 0
                              ).pushsig(keys.key1, sighash_all, 31, 32).editpush(1, "43021f", "44022000"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "p2pk with too much r padding", script_verify_dersig
                             ).pushsig(keys.key1, sighash_all, 31, 32).editpush(1, "43021f", "44022000"));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                               "p2pk with too much s padding but no dersig", 0
                              ).pushsig(keys.key1, sighash_all).editpush(1, "44", "45").editpush(37, "20", "2100"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "p2pk with too much s padding", script_verify_dersig
                             ).pushsig(keys.key1, sighash_all).editpush(1, "44", "45").editpush(37, "20", "2100"));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                               "p2pk with too little r padding but no dersig", 0
                              ).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "p2pk with too little r padding", script_verify_dersig
                             ).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig << op_not,
                               "p2pk not with bad sig with too much r padding but no dersig", 0
                              ).pushsig(keys.key2, sighash_all, 31, 32).editpush(1, "43021f", "44022000").damagepush(10));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig << op_not,
                              "p2pk not with bad sig with too much r padding", script_verify_dersig
                             ).pushsig(keys.key2, sighash_all, 31, 32).editpush(1, "43021f", "44022000").damagepush(10));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig << op_not,
                              "p2pk not with too much r padding but no dersig", 0
                             ).pushsig(keys.key2, sighash_all, 31, 32).editpush(1, "43021f", "44022000"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig << op_not,
                              "p2pk not with too much r padding", script_verify_dersig
                             ).pushsig(keys.key2, sighash_all, 31, 32).editpush(1, "43021f", "44022000"));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                               "bip66 example 1, without dersig", 0
                              ).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "bip66 example 1, with dersig", script_verify_dersig
                             ).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig << op_not,
                              "bip66 example 2, without dersig", 0
                             ).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig << op_not,
                              "bip66 example 2, with dersig", script_verify_dersig
                             ).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "bip66 example 3, without dersig", 0
                             ).num(0));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "bip66 example 3, with dersig", script_verify_dersig
                             ).num(0));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig << op_not,
                               "bip66 example 4, without dersig", 0
                              ).num(0));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig << op_not,
                               "bip66 example 4, with dersig", script_verify_dersig
                              ).num(0));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "bip66 example 5, without dersig", 0
                             ).num(1));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig,
                              "bip66 example 5, with dersig", script_verify_dersig
                             ).num(1));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig << op_not,
                               "bip66 example 6, without dersig", 0
                              ).num(1));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1c) << op_checksig << op_not,
                              "bip66 example 6, with dersig", script_verify_dersig
                             ).num(1));
    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig,
                               "bip66 example 7, without dersig", 0
                              ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").pushsig(keys.key2));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig,
                              "bip66 example 7, with dersig", script_verify_dersig
                             ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").pushsig(keys.key2));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig << op_not,
                              "bip66 example 8, without dersig", 0
                             ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").pushsig(keys.key2));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig << op_not,
                              "bip66 example 8, with dersig", script_verify_dersig
                             ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").pushsig(keys.key2));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig,
                              "bip66 example 9, without dersig", 0
                             ).num(0).num(0).pushsig(keys.key2, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig,
                              "bip66 example 9, with dersig", script_verify_dersig
                             ).num(0).num(0).pushsig(keys.key2, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig << op_not,
                               "bip66 example 10, without dersig", 0
                              ).num(0).num(0).pushsig(keys.key2, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig << op_not,
                              "bip66 example 10, with dersig", script_verify_dersig
                             ).num(0).num(0).pushsig(keys.key2, sighash_all, 33, 32).editpush(1, "45022100", "440220"));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig,
                              "bip66 example 11, without dersig", 0
                             ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").num(0));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig,
                              "bip66 example 11, with dersig", script_verify_dersig
                             ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").num(0));
    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig << op_not,
                               "bip66 example 12, without dersig", 0
                              ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").num(0));
    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_2 << op_checkmultisig << op_not,
                               "bip66 example 12, with dersig", script_verify_dersig
                              ).num(0).pushsig(keys.key1, sighash_all, 33, 32).editpush(1, "45022100", "440220").num(0));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig,
                               "p2pk with multi-byte hashtype, without dersig", 0
                              ).pushsig(keys.key2, sighash_all).editpush(70, "01", "0101"));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig,
                               "p2pk with multi-byte hashtype, with dersig", script_verify_dersig
                              ).pushsig(keys.key2, sighash_all).editpush(70, "01", "0101"));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig,
                               "p2pk with high s but no low_s", 0
                              ).pushsig(keys.key2, sighash_all, 32, 33));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig,
                              "p2pk with high s", script_verify_low_s
                             ).pushsig(keys.key2, sighash_all, 32, 33));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0h) << op_checksig,
                               "p2pk with hybrid pubkey but no strictenc", 0
                              ).pushsig(keys.key0, sighash_all));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0h) << op_checksig,
                              "p2pk with hybrid pubkey", script_verify_strictenc
                             ).pushsig(keys.key0, sighash_all));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0h) << op_checksig << op_not,
                              "p2pk not with hybrid pubkey but no strictenc", 0
                             ).pushsig(keys.key0, sighash_all));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0h) << op_checksig << op_not,
                              "p2pk not with hybrid pubkey", script_verify_strictenc
                             ).pushsig(keys.key0, sighash_all));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0h) << op_checksig << op_not,
                               "p2pk not with invalid hybrid pubkey but no strictenc", 0
                              ).pushsig(keys.key0, sighash_all).damagepush(10));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0h) << op_checksig << op_not,
                              "p2pk not with invalid hybrid pubkey", script_verify_strictenc
                             ).pushsig(keys.key0, sighash_all).damagepush(10));
    good.push_back(testbuilder(cscript() << op_1 << tobytevector(keys.pubkey0h) << tobytevector(keys.pubkey1c) << op_2 << op_checkmultisig,
                               "1-of-2 with the second 1 hybrid pubkey and no strictenc", 0
                              ).num(0).pushsig(keys.key1, sighash_all));
    good.push_back(testbuilder(cscript() << op_1 << tobytevector(keys.pubkey0h) << tobytevector(keys.pubkey1c) << op_2 << op_checkmultisig,
                               "1-of-2 with the second 1 hybrid pubkey", script_verify_strictenc
                              ).num(0).pushsig(keys.key1, sighash_all));
    bad.push_back(testbuilder(cscript() << op_1 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey0h) << op_2 << op_checkmultisig,
                              "1-of-2 with the first 1 hybrid pubkey", script_verify_strictenc
                             ).num(0).pushsig(keys.key1, sighash_all));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1) << op_checksig,
                               "p2pk with undefined hashtype but no strictenc", 0
                              ).pushsig(keys.key1, 5));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1) << op_checksig,
                              "p2pk with undefined hashtype", script_verify_strictenc
                             ).pushsig(keys.key1, 5));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1) << op_checksig << op_not,
                               "p2pk not with invalid sig and undefined hashtype but no strictenc", 0
                              ).pushsig(keys.key1, 5).damagepush(10));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey1) << op_checksig << op_not,
                              "p2pk not with invalid sig and undefined hashtype", script_verify_strictenc
                             ).pushsig(keys.key1, 5).damagepush(10));

    good.push_back(testbuilder(cscript() << op_3 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig,
                               "3-of-3 with nonzero dummy but no nulldummy", 0
                              ).num(1).pushsig(keys.key0).pushsig(keys.key1).pushsig(keys.key2));
    bad.push_back(testbuilder(cscript() << op_3 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig,
                              "3-of-3 with nonzero dummy", script_verify_nulldummy
                             ).num(1).pushsig(keys.key0).pushsig(keys.key1).pushsig(keys.key2));
    good.push_back(testbuilder(cscript() << op_3 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig << op_not,
                               "3-of-3 not with invalid sig and nonzero dummy but no nulldummy", 0
                              ).num(1).pushsig(keys.key0).pushsig(keys.key1).pushsig(keys.key2).damagepush(10));
    bad.push_back(testbuilder(cscript() << op_3 << tobytevector(keys.pubkey0c) << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey2c) << op_3 << op_checkmultisig << op_not,
                              "3-of-3 not with invalid sig with nonzero dummy", script_verify_nulldummy
                             ).num(1).pushsig(keys.key0).pushsig(keys.key1).pushsig(keys.key2).damagepush(10));

    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey1c) << op_2 << op_checkmultisig,
                               "2-of-2 with two identical keys and sigs pushed using op_dup but no sigpushonly", 0
                              ).num(0).pushsig(keys.key1).add(cscript() << op_dup));
    bad.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey1c) << op_2 << op_checkmultisig,
                              "2-of-2 with two identical keys and sigs pushed using op_dup", script_verify_sigpushonly
                             ).num(0).pushsig(keys.key1).add(cscript() << op_dup));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig,
                              "p2sh(p2pk) with non-push scriptsig but no sigpushonly", 0
                             ).pushsig(keys.key2).pushredeem());
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey2c) << op_checksig,
                              "p2sh(p2pk) with non-push scriptsig", script_verify_sigpushonly
                             ).pushsig(keys.key2).pushredeem());
    good.push_back(testbuilder(cscript() << op_2 << tobytevector(keys.pubkey1c) << tobytevector(keys.pubkey1c) << op_2 << op_checkmultisig,
                               "2-of-2 with two identical keys and sigs pushed", script_verify_sigpushonly
                              ).num(0).pushsig(keys.key1).pushsig(keys.key1));

    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                               "p2pk with unnecessary input but no cleanstack", script_verify_p2sh
                              ).num(11).pushsig(keys.key0));
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                              "p2pk with unnecessary input", script_verify_cleanstack | script_verify_p2sh
                             ).num(11).pushsig(keys.key0));
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                               "p2sh with unnecessary input but no cleanstack", script_verify_p2sh, true
                              ).num(11).pushsig(keys.key0).pushredeem());
    bad.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                              "p2sh with unnecessary input", script_verify_cleanstack | script_verify_p2sh, true
                             ).num(11).pushsig(keys.key0).pushredeem());
    good.push_back(testbuilder(cscript() << tobytevector(keys.pubkey0) << op_checksig,
                               "p2sh with cleanstack", script_verify_cleanstack | script_verify_p2sh, true
                              ).pushsig(keys.key0).pushredeem());


    std::set<std::string> tests_good;
    std::set<std::string> tests_bad;

    {
        univalue json_good = read_json(std::string(json_tests::script_valid, json_tests::script_valid + sizeof(json_tests::script_valid)));
        univalue json_bad = read_json(std::string(json_tests::script_invalid, json_tests::script_invalid + sizeof(json_tests::script_invalid)));

        for (unsigned int idx = 0; idx < json_good.size(); idx++) {
            const univalue& tv = json_good[idx];
            tests_good.insert(tv.get_array().write());
        }
        for (unsigned int idx = 0; idx < json_bad.size(); idx++) {
            const univalue& tv = json_bad[idx];
            tests_bad.insert(tv.get_array().write());
        }
    }

    std::string strgood;
    std::string strbad;

    boost_foreach(testbuilder& test, good) {
        test.test(true);
        std::string str = test.getjson().write();
#ifndef update_json_tests
        if (tests_good.count(str) == 0) {
            boost_check_message(false, "missing auto script_valid test: " + test.getcomment());
        }
#endif
        strgood += str + ",\n";
    }
    boost_foreach(testbuilder& test, bad) {
        test.test(false);
        std::string str = test.getjson().write();
#ifndef update_json_tests
        if (tests_bad.count(str) == 0) {
            boost_check_message(false, "missing auto script_invalid test: " + test.getcomment());
        }
#endif
        strbad += str + ",\n";
    }

#ifdef update_json_tests
    file* valid = fopen("script_valid.json.gen", "w");
    fputs(strgood.c_str(), valid);
    fclose(valid);
    file* invalid = fopen("script_invalid.json.gen", "w");
    fputs(strbad.c_str(), invalid);
    fclose(invalid);
#endif
}

boost_auto_test_case(script_valid)
{
    // read tests from test/data/script_valid.json
    // format is an array of arrays
    // inner arrays are [ "scriptsig", "scriptpubkey", "flags" ]
    // ... where scriptsig and scriptpubkey are stringified
    // scripts.
    univalue tests = read_json(std::string(json_tests::script_valid, json_tests::script_valid + sizeof(json_tests::script_valid)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        string strtest = test.write();
        if (test.size() < 3) // allow size > 3; extra stuff ignored (useful for comments)
        {
            if (test.size() != 1) {
                boost_error("bad test: " << strtest);
            }
            continue;
        }
        string scriptsigstring = test[0].get_str();
        cscript scriptsig = parsescript(scriptsigstring);
        string scriptpubkeystring = test[1].get_str();
        cscript scriptpubkey = parsescript(scriptpubkeystring);
        unsigned int scriptflags = parsescriptflags(test[2].get_str());

        dotest(scriptpubkey, scriptsig, scriptflags, true, strtest);
    }
}

boost_auto_test_case(script_invalid)
{
    // scripts that should evaluate as invalid
    univalue tests = read_json(std::string(json_tests::script_invalid, json_tests::script_invalid + sizeof(json_tests::script_invalid)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        string strtest = test.write();
        if (test.size() < 3) // allow size > 2; extra stuff ignored (useful for comments)
        {
            if (test.size() != 1) {
                boost_error("bad test: " << strtest);
            }
            continue;
        }
        string scriptsigstring = test[0].get_str();
        cscript scriptsig = parsescript(scriptsigstring);
        string scriptpubkeystring = test[1].get_str();
        cscript scriptpubkey = parsescript(scriptpubkeystring);
        unsigned int scriptflags = parsescriptflags(test[2].get_str());

        dotest(scriptpubkey, scriptsig, scriptflags, false, strtest);
    }
}

boost_auto_test_case(script_pushdata)
{
    // check that pushdata1, pushdata2, and pushdata4 create the same value on
    // the stack as the 1-75 opcodes do.
    static const unsigned char direct[] = { 1, 0x5a };
    static const unsigned char pushdata1[] = { op_pushdata1, 1, 0x5a };
    static const unsigned char pushdata2[] = { op_pushdata2, 1, 0, 0x5a };
    static const unsigned char pushdata4[] = { op_pushdata4, 1, 0, 0, 0, 0x5a };

    scripterror err;
    vector<vector<unsigned char> > directstack;
    boost_check(evalscript(directstack, cscript(&direct[0], &direct[sizeof(direct)]), script_verify_p2sh, basesignaturechecker(), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    vector<vector<unsigned char> > pushdata1stack;
    boost_check(evalscript(pushdata1stack, cscript(&pushdata1[0], &pushdata1[sizeof(pushdata1)]), script_verify_p2sh, basesignaturechecker(), &err));
    boost_check(pushdata1stack == directstack);
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    vector<vector<unsigned char> > pushdata2stack;
    boost_check(evalscript(pushdata2stack, cscript(&pushdata2[0], &pushdata2[sizeof(pushdata2)]), script_verify_p2sh, basesignaturechecker(), &err));
    boost_check(pushdata2stack == directstack);
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    vector<vector<unsigned char> > pushdata4stack;
    boost_check(evalscript(pushdata4stack, cscript(&pushdata4[0], &pushdata4[sizeof(pushdata4)]), script_verify_p2sh, basesignaturechecker(), &err));
    boost_check(pushdata4stack == directstack);
    boost_check_message(err == script_err_ok, scripterrorstring(err));
}

cscript
sign_multisig(cscript scriptpubkey, std::vector<ckey> keys, ctransaction transaction)
{
    uint256 hash = signaturehash(scriptpubkey, transaction, 0, sighash_all);

    cscript result;
    //
    // note: checkmultisig has an unfortunate bug; it requires
    // one extra item on the stack, before the signatures.
    // putting op_0 on the stack is the workaround;
    // fixing the bug would mean splitting the block chain (old
    // clients would not accept new checkmultisig transactions,
    // and vice-versa)
    //
    result << op_0;
    boost_foreach(const ckey &key, keys)
    {
        vector<unsigned char> vchsig;
        boost_check(key.sign(hash, vchsig));
        vchsig.push_back((unsigned char)sighash_all);
        result << vchsig;
    }
    return result;
}
cscript
sign_multisig(cscript scriptpubkey, const ckey &key, ctransaction transaction)
{
    std::vector<ckey> keys;
    keys.push_back(key);
    return sign_multisig(scriptpubkey, keys, transaction);
}

boost_auto_test_case(script_checkmultisig12)
{
    scripterror err;
    ckey key1, key2, key3;
    key1.makenewkey(true);
    key2.makenewkey(false);
    key3.makenewkey(true);

    cscript scriptpubkey12;
    scriptpubkey12 << op_1 << tobytevector(key1.getpubkey()) << tobytevector(key2.getpubkey()) << op_2 << op_checkmultisig;

    cmutabletransaction txfrom12 = buildcreditingtransaction(scriptpubkey12);
    cmutabletransaction txto12 = buildspendingtransaction(cscript(), txfrom12);

    cscript goodsig1 = sign_multisig(scriptpubkey12, key1, txto12);
    boost_check(verifyscript(goodsig1, scriptpubkey12, flags, mutabletransactionsignaturechecker(&txto12, 0), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));
    txto12.vout[0].nvalue = 2;
    boost_check(!verifyscript(goodsig1, scriptpubkey12, flags, mutabletransactionsignaturechecker(&txto12, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));

    cscript goodsig2 = sign_multisig(scriptpubkey12, key2, txto12);
    boost_check(verifyscript(goodsig2, scriptpubkey12, flags, mutabletransactionsignaturechecker(&txto12, 0), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    cscript badsig1 = sign_multisig(scriptpubkey12, key3, txto12);
    boost_check(!verifyscript(badsig1, scriptpubkey12, flags, mutabletransactionsignaturechecker(&txto12, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));
}

boost_auto_test_case(script_checkmultisig23)
{
    scripterror err;
    ckey key1, key2, key3, key4;
    key1.makenewkey(true);
    key2.makenewkey(false);
    key3.makenewkey(true);
    key4.makenewkey(false);

    cscript scriptpubkey23;
    scriptpubkey23 << op_2 << tobytevector(key1.getpubkey()) << tobytevector(key2.getpubkey()) << tobytevector(key3.getpubkey()) << op_3 << op_checkmultisig;

    cmutabletransaction txfrom23 = buildcreditingtransaction(scriptpubkey23);
    cmutabletransaction txto23 = buildspendingtransaction(cscript(), txfrom23);

    std::vector<ckey> keys;
    keys.push_back(key1); keys.push_back(key2);
    cscript goodsig1 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(verifyscript(goodsig1, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    keys.clear();
    keys.push_back(key1); keys.push_back(key3);
    cscript goodsig2 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(verifyscript(goodsig2, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    keys.clear();
    keys.push_back(key2); keys.push_back(key3);
    cscript goodsig3 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(verifyscript(goodsig3, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    keys.clear();
    keys.push_back(key2); keys.push_back(key2); // can't re-use sig
    cscript badsig1 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(!verifyscript(badsig1, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));

    keys.clear();
    keys.push_back(key2); keys.push_back(key1); // sigs must be in correct order
    cscript badsig2 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(!verifyscript(badsig2, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));

    keys.clear();
    keys.push_back(key3); keys.push_back(key2); // sigs must be in correct order
    cscript badsig3 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(!verifyscript(badsig3, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));

    keys.clear();
    keys.push_back(key4); keys.push_back(key2); // sigs must match pubkeys
    cscript badsig4 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(!verifyscript(badsig4, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));

    keys.clear();
    keys.push_back(key1); keys.push_back(key4); // sigs must match pubkeys
    cscript badsig5 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(!verifyscript(badsig5, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_eval_false, scripterrorstring(err));

    keys.clear(); // must have signatures
    cscript badsig6 = sign_multisig(scriptpubkey23, keys, txto23);
    boost_check(!verifyscript(badsig6, scriptpubkey23, flags, mutabletransactionsignaturechecker(&txto23, 0), &err));
    boost_check_message(err == script_err_invalid_stack_operation, scripterrorstring(err));
}    

boost_auto_test_case(script_combinesigs)
{
    // test the combinesignatures function
    cbasickeystore keystore;
    vector<ckey> keys;
    vector<cpubkey> pubkeys;
    for (int i = 0; i < 3; i++)
    {
        ckey key;
        key.makenewkey(i%2 == 1);
        keys.push_back(key);
        pubkeys.push_back(key.getpubkey());
        keystore.addkey(key);
    }

    cmutabletransaction txfrom = buildcreditingtransaction(getscriptfordestination(keys[0].getpubkey().getid()));
    cmutabletransaction txto = buildspendingtransaction(cscript(), txfrom);
    cscript& scriptpubkey = txfrom.vout[0].scriptpubkey;
    cscript& scriptsig = txto.vin[0].scriptsig;

    cscript empty;
    cscript combined = combinesignatures(scriptpubkey, txto, 0, empty, empty);
    boost_check(combined.empty());

    // single signature case:
    signsignature(keystore, txfrom, txto, 0); // changes scriptsig
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsig, empty);
    boost_check(combined == scriptsig);
    combined = combinesignatures(scriptpubkey, txto, 0, empty, scriptsig);
    boost_check(combined == scriptsig);
    cscript scriptsigcopy = scriptsig;
    // signing again will give a different, valid signature:
    signsignature(keystore, txfrom, txto, 0);
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsigcopy, scriptsig);
    boost_check(combined == scriptsigcopy || combined == scriptsig);

    // p2sh, single-signature case:
    cscript pksingle; pksingle << tobytevector(keys[0].getpubkey()) << op_checksig;
    keystore.addcscript(pksingle);
    scriptpubkey = getscriptfordestination(cscriptid(pksingle));
    signsignature(keystore, txfrom, txto, 0);
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsig, empty);
    boost_check(combined == scriptsig);
    combined = combinesignatures(scriptpubkey, txto, 0, empty, scriptsig);
    boost_check(combined == scriptsig);
    scriptsigcopy = scriptsig;
    signsignature(keystore, txfrom, txto, 0);
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsigcopy, scriptsig);
    boost_check(combined == scriptsigcopy || combined == scriptsig);
    // dummy scriptsigcopy with placeholder, should always choose non-placeholder:
    scriptsigcopy = cscript() << op_0 << static_cast<vector<unsigned char> >(pksingle);
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsigcopy, scriptsig);
    boost_check(combined == scriptsig);
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsig, scriptsigcopy);
    boost_check(combined == scriptsig);

    // hardest case:  multisig 2-of-3
    scriptpubkey = getscriptformultisig(2, pubkeys);
    keystore.addcscript(scriptpubkey);
    signsignature(keystore, txfrom, txto, 0);
    combined = combinesignatures(scriptpubkey, txto, 0, scriptsig, empty);
    boost_check(combined == scriptsig);
    combined = combinesignatures(scriptpubkey, txto, 0, empty, scriptsig);
    boost_check(combined == scriptsig);

    // a couple of partially-signed versions:
    vector<unsigned char> sig1;
    uint256 hash1 = signaturehash(scriptpubkey, txto, 0, sighash_all);
    boost_check(keys[0].sign(hash1, sig1));
    sig1.push_back(sighash_all);
    vector<unsigned char> sig2;
    uint256 hash2 = signaturehash(scriptpubkey, txto, 0, sighash_none);
    boost_check(keys[1].sign(hash2, sig2));
    sig2.push_back(sighash_none);
    vector<unsigned char> sig3;
    uint256 hash3 = signaturehash(scriptpubkey, txto, 0, sighash_single);
    boost_check(keys[2].sign(hash3, sig3));
    sig3.push_back(sighash_single);

    // not fussy about order (or even existence) of placeholders or signatures:
    cscript partial1a = cscript() << op_0 << sig1 << op_0;
    cscript partial1b = cscript() << op_0 << op_0 << sig1;
    cscript partial2a = cscript() << op_0 << sig2;
    cscript partial2b = cscript() << sig2 << op_0;
    cscript partial3a = cscript() << sig3;
    cscript partial3b = cscript() << op_0 << op_0 << sig3;
    cscript partial3c = cscript() << op_0 << sig3 << op_0;
    cscript complete12 = cscript() << op_0 << sig1 << sig2;
    cscript complete13 = cscript() << op_0 << sig1 << sig3;
    cscript complete23 = cscript() << op_0 << sig2 << sig3;

    combined = combinesignatures(scriptpubkey, txto, 0, partial1a, partial1b);
    boost_check(combined == partial1a);
    combined = combinesignatures(scriptpubkey, txto, 0, partial1a, partial2a);
    boost_check(combined == complete12);
    combined = combinesignatures(scriptpubkey, txto, 0, partial2a, partial1a);
    boost_check(combined == complete12);
    combined = combinesignatures(scriptpubkey, txto, 0, partial1b, partial2b);
    boost_check(combined == complete12);
    combined = combinesignatures(scriptpubkey, txto, 0, partial3b, partial1b);
    boost_check(combined == complete13);
    combined = combinesignatures(scriptpubkey, txto, 0, partial2a, partial3a);
    boost_check(combined == complete23);
    combined = combinesignatures(scriptpubkey, txto, 0, partial3b, partial2b);
    boost_check(combined == complete23);
    combined = combinesignatures(scriptpubkey, txto, 0, partial3b, partial3a);
    boost_check(combined == partial3c);
}

boost_auto_test_case(script_standard_push)
{
    scripterror err;
    for (int i=0; i<67000; i++) {
        cscript script;
        script << i;
        boost_check_message(script.ispushonly(), "number " << i << " is not pure push.");
        boost_check_message(verifyscript(script, cscript() << op_1, script_verify_minimaldata, basesignaturechecker(), &err), "number " << i << " push is not minimal data.");
        boost_check_message(err == script_err_ok, scripterrorstring(err));
    }

    for (unsigned int i=0; i<=max_script_element_size; i++) {
        std::vector<unsigned char> data(i, '\111');
        cscript script;
        script << data;
        boost_check_message(script.ispushonly(), "length " << i << " is not pure push.");
        boost_check_message(verifyscript(script, cscript() << op_1, script_verify_minimaldata, basesignaturechecker(), &err), "length " << i << " push is not minimal data.");
        boost_check_message(err == script_err_ok, scripterrorstring(err));
    }
}

boost_auto_test_case(script_ispushonly_on_invalid_scripts)
{
    // ispushonly returns false when given a script containing only pushes that
    // are invalid due to truncation. ispushonly() is consensus critical
    // because p2sh evaluation uses it, although this specific behavior should
    // not be consensus critical as the p2sh evaluation would fail first due to
    // the invalid push. still, it doesn't hurt to test it explicitly.
    static const unsigned char direct[] = { 1 };
    boost_check(!cscript(direct, direct+sizeof(direct)).ispushonly());
}

boost_auto_test_suite_end()
