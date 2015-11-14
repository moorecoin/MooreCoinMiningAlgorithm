// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "test/test_moorecoin.h"

#ifdef enable_wallet
#include "wallet/wallet_ismine.h"
#endif

#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

// helpers:
static std::vector<unsigned char>
serialize(const cscript& s)
{
    std::vector<unsigned char> sserialized(s);
    return sserialized;
}

static bool
verify(const cscript& scriptsig, const cscript& scriptpubkey, bool fstrict, scripterror& err)
{
    // create dummy to/from transactions:
    cmutabletransaction txfrom;
    txfrom.vout.resize(1);
    txfrom.vout[0].scriptpubkey = scriptpubkey;

    cmutabletransaction txto;
    txto.vin.resize(1);
    txto.vout.resize(1);
    txto.vin[0].prevout.n = 0;
    txto.vin[0].prevout.hash = txfrom.gethash();
    txto.vin[0].scriptsig = scriptsig;
    txto.vout[0].nvalue = 1;

    return verifyscript(scriptsig, scriptpubkey, fstrict ? script_verify_p2sh : script_verify_none, mutabletransactionsignaturechecker(&txto, 0), &err);
}


boost_fixture_test_suite(script_p2sh_tests, basictestingsetup)

boost_auto_test_case(sign)
{
    lock(cs_main);
    // pay-to-script-hash looks like this:
    // scriptsig:    <sig> <sig...> <serialized_script>
    // scriptpubkey: hash160 <hash> equal

    // test signsignature() (and therefore the version of solver() that signs transactions)
    cbasickeystore keystore;
    ckey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].makenewkey(true);
        keystore.addkey(key[i]);
    }

    // 8 scripts: checking all combinations of
    // different keys, straight/p2sh, pubkey/pubkeyhash
    cscript standardscripts[4];
    standardscripts[0] << tobytevector(key[0].getpubkey()) << op_checksig;
    standardscripts[1] = getscriptfordestination(key[1].getpubkey().getid());
    standardscripts[2] << tobytevector(key[1].getpubkey()) << op_checksig;
    standardscripts[3] = getscriptfordestination(key[2].getpubkey().getid());
    cscript evalscripts[4];
    for (int i = 0; i < 4; i++)
    {
        keystore.addcscript(standardscripts[i]);
        evalscripts[i] = getscriptfordestination(cscriptid(standardscripts[i]));
    }

    cmutabletransaction txfrom;  // funding transaction:
    string reason;
    txfrom.vout.resize(8);
    for (int i = 0; i < 4; i++)
    {
        txfrom.vout[i].scriptpubkey = evalscripts[i];
        txfrom.vout[i].nvalue = coin;
        txfrom.vout[i+4].scriptpubkey = standardscripts[i];
        txfrom.vout[i+4].nvalue = coin;
    }
    boost_check(isstandardtx(txfrom, reason));

    cmutabletransaction txto[8]; // spending transactions
    for (int i = 0; i < 8; i++)
    {
        txto[i].vin.resize(1);
        txto[i].vout.resize(1);
        txto[i].vin[0].prevout.n = i;
        txto[i].vin[0].prevout.hash = txfrom.gethash();
        txto[i].vout[0].nvalue = 1;
#ifdef enable_wallet
        boost_check_message(ismine(keystore, txfrom.vout[i].scriptpubkey), strprintf("ismine %d", i));
#endif
    }
    for (int i = 0; i < 8; i++)
    {
        boost_check_message(signsignature(keystore, txfrom, txto[i], 0), strprintf("signsignature %d", i));
    }
    // all of the above should be ok, and the txtos have valid signatures
    // check to make sure signature verification fails if we use the wrong scriptsig:
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
        {
            cscript sigsave = txto[i].vin[0].scriptsig;
            txto[i].vin[0].scriptsig = txto[j].vin[0].scriptsig;
            bool sigok = cscriptcheck(ccoins(txfrom, 0), txto[i], 0, script_verify_p2sh | script_verify_strictenc, false)();
            if (i == j)
                boost_check_message(sigok, strprintf("verifysignature %d %d", i, j));
            else
                boost_check_message(!sigok, strprintf("verifysignature %d %d", i, j));
            txto[i].vin[0].scriptsig = sigsave;
        }
}

boost_auto_test_case(norecurse)
{
    scripterror err;
    // make sure only the outer pay-to-script-hash does the
    // extra-validation thing:
    cscript invalidasscript;
    invalidasscript << op_invalidopcode << op_invalidopcode;

    cscript p2sh = getscriptfordestination(cscriptid(invalidasscript));

    cscript scriptsig;
    scriptsig << serialize(invalidasscript);

    // should not verify, because it will try to execute op_invalidopcode
    boost_check(!verify(scriptsig, p2sh, true, err));
    boost_check_message(err == script_err_bad_opcode, scripterrorstring(err));

    // try to recur, and verification should succeed because
    // the inner hash160 <> equal should only check the hash:
    cscript p2sh2 = getscriptfordestination(cscriptid(p2sh));
    cscript scriptsig2;
    scriptsig2 << serialize(invalidasscript) << serialize(p2sh);

    boost_check(verify(scriptsig2, p2sh2, true, err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));
}

boost_auto_test_case(set)
{
    lock(cs_main);
    // test the cscript::set* methods
    cbasickeystore keystore;
    ckey key[4];
    std::vector<cpubkey> keys;
    for (int i = 0; i < 4; i++)
    {
        key[i].makenewkey(true);
        keystore.addkey(key[i]);
        keys.push_back(key[i].getpubkey());
    }

    cscript inner[4];
    inner[0] = getscriptfordestination(key[0].getpubkey().getid());
    inner[1] = getscriptformultisig(2, std::vector<cpubkey>(keys.begin(), keys.begin()+2));
    inner[2] = getscriptformultisig(1, std::vector<cpubkey>(keys.begin(), keys.begin()+2));
    inner[3] = getscriptformultisig(2, std::vector<cpubkey>(keys.begin(), keys.begin()+3));

    cscript outer[4];
    for (int i = 0; i < 4; i++)
    {
        outer[i] = getscriptfordestination(cscriptid(inner[i]));
        keystore.addcscript(inner[i]);
    }

    cmutabletransaction txfrom;  // funding transaction:
    string reason;
    txfrom.vout.resize(4);
    for (int i = 0; i < 4; i++)
    {
        txfrom.vout[i].scriptpubkey = outer[i];
        txfrom.vout[i].nvalue = cent;
    }
    boost_check(isstandardtx(txfrom, reason));

    cmutabletransaction txto[4]; // spending transactions
    for (int i = 0; i < 4; i++)
    {
        txto[i].vin.resize(1);
        txto[i].vout.resize(1);
        txto[i].vin[0].prevout.n = i;
        txto[i].vin[0].prevout.hash = txfrom.gethash();
        txto[i].vout[0].nvalue = 1*cent;
        txto[i].vout[0].scriptpubkey = inner[i];
#ifdef enable_wallet
        boost_check_message(ismine(keystore, txfrom.vout[i].scriptpubkey), strprintf("ismine %d", i));
#endif
    }
    for (int i = 0; i < 4; i++)
    {
        boost_check_message(signsignature(keystore, txfrom, txto[i], 0), strprintf("signsignature %d", i));
        boost_check_message(isstandardtx(txto[i], reason), strprintf("txto[%d].isstandard", i));
    }
}

boost_auto_test_case(is)
{
    // test cscript::ispaytoscripthash()
    uint160 dummy;
    cscript p2sh;
    p2sh << op_hash160 << tobytevector(dummy) << op_equal;
    boost_check(p2sh.ispaytoscripthash());

    // not considered pay-to-script-hash if using one of the op_pushdata opcodes:
    static const unsigned char direct[] =    { op_hash160, 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, op_equal };
    boost_check(cscript(direct, direct+sizeof(direct)).ispaytoscripthash());
    static const unsigned char pushdata1[] = { op_hash160, op_pushdata1, 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, op_equal };
    boost_check(!cscript(pushdata1, pushdata1+sizeof(pushdata1)).ispaytoscripthash());
    static const unsigned char pushdata2[] = { op_hash160, op_pushdata2, 20,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, op_equal };
    boost_check(!cscript(pushdata2, pushdata2+sizeof(pushdata2)).ispaytoscripthash());
    static const unsigned char pushdata4[] = { op_hash160, op_pushdata4, 20,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, op_equal };
    boost_check(!cscript(pushdata4, pushdata4+sizeof(pushdata4)).ispaytoscripthash());

    cscript not_p2sh;
    boost_check(!not_p2sh.ispaytoscripthash());

    not_p2sh.clear(); not_p2sh << op_hash160 << tobytevector(dummy) << tobytevector(dummy) << op_equal;
    boost_check(!not_p2sh.ispaytoscripthash());

    not_p2sh.clear(); not_p2sh << op_nop << tobytevector(dummy) << op_equal;
    boost_check(!not_p2sh.ispaytoscripthash());

    not_p2sh.clear(); not_p2sh << op_hash160 << tobytevector(dummy) << op_checksig;
    boost_check(!not_p2sh.ispaytoscripthash());
}

boost_auto_test_case(switchover)
{
    // test switch over code
    cscript notvalid;
    scripterror err;
    notvalid << op_11 << op_12 << op_equalverify;
    cscript scriptsig;
    scriptsig << serialize(notvalid);

    cscript fund = getscriptfordestination(cscriptid(notvalid));


    // validation should succeed under old rules (hash is correct):
    boost_check(verify(scriptsig, fund, false, err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));
    // fail under new:
    boost_check(!verify(scriptsig, fund, true, err));
    boost_check_message(err == script_err_equalverify, scripterrorstring(err));
}

boost_auto_test_case(areinputsstandard)
{
    lock(cs_main);
    ccoinsview coinsdummy;
    ccoinsviewcache coins(&coinsdummy);
    cbasickeystore keystore;
    ckey key[6];
    vector<cpubkey> keys;
    for (int i = 0; i < 6; i++)
    {
        key[i].makenewkey(true);
        keystore.addkey(key[i]);
    }
    for (int i = 0; i < 3; i++)
        keys.push_back(key[i].getpubkey());

    cmutabletransaction txfrom;
    txfrom.vout.resize(7);

    // first three are standard:
    cscript pay1 = getscriptfordestination(key[0].getpubkey().getid());
    keystore.addcscript(pay1);
    cscript pay1of3 = getscriptformultisig(1, keys);

    txfrom.vout[0].scriptpubkey = getscriptfordestination(cscriptid(pay1)); // p2sh (op_checksig)
    txfrom.vout[0].nvalue = 1000;
    txfrom.vout[1].scriptpubkey = pay1; // ordinary op_checksig
    txfrom.vout[1].nvalue = 2000;
    txfrom.vout[2].scriptpubkey = pay1of3; // ordinary op_checkmultisig
    txfrom.vout[2].nvalue = 3000;

    // vout[3] is complicated 1-of-3 and 2-of-3
    // ... that is ok if wrapped in p2sh:
    cscript oneandtwo;
    oneandtwo << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << tobytevector(key[2].getpubkey());
    oneandtwo << op_3 << op_checkmultisigverify;
    oneandtwo << op_2 << tobytevector(key[3].getpubkey()) << tobytevector(key[4].getpubkey()) << tobytevector(key[5].getpubkey());
    oneandtwo << op_3 << op_checkmultisig;
    keystore.addcscript(oneandtwo);
    txfrom.vout[3].scriptpubkey = getscriptfordestination(cscriptid(oneandtwo));
    txfrom.vout[3].nvalue = 4000;

    // vout[4] is max sigops:
    cscript fifteensigops; fifteensigops << op_1;
    for (unsigned i = 0; i < max_p2sh_sigops; i++)
        fifteensigops << tobytevector(key[i%3].getpubkey());
    fifteensigops << op_15 << op_checkmultisig;
    keystore.addcscript(fifteensigops);
    txfrom.vout[4].scriptpubkey = getscriptfordestination(cscriptid(fifteensigops));
    txfrom.vout[4].nvalue = 5000;

    // vout[5/6] are non-standard because they exceed max_p2sh_sigops
    cscript sixteensigops; sixteensigops << op_16 << op_checkmultisig;
    keystore.addcscript(sixteensigops);
    txfrom.vout[5].scriptpubkey = getscriptfordestination(cscriptid(fifteensigops));
    txfrom.vout[5].nvalue = 5000;
    cscript twentysigops; twentysigops << op_checkmultisig;
    keystore.addcscript(twentysigops);
    txfrom.vout[6].scriptpubkey = getscriptfordestination(cscriptid(twentysigops));
    txfrom.vout[6].nvalue = 6000;

    coins.modifycoins(txfrom.gethash())->fromtx(txfrom, 0);

    cmutabletransaction txto;
    txto.vout.resize(1);
    txto.vout[0].scriptpubkey = getscriptfordestination(key[1].getpubkey().getid());

    txto.vin.resize(5);
    for (int i = 0; i < 5; i++)
    {
        txto.vin[i].prevout.n = i;
        txto.vin[i].prevout.hash = txfrom.gethash();
    }
    boost_check(signsignature(keystore, txfrom, txto, 0));
    boost_check(signsignature(keystore, txfrom, txto, 1));
    boost_check(signsignature(keystore, txfrom, txto, 2));
    // signsignature doesn't know how to sign these. we're
    // not testing validating signatures, so just create
    // dummy signatures that do include the correct p2sh scripts:
    txto.vin[3].scriptsig << op_11 << op_11 << static_cast<vector<unsigned char> >(oneandtwo);
    txto.vin[4].scriptsig << static_cast<vector<unsigned char> >(fifteensigops);

    boost_check(::areinputsstandard(txto, coins));
    // 22 p2sh sigops for all inputs (1 for vin[0], 6 for vin[3], 15 for vin[4]
    boost_check_equal(getp2shsigopcount(txto, coins), 22u);

    // make sure adding crap to the scriptsigs makes them non-standard:
    for (int i = 0; i < 3; i++)
    {
        cscript t = txto.vin[i].scriptsig;
        txto.vin[i].scriptsig = (cscript() << 11) + t;
        boost_check(!::areinputsstandard(txto, coins));
        txto.vin[i].scriptsig = t;
    }

    cmutabletransaction txtononstd1;
    txtononstd1.vout.resize(1);
    txtononstd1.vout[0].scriptpubkey = getscriptfordestination(key[1].getpubkey().getid());
    txtononstd1.vout[0].nvalue = 1000;
    txtononstd1.vin.resize(1);
    txtononstd1.vin[0].prevout.n = 5;
    txtononstd1.vin[0].prevout.hash = txfrom.gethash();
    txtononstd1.vin[0].scriptsig << static_cast<vector<unsigned char> >(sixteensigops);

    boost_check(!::areinputsstandard(txtononstd1, coins));
    boost_check_equal(getp2shsigopcount(txtononstd1, coins), 16u);

    cmutabletransaction txtononstd2;
    txtononstd2.vout.resize(1);
    txtononstd2.vout[0].scriptpubkey = getscriptfordestination(key[1].getpubkey().getid());
    txtononstd2.vout[0].nvalue = 1000;
    txtononstd2.vin.resize(1);
    txtononstd2.vin[0].prevout.n = 6;
    txtononstd2.vin[0].prevout.hash = txfrom.gethash();
    txtononstd2.vin[0].scriptsig << static_cast<vector<unsigned char> >(twentysigops);

    boost_check(!::areinputsstandard(txtononstd2, coins));
    boost_check_equal(getp2shsigopcount(txtononstd2, coins), 20u);
}

boost_auto_test_suite_end()
