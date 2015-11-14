#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test the bip66 changeover logic
#

from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import os
import shutil

class bip66test(moorecointestframework):

    def setup_network(self):
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, []))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-blockversion=2"]))
        self.nodes.append(start_node(2, self.options.tmpdir, ["-blockversion=3"]))
        connect_nodes(self.nodes[1], 0)
        connect_nodes(self.nodes[2], 0)
        self.is_network_split = false
        self.sync_all()

    def run_test(self):
        cnt = self.nodes[0].getblockcount()

        # mine some old-version blocks
        self.nodes[1].generate(100)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 100):
            raise assertionerror("failed to mine 100 version=2 blocks")

        # mine 750 new-version blocks
        for i in xrange(15):
            self.nodes[2].generate(50)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 850):
            raise assertionerror("failed to mine 750 version=3 blocks")

        # todo: check that new dersig rules are not enforced

        # mine 1 new-version block
        self.nodes[2].generate(1)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 851):
            raise assertionfailure("failed to mine a version=3 blocks")

        # todo: check that new dersig rules are enforced

        # mine 198 new-version blocks
        for i in xrange(2):
            self.nodes[2].generate(99)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 1049):
            raise assertionerror("failed to mine 198 version=3 blocks")

        # mine 1 old-version block
        self.nodes[1].generate(1)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 1050):
            raise assertionerror("failed to mine a version=2 block after 949 version=3 blocks")

        # mine 1 new-version blocks
        self.nodes[2].generate(1)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 1051):
            raise assertionerror("failed to mine a version=3 block")

        # mine 1 old-version blocks
        try:
            self.nodes[1].generate(1)
            raise assertionerror("succeeded to mine a version=2 block after 950 version=3 blocks")
        except jsonrpcexception:
            pass
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 1051):
            raise assertionerror("accepted a version=2 block after 950 version=3 blocks")

        # mine 1 new-version blocks
        self.nodes[2].generate(1)
        self.sync_all()
        if (self.nodes[0].getblockcount() != cnt + 1052):
            raise assertionerror("failed to mine a version=3 block")

if __name__ == '__main__':
    bip66test().main()
