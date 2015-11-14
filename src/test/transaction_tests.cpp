// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "data/tx_invalid.json.h"
#include "data/tx_valid.json.h"
#include "test/test_moorecoin.h"

#include "clientversion.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "script/script.h"
#include "script/script_error.h"

#include <map>
#include <string>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>

#include "univalue/univalue.h"

using namespace std;

// in script_tests.cpp
extern univalue read_json(const std::string& jsondata);

static std::map<string, unsigned int> mapflagnames = boost::assign::map_list_of
    (string("none"), (unsigned int)script_verify_none)
    (string("p2sh"), (unsigned int)script_verify_p2sh)
    (string("strictenc"), (unsigned int)script_verify_strictenc)
    (string("dersig"), (unsigned int)script_verify_dersig)
    (string("low_s"), (unsigned int)script_verify_low_s)
    (string("sigpushonly"), (unsigned int)script_verify_sigpushonly)
    (string("minimaldata"), (unsigned int)script_verify_minimaldata)
    (string("nulldummy"), (unsigned int)script_verify_nulldummy)
    (string("discourage_upgradable_nops"), (unsigned int)script_verify_discourage_upgradable_nops)
    (string("cleanstack"), (unsigned int)script_verify_cleanstack);

unsigned int parsescriptflags(string strflags)
{
    if (strflags.empty()) {
        return 0;
    }
    unsigned int flags = 0;
    vector<string> words;
    boost::algorithm::split(words, strflags, boost::algorithm::is_any_of(","));

    boost_foreach(string word, words)
    {
        if (!mapflagnames.count(word))
            boost_error("bad test: unknown verification flag '" << word << "'");
        flags |= mapflagnames[word];
    }

    return flags;
}

string formatscriptflags(unsigned int flags)
{
    if (flags == 0) {
        return "";
    }
    string ret;
    std::map<string, unsigned int>::const_iterator it = mapflagnames.begin();
    while (it != mapflagnames.end()) {
        if (flags & it->second) {
            ret += it->first + ",";
        }
        it++;
    }
    return ret.substr(0, ret.size() - 1);
}

boost_fixture_test_suite(transaction_tests, basictestingsetup)

boost_auto_test_case(tx_valid)
{
    // read tests from test/data/tx_valid.json
    // format is an array of arrays
    // inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptpubkey], [input 2], ...],"], serializedtransaction, verifyflags
    // ... where all scripts are stringified scripts.
    //
    // verifyflags is a comma separated list of script verification flags to apply, or "none"
    univalue tests = read_json(std::string(json_tests::tx_valid, json_tests::tx_valid + sizeof(json_tests::tx_valid)));

    scripterror err;
    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        string strtest = test.write();
        if (test[0].isarray())
        {
            if (test.size() != 3 || !test[1].isstr() || !test[2].isstr())
            {
                boost_error("bad test: " << strtest);
                continue;
            }

            map<coutpoint, cscript> mapprevoutscriptpubkeys;
            univalue inputs = test[0].get_array();
            bool fvalid = true;
	    for (unsigned int inpidx = 0; inpidx < inputs.size(); inpidx++) {
	        const univalue& input = inputs[inpidx];
                if (!input.isarray())
                {
                    fvalid = false;
                    break;
                }
                univalue vinput = input.get_array();
                if (vinput.size() != 3)
                {
                    fvalid = false;
                    break;
                }

                mapprevoutscriptpubkeys[coutpoint(uint256s(vinput[0].get_str()), vinput[1].get_int())] = parsescript(vinput[2].get_str());
            }
            if (!fvalid)
            {
                boost_error("bad test: " << strtest);
                continue;
            }

            string transaction = test[1].get_str();
            cdatastream stream(parsehex(transaction), ser_network, protocol_version);
            ctransaction tx;
            stream >> tx;

            cvalidationstate state;
            boost_check_message(checktransaction(tx, state), strtest);
            boost_check(state.isvalid());

            for (unsigned int i = 0; i < tx.vin.size(); i++)
            {
                if (!mapprevoutscriptpubkeys.count(tx.vin[i].prevout))
                {
                    boost_error("bad test: " << strtest);
                    break;
                }

                unsigned int verify_flags = parsescriptflags(test[2].get_str());
                boost_check_message(verifyscript(tx.vin[i].scriptsig, mapprevoutscriptpubkeys[tx.vin[i].prevout],
                                                 verify_flags, transactionsignaturechecker(&tx, i), &err),
                                    strtest);
                boost_check_message(err == script_err_ok, scripterrorstring(err));
            }
        }
    }
}

boost_auto_test_case(tx_invalid)
{
    // read tests from test/data/tx_invalid.json
    // format is an array of arrays
    // inner arrays are either [ "comment" ]
    // or [[[prevout hash, prevout index, prevout scriptpubkey], [input 2], ...],"], serializedtransaction, verifyflags
    // ... where all scripts are stringified scripts.
    //
    // verifyflags is a comma separated list of script verification flags to apply, or "none"
    univalue tests = read_json(std::string(json_tests::tx_invalid, json_tests::tx_invalid + sizeof(json_tests::tx_invalid)));

    scripterror err;
    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        string strtest = test.write();
        if (test[0].isarray())
        {
            if (test.size() != 3 || !test[1].isstr() || !test[2].isstr())
            {
                boost_error("bad test: " << strtest);
                continue;
            }

            map<coutpoint, cscript> mapprevoutscriptpubkeys;
            univalue inputs = test[0].get_array();
            bool fvalid = true;
	    for (unsigned int inpidx = 0; inpidx < inputs.size(); inpidx++) {
	        const univalue& input = inputs[inpidx];
                if (!input.isarray())
                {
                    fvalid = false;
                    break;
                }
                univalue vinput = input.get_array();
                if (vinput.size() != 3)
                {
                    fvalid = false;
                    break;
                }

                mapprevoutscriptpubkeys[coutpoint(uint256s(vinput[0].get_str()), vinput[1].get_int())] = parsescript(vinput[2].get_str());
            }
            if (!fvalid)
            {
                boost_error("bad test: " << strtest);
                continue;
            }

            string transaction = test[1].get_str();
            cdatastream stream(parsehex(transaction), ser_network, protocol_version);
            ctransaction tx;
            stream >> tx;

            cvalidationstate state;
            fvalid = checktransaction(tx, state) && state.isvalid();

            for (unsigned int i = 0; i < tx.vin.size() && fvalid; i++)
            {
                if (!mapprevoutscriptpubkeys.count(tx.vin[i].prevout))
                {
                    boost_error("bad test: " << strtest);
                    break;
                }

                unsigned int verify_flags = parsescriptflags(test[2].get_str());
                fvalid = verifyscript(tx.vin[i].scriptsig, mapprevoutscriptpubkeys[tx.vin[i].prevout],
                                      verify_flags, transactionsignaturechecker(&tx, i), &err);
            }
            boost_check_message(!fvalid, strtest);
            boost_check_message(err != script_err_ok, scripterrorstring(err));
        }
    }
}

boost_auto_test_case(basic_transaction_tests)
{
    // random real transaction (e2769b09e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436)
    unsigned char ch[] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x6b, 0xff, 0x7f, 0xcd, 0x4f, 0x85, 0x65, 0xef, 0x40, 0x6d, 0xd5, 0xd6, 0x3d, 0x4f, 0xf9, 0x4f, 0x31, 0x8f, 0xe8, 0x20, 0x27, 0xfd, 0x4d, 0xc4, 0x51, 0xb0, 0x44, 0x74, 0x01, 0x9f, 0x74, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x49, 0x30, 0x46, 0x02, 0x21, 0x00, 0xda, 0x0d, 0xc6, 0xae, 0xce, 0xfe, 0x1e, 0x06, 0xef, 0xdf, 0x05, 0x77, 0x37, 0x57, 0xde, 0xb1, 0x68, 0x82, 0x09, 0x30, 0xe3, 0xb0, 0xd0, 0x3f, 0x46, 0xf5, 0xfc, 0xf1, 0x50, 0xbf, 0x99, 0x0c, 0x02, 0x21, 0x00, 0xd2, 0x5b, 0x5c, 0x87, 0x04, 0x00, 0x76, 0xe4, 0xf2, 0x53, 0xf8, 0x26, 0x2e, 0x76, 0x3e, 0x2d, 0xd5, 0x1e, 0x7f, 0xf0, 0xbe, 0x15, 0x77, 0x27, 0xc4, 0xbc, 0x42, 0x80, 0x7f, 0x17, 0xbd, 0x39, 0x01, 0x41, 0x04, 0xe6, 0xc2, 0x6e, 0xf6, 0x7d, 0xc6, 0x10, 0xd2, 0xcd, 0x19, 0x24, 0x84, 0x78, 0x9a, 0x6c, 0xf9, 0xae, 0xa9, 0x93, 0x0b, 0x94, 0x4b, 0x7e, 0x2d, 0xb5, 0x34, 0x2b, 0x9d, 0x9e, 0x5b, 0x9f, 0xf7, 0x9a, 0xff, 0x9a, 0x2e, 0xe1, 0x97, 0x8d, 0xd7, 0xfd, 0x01, 0xdf, 0xc5, 0x22, 0xee, 0x02, 0x28, 0x3d, 0x3b, 0x06, 0xa9, 0xd0, 0x3a, 0xcf, 0x80, 0x96, 0x96, 0x8d, 0x7d, 0xbb, 0x0f, 0x91, 0x78, 0xff, 0xff, 0xff, 0xff, 0x02, 0x8b, 0xa7, 0x94, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xba, 0xde, 0xec, 0xfd, 0xef, 0x05, 0x07, 0x24, 0x7f, 0xc8, 0xf7, 0x42, 0x41, 0xd7, 0x3b, 0xc0, 0x39, 0x97, 0x2d, 0x7b, 0x88, 0xac, 0x40, 0x94, 0xa8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xc1, 0x09, 0x32, 0x48, 0x3f, 0xec, 0x93, 0xed, 0x51, 0xf5, 0xfe, 0x95, 0xe7, 0x25, 0x59, 0xf2, 0xcc, 0x70, 0x43, 0xf9, 0x88, 0xac, 0x00, 0x00, 0x00, 0x00, 0x00};
    vector<unsigned char> vch(ch, ch + sizeof(ch) -1);
    cdatastream stream(vch, ser_disk, client_version);
    cmutabletransaction tx;
    stream >> tx;
    cvalidationstate state;
    boost_check_message(checktransaction(tx, state) && state.isvalid(), "simple deserialized transaction should be valid.");

    // check that duplicate txins fail
    tx.vin.push_back(tx.vin[0]);
    boost_check_message(!checktransaction(tx, state) || !state.isvalid(), "transaction with duplicate txins should be invalid.");
}

//
// helper: create two dummy transactions, each with
// two outputs.  the first has 11 and 50 cent outputs
// paid to a tx_pubkey, the second 21 and 22 cent outputs
// paid to a tx_pubkeyhash.
//
static std::vector<cmutabletransaction>
setupdummyinputs(cbasickeystore& keystoreret, ccoinsviewcache& coinsret)
{
    std::vector<cmutabletransaction> dummytransactions;
    dummytransactions.resize(2);

    // add some keys to the keystore:
    ckey key[4];
    for (int i = 0; i < 4; i++)
    {
        key[i].makenewkey(i % 2);
        keystoreret.addkey(key[i]);
    }

    // create some dummy input transactions
    dummytransactions[0].vout.resize(2);
    dummytransactions[0].vout[0].nvalue = 11*cent;
    dummytransactions[0].vout[0].scriptpubkey << tobytevector(key[0].getpubkey()) << op_checksig;
    dummytransactions[0].vout[1].nvalue = 50*cent;
    dummytransactions[0].vout[1].scriptpubkey << tobytevector(key[1].getpubkey()) << op_checksig;
    coinsret.modifycoins(dummytransactions[0].gethash())->fromtx(dummytransactions[0], 0);

    dummytransactions[1].vout.resize(2);
    dummytransactions[1].vout[0].nvalue = 21*cent;
    dummytransactions[1].vout[0].scriptpubkey = getscriptfordestination(key[2].getpubkey().getid());
    dummytransactions[1].vout[1].nvalue = 22*cent;
    dummytransactions[1].vout[1].scriptpubkey = getscriptfordestination(key[3].getpubkey().getid());
    coinsret.modifycoins(dummytransactions[1].gethash())->fromtx(dummytransactions[1], 0);

    return dummytransactions;
}

boost_auto_test_case(test_get)
{
    cbasickeystore keystore;
    ccoinsview coinsdummy;
    ccoinsviewcache coins(&coinsdummy);
    std::vector<cmutabletransaction> dummytransactions = setupdummyinputs(keystore, coins);

    cmutabletransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout.hash = dummytransactions[0].gethash();
    t1.vin[0].prevout.n = 1;
    t1.vin[0].scriptsig << std::vector<unsigned char>(65, 0);
    t1.vin[1].prevout.hash = dummytransactions[1].gethash();
    t1.vin[1].prevout.n = 0;
    t1.vin[1].scriptsig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vin[2].prevout.hash = dummytransactions[1].gethash();
    t1.vin[2].prevout.n = 1;
    t1.vin[2].scriptsig << std::vector<unsigned char>(65, 0) << std::vector<unsigned char>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nvalue = 90*cent;
    t1.vout[0].scriptpubkey << op_1;

    boost_check(areinputsstandard(t1, coins));
    boost_check_equal(coins.getvaluein(t1), (50+21+22)*cent);

    // adding extra junk to the scriptsig should make it non-standard:
    t1.vin[0].scriptsig << op_11;
    boost_check(!areinputsstandard(t1, coins));

    // ... as should not having enough:
    t1.vin[0].scriptsig = cscript();
    boost_check(!areinputsstandard(t1, coins));
}

boost_auto_test_case(test_isstandard)
{
    lock(cs_main);
    cbasickeystore keystore;
    ccoinsview coinsdummy;
    ccoinsviewcache coins(&coinsdummy);
    std::vector<cmutabletransaction> dummytransactions = setupdummyinputs(keystore, coins);

    cmutabletransaction t;
    t.vin.resize(1);
    t.vin[0].prevout.hash = dummytransactions[0].gethash();
    t.vin[0].prevout.n = 1;
    t.vin[0].scriptsig << std::vector<unsigned char>(65, 0);
    t.vout.resize(1);
    t.vout[0].nvalue = 90*cent;
    ckey key;
    key.makenewkey(true);
    t.vout[0].scriptpubkey = getscriptfordestination(key.getpubkey().getid());

    string reason;
    boost_check(isstandardtx(t, reason));

    t.vout[0].nvalue = 501; // dust
    boost_check(!isstandardtx(t, reason));

    t.vout[0].nvalue = 601; // not dust
    boost_check(isstandardtx(t, reason));

    t.vout[0].scriptpubkey = cscript() << op_1;
    boost_check(!isstandardtx(t, reason));

    // 80-byte tx_null_data (standard)
    t.vout[0].scriptpubkey = cscript() << op_return << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    boost_check(isstandardtx(t, reason));

    // 81-byte tx_null_data (non-standard)
    t.vout[0].scriptpubkey = cscript() << op_return << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3804678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef3800");
    boost_check(!isstandardtx(t, reason));

    // tx_null_data w/o pushdata
    t.vout.resize(1);
    t.vout[0].scriptpubkey = cscript() << op_return;
    boost_check(isstandardtx(t, reason));

    // only one tx_null_data permitted in all cases
    t.vout.resize(2);
    t.vout[0].scriptpubkey = cscript() << op_return << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptpubkey = cscript() << op_return << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    boost_check(!isstandardtx(t, reason));

    t.vout[0].scriptpubkey = cscript() << op_return << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
    t.vout[1].scriptpubkey = cscript() << op_return;
    boost_check(!isstandardtx(t, reason));

    t.vout[0].scriptpubkey = cscript() << op_return;
    t.vout[1].scriptpubkey = cscript() << op_return;
    boost_check(!isstandardtx(t, reason));
}

boost_auto_test_suite_end()
