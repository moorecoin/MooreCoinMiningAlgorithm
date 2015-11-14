// copyright (c) 2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/validation.h"
#include "data/sighash.json.h"
#include "main.h"
#include "random.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "serialize.h"
#include "test/test_moorecoin.h"
#include "util.h"
#include "version.h"

#include <iostream>

#include <boost/test/unit_test.hpp>

#include "univalue/univalue.h"

extern univalue read_json(const std::string& jsondata);

// old script.cpp signaturehash function
uint256 static signaturehashold(cscript scriptcode, const ctransaction& txto, unsigned int nin, int nhashtype)
{
    static const uint256 one(uint256s("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nin >= txto.vin.size())
    {
        printf("error: signaturehash(): nin=%d out of range\n", nin);
        return one;
    }
    cmutabletransaction txtmp(txto);

    // in case concatenating two scripts ends up with two codeseparators,
    // or an extra one at the end, this prevents all those possible incompatibilities.
    scriptcode.findanddelete(cscript(op_codeseparator));

    // blank out other inputs' signatures
    for (unsigned int i = 0; i < txtmp.vin.size(); i++)
        txtmp.vin[i].scriptsig = cscript();
    txtmp.vin[nin].scriptsig = scriptcode;

    // blank out some of the outputs
    if ((nhashtype & 0x1f) == sighash_none)
    {
        // wildcard payee
        txtmp.vout.clear();

        // let the others update at will
        for (unsigned int i = 0; i < txtmp.vin.size(); i++)
            if (i != nin)
                txtmp.vin[i].nsequence = 0;
    }
    else if ((nhashtype & 0x1f) == sighash_single)
    {
        // only lock-in the txout payee at same index as txin
        unsigned int nout = nin;
        if (nout >= txtmp.vout.size())
        {
            printf("error: signaturehash(): nout=%d out of range\n", nout);
            return one;
        }
        txtmp.vout.resize(nout+1);
        for (unsigned int i = 0; i < nout; i++)
            txtmp.vout[i].setnull();

        // let the others update at will
        for (unsigned int i = 0; i < txtmp.vin.size(); i++)
            if (i != nin)
                txtmp.vin[i].nsequence = 0;
    }

    // blank out other inputs completely, not recommended for open transactions
    if (nhashtype & sighash_anyonecanpay)
    {
        txtmp.vin[0] = txtmp.vin[nin];
        txtmp.vin.resize(1);
    }

    // serialize and hash
    chashwriter ss(ser_gethash, 0);
    ss << txtmp << nhashtype;
    return ss.gethash();
}

void static randomscript(cscript &script) {
    static const opcodetype oplist[] = {op_false, op_1, op_2, op_3, op_checksig, op_if, op_verif, op_return, op_codeseparator};
    script = cscript();
    int ops = (insecure_rand() % 10);
    for (int i=0; i<ops; i++)
        script << oplist[insecure_rand() % (sizeof(oplist)/sizeof(oplist[0]))];
}

void static randomtransaction(cmutabletransaction &tx, bool fsingle) {
    tx.nversion = insecure_rand();
    tx.vin.clear();
    tx.vout.clear();
    tx.nlocktime = (insecure_rand() % 2) ? insecure_rand() : 0;
    int ins = (insecure_rand() % 4) + 1;
    int outs = fsingle ? ins : (insecure_rand() % 4) + 1;
    for (int in = 0; in < ins; in++) {
        tx.vin.push_back(ctxin());
        ctxin &txin = tx.vin.back();
        txin.prevout.hash = getrandhash();
        txin.prevout.n = insecure_rand() % 4;
        randomscript(txin.scriptsig);
        txin.nsequence = (insecure_rand() % 2) ? insecure_rand() : (unsigned int)-1;
    }
    for (int out = 0; out < outs; out++) {
        tx.vout.push_back(ctxout());
        ctxout &txout = tx.vout.back();
        txout.nvalue = insecure_rand() % 100000000;
        randomscript(txout.scriptpubkey);
    }
}

boost_fixture_test_suite(sighash_tests, basictestingsetup)

boost_auto_test_case(sighash_test)
{
    seed_insecure_rand(false);

    #if defined(print_sighash_json)
    std::cout << "[\n";
    std::cout << "\t[\"raw_transaction, script, input_index, hashtype, signature_hash (result)\"],\n";
    #endif
    int nrandomtests = 50000;

    #if defined(print_sighash_json)
    nrandomtests = 500;
    #endif
    for (int i=0; i<nrandomtests; i++) {
        int nhashtype = insecure_rand();
        cmutabletransaction txto;
        randomtransaction(txto, (nhashtype & 0x1f) == sighash_single);
        cscript scriptcode;
        randomscript(scriptcode);
        int nin = insecure_rand() % txto.vin.size();

        uint256 sh, sho;
        sho = signaturehashold(scriptcode, txto, nin, nhashtype);
        sh = signaturehash(scriptcode, txto, nin, nhashtype);
        #if defined(print_sighash_json)
        cdatastream ss(ser_network, protocol_version);
        ss << txto;

        std::cout << "\t[\"" ;
        std::cout << hexstr(ss.begin(), ss.end()) << "\", \"";
        std::cout << hexstr(scriptcode) << "\", ";
        std::cout << nin << ", ";
        std::cout << nhashtype << ", \"";
        std::cout << sho.gethex() << "\"]";
        if (i+1 != nrandomtests) {
          std::cout << ",";
        }
        std::cout << "\n";
        #endif
        boost_check(sh == sho);
    }
    #if defined(print_sighash_json)
    std::cout << "]\n";
    #endif
}

// goal: check that signaturehash generates correct hash
boost_auto_test_case(sighash_from_data)
{
    univalue tests = read_json(std::string(json_tests::sighash, json_tests::sighash + sizeof(json_tests::sighash)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        std::string strtest = test.write();
        if (test.size() < 1) // allow for extra stuff (useful for comments)
        {
            boost_error("bad test: " << strtest);
            continue;
        }
        if (test.size() == 1) continue; // comment

        std::string raw_tx, raw_script, sighashhex;
        int nin, nhashtype;
        uint256 sh;
        ctransaction tx;
        cscript scriptcode = cscript();

        try {
          // deserialize test data
          raw_tx = test[0].get_str();
          raw_script = test[1].get_str();
          nin = test[2].get_int();
          nhashtype = test[3].get_int();
          sighashhex = test[4].get_str();

          uint256 sh;
          cdatastream stream(parsehex(raw_tx), ser_network, protocol_version);
          stream >> tx;

          cvalidationstate state;
          boost_check_message(checktransaction(tx, state), strtest);
          boost_check(state.isvalid());

          std::vector<unsigned char> raw = parsehex(raw_script);
          scriptcode.insert(scriptcode.end(), raw.begin(), raw.end());
        } catch (...) {
          boost_error("bad test, couldn't deserialize data: " << strtest);
          continue;
        }

        sh = signaturehash(scriptcode, tx, nin, nhashtype);
        boost_check_message(sh.gethex() == sighashhex, strtest);
    }
}
boost_auto_test_suite_end()
