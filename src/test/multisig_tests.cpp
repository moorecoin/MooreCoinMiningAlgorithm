// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "uint256.h"
#include "test/test_moorecoin.h"

#ifdef enable_wallet
#include "wallet/wallet_ismine.h"
#endif

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

boost_fixture_test_suite(multisig_tests, basictestingsetup)

cscript
sign_multisig(cscript scriptpubkey, vector<ckey> keys, ctransaction transaction, int whichin)
{
    uint256 hash = signaturehash(scriptpubkey, transaction, whichin, sighash_all);

    cscript result;
    result << op_0; // checkmultisig bug workaround
    boost_foreach(const ckey &key, keys)
    {
        vector<unsigned char> vchsig;
        boost_check(key.sign(hash, vchsig));
        vchsig.push_back((unsigned char)sighash_all);
        result << vchsig;
    }
    return result;
}

boost_auto_test_case(multisig_verify)
{
    unsigned int flags = script_verify_p2sh | script_verify_strictenc;

    scripterror err;
    ckey key[4];
    for (int i = 0; i < 4; i++)
        key[i].makenewkey(true);

    cscript a_and_b;
    a_and_b << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;

    cscript a_or_b;
    a_or_b << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;

    cscript escrow;
    escrow << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << tobytevector(key[2].getpubkey()) << op_3 << op_checkmultisig;

    cmutabletransaction txfrom;  // funding transaction
    txfrom.vout.resize(3);
    txfrom.vout[0].scriptpubkey = a_and_b;
    txfrom.vout[1].scriptpubkey = a_or_b;
    txfrom.vout[2].scriptpubkey = escrow;

    cmutabletransaction txto[3]; // spending transaction
    for (int i = 0; i < 3; i++)
    {
        txto[i].vin.resize(1);
        txto[i].vout.resize(1);
        txto[i].vin[0].prevout.n = i;
        txto[i].vin[0].prevout.hash = txfrom.gethash();
        txto[i].vout[0].nvalue = 1;
    }

    vector<ckey> keys;
    cscript s;

    // test a and b:
    keys.assign(1,key[0]);
    keys.push_back(key[1]);
    s = sign_multisig(a_and_b, keys, txto[0], 0);
    boost_check(verifyscript(s, a_and_b, flags, mutabletransactionsignaturechecker(&txto[0], 0), &err));
    boost_check_message(err == script_err_ok, scripterrorstring(err));

    for (int i = 0; i < 4; i++)
    {
        keys.assign(1,key[i]);
        s = sign_multisig(a_and_b, keys, txto[0], 0);
        boost_check_message(!verifyscript(s, a_and_b, flags, mutabletransactionsignaturechecker(&txto[0], 0), &err), strprintf("a&b 1: %d", i));
        boost_check_message(err == script_err_invalid_stack_operation, scripterrorstring(err));

        keys.assign(1,key[1]);
        keys.push_back(key[i]);
        s = sign_multisig(a_and_b, keys, txto[0], 0);
        boost_check_message(!verifyscript(s, a_and_b, flags, mutabletransactionsignaturechecker(&txto[0], 0), &err), strprintf("a&b 2: %d", i));
        boost_check_message(err == script_err_eval_false, scripterrorstring(err));
    }

    // test a or b:
    for (int i = 0; i < 4; i++)
    {
        keys.assign(1,key[i]);
        s = sign_multisig(a_or_b, keys, txto[1], 0);
        if (i == 0 || i == 1)
        {
            boost_check_message(verifyscript(s, a_or_b, flags, mutabletransactionsignaturechecker(&txto[1], 0), &err), strprintf("a|b: %d", i));
            boost_check_message(err == script_err_ok, scripterrorstring(err));
        }
        else
        {
            boost_check_message(!verifyscript(s, a_or_b, flags, mutabletransactionsignaturechecker(&txto[1], 0), &err), strprintf("a|b: %d", i));
            boost_check_message(err == script_err_eval_false, scripterrorstring(err));
        }
    }
    s.clear();
    s << op_0 << op_1;
    boost_check(!verifyscript(s, a_or_b, flags, mutabletransactionsignaturechecker(&txto[1], 0), &err));
    boost_check_message(err == script_err_sig_der, scripterrorstring(err));


    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            keys.assign(1,key[i]);
            keys.push_back(key[j]);
            s = sign_multisig(escrow, keys, txto[2], 0);
            if (i < j && i < 3 && j < 3)
            {
                boost_check_message(verifyscript(s, escrow, flags, mutabletransactionsignaturechecker(&txto[2], 0), &err), strprintf("escrow 1: %d %d", i, j));
                boost_check_message(err == script_err_ok, scripterrorstring(err));
            }
            else
            {
                boost_check_message(!verifyscript(s, escrow, flags, mutabletransactionsignaturechecker(&txto[2], 0), &err), strprintf("escrow 2: %d %d", i, j));
                boost_check_message(err == script_err_eval_false, scripterrorstring(err));
            }
        }
}

boost_auto_test_case(multisig_isstandard)
{
    ckey key[4];
    for (int i = 0; i < 4; i++)
        key[i].makenewkey(true);

    txnouttype whichtype;

    cscript a_and_b;
    a_and_b << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;
    boost_check(::isstandard(a_and_b, whichtype));

    cscript a_or_b;
    a_or_b  << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;
    boost_check(::isstandard(a_or_b, whichtype));

    cscript escrow;
    escrow << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << tobytevector(key[2].getpubkey()) << op_3 << op_checkmultisig;
    boost_check(::isstandard(escrow, whichtype));

    cscript one_of_four;
    one_of_four << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << tobytevector(key[2].getpubkey()) << tobytevector(key[3].getpubkey()) << op_4 << op_checkmultisig;
    boost_check(!::isstandard(one_of_four, whichtype));

    cscript malformed[6];
    malformed[0] << op_3 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;
    malformed[1] << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_3 << op_checkmultisig;
    malformed[2] << op_0 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;
    malformed[3] << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_0 << op_checkmultisig;
    malformed[4] << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_checkmultisig;
    malformed[5] << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey());

    for (int i = 0; i < 6; i++)
        boost_check(!::isstandard(malformed[i], whichtype));
}

boost_auto_test_case(multisig_solver1)
{
    // tests solver() that returns lists of keys that are
    // required to satisfy a scriptpubkey
    //
    // also tests ismine() and extractdestination()
    //
    // note: extractdestination for the multisignature transactions
    // always returns false for this release, even if you have
    // one key that would satisfy an (a|b) or 2-of-3 keys needed
    // to spend an escrow transaction.
    //
    cbasickeystore keystore, emptykeystore, partialkeystore;
    ckey key[3];
    ctxdestination keyaddr[3];
    for (int i = 0; i < 3; i++)
    {
        key[i].makenewkey(true);
        keystore.addkey(key[i]);
        keyaddr[i] = key[i].getpubkey().getid();
    }
    partialkeystore.addkey(key[0]);

    {
        vector<valtype> solutions;
        txnouttype whichtype;
        cscript s;
        s << tobytevector(key[0].getpubkey()) << op_checksig;
        boost_check(solver(s, whichtype, solutions));
        boost_check(solutions.size() == 1);
        ctxdestination addr;
        boost_check(extractdestination(s, addr));
        boost_check(addr == keyaddr[0]);
#ifdef enable_wallet
        boost_check(ismine(keystore, s));
        boost_check(!ismine(emptykeystore, s));
#endif
    }
    {
        vector<valtype> solutions;
        txnouttype whichtype;
        cscript s;
        s << op_dup << op_hash160 << tobytevector(key[0].getpubkey().getid()) << op_equalverify << op_checksig;
        boost_check(solver(s, whichtype, solutions));
        boost_check(solutions.size() == 1);
        ctxdestination addr;
        boost_check(extractdestination(s, addr));
        boost_check(addr == keyaddr[0]);
#ifdef enable_wallet
        boost_check(ismine(keystore, s));
        boost_check(!ismine(emptykeystore, s));
#endif
    }
    {
        vector<valtype> solutions;
        txnouttype whichtype;
        cscript s;
        s << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;
        boost_check(solver(s, whichtype, solutions));
        boost_check_equal(solutions.size(), 4u);
        ctxdestination addr;
        boost_check(!extractdestination(s, addr));
#ifdef enable_wallet
        boost_check(ismine(keystore, s));
        boost_check(!ismine(emptykeystore, s));
        boost_check(!ismine(partialkeystore, s));
#endif
    }
    {
        vector<valtype> solutions;
        txnouttype whichtype;
        cscript s;
        s << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;
        boost_check(solver(s, whichtype, solutions));
        boost_check_equal(solutions.size(), 4u);
        vector<ctxdestination> addrs;
        int nrequired;
        boost_check(extractdestinations(s, whichtype, addrs, nrequired));
        boost_check(addrs[0] == keyaddr[0]);
        boost_check(addrs[1] == keyaddr[1]);
        boost_check(nrequired == 1);
#ifdef enable_wallet
        boost_check(ismine(keystore, s));
        boost_check(!ismine(emptykeystore, s));
        boost_check(!ismine(partialkeystore, s));
#endif
    }
    {
        vector<valtype> solutions;
        txnouttype whichtype;
        cscript s;
        s << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << tobytevector(key[2].getpubkey()) << op_3 << op_checkmultisig;
        boost_check(solver(s, whichtype, solutions));
        boost_check(solutions.size() == 5);
    }
}

boost_auto_test_case(multisig_sign)
{
    // test signsignature() (and therefore the version of solver() that signs transactions)
    cbasickeystore keystore;
    ckey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].makenewkey(true);
        keystore.addkey(key[i]);
    }

    cscript a_and_b;
    a_and_b << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;

    cscript a_or_b;
    a_or_b  << op_1 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << op_2 << op_checkmultisig;

    cscript escrow;
    escrow << op_2 << tobytevector(key[0].getpubkey()) << tobytevector(key[1].getpubkey()) << tobytevector(key[2].getpubkey()) << op_3 << op_checkmultisig;

    cmutabletransaction txfrom;  // funding transaction
    txfrom.vout.resize(3);
    txfrom.vout[0].scriptpubkey = a_and_b;
    txfrom.vout[1].scriptpubkey = a_or_b;
    txfrom.vout[2].scriptpubkey = escrow;

    cmutabletransaction txto[3]; // spending transaction
    for (int i = 0; i < 3; i++)
    {
        txto[i].vin.resize(1);
        txto[i].vout.resize(1);
        txto[i].vin[0].prevout.n = i;
        txto[i].vin[0].prevout.hash = txfrom.gethash();
        txto[i].vout[0].nvalue = 1;
    }

    for (int i = 0; i < 3; i++)
    {
        boost_check_message(signsignature(keystore, txfrom, txto[i], 0), strprintf("signsignature %d", i));
    }
}


boost_auto_test_suite_end()
