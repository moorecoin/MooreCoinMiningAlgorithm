#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test pruning code
# ********
# warning:
# this test uses 4gb of disk space.
# this test takes 30 mins or more (up to 2 hours)
# ********

from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import os.path

def calc_usage(blockdir):
    return sum(os.path.getsize(blockdir+f) for f in os.listdir(blockdir) if os.path.isfile(blockdir+f))/(1024*1024)

class prunetest(moorecointestframework):

    def __init__(self):
        self.utxo = []
        self.address = ["",""]

        # some pre-processing to create a bunch of op_return txouts to insert into transactions we create
        # so we have big transactions and full blocks to fill up our block files

        # create one script_pubkey
        script_pubkey = "6a4d0200" #op_return op_push2 512 bytes
        for i in xrange (512):
            script_pubkey = script_pubkey + "01"
        # concatenate 128 txouts of above script_pubkey which we'll insert before the txout for change
        self.txouts = "81"
        for k in xrange(128):
            # add txout value
            self.txouts = self.txouts + "0000000000000000"
            # add length of script_pubkey
            self.txouts = self.txouts + "fd0402"
            # add script_pubkey
            self.txouts = self.txouts + script_pubkey


    def setup_chain(self):
        print("initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        self.nodes = []
        self.is_network_split = false

        # create nodes 0 and 1 to mine
        self.nodes.append(start_node(0, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=999000", "-checkblocks=5"], timewait=900))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=999000", "-checkblocks=5"], timewait=900))

        # create node 2 to test pruning
        self.nodes.append(start_node(2, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-prune=550"], timewait=900))
        self.prunedir = self.options.tmpdir+"/node2/regtest/blocks/"

        self.address[0] = self.nodes[0].getnewaddress()
        self.address[1] = self.nodes[1].getnewaddress()

        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[1], 2)
        connect_nodes(self.nodes[2], 0)
        sync_blocks(self.nodes[0:3])

    def create_big_chain(self):
        # start by creating some coinbases we can spend later
        self.nodes[1].generate(200)
        sync_blocks(self.nodes[0:2])
        self.nodes[0].generate(150)
        # then mine enough full blocks to create more than 550mb of data
        for i in xrange(645):
            self.mine_full_block(self.nodes[0], self.address[0])

        sync_blocks(self.nodes[0:3])

    def test_height_min(self):
        if not os.path.isfile(self.prunedir+"blk00000.dat"):
            raise assertionerror("blk00000.dat is missing, pruning too early")
        print "success"
        print "though we're already using more than 550mb, current usage:", calc_usage(self.prunedir)
        print "mining 25 more blocks should cause the first block file to be pruned"
        # pruning doesn't run until we're allocating another chunk, 20 full blocks past the height cutoff will ensure this
        for i in xrange(25):
            self.mine_full_block(self.nodes[0],self.address[0])

        waitstart = time.time()
        while os.path.isfile(self.prunedir+"blk00000.dat"):
            time.sleep(0.1)
            if time.time() - waitstart > 10:
                raise assertionerror("blk00000.dat not pruned when it should be")

        print "success"
        usage = calc_usage(self.prunedir)
        print "usage should be below target:", usage
        if (usage > 550):
            raise assertionerror("pruning target not being met")

    def create_chain_with_staleblocks(self):
        # create stale blocks in manageable sized chunks
        print "mine 24 (stale) blocks on node 1, followed by 25 (main chain) block reorg from node 0, for 12 rounds"

        for j in xrange(12):
            # disconnect node 0 so it can mine a longer reorg chain without knowing about node 1's soon-to-be-stale chain
            # node 2 stays connected, so it hears about the stale blocks and then reorg's when node0 reconnects
            # stopping node 0 also clears its mempool, so it doesn't have node1's transactions to accidentally mine
            stop_node(self.nodes[0],0)
            self.nodes[0]=start_node(0, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=999000", "-checkblocks=5"], timewait=900)
            # mine 24 blocks in node 1
            self.utxo = self.nodes[1].listunspent()
            for i in xrange(24):
                if j == 0:
                    self.mine_full_block(self.nodes[1],self.address[1])
                else:
                    self.nodes[1].generate(1) #tx's already in mempool from previous disconnects

            # reorg back with 25 block chain from node 0
            self.utxo = self.nodes[0].listunspent()
            for i in xrange(25): 
                self.mine_full_block(self.nodes[0],self.address[0])

            # create connections in the order so both nodes can see the reorg at the same time
            connect_nodes(self.nodes[1], 0)
            connect_nodes(self.nodes[2], 0)
            sync_blocks(self.nodes[0:3])

        print "usage can be over target because of high stale rate:", calc_usage(self.prunedir)

    def reorg_test(self):
        # node 1 will mine a 300 block chain starting 287 blocks back from node 0 and node 2's tip
        # this will cause node 2 to do a reorg requiring 288 blocks of undo data to the reorg_test chain
        # reboot node 1 to clear its mempool (hopefully make the invalidate faster)
        # lower the block max size so we don't keep mining all our big mempool transactions (from disconnected blocks)
        stop_node(self.nodes[1],1)
        self.nodes[1]=start_node(1, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=5000", "-checkblocks=5", "-disablesafemode"], timewait=900)

        height = self.nodes[1].getblockcount()
        print "current block height:", height

        invalidheight = height-287
        badhash = self.nodes[1].getblockhash(invalidheight)
        print "invalidating block at height:",invalidheight,badhash
        self.nodes[1].invalidateblock(badhash)

        # we've now switched to our previously mined-24 block fork on node 1, but thats not what we want
        # so invalidate that fork as well, until we're on the same chain as node 0/2 (but at an ancestor 288 blocks ago)
        mainchainhash = self.nodes[0].getblockhash(invalidheight - 1)
        curhash = self.nodes[1].getblockhash(invalidheight - 1)
        while curhash != mainchainhash:
            self.nodes[1].invalidateblock(curhash)
            curhash = self.nodes[1].getblockhash(invalidheight - 1)

        assert(self.nodes[1].getblockcount() == invalidheight - 1)
        print "new best height", self.nodes[1].getblockcount()

        # reboot node1 to clear those giant tx's from mempool
        stop_node(self.nodes[1],1)
        self.nodes[1]=start_node(1, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=5000", "-checkblocks=5", "-disablesafemode"], timewait=900)

        print "generating new longer chain of 300 more blocks"
        self.nodes[1].generate(300)

        print "reconnect nodes"
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[2], 1)
        sync_blocks(self.nodes[0:3])

        print "verify height on node 2:",self.nodes[2].getblockcount()
        print "usage possibly still high bc of stale blocks in block files:", calc_usage(self.prunedir)

        print "mine 220 more blocks so we have requisite history (some blocks will be big and cause pruning of previous chain)"
        self.nodes[0].generate(220) #node 0 has many large tx's in its mempool from the disconnects
        sync_blocks(self.nodes[0:3])

        usage = calc_usage(self.prunedir)
        print "usage should be below target:", usage
        if (usage > 550):
            raise assertionerror("pruning target not being met")

        return invalidheight,badhash

    def reorg_back(self):
        # verify that a block on the old main chain fork has been pruned away
        try:
            self.nodes[2].getblock(self.forkhash)
            raise assertionerror("old block wasn't pruned so can't test redownload")
        except jsonrpcexception as e:
            print "will need to redownload block",self.forkheight

        # verify that we have enough history to reorg back to the fork point
        # although this is more than 288 blocks, because this chain was written more recently
        # and only its other 299 small and 220 large block are in the block files after it,
        # its expected to still be retained
        self.nodes[2].getblock(self.nodes[2].getblockhash(self.forkheight))

        first_reorg_height = self.nodes[2].getblockcount()
        curchainhash = self.nodes[2].getblockhash(self.mainchainheight)
        self.nodes[2].invalidateblock(curchainhash)
        goalbestheight = self.mainchainheight
        goalbesthash = self.mainchainhash2

        # as of 0.10 the current block download logic is not able to reorg to the original chain created in
        # create_chain_with_stale_blocks because it doesn't know of any peer thats on that chain from which to
        # redownload its missing blocks.
        # invalidate the reorg_test chain in node 0 as well, it can successfully switch to the original chain
        # because it has all the block data.
        # however it must mine enough blocks to have a more work chain than the reorg_test chain in order
        # to trigger node 2's block download logic.
        # at this point node 2 is within 288 blocks of the fork point so it will preserve its ability to reorg
        if self.nodes[2].getblockcount() < self.mainchainheight:
            blocks_to_mine = first_reorg_height + 1 - self.mainchainheight
            print "rewind node 0 to prev main chain to mine longer chain to trigger redownload. blocks needed:", blocks_to_mine
            self.nodes[0].invalidateblock(curchainhash)
            assert(self.nodes[0].getblockcount() == self.mainchainheight)
            assert(self.nodes[0].getbestblockhash() == self.mainchainhash2)
            goalbesthash = self.nodes[0].generate(blocks_to_mine)[-1]
            goalbestheight = first_reorg_height + 1

        print "verify node 2 reorged back to the main chain, some blocks of which it had to redownload"
        waitstart = time.time()
        while self.nodes[2].getblockcount() < goalbestheight:
            time.sleep(0.1)
            if time.time() - waitstart > 900:
                raise assertionerror("node 2 didn't reorg to proper height")
        assert(self.nodes[2].getbestblockhash() == goalbesthash)
        # verify we can now have the data for a block previously pruned
        assert(self.nodes[2].getblock(self.forkhash)["height"] == self.forkheight)

    def mine_full_block(self, node, address):
        # want to create a full block
        # we'll generate a 66k transaction below, and 14 of them is close to the 1mb block limit
        for j in xrange(14):
            if len(self.utxo) < 14:
                self.utxo = node.listunspent()
            inputs=[]
            outputs = {}
            t = self.utxo.pop()
            inputs.append({ "txid" : t["txid"], "vout" : t["vout"]})
            remchange = t["amount"] - decimal("0.001000")
            outputs[address]=remchange
            # create a basic transaction that will send change back to ourself after account for a fee
            # and then insert the 128 generated transaction outs in the middle rawtx[92] is where the #
            # of txouts is stored and is the only thing we overwrite from the original transaction
            rawtx = node.createrawtransaction(inputs, outputs)
            newtx = rawtx[0:92]
            newtx = newtx + self.txouts
            newtx = newtx + rawtx[94:]
            # appears to be ever so slightly faster to sign with sighash_none
            signresult = node.signrawtransaction(newtx,none,none,"none")
            txid = node.sendrawtransaction(signresult["hex"], true)
        # mine a full sized block which will be these transactions we just created
        node.generate(1)


    def run_test(self):
        print "warning! this test requires 4gb of disk space and takes over 30 mins (up to 2 hours)"
        print "mining a big blockchain of 995 blocks"
        self.create_big_chain()
        # chain diagram key:
        # *   blocks on main chain
        # +,&,$,@ blocks on other forks
        # x   invalidated block
        # n1  node 1
        #
        # start by mining a simple chain that all nodes have
        # n0=n1=n2 **...*(995)

        print "check that we haven't started pruning yet because we're below pruneafterheight"
        self.test_height_min()
        # extend this chain past the pruneafterheight
        # n0=n1=n2 **...*(1020)

        print "check that we'll exceed disk space target if we have a very high stale block rate"
        self.create_chain_with_staleblocks()
        # disconnect n0
        # and mine a 24 block chain on n1 and a separate 25 block chain on n0
        # n1=n2 **...*+...+(1044)
        # n0    **...**...**(1045)
        #
        # reconnect nodes causing reorg on n1 and n2
        # n1=n2 **...*(1020) *...**(1045)
        #                   \
        #                    +...+(1044)
        #
        # repeat this process until you have 12 stale forks hanging off the
        # main chain on n1 and n2
        # n0    *************************...***************************(1320)
        #
        # n1=n2 **...*(1020) *...**(1045) *..         ..**(1295) *...**(1320)
        #                   \            \                      \
        #                    +...+(1044)  &..                    $...$(1319)

        # save some current chain state for later use
        self.mainchainheight = self.nodes[2].getblockcount()   #1320
        self.mainchainhash2 = self.nodes[2].getblockhash(self.mainchainheight)

        print "check that we can survive a 288 block reorg still"
        (self.forkheight,self.forkhash) = self.reorg_test() #(1033, )
        # now create a 288 block reorg by mining a longer chain on n1
        # first disconnect n1
        # then invalidate 1033 on main chain and 1032 on fork so height is 1032 on main chain
        # n1   **...*(1020) **...**(1032)x..
        #                  \
        #                   ++...+(1031)x..
        #
        # now mine 300 more blocks on n1
        # n1    **...*(1020) **...**(1032) @@...@(1332)
        #                 \               \
        #                  \               x...
        #                   \                 \
        #                    ++...+(1031)x..   ..
        #
        # reconnect nodes and mine 220 more blocks on n1
        # n1    **...*(1020) **...**(1032) @@...@@@(1552)
        #                 \               \
        #                  \               x...
        #                   \                 \
        #                    ++...+(1031)x..   ..
        #
        # n2    **...*(1020) **...**(1032) @@...@@@(1552)
        #                 \               \
        #                  \               *...**(1320)
        #                   \                 \
        #                    ++...++(1044)     ..
        #
        # n0    ********************(1032) @@...@@@(1552) 
        #                                 \
        #                                  *...**(1320)

        print "test that we can rerequest a block we previously pruned if needed for a reorg"
        self.reorg_back()
        # verify that n2 still has block 1033 on current chain (@), but not on main chain (*)
        # invalidate 1033 on current chain (@) on n2 and we should be able to reorg to
        # original main chain (*), but will require redownload of some blocks
        # in order to have a peer we think we can download from, must also perform this invalidation
        # on n0 and mine a new longest chain to trigger.
        # final result:
        # n0    ********************(1032) **...****(1553)
        #                                 \
        #                                  x@...@@@(1552)
        #
        # n2    **...*(1020) **...**(1032) **...****(1553)
        #                 \               \
        #                  \               x@...@@@(1552)
        #                   \
        #                    +..
        #
        # n1 doesn't change because 1033 on main chain (*) is invalid

        print "done"

if __name__ == '__main__':
    prunetest().main()
