#!/usr/bin/env python2
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

from test_framework.test_framework import comparisontestframework
from test_framework.util import *
from test_framework.mininode import ctransaction, networkthread
from test_framework.blocktools import create_coinbase, create_block
from test_framework.comptool import testinstance, testmanager
from test_framework.script import cscript
from binascii import hexlify, unhexlify
import cstringio
import time

# a canonical signature consists of: 
# <30> <total len> <02> <len r> <r> <02> <len s> <s> <hashtype>
def underify(tx):
    '''
    make the signature in vin 0 of a tx non-der-compliant,
    by adding padding after the s-value.
    '''
    scriptsig = cscript(tx.vin[0].scriptsig)
    newscript = []
    for i in scriptsig:
        if (len(newscript) == 0):
            newscript.append(i[0:-1] + '\0' + i[-1])
        else:
            newscript.append(i)
    tx.vin[0].scriptsig = cscript(newscript)
    
'''
this test is meant to exercise bip66 (der sig).
connect to a single node.
mine 2 (version 2) blocks (save the coinbases for later).
generate 98 more version 2 blocks, verify the node accepts.
mine 749 version 3 blocks, verify the node accepts.
check that the new dersig rules are not enforced on the 750th version 3 block.
check that the new dersig rules are enforced on the 751st version 3 block.
mine 199 new version blocks.
mine 1 old-version block.
mine 1 new version block.
mine 1 old version block, see that the node rejects.
'''
            
class bip66test(comparisontestframework):

    def __init__(self):
        self.num_nodes = 1

    def setup_network(self):
        # must set the blockversion for this test
        self.nodes = start_nodes(1, self.options.tmpdir, 
                                 extra_args=[['-debug', '-whitelist=127.0.0.1', '-blockversion=2']],
                                 binary=[self.options.testbinary])

    def run_test(self):
        test = testmanager(self, self.options.tmpdir)
        test.add_all_connections(self.nodes)
        networkthread().start() # start up network handling in another thread
        test.run()

    def create_transaction(self, node, coinbase, to_address, amount):
        from_txid = node.getblock(coinbase)['tx'][0]
        inputs = [{ "txid" : from_txid, "vout" : 0}]
        outputs = { to_address : amount }
        rawtx = node.createrawtransaction(inputs, outputs)
        signresult = node.signrawtransaction(rawtx)
        tx = ctransaction()
        f = cstringio.stringio(unhexlify(signresult['hex']))
        tx.deserialize(f)
        return tx

    def get_tests(self):

        self.coinbase_blocks = self.nodes[0].generate(2)
        self.tip = int ("0x" + self.nodes[0].getbestblockhash() + "l", 0)
        self.nodeaddress = self.nodes[0].getnewaddress()
        self.last_block_time = time.time()

        ''' 98 more version 2 blocks '''
        test_blocks = []
        for i in xrange(98):
            block = create_block(self.tip, create_coinbase(2), self.last_block_time + 1)
            block.nversion = 2
            block.rehash()
            block.solve()
            test_blocks.append([block, true])
            self.last_block_time += 1
            self.tip = block.sha256
        yield testinstance(test_blocks, sync_every_block=false)

        ''' mine 749 version 3 blocks '''
        test_blocks = []
        for i in xrange(749):
            block = create_block(self.tip, create_coinbase(2), self.last_block_time + 1)
            block.nversion = 3
            block.rehash()
            block.solve()
            test_blocks.append([block, true])
            self.last_block_time += 1
            self.tip = block.sha256
        yield testinstance(test_blocks, sync_every_block=false)

        ''' 
        check that the new dersig rules are not enforced in the 750th
        version 3 block.
        '''
        spendtx = self.create_transaction(self.nodes[0],
                self.coinbase_blocks[0], self.nodeaddress, 1.0)
        underify(spendtx)
        spendtx.rehash()

        block = create_block(self.tip, create_coinbase(2), self.last_block_time + 1)
        block.nversion = 3
        block.vtx.append(spendtx)
        block.hashmerkleroot = block.calc_merkle_root()
        block.rehash()
        block.solve()

        self.last_block_time += 1
        self.tip = block.sha256
        yield testinstance([[block, true]])

        ''' 
        check that the new dersig rules are enforced in the 751st version 3
        block.
        '''
        spendtx = self.create_transaction(self.nodes[0],
                self.coinbase_blocks[1], self.nodeaddress, 1.0)
        underify(spendtx)
        spendtx.rehash()

        block = create_block(self.tip, create_coinbase(1), self.last_block_time + 1)
        block.nversion = 3
        block.vtx.append(spendtx)
        block.hashmerkleroot = block.calc_merkle_root()
        block.rehash()
        block.solve()
        self.last_block_time += 1
        yield testinstance([[block, false]])

        ''' mine 199 new version blocks on last valid tip '''
        test_blocks = []
        for i in xrange(199):
            block = create_block(self.tip, create_coinbase(1), self.last_block_time + 1)
            block.nversion = 3
            block.rehash()
            block.solve()
            test_blocks.append([block, true])
            self.last_block_time += 1
            self.tip = block.sha256
        yield testinstance(test_blocks, sync_every_block=false)

        ''' mine 1 old version block '''
        block = create_block(self.tip, create_coinbase(1), self.last_block_time + 1)
        block.nversion = 2
        block.rehash()
        block.solve()
        self.last_block_time += 1
        self.tip = block.sha256
        yield testinstance([[block, true]])

        ''' mine 1 new version block '''
        block = create_block(self.tip, create_coinbase(1), self.last_block_time + 1)
        block.nversion = 3
        block.rehash()
        block.solve()
        self.last_block_time += 1
        self.tip = block.sha256
        yield testinstance([[block, true]])

        ''' mine 1 old version block, should be invalid '''
        block = create_block(self.tip, create_coinbase(1), self.last_block_time + 1)
        block.nversion = 2
        block.rehash()
        block.solve()
        self.last_block_time += 1
        yield testinstance([[block, false]])

if __name__ == '__main__':
    bip66test().main()
