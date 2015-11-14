#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test node handling
#

from test_framework.test_framework import moorecointestframework
from test_framework.util import *
import base64

try:
    import http.client as httplib
except importerror:
    import httplib
try:
    import urllib.parse as urlparse
except importerror:
    import urlparse

class nodehandlingtest (moorecointestframework):
    def run_test(self):
        ###########################
        # setban/listbanned tests #
        ###########################
        assert_equal(len(self.nodes[2].getpeerinfo()), 4) #we should have 4 nodes at this point
        self.nodes[2].setban("127.0.0.1", "add")
        time.sleep(3) #wait till the nodes are disconected
        assert_equal(len(self.nodes[2].getpeerinfo()), 0) #all nodes must be disconnected at this point
        assert_equal(len(self.nodes[2].listbanned()), 1)
        self.nodes[2].clearbanned()
        assert_equal(len(self.nodes[2].listbanned()), 0)
        self.nodes[2].setban("127.0.0.0/24", "add")
        assert_equal(len(self.nodes[2].listbanned()), 1)
        try:
            self.nodes[2].setban("127.0.0.1", "add") #throws exception because 127.0.0.1 is within range 127.0.0.0/24
        except:
            pass
        assert_equal(len(self.nodes[2].listbanned()), 1) #still only one banned ip because 127.0.0.1 is within the range of 127.0.0.0/24
        try:
            self.nodes[2].setban("127.0.0.1", "remove")
        except:
            pass
        assert_equal(len(self.nodes[2].listbanned()), 1)
        self.nodes[2].setban("127.0.0.0/24", "remove")
        assert_equal(len(self.nodes[2].listbanned()), 0)
        self.nodes[2].clearbanned()
        assert_equal(len(self.nodes[2].listbanned()), 0)
        
        ###########################
        # rpc disconnectnode test #
        ###########################
        url = urlparse.urlparse(self.nodes[1].url)
        self.nodes[0].disconnectnode(url.hostname+":"+str(p2p_port(1)))
        time.sleep(2) #disconnecting a node needs a little bit of time
        for node in self.nodes[0].getpeerinfo():
            assert(node['addr'] != url.hostname+":"+str(p2p_port(1)))

        connect_nodes_bi(self.nodes,0,1) #reconnect the node
        found = false
        for node in self.nodes[0].getpeerinfo():
            if node['addr'] == url.hostname+":"+str(p2p_port(1)):
                found = true
        assert(found)

if __name__ == '__main__':
    nodehandlingtest ().main ()
