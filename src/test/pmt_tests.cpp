// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "merkleblock.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "version.h"
#include "random.h"
#include "test/test_moorecoin.h"

#include <vector>

#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

class cpartialmerkletreetester : public cpartialmerkletree
{
public:
    // flip one bit in one of the hashes - this should break the authentication
    void damage() {
        unsigned int n = insecure_rand() % vhash.size();
        int bit = insecure_rand() % 256;
        *(vhash[n].begin() + (bit>>3)) ^= 1<<(bit&7);
    }
};

boost_fixture_test_suite(pmt_tests, basictestingsetup)

boost_auto_test_case(pmt_test1)
{
    seed_insecure_rand(false);
    static const unsigned int ntxcounts[] = {1, 4, 7, 17, 56, 100, 127, 256, 312, 513, 1000, 4095};

    for (int n = 0; n < 12; n++) {
        unsigned int ntx = ntxcounts[n];

        // build a block with some dummy transactions
        cblock block;
        for (unsigned int j=0; j<ntx; j++) {
            cmutabletransaction tx;
            tx.nlocktime = j; // actual transaction data doesn't matter; just make the nlocktime's unique
            block.vtx.push_back(ctransaction(tx));
        }

        // calculate actual merkle root and height
        uint256 merkleroot1 = block.buildmerkletree();
        std::vector<uint256> vtxid(ntx, uint256());
        for (unsigned int j=0; j<ntx; j++)
            vtxid[j] = block.vtx[j].gethash();
        int nheight = 1, ntx_ = ntx;
        while (ntx_ > 1) {
            ntx_ = (ntx_+1)/2;
            nheight++;
        }

        // check with random subsets with inclusion chances 1, 1/2, 1/4, ..., 1/128
        for (int att = 1; att < 15; att++) {
            // build random subset of txid's
            std::vector<bool> vmatch(ntx, false);
            std::vector<uint256> vmatchtxid1;
            for (unsigned int j=0; j<ntx; j++) {
                bool finclude = (insecure_rand() & ((1 << (att/2)) - 1)) == 0;
                vmatch[j] = finclude;
                if (finclude)
                    vmatchtxid1.push_back(vtxid[j]);
            }

            // build the partial merkle tree
            cpartialmerkletree pmt1(vtxid, vmatch);

            // serialize
            cdatastream ss(ser_network, protocol_version);
            ss << pmt1;

            // verify cpartialmerkletree's size guarantees
            unsigned int n = std::min<unsigned int>(ntx, 1 + vmatchtxid1.size()*nheight);
            boost_check(ss.size() <= 10 + (258*n+7)/8);

            // deserialize into a tester copy
            cpartialmerkletreetester pmt2;
            ss >> pmt2;

            // extract merkle root and matched txids from copy
            std::vector<uint256> vmatchtxid2;
            uint256 merkleroot2 = pmt2.extractmatches(vmatchtxid2);

            // check that it has the same merkle root as the original, and a valid one
            boost_check(merkleroot1 == merkleroot2);
            boost_check(!merkleroot2.isnull());

            // check that it contains the matched transactions (in the same order!)
            boost_check(vmatchtxid1 == vmatchtxid2);

            // check that random bit flips break the authentication
            for (int j=0; j<4; j++) {
                cpartialmerkletreetester pmt3(pmt2);
                pmt3.damage();
                std::vector<uint256> vmatchtxid3;
                uint256 merkleroot3 = pmt3.extractmatches(vmatchtxid3);
                boost_check(merkleroot3 != merkleroot1);
            }
        }
    }
}

boost_auto_test_case(pmt_malleability)
{
    std::vector<uint256> vtxid = boost::assign::list_of
        (arithtouint256(1))(arithtouint256(2))
        (arithtouint256(3))(arithtouint256(4))
        (arithtouint256(5))(arithtouint256(6))
        (arithtouint256(7))(arithtouint256(8))
        (arithtouint256(9))(arithtouint256(10))
        (arithtouint256(9))(arithtouint256(10));
    std::vector<bool> vmatch = boost::assign::list_of(false)(false)(false)(false)(false)(false)(false)(false)(false)(true)(true)(false);

    cpartialmerkletree tree(vtxid, vmatch);
    std::vector<uint256> vtxid2;
    boost_check(tree.extractmatches(vtxid).isnull());
}

boost_auto_test_suite_end()
