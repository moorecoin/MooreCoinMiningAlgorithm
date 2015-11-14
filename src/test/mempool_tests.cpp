// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "txmempool.h"
#include "util.h"

#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>
#include <list>

boost_fixture_test_suite(mempool_tests, testingsetup)

boost_auto_test_case(mempoolremovetest)
{
    // test ctxmempool::remove functionality

    // parent transaction with three children,
    // and three grand-children:
    cmutabletransaction txparent;
    txparent.vin.resize(1);
    txparent.vin[0].scriptsig = cscript() << op_11;
    txparent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        txparent.vout[i].scriptpubkey = cscript() << op_11 << op_equal;
        txparent.vout[i].nvalue = 33000ll;
    }
    cmutabletransaction txchild[3];
    for (int i = 0; i < 3; i++)
    {
        txchild[i].vin.resize(1);
        txchild[i].vin[0].scriptsig = cscript() << op_11;
        txchild[i].vin[0].prevout.hash = txparent.gethash();
        txchild[i].vin[0].prevout.n = i;
        txchild[i].vout.resize(1);
        txchild[i].vout[0].scriptpubkey = cscript() << op_11 << op_equal;
        txchild[i].vout[0].nvalue = 11000ll;
    }
    cmutabletransaction txgrandchild[3];
    for (int i = 0; i < 3; i++)
    {
        txgrandchild[i].vin.resize(1);
        txgrandchild[i].vin[0].scriptsig = cscript() << op_11;
        txgrandchild[i].vin[0].prevout.hash = txchild[i].gethash();
        txgrandchild[i].vin[0].prevout.n = 0;
        txgrandchild[i].vout.resize(1);
        txgrandchild[i].vout[0].scriptpubkey = cscript() << op_11 << op_equal;
        txgrandchild[i].vout[0].nvalue = 11000ll;
    }


    ctxmempool testpool(cfeerate(0));
    std::list<ctransaction> removed;

    // nothing in pool, remove should do nothing:
    testpool.remove(txparent, removed, true);
    boost_check_equal(removed.size(), 0);

    // just the parent:
    testpool.addunchecked(txparent.gethash(), ctxmempoolentry(txparent, 0, 0, 0.0, 1));
    testpool.remove(txparent, removed, true);
    boost_check_equal(removed.size(), 1);
    removed.clear();
    
    // parent, children, grandchildren:
    testpool.addunchecked(txparent.gethash(), ctxmempoolentry(txparent, 0, 0, 0.0, 1));
    for (int i = 0; i < 3; i++)
    {
        testpool.addunchecked(txchild[i].gethash(), ctxmempoolentry(txchild[i], 0, 0, 0.0, 1));
        testpool.addunchecked(txgrandchild[i].gethash(), ctxmempoolentry(txgrandchild[i], 0, 0, 0.0, 1));
    }
    // remove child[0], grandchild[0] should be removed:
    testpool.remove(txchild[0], removed, true);
    boost_check_equal(removed.size(), 2);
    removed.clear();
    // ... make sure grandchild and child are gone:
    testpool.remove(txgrandchild[0], removed, true);
    boost_check_equal(removed.size(), 0);
    testpool.remove(txchild[0], removed, true);
    boost_check_equal(removed.size(), 0);
    // remove parent, all children/grandchildren should go:
    testpool.remove(txparent, removed, true);
    boost_check_equal(removed.size(), 5);
    boost_check_equal(testpool.size(), 0);
    removed.clear();

    // add children and grandchildren, but not the parent (simulate the parent being in a block)
    for (int i = 0; i < 3; i++)
    {
        testpool.addunchecked(txchild[i].gethash(), ctxmempoolentry(txchild[i], 0, 0, 0.0, 1));
        testpool.addunchecked(txgrandchild[i].gethash(), ctxmempoolentry(txgrandchild[i], 0, 0, 0.0, 1));
    }
    // now remove the parent, as might happen if a block-re-org occurs but the parent cannot be
    // put into the mempool (maybe because it is non-standard):
    testpool.remove(txparent, removed, true);
    boost_check_equal(removed.size(), 6);
    boost_check_equal(testpool.size(), 0);
    removed.clear();
}

boost_auto_test_suite_end()
