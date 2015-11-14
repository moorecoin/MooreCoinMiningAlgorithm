#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test -reindex with checkblockindex
#
from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import os.path

class reindextest(moorecointestframework):

    def setup_chain(self):
        print("initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self):
        self.nodes = []
        self.is_network_split = false
        self.nodes.append(start_node(0, self.options.tmpdir))

    def run_test(self):
        self.nodes[0].generate(3)
        stop_node(self.nodes[0], 0)
        wait_moorecoinds()
        self.nodes[0]=start_node(0, self.options.tmpdir, ["-debug", "-reindex", "-checkblockindex=1"])
        assert_equal(self.nodes[0].getblockcount(), 3)
        print "success"

if __name__ == '__main__':
    reindextest().main()
