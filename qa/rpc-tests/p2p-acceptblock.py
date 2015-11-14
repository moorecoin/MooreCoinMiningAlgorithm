#!/usr/bin/env python2
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

from test_framework.mininode import *
from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import time
from test_framework.blocktools import create_block, create_coinbase

'''
acceptblocktest -- test processing of unrequested blocks.

since behavior differs when receiving unrequested blocks from whitelisted peers
versus non-whitelisted peers, this tests the behavior of both (effectively two
separate tests running in parallel).

setup: two nodes, node0 and node1, not connected to each other.  node0 does not
whitelist localhost, but node1 does. they will each be on their own chain for
this test.

we have one nodeconn connection to each, test_node and white_node respectively.

the test:
1. generate one block on each node, to leave ibd.

2. mine a new block on each tip, and deliver to each node from node's peer.
   the tip should advance.

3. mine a block that forks the previous block, and deliver to each node from
   corresponding peer.
   node0 should not process this block (just accept the header), because it is
   unrequested and doesn't have more work than the tip.
   node1 should process because this is coming from a whitelisted peer.

4. send another block that builds on the forking block.
   node0 should process this block but be stuck on the shorter chain, because
   it's missing an intermediate block.
   node1 should reorg to this longer chain.

5. send a duplicate of the block in #3 to node0.
   node0 should not process the block because it is unrequested, and stay on
   the shorter chain.

6. send node0 an inv for the height 3 block produced in #4 above.
   node0 should figure out that node0 has the missing height 2 block and send a
   getdata.

7. send node0 the missing block again.
   node0 should process and the tip should advance.
'''

# testnode: bare-bones "peer".  used mostly as a conduit for a test to sending
# p2p messages to a node, generating the messages in the main testing logic.
class testnode(nodeconncb):
    def __init__(self):
        nodeconncb.__init__(self)
        self.create_callback_map()
        self.connection = none

    def add_connection(self, conn):
        self.connection = conn

    # track the last getdata message we receive (used in the test)
    def on_getdata(self, conn, message):
        self.last_getdata = message

    # spin until verack message is received from the node.
    # we use this to signal that our test can begin. this
    # is called from the testing thread, so it needs to acquire
    # the global lock.
    def wait_for_verack(self):
        while true:
            with mininode_lock:
                if self.verack_received:
                    return
            time.sleep(0.05)

    # wrapper for the nodeconn's send_message function
    def send_message(self, message):
        self.connection.send_message(message)

class acceptblocktest(moorecointestframework):
    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("moorecoind", "moorecoind"),
                          help="moorecoind binary to test")

    def setup_chain(self):
        initialize_chain_clean(self.options.tmpdir, 2)

    def setup_network(self):
        # node0 will be used to test behavior of processing unrequested blocks
        # from peers which are not whitelisted, while node1 will be used for
        # the whitelisted case.
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, ["-debug"],
                                     binary=self.options.testbinary))
        self.nodes.append(start_node(1, self.options.tmpdir,
                                     ["-debug", "-whitelist=127.0.0.1"],
                                     binary=self.options.testbinary))

    def run_test(self):
        # setup the p2p connections and start up the network thread.
        test_node = testnode()   # connects to node0 (not whitelisted)
        white_node = testnode()  # connects to node1 (whitelisted)

        connections = []
        connections.append(nodeconn('127.0.0.1', p2p_port(0), self.nodes[0], test_node))
        connections.append(nodeconn('127.0.0.1', p2p_port(1), self.nodes[1], white_node))
        test_node.add_connection(connections[0])
        white_node.add_connection(connections[1])

        networkthread().start() # start up network handling in another thread

        # test logic begins here
        test_node.wait_for_verack()
        white_node.wait_for_verack()

        # 1. have both nodes mine a block (leave ibd)
        [ n.generate(1) for n in self.nodes ]
        tips = [ int ("0x" + n.getbestblockhash() + "l", 0) for n in self.nodes ]

        # 2. send one block that builds on each tip.
        # this should be accepted.
        blocks_h2 = []  # the height 2 blocks on each node's chain
        for i in xrange(2):
            blocks_h2.append(create_block(tips[i], create_coinbase(), time.time()+1))
            blocks_h2[i].solve()
        test_node.send_message(msg_block(blocks_h2[0]))
        white_node.send_message(msg_block(blocks_h2[1]))

        time.sleep(1)
        assert_equal(self.nodes[0].getblockcount(), 2)
        assert_equal(self.nodes[1].getblockcount(), 2)
        print "first height 2 block accepted by both nodes"

        # 3. send another block that builds on the original tip.
        blocks_h2f = []  # blocks at height 2 that fork off the main chain
        for i in xrange(2):
            blocks_h2f.append(create_block(tips[i], create_coinbase(), blocks_h2[i].ntime+1))
            blocks_h2f[i].solve()
        test_node.send_message(msg_block(blocks_h2f[0]))
        white_node.send_message(msg_block(blocks_h2f[1]))

        time.sleep(1)  # give time to process the block
        for x in self.nodes[0].getchaintips():
            if x['hash'] == blocks_h2f[0].hash:
                assert_equal(x['status'], "headers-only")

        for x in self.nodes[1].getchaintips():
            if x['hash'] == blocks_h2f[1].hash:
                assert_equal(x['status'], "valid-headers")

        print "second height 2 block accepted only from whitelisted peer"

        # 4. now send another block that builds on the forking chain.
        blocks_h3 = []
        for i in xrange(2):
            blocks_h3.append(create_block(blocks_h2f[i].sha256, create_coinbase(), blocks_h2f[i].ntime+1))
            blocks_h3[i].solve()
        test_node.send_message(msg_block(blocks_h3[0]))
        white_node.send_message(msg_block(blocks_h3[1]))

        time.sleep(1)
        # since the earlier block was not processed by node0, the new block
        # can't be fully validated.
        for x in self.nodes[0].getchaintips():
            if x['hash'] == blocks_h3[0].hash:
                assert_equal(x['status'], "headers-only")

        # but this block should be accepted by node0 since it has more work.
        try:
            self.nodes[0].getblock(blocks_h3[0].hash)
            print "unrequested more-work block accepted from non-whitelisted peer"
        except:
            raise assertionerror("unrequested more work block was not processed")

        # node1 should have accepted and reorged.
        assert_equal(self.nodes[1].getblockcount(), 3)
        print "successfully reorged to length 3 chain from whitelisted peer"

        # 5. test handling of unrequested block on the node that didn't process
        # should still not be processed (even though it has a child that has more
        # work).
        test_node.send_message(msg_block(blocks_h2f[0]))

        # here, if the sleep is too short, the test could falsely succeed (if the
        # node hasn't processed the block by the time the sleep returns, and then
        # the node processes it and incorrectly advances the tip).
        # but this would be caught later on, when we verify that an inv triggers
        # a getdata request for this block.
        time.sleep(1)
        assert_equal(self.nodes[0].getblockcount(), 2)
        print "unrequested block that would complete more-work chain was ignored"

        # 6. try to get node to request the missing block.
        # poke the node with an inv for block at height 3 and see if that
        # triggers a getdata on block 2 (it should if block 2 is missing).
        with mininode_lock:
            # clear state so we can check the getdata request
            test_node.last_getdata = none
            test_node.send_message(msg_inv([cinv(2, blocks_h3[0].sha256)]))

        time.sleep(1)
        with mininode_lock:
            getdata = test_node.last_getdata

        # check that the getdata is for the right block
        assert_equal(len(getdata.inv), 1)
        assert_equal(getdata.inv[0].hash, blocks_h2f[0].sha256)
        print "inv at tip triggered getdata for unprocessed block"

        # 7. send the missing block for the third time (now it is requested)
        test_node.send_message(msg_block(blocks_h2f[0]))

        time.sleep(1)
        assert_equal(self.nodes[0].getblockcount(), 3)
        print "successfully reorged to length 3 chain from non-whitelisted peer"

        [ c.disconnect_node() for c in connections ]

if __name__ == '__main__':
    acceptblocktest().main()
