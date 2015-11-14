// copyright (c) 2011-2015 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "policy/fees.h"
#include "txmempool.h"
#include "uint256.h"
#include "util.h"

#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(policyestimator_tests, basictestingsetup)

boost_auto_test_case(blockpolicyestimates)
{
    ctxmempool mpool(cfeerate(1000));
    camount basefee(2000);
    double basepri = 1e6;
    camount deltafee(100);
    double deltapri=5e5;
    std::vector<camount> feev[2];
    std::vector<double> priv[2];

    // populate vectors of increasing fees or priorities
    for (int j = 0; j < 10; j++) {
        //v[0] is for fee transactions
        feev[0].push_back(basefee * (j+1));
        priv[0].push_back(0);
        //v[1] is for priority transactions
        feev[1].push_back(camount(0));
        priv[1].push_back(basepri * pow(10, j+1));
    }

    // store the hashes of transactions that have been
    // added to the mempool by their associate fee/pri
    // txhashes[j] is populated with transactions either of
    // fee = basefee * (j+1)  or  pri = 10^6 * 10^(j+1)
    std::vector<uint256> txhashes[10];

    // create a transaction template
    cscript garbage;
    for (unsigned int i = 0; i < 128; i++)
        garbage.push_back('x');
    cmutabletransaction tx;
    std::list<ctransaction> dummyconflicted;
    tx.vin.resize(1);
    tx.vin[0].scriptsig = garbage;
    tx.vout.resize(1);
    tx.vout[0].nvalue=0ll;
    cfeerate baserate(basefee, ::getserializesize(tx, ser_network, protocol_version));

    // create a fake block
    std::vector<ctransaction> block;
    int blocknum = 0;

    // loop through 200 blocks
    // at a decay .998 and 4 fee transactions per block
    // this makes the tx count about 1.33 per bucket, above the 1 threshold
    while (blocknum < 200) {
        for (int j = 0; j < 10; j++) { // for each fee/pri multiple
            for (int k = 0; k < 5; k++) { // add 4 fee txs for every priority tx
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k; // make transaction unique
                uint256 hash = tx.gethash();
                mpool.addunchecked(hash, ctxmempoolentry(tx, feev[k/4][j], gettime(), priv[k/4][j], blocknum, mpool.hasnoinputsof(tx)));
                txhashes[j].push_back(hash);
            }
        }
        //create blocks where higher fee/pri txs are included more often
        for (int h = 0; h <= blocknum%10; h++) {
            // 10/10 blocks add highest fee/pri transactions
            // 9/10 blocks add 2nd highest and so on until ...
            // 1/10 blocks add lowest fee/pri transactions
            while (txhashes[9-h].size()) {
                ctransaction btx;
                if (mpool.lookup(txhashes[9-h].back(), btx))
                    block.push_back(btx);
                txhashes[9-h].pop_back();
            }
        }
        mpool.removeforblock(block, ++blocknum, dummyconflicted);
        block.clear();
        if (blocknum == 30) {
            // at this point we should need to combine 5 buckets to get enough data points
            // so estimatefee(1) should fail and estimatefee(2) should return somewhere around
            // 8*baserate
            boost_check(mpool.estimatefee(1) == cfeerate(0));
            boost_check(mpool.estimatefee(2).getfeeperk() < 8*baserate.getfeeperk() + deltafee);
            boost_check(mpool.estimatefee(2).getfeeperk() > 8*baserate.getfeeperk() - deltafee);
        }
    }

    std::vector<camount> origfeeest;
    std::vector<double> origpriest;
    // highest feerate is 10*baserate and gets in all blocks,
    // second highest feerate is 9*baserate and gets in 9/10 blocks = 90%,
    // third highest feerate is 8*base rate, and gets in 8/10 blocks = 80%,
    // so estimatefee(1) should return 9*baserate.
    // third highest feerate has 90% chance of being included by 2 blocks,
    // so estimatefee(2) should return 8*baserate etc...
    for (int i = 1; i < 10;i++) {
        origfeeest.push_back(mpool.estimatefee(i).getfeeperk());
        origpriest.push_back(mpool.estimatepriority(i));
        if (i > 1) { // fee estimates should be monotonically decreasing
            boost_check(origfeeest[i-1] <= origfeeest[i-2]);
            boost_check(origpriest[i-1] <= origpriest[i-2]);
        }
        boost_check(origfeeest[i-1] < (10-i)*baserate.getfeeperk() + deltafee);
        boost_check(origfeeest[i-1] > (10-i)*baserate.getfeeperk() - deltafee);
        boost_check(origpriest[i-1] < pow(10,10-i) * basepri + deltapri);
        boost_check(origpriest[i-1] > pow(10,10-i) * basepri - deltapri);
    }

    // mine 50 more blocks with no transactions happening, estimates shouldn't change
    // we haven't decayed the moving average enough so we still have enough data points in every bucket
    while (blocknum < 250)
        mpool.removeforblock(block, ++blocknum, dummyconflicted);

    for (int i = 1; i < 10;i++) {
        boost_check(mpool.estimatefee(i).getfeeperk() < origfeeest[i-1] + deltafee);
        boost_check(mpool.estimatefee(i).getfeeperk() > origfeeest[i-1] - deltafee);
        boost_check(mpool.estimatepriority(i) < origpriest[i-1] + deltapri);
        boost_check(mpool.estimatepriority(i) > origpriest[i-1] - deltapri);
    }


    // mine 15 more blocks with lots of transactions happening and not getting mined
    // estimates should go up
    while (blocknum < 265) {
        for (int j = 0; j < 10; j++) { // for each fee/pri multiple
            for (int k = 0; k < 5; k++) { // add 4 fee txs for every priority tx
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k;
                uint256 hash = tx.gethash();
                mpool.addunchecked(hash, ctxmempoolentry(tx, feev[k/4][j], gettime(), priv[k/4][j], blocknum, mpool.hasnoinputsof(tx)));
                txhashes[j].push_back(hash);
            }
        }
        mpool.removeforblock(block, ++blocknum, dummyconflicted);
    }

    for (int i = 1; i < 10;i++) {
        boost_check(mpool.estimatefee(i).getfeeperk() > origfeeest[i-1] - deltafee);
        boost_check(mpool.estimatepriority(i) > origpriest[i-1] - deltapri);
    }

    // mine all those transactions
    // estimates should still not be below original
    for (int j = 0; j < 10; j++) {
        while(txhashes[j].size()) {
            ctransaction btx;
            if (mpool.lookup(txhashes[j].back(), btx))
                block.push_back(btx);
            txhashes[j].pop_back();
        }
    }
    mpool.removeforblock(block, 265, dummyconflicted);
    block.clear();
    for (int i = 1; i < 10;i++) {
        boost_check(mpool.estimatefee(i).getfeeperk() > origfeeest[i-1] - deltafee);
        boost_check(mpool.estimatepriority(i) > origpriest[i-1] - deltapri);
    }

    // mine 100 more blocks where everything is mined every block
    // estimates should be below original estimates (not possible for last estimate)
    while (blocknum < 365) {
        for (int j = 0; j < 10; j++) { // for each fee/pri multiple
            for (int k = 0; k < 5; k++) { // add 4 fee txs for every priority tx
                tx.vin[0].prevout.n = 10000*blocknum+100*j+k;
                uint256 hash = tx.gethash();
                mpool.addunchecked(hash, ctxmempoolentry(tx, feev[k/4][j], gettime(), priv[k/4][j], blocknum, mpool.hasnoinputsof(tx)));
                ctransaction btx;
                if (mpool.lookup(hash, btx))
                    block.push_back(btx);
            }
        }
        mpool.removeforblock(block, ++blocknum, dummyconflicted);
        block.clear();
    }
    for (int i = 1; i < 9; i++) {
        boost_check(mpool.estimatefee(i).getfeeperk() < origfeeest[i-1] - deltafee);
        boost_check(mpool.estimatepriority(i) < origpriest[i-1] - deltapri);
    }
}

boost_auto_test_suite_end()
