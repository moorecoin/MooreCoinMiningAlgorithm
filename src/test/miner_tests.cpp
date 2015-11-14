// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/validation.h"
#include "main.h"
#include "miner.h"
#include "pubkey.h"
#include "uint256.h"
#include "util.h"

#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(miner_tests, testingsetup)

static
struct {
    unsigned char extranonce;
    unsigned int nonce;
} blockinfo[] = {
    {4, 0xa4a3e223}, {2, 0x15c32f9e}, {1, 0x0375b547}, {1, 0x7004a8a5},
    {2, 0xce440296}, {2, 0x52cfe198}, {1, 0x77a72cd0}, {2, 0xbb5d6f84},
    {2, 0x83f30c2c}, {1, 0x48a73d5b}, {1, 0xef7dcd01}, {2, 0x6809c6c4},
    {2, 0x0883ab3c}, {1, 0x087bbbe2}, {2, 0x2104a814}, {2, 0xdffb6daa},
    {1, 0xee8a0a08}, {2, 0xba4237c1}, {1, 0xa70349dc}, {1, 0x344722bb},
    {3, 0xd6294733}, {2, 0xec9f5c94}, {2, 0xca2fbc28}, {1, 0x6ba4f406},
    {2, 0x015d4532}, {1, 0x6e119b7c}, {2, 0x43e8f314}, {2, 0x27962f38},
    {2, 0xb571b51b}, {2, 0xb36bee23}, {2, 0xd17924a8}, {2, 0x6bc212d9},
    {1, 0x630d4948}, {2, 0x9a4c4ebb}, {2, 0x554be537}, {1, 0xd63ddfc7},
    {2, 0xa10acc11}, {1, 0x759a8363}, {2, 0xfb73090d}, {1, 0xe82c6a34},
    {1, 0xe33e92d7}, {3, 0x658ef5cb}, {2, 0xba32ff22}, {5, 0x0227a10c},
    {1, 0xa9a70155}, {5, 0xd096d809}, {1, 0x37176174}, {1, 0x830b8d0f},
    {1, 0xc6e3910e}, {2, 0x823f3ca8}, {1, 0x99850849}, {1, 0x7521fb81},
    {1, 0xaacaabab}, {1, 0xd645a2eb}, {5, 0x7aea1781}, {5, 0x9d6e4b78},
    {1, 0x4ce90fd8}, {1, 0xabdc832d}, {6, 0x4a34f32a}, {2, 0xf2524c1c},
    {2, 0x1bbeb08a}, {1, 0xad47f480}, {1, 0x9f026aeb}, {1, 0x15a95049},
    {2, 0xd1cb95b2}, {2, 0xf84bbda5}, {1, 0x0fa62cd1}, {1, 0xe05f9169},
    {1, 0x78d194a9}, {5, 0x3e38147b}, {5, 0x737ba0d4}, {1, 0x63378e10},
    {1, 0x6d5f91cf}, {2, 0x88612eb8}, {2, 0xe9639484}, {1, 0xb7fabc9d},
    {2, 0x19b01592}, {1, 0x5a90dd31}, {2, 0x5bd7e028}, {2, 0x94d00323},
    {1, 0xa9b9c01a}, {1, 0x3a40de61}, {1, 0x56e7eec7}, {5, 0x859f7ef6},
    {1, 0xfd8e5630}, {1, 0x2b0c9f7f}, {1, 0xba700e26}, {1, 0x7170a408},
    {1, 0x70de86a8}, {1, 0x74d64cd5}, {1, 0x49e738a1}, {2, 0x6910b602},
    {0, 0x643c565f}, {1, 0x54264b3f}, {2, 0x97ea6396}, {2, 0x55174459},
    {2, 0x03e8779a}, {1, 0x98f34d8f}, {1, 0xc07b2b07}, {1, 0xdfe29668},
    {1, 0x3141c7c1}, {1, 0xb3b595f4}, {1, 0x735abf08}, {5, 0x623bfbce},
    {2, 0xd351e722}, {1, 0xf4ca48c9}, {1, 0x5b19c670}, {1, 0xa164bf0e},
    {2, 0xbbbeb305}, {2, 0xfe1c810a},
};

// note: these tests rely on createnewblock doing its own self-validation!
boost_auto_test_case(createnewblock_validity)
{
    cscript scriptpubkey = cscript() << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << op_checksig;
    cblocktemplate *pblocktemplate;
    cmutabletransaction tx,tx2;
    cscript script;
    uint256 hash;

    lock(cs_main);
    fcheckpointsenabled = false;

    // simple block creation, nothing special yet:
    boost_check(pblocktemplate = createnewblock(scriptpubkey));

    // we can't make transactions until we have inputs
    // therefore, load 100 blocks :)
    std::vector<ctransaction*>txfirst;
    for (unsigned int i = 0; i < sizeof(blockinfo)/sizeof(*blockinfo); ++i)
    {
        cblock *pblock = &pblocktemplate->block; // pointer for convenience
        pblock->nversion = 1;
        pblock->ntime = chainactive.tip()->getmediantimepast()+1;
        cmutabletransaction tmoorecoinbase(pblock->vtx[0]);
        tmoorecoinbase.nversion = 1;
        tmoorecoinbase.vin[0].scriptsig = cscript();
        tmoorecoinbase.vin[0].scriptsig.push_back(blockinfo[i].extranonce);
        tmoorecoinbase.vin[0].scriptsig.push_back(chainactive.height());
        tmoorecoinbase.vout[0].scriptpubkey = cscript();
        pblock->vtx[0] = ctransaction(tmoorecoinbase);
        if (txfirst.size() < 2)
            txfirst.push_back(new ctransaction(pblock->vtx[0]));
        pblock->hashmerkleroot = pblock->buildmerkletree();
        pblock->nnonce = blockinfo[i].nonce;
        cvalidationstate state;
        boost_check(processnewblock(state, null, pblock, true, null));
        boost_check(state.isvalid());
        pblock->hashprevblock = pblock->gethash();
    }
    delete pblocktemplate;

    // just to make sure we can still make simple blocks
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;

    // block sigops > limit: 1000 checkmultisig + 1
    tx.vin.resize(1);
    // note: op_nop is used to force 20 sigops for the checkmultisig
    tx.vin[0].scriptsig = cscript() << op_0 << op_0 << op_0 << op_nop << op_checkmultisig << op_1;
    tx.vin[0].prevout.hash = txfirst[0]->gethash();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].nvalue = 5000000000ll;
    for (unsigned int i = 0; i < 1001; ++i)
    {
        tx.vout[0].nvalue -= 1000000;
        hash = tx.gethash();
        mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
        tx.vin[0].prevout.hash = hash;
    }
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // block size > limit
    tx.vin[0].scriptsig = cscript();
    // 18 * (520char + drop) + op_1 = 9433 bytes
    std::vector<unsigned char> vchdata(520);
    for (unsigned int i = 0; i < 18; ++i)
        tx.vin[0].scriptsig << vchdata << op_drop;
    tx.vin[0].scriptsig << op_1;
    tx.vin[0].prevout.hash = txfirst[0]->gethash();
    tx.vout[0].nvalue = 5000000000ll;
    for (unsigned int i = 0; i < 128; ++i)
    {
        tx.vout[0].nvalue -= 10000000;
        hash = tx.gethash();
        mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
        tx.vin[0].prevout.hash = hash;
    }
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // orphan in mempool
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // child with higher priority than parent
    tx.vin[0].scriptsig = cscript() << op_1;
    tx.vin[0].prevout.hash = txfirst[1]->gethash();
    tx.vout[0].nvalue = 4900000000ll;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    tx.vin[0].prevout.hash = hash;
    tx.vin.resize(2);
    tx.vin[1].scriptsig = cscript() << op_1;
    tx.vin[1].prevout.hash = txfirst[0]->gethash();
    tx.vin[1].prevout.n = 0;
    tx.vout[0].nvalue = 5900000000ll;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // coinbase in mempool
    tx.vin.resize(1);
    tx.vin[0].prevout.setnull();
    tx.vin[0].scriptsig = cscript() << op_0 << op_1;
    tx.vout[0].nvalue = 0;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // invalid (pre-p2sh) txn in mempool
    tx.vin[0].prevout.hash = txfirst[0]->gethash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptsig = cscript() << op_1;
    tx.vout[0].nvalue = 4900000000ll;
    script = cscript() << op_0;
    tx.vout[0].scriptpubkey = getscriptfordestination(cscriptid(script));
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    tx.vin[0].prevout.hash = hash;
    tx.vin[0].scriptsig = cscript() << (std::vector<unsigned char>)script;
    tx.vout[0].nvalue -= 1000000;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // double spend txn pair in mempool
    tx.vin[0].prevout.hash = txfirst[0]->gethash();
    tx.vin[0].scriptsig = cscript() << op_1;
    tx.vout[0].nvalue = 4900000000ll;
    tx.vout[0].scriptpubkey = cscript() << op_1;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    tx.vout[0].scriptpubkey = cscript() << op_2;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    mempool.clear();

    // subsidy changing
    int nheight = chainactive.height();
    chainactive.tip()->nheight = 209999;
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    chainactive.tip()->nheight = 210000;
    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    delete pblocktemplate;
    chainactive.tip()->nheight = nheight;

    // non-final txs in mempool
    setmocktime(chainactive.tip()->getmediantimepast()+1);

    // height locked
    tx.vin[0].prevout.hash = txfirst[0]->gethash();
    tx.vin[0].scriptsig = cscript() << op_1;
    tx.vin[0].nsequence = 0;
    tx.vout[0].nvalue = 4900000000ll;
    tx.vout[0].scriptpubkey = cscript() << op_1;
    tx.nlocktime = chainactive.tip()->nheight+1;
    hash = tx.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx, 11, gettime(), 111.0, 11));
    boost_check(!checkfinaltx(tx));

    // time locked
    tx2.vin.resize(1);
    tx2.vin[0].prevout.hash = txfirst[1]->gethash();
    tx2.vin[0].prevout.n = 0;
    tx2.vin[0].scriptsig = cscript() << op_1;
    tx2.vin[0].nsequence = 0;
    tx2.vout.resize(1);
    tx2.vout[0].nvalue = 4900000000ll;
    tx2.vout[0].scriptpubkey = cscript() << op_1;
    tx2.nlocktime = chainactive.tip()->getmediantimepast()+1;
    hash = tx2.gethash();
    mempool.addunchecked(hash, ctxmempoolentry(tx2, 11, gettime(), 111.0, 11));
    boost_check(!checkfinaltx(tx2));

    boost_check(pblocktemplate = createnewblock(scriptpubkey));

    // neither tx should have make it into the template.
    boost_check_equal(pblocktemplate->block.vtx.size(), 1);
    delete pblocktemplate;

    // however if we advance height and time by one, both will.
    chainactive.tip()->nheight++;
    setmocktime(chainactive.tip()->getmediantimepast()+2);

    // fixme: we should *actually* create a new block so the following test
    //        works; checkfinaltx() isn't fooled by monkey-patching nheight.
    //boost_check(checkfinaltx(tx));
    //boost_check(checkfinaltx(tx2));

    boost_check(pblocktemplate = createnewblock(scriptpubkey));
    boost_check_equal(pblocktemplate->block.vtx.size(), 3);
    delete pblocktemplate;

    chainactive.tip()->nheight--;
    setmocktime(0);
    mempool.clear();

    boost_foreach(ctransaction *tx, txfirst)
        delete tx;

    fcheckpointsenabled = true;
}

boost_auto_test_suite_end()
