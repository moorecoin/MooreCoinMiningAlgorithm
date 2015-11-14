#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# base class for rpc testing

# add python-moorecoinrpc to module search path:
import os
import sys

import shutil
import tempfile
import traceback

from authproxy import authserviceproxy, jsonrpcexception
from util import *


class moorecointestframework(object):

    # these may be over-ridden by subclasses:
    def run_test(self):
        for node in self.nodes:
            assert_equal(node.getblockcount(), 200)
            assert_equal(node.getbalance(), 25*50)

    def add_options(self, parser):
        pass

    def setup_chain(self):
        print("initializing test directory "+self.options.tmpdir)
        initialize_chain(self.options.tmpdir)

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir)

    def setup_network(self, split = false):
        self.nodes = self.setup_nodes()

        # connect the nodes as a "chain".  this allows us
        # to split the network between nodes 1 and 2 to get
        # two halves that can work on competing chains.

        # if we joined network halves, connect the nodes from the joint
        # on outward.  this ensures that chains are properly reorganised.
        if not split:
            connect_nodes_bi(self.nodes, 1, 2)
            sync_blocks(self.nodes[1:3])
            sync_mempools(self.nodes[1:3])

        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 2, 3)
        self.is_network_split = split
        self.sync_all()

    def split_network(self):
        """
        split the network of four nodes into nodes 0/1 and 2/3.
        """
        assert not self.is_network_split
        stop_nodes(self.nodes)
        wait_moorecoinds()
        self.setup_network(true)

    def sync_all(self):
        if self.is_network_split:
            sync_blocks(self.nodes[:2])
            sync_blocks(self.nodes[2:])
            sync_mempools(self.nodes[:2])
            sync_mempools(self.nodes[2:])
        else:
            sync_blocks(self.nodes)
            sync_mempools(self.nodes)

    def join_network(self):
        """
        join the (previously split) network halves together.
        """
        assert self.is_network_split
        stop_nodes(self.nodes)
        wait_moorecoinds()
        self.setup_network(false)

    def main(self):
        import optparse

        parser = optparse.optionparser(usage="%prog [options]")
        parser.add_option("--nocleanup", dest="nocleanup", default=false, action="store_true",
                          help="leave moorecoinds and test.* datadir on exit or error")
        parser.add_option("--noshutdown", dest="noshutdown", default=false, action="store_true",
                          help="don't stop moorecoinds after the test execution")
        parser.add_option("--srcdir", dest="srcdir", default="../../src",
                          help="source directory containing moorecoind/moorecoin-cli (default: %default)")
        parser.add_option("--tmpdir", dest="tmpdir", default=tempfile.mkdtemp(prefix="test"),
                          help="root directory for datadirs")
        parser.add_option("--tracerpc", dest="trace_rpc", default=false, action="store_true",
                          help="print out all rpc calls as they are made")
        self.add_options(parser)
        (self.options, self.args) = parser.parse_args()

        if self.options.trace_rpc:
            import logging
            logging.basicconfig(level=logging.debug)

        os.environ['path'] = self.options.srcdir+":"+os.environ['path']

        check_json_precision()

        success = false
        try:
            if not os.path.isdir(self.options.tmpdir):
                os.makedirs(self.options.tmpdir)
            self.setup_chain()

            self.setup_network()

            self.run_test()

            success = true

        except jsonrpcexception as e:
            print("jsonrpc error: "+e.error['message'])
            traceback.print_tb(sys.exc_info()[2])
        except assertionerror as e:
            print("assertion failed: "+e.message)
            traceback.print_tb(sys.exc_info()[2])
        except exception as e:
            print("unexpected exception caught during testing: "+str(e))
            traceback.print_tb(sys.exc_info()[2])

        if not self.options.noshutdown:
            print("stopping nodes")
            stop_nodes(self.nodes)
            wait_moorecoinds()
        else:
            print("note: moorecoinds were not stopped and may still be running")

        if not self.options.nocleanup and not self.options.noshutdown:
            print("cleaning up")
            shutil.rmtree(self.options.tmpdir)

        if success:
            print("tests successful")
            sys.exit(0)
        else:
            print("failed")
            sys.exit(1)


# test framework for doing p2p comparison testing, which sets up some moorecoind
# binaries:
# 1 binary: test binary
# 2 binaries: 1 test binary, 1 ref binary
# n>2 binaries: 1 test binary, n-1 ref binaries

class comparisontestframework(moorecointestframework):

    # can override the num_nodes variable to indicate how many nodes to run.
    def __init__(self):
        self.num_nodes = 2

    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("moorecoind", "moorecoind"),
                          help="moorecoind binary to test")
        parser.add_option("--refbinary", dest="refbinary",
                          default=os.getenv("moorecoind", "moorecoind"),
                          help="moorecoind binary to use for reference nodes (if any)")

    def setup_chain(self):
        print "initializing test directory "+self.options.tmpdir
        initialize_chain_clean(self.options.tmpdir, self.num_nodes)

    def setup_network(self):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir,
                                    extra_args=[['-debug', '-whitelist=127.0.0.1']] * self.num_nodes,
                                    binary=[self.options.testbinary] +
                                           [self.options.refbinary]*(self.num_nodes-1))
