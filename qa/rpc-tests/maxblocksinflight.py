#!/usr/bin/env python2
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

from test_framework.mininode import *
from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import logging

'''
in this test we connect to one node over p2p, send it numerous inv's, and
compare the resulting number of getdata requests to a max allowed value.  we
test for exceeding 128 blocks in flight, which was the limit an 0.9 client will
reach. [0.10 clients shouldn't request more than 16 from a single peer.]
'''
max_requests = 128

class testmanager(nodeconncb):
    # set up nodeconncb callbacks, overriding base class
    def on_getdata(self, conn, message):
        self.log.debug("got getdata %s" % repr(message))
        # log the requests
        for inv in message.inv:
            if inv.hash not in self.blockreqcounts:
                self.blockreqcounts[inv.hash] = 0
            self.blockreqcounts[inv.hash] += 1

    def on_close(self, conn):
        if not self.disconnectokay:
            raise earlydisconnecterror(0)

    def __init__(self):
        nodeconncb.__init__(self)
        self.log = logging.getlogger("blockrelaytest")
        self.create_callback_map()

    def add_new_connection(self, connection):
        self.connection = connection
        self.blockreqcounts = {}
        self.disconnectokay = false

    def run(self):
        try:
            fail = false
            self.connection.rpc.generate(1) # leave ibd

            numblockstogenerate = [ 8, 16, 128, 1024 ]
            for count in range(len(numblockstogenerate)):
                current_invs = []
                for i in range(numblockstogenerate[count]):
                    current_invs.append(cinv(2, random.randrange(0, 1<<256)))
                    if len(current_invs) >= 50000:
                        self.connection.send_message(msg_inv(current_invs))
                        current_invs = []
                if len(current_invs) > 0:
                    self.connection.send_message(msg_inv(current_invs))
                
                # wait and see how many blocks were requested
                time.sleep(2)

                total_requests = 0
                with mininode_lock:
                    for key in self.blockreqcounts:
                        total_requests += self.blockreqcounts[key]
                        if self.blockreqcounts[key] > 1:
                            raise assertionerror("error, test failed: block %064x requested more than once" % key)
                if total_requests > max_requests:
                    raise assertionerror("error, too many blocks (%d) requested" % total_requests)
                print "round %d: success (total requests: %d)" % (count, total_requests)
        except assertionerror as e:
            print "test failed: ", e.args

        self.disconnectokay = true
        self.connection.disconnect_node()

        
class maxblocksinflighttest(moorecointestframework):
    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("moorecoind", "moorecoind"),
                          help="binary to test max block requests behavior")

    def setup_chain(self):
        print "initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = start_nodes(1, self.options.tmpdir, 
                                 extra_args=[['-debug', '-whitelist=127.0.0.1']],
                                 binary=[self.options.testbinary])

    def run_test(self):
        test = testmanager()
        test.add_new_connection(nodeconn('127.0.0.1', p2p_port(0), self.nodes[0], test))
        networkthread().start()  # start up network handling in another thread
        test.run()

if __name__ == '__main__':
    maxblocksinflighttest().main()
