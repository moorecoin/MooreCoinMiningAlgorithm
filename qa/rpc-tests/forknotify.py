#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test -alertnotify 
#

from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import os
import shutil

class forknotifytest(moorecointestframework):

    alert_filename = none  # set by setup_network

    def setup_network(self):
        self.nodes = []
        self.alert_filename = os.path.join(self.options.tmpdir, "alert.txt")
        with open(self.alert_filename, 'w') as f:
            pass  # just open then close to create zero-length file
        self.nodes.append(start_node(0, self.options.tmpdir,
                            ["-blockversion=2", "-alertnotify=echo %s >> \"" + self.alert_filename + "\""]))
        # node1 mines block.version=211 blocks
        self.nodes.append(start_node(1, self.options.tmpdir,
                                ["-blockversion=211"]))
        connect_nodes(self.nodes[1], 0)

        self.is_network_split = false
        self.sync_all()

    def run_test(self):
        # mine 51 up-version blocks
        self.nodes[1].generate(51)
        self.sync_all()
        # -alertnotify should trigger on the 51'st,
        # but mine and sync another to give
        # -alertnotify time to write
        self.nodes[1].generate(1)
        self.sync_all()

        with open(self.alert_filename, 'r') as f:
            alert_text = f.read()

        if len(alert_text) == 0:
            raise assertionerror("-alertnotify did not warn of up-version blocks")

        # mine more up-version blocks, should not get more alerts:
        self.nodes[1].generate(1)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        with open(self.alert_filename, 'r') as f:
            alert_text2 = f.read()

        if alert_text != alert_text2:
            raise assertionerror("-alertnotify excessive warning of up-version blocks")

if __name__ == '__main__':
    forknotifytest().main()
