// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"

#include "data/base58_encode_decode.json.h"
#include "data/base58_keys_invalid.json.h"
#include "data/base58_keys_valid.json.h"

#include "key.h"
#include "script/script.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_moorecoin.h"

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#include "univalue/univalue.h"

extern univalue read_json(const std::string& jsondata);

boost_fixture_test_suite(base58_tests, basictestingsetup)

// goal: test low-level base58 encoding functionality
boost_auto_test_case(base58_encodebase58)
{
    univalue tests = read_json(std::string(json_tests::base58_encode_decode, json_tests::base58_encode_decode + sizeof(json_tests::base58_encode_decode)));
    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        std::string strtest = test.write();
        if (test.size() < 2) // allow for extra stuff (useful for comments)
        {
            boost_error("bad test: " << strtest);
            continue;
        }
        std::vector<unsigned char> sourcedata = parsehex(test[0].get_str());
        std::string base58string = test[1].get_str();
        boost_check_message(
                    encodebase58(begin_ptr(sourcedata), end_ptr(sourcedata)) == base58string,
                    strtest);
    }
}

// goal: test low-level base58 decoding functionality
boost_auto_test_case(base58_decodebase58)
{
    univalue tests = read_json(std::string(json_tests::base58_encode_decode, json_tests::base58_encode_decode + sizeof(json_tests::base58_encode_decode)));
    std::vector<unsigned char> result;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        std::string strtest = test.write();
        if (test.size() < 2) // allow for extra stuff (useful for comments)
        {
            boost_error("bad test: " << strtest);
            continue;
        }
        std::vector<unsigned char> expected = parsehex(test[0].get_str());
        std::string base58string = test[1].get_str();
        boost_check_message(decodebase58(base58string, result), strtest);
        boost_check_message(result.size() == expected.size() && std::equal(result.begin(), result.end(), expected.begin()), strtest);
    }

    boost_check(!decodebase58("invalid", result));

    // check that decodebase58 skips whitespace, but still fails with unexpected non-whitespace at the end.
    boost_check(!decodebase58(" \t\n\v\f\r skip \r\f\v\n\t a", result));
    boost_check( decodebase58(" \t\n\v\f\r skip \r\f\v\n\t ", result));
    std::vector<unsigned char> expected = parsehex("971a55");
    boost_check_equal_collections(result.begin(), result.end(), expected.begin(), expected.end());
}

// visitor to check address type
class testaddrtypevisitor : public boost::static_visitor<bool>
{
private:
    std::string exp_addrtype;
public:
    testaddrtypevisitor(const std::string &exp_addrtype) : exp_addrtype(exp_addrtype) { }
    bool operator()(const ckeyid &id) const
    {
        return (exp_addrtype == "pubkey");
    }
    bool operator()(const cscriptid &id) const
    {
        return (exp_addrtype == "script");
    }
    bool operator()(const cnodestination &no) const
    {
        return (exp_addrtype == "none");
    }
};

// visitor to check address payload
class testpayloadvisitor : public boost::static_visitor<bool>
{
private:
    std::vector<unsigned char> exp_payload;
public:
    testpayloadvisitor(std::vector<unsigned char> &exp_payload) : exp_payload(exp_payload) { }
    bool operator()(const ckeyid &id) const
    {
        uint160 exp_key(exp_payload);
        return exp_key == id;
    }
    bool operator()(const cscriptid &id) const
    {
        uint160 exp_key(exp_payload);
        return exp_key == id;
    }
    bool operator()(const cnodestination &no) const
    {
        return exp_payload.size() == 0;
    }
};

// goal: check that parsed keys match test payload
boost_auto_test_case(base58_keys_valid_parse)
{
    univalue tests = read_json(std::string(json_tests::base58_keys_valid, json_tests::base58_keys_valid + sizeof(json_tests::base58_keys_valid)));
    std::vector<unsigned char> result;
    cmoorecoinsecret secret;
    cmoorecoinaddress addr;
    selectparams(cbasechainparams::main);

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        std::string strtest = test.write();
        if (test.size() < 3) // allow for extra stuff (useful for comments)
        {
            boost_error("bad test: " << strtest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<unsigned char> exp_payload = parsehex(test[1].get_str());
        const univalue &metadata = test[2].get_obj();
        bool isprivkey = find_value(metadata, "isprivkey").get_bool();
        bool istestnet = find_value(metadata, "istestnet").get_bool();
        if (istestnet)
            selectparams(cbasechainparams::testnet);
        else
            selectparams(cbasechainparams::main);
        if(isprivkey)
        {
            bool iscompressed = find_value(metadata, "iscompressed").get_bool();
            // must be valid private key
            // note: cmoorecoinsecret::setstring tests isvalid, whereas cmoorecoinaddress does not!
            boost_check_message(secret.setstring(exp_base58string), "!setstring:"+ strtest);
            boost_check_message(secret.isvalid(), "!isvalid:" + strtest);
            ckey privkey = secret.getkey();
            boost_check_message(privkey.iscompressed() == iscompressed, "compressed mismatch:" + strtest);
            boost_check_message(privkey.size() == exp_payload.size() && std::equal(privkey.begin(), privkey.end(), exp_payload.begin()), "key mismatch:" + strtest);

            // private key must be invalid public key
            addr.setstring(exp_base58string);
            boost_check_message(!addr.isvalid(), "isvalid privkey as pubkey:" + strtest);
        }
        else
        {
            std::string exp_addrtype = find_value(metadata, "addrtype").get_str(); // "script" or "pubkey"
            // must be valid public key
            boost_check_message(addr.setstring(exp_base58string), "setstring:" + strtest);
            boost_check_message(addr.isvalid(), "!isvalid:" + strtest);
            boost_check_message(addr.isscript() == (exp_addrtype == "script"), "isscript mismatch" + strtest);
            ctxdestination dest = addr.get();
            boost_check_message(boost::apply_visitor(testaddrtypevisitor(exp_addrtype), dest), "addrtype mismatch" + strtest);

            // public key must be invalid private key
            secret.setstring(exp_base58string);
            boost_check_message(!secret.isvalid(), "isvalid pubkey as privkey:" + strtest);
        }
    }
}

// goal: check that generated keys match test vectors
boost_auto_test_case(base58_keys_valid_gen)
{
    univalue tests = read_json(std::string(json_tests::base58_keys_valid, json_tests::base58_keys_valid + sizeof(json_tests::base58_keys_valid)));
    std::vector<unsigned char> result;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        std::string strtest = test.write();
        if (test.size() < 3) // allow for extra stuff (useful for comments)
        {
            boost_error("bad test: " << strtest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<unsigned char> exp_payload = parsehex(test[1].get_str());
        const univalue &metadata = test[2].get_obj();
        bool isprivkey = find_value(metadata, "isprivkey").get_bool();
        bool istestnet = find_value(metadata, "istestnet").get_bool();
        if (istestnet)
            selectparams(cbasechainparams::testnet);
        else
            selectparams(cbasechainparams::main);
        if(isprivkey)
        {
            bool iscompressed = find_value(metadata, "iscompressed").get_bool();
            ckey key;
            key.set(exp_payload.begin(), exp_payload.end(), iscompressed);
            assert(key.isvalid());
            cmoorecoinsecret secret;
            secret.setkey(key);
            boost_check_message(secret.tostring() == exp_base58string, "result mismatch: " + strtest);
        }
        else
        {
            std::string exp_addrtype = find_value(metadata, "addrtype").get_str();
            ctxdestination dest;
            if(exp_addrtype == "pubkey")
            {
                dest = ckeyid(uint160(exp_payload));
            }
            else if(exp_addrtype == "script")
            {
                dest = cscriptid(uint160(exp_payload));
            }
            else if(exp_addrtype == "none")
            {
                dest = cnodestination();
            }
            else
            {
                boost_error("bad addrtype: " << strtest);
                continue;
            }
            cmoorecoinaddress addrout;
            boost_check_message(addrout.set(dest), "encode dest: " + strtest);
            boost_check_message(addrout.tostring() == exp_base58string, "mismatch: " + strtest);
        }
    }

    // visiting a cnodestination must fail
    cmoorecoinaddress dummyaddr;
    ctxdestination nodest = cnodestination();
    boost_check(!dummyaddr.set(nodest));

    selectparams(cbasechainparams::main);
}

// goal: check that base58 parsing code is robust against a variety of corrupted data
boost_auto_test_case(base58_keys_invalid)
{
    univalue tests = read_json(std::string(json_tests::base58_keys_invalid, json_tests::base58_keys_invalid + sizeof(json_tests::base58_keys_invalid))); // negative testcases
    std::vector<unsigned char> result;
    cmoorecoinsecret secret;
    cmoorecoinaddress addr;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        univalue test = tests[idx];
        std::string strtest = test.write();
        if (test.size() < 1) // allow for extra stuff (useful for comments)
        {
            boost_error("bad test: " << strtest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();

        // must be invalid as public and as private key
        addr.setstring(exp_base58string);
        boost_check_message(!addr.isvalid(), "isvalid pubkey:" + strtest);
        secret.setstring(exp_base58string);
        boost_check_message(!secret.isvalid(), "isvalid privkey:" + strtest);
    }
}


boost_auto_test_suite_end()

