#!/usr/bin/env python2
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

from test_framework.test_framework import comparisontestframework
from test_framework.util import *
from test_framework.comptool import testmanager, testinstance
from test_framework.mininode import *
from test_framework.blocktools import *
import logging
import copy
import time


'''
in this test we connect to one node over p2p, and test block requests:
1) valid blocks should be requested and become chain tip.
2) invalid block with duplicated transaction should be re-requested.
3) invalid block with bad coinbase value should be rejected and not
re-requested.
'''

# use the comparisontestframework with 1 node: only use --testbinary.
class invalidblockrequesttest(comparisontestframework):

    ''' can either run this test as 1 node with expected answers, or two and compare them. 
        change the "outcome" variable from each testinstance object to only do the comparison. '''
    def __init__(self):
        self.num_nodes = 1

    def run_test(self):
        test = testmanager(self, self.options.tmpdir)
        test.add_all_connections(self.nodes)
        self.tip = none
        self.block_time = none
        networkthread().start() # start up network handling in another thread
        test.run()

    def get_tests(self):
        if self.tip is none:
            self.tip = int ("0x" + self.nodes[0].getbestblockhash() + "l", 0)
        self.block_time = int(time.time())+1

        '''
        create a new block with an anyone-can-spend coinbase
        '''
        block = create_block(self.tip, create_coinbase(), self.block_time)
        self.block_time += 1
        block.solve()
        # save the coinbase for later
        self.block1 = block
        self.tip = block.sha256
        yield testinstance([[block, true]])

        '''
        now we need that block to mature so we can spend the coinbase.
        '''
        test = testinstance(sync_every_block=false)
        for i in xrange(100):
            block = create_block(self.tip, create_coinbase(), self.block_time)
            block.solve()
            self.tip = block.sha256
            self.block_time += 1
            test.blocks_and_transactions.append([block, true])
        yield test

        '''
        now we use merkle-root malleability to generate an invalid block with
        same blockheader.
        manufacture a block with 3 transactions (coinbase, spend of prior
        coinbase, spend of that spend).  duplicate the 3rd transaction to 
        leave merkle root and blockheader unchanged but invalidate the block.
        '''
        block2 = create_block(self.tip, create_coinbase(), self.block_time)
        self.block_time += 1

        # chr(81) is op_true
        tx1 = create_transaction(self.block1.vtx[0], 0, chr(81), 50*100000000)
        tx2 = create_transaction(tx1, 0, chr(81), 50*100000000)

        block2.vtx.extend([tx1, tx2])
        block2.hashmerkleroot = block2.calc_merkle_root()
        block2.rehash()
        block2.solve()
        orig_hash = block2.sha256
        block2_orig = copy.deepcopy(block2)

        # mutate block 2
        block2.vtx.append(tx2)
        assert_equal(block2.hashmerkleroot, block2.calc_merkle_root())
        assert_equal(orig_hash, block2.rehash())
        assert(block2_orig.vtx != block2.vtx)

        self.tip = block2.sha256
        yield testinstance([[block2, false], [block2_orig, true]])

        '''
        make sure that a totally screwed up block is not valid.
        '''
        block3 = create_block(self.tip, create_coinbase(), self.block_time)
        self.block_time += 1
        block3.vtx[0].vout[0].nvalue = 100*100000000 # too high!
        block3.vtx[0].sha256=none
        block3.vtx[0].calc_sha256()
        block3.hashmerkleroot = block3.calc_merkle_root()
        block3.rehash()
        block3.solve()

        yield testinstance([[block3, false]])


if __name__ == '__main__':
    invalidblockrequesttest().main()
