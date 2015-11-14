#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import moorecointestframework
from test_framework.util import *


def check_array_result(object_array, to_match, expected):
    """
    pass in array of json objects, a dictionary with key/value pairs
    to match against, and another dictionary with expected key/value
    pairs.
    """
    num_matched = 0
    for item in object_array:
        all_match = true
        for key,value in to_match.items():
            if item[key] != value:
                all_match = false
        if not all_match:
            continue
        for key,value in expected.items():
            if item[key] != value:
                raise assertionerror("%s : expected %s=%s"%(str(item), str(key), str(value)))
            num_matched = num_matched+1
    if num_matched == 0:
        raise assertionerror("no objects matched %s"%(str(to_match)))

import threading

class longpollthread(threading.thread):
    def __init__(self, node):
        threading.thread.__init__(self)
        # query current longpollid
        templat = node.getblocktemplate()
        self.longpollid = templat['longpollid']
        # create a new connection to the node, we can't use the same
        # connection from two threads
        self.node = authserviceproxy(node.url, timeout=600)

    def run(self):
        self.node.getblocktemplate({'longpollid':self.longpollid})

class getblocktemplatelptest(moorecointestframework):
    '''
    test longpolling with getblocktemplate.
    '''

    def run_test(self):
        print "warning: this test will take about 70 seconds in the best case. be patient."
        self.nodes[0].generate(10)
        templat = self.nodes[0].getblocktemplate()
        longpollid = templat['longpollid']
        # longpollid should not change between successive invocations if nothing else happens
        templat2 = self.nodes[0].getblocktemplate()
        assert(templat2['longpollid'] == longpollid)

        # test 1: test that the longpolling wait if we do nothing
        thr = longpollthread(self.nodes[0])
        thr.start()
        # check that thread still lives
        thr.join(5)  # wait 5 seconds or until thread exits
        assert(thr.is_alive())

        # test 2: test that longpoll will terminate if another node generates a block
        self.nodes[1].generate(1)  # generate a block on another node
        # check that thread will exit now that new transaction entered mempool
        thr.join(5)  # wait 5 seconds or until thread exits
        assert(not thr.is_alive())

        # test 3: test that longpoll will terminate if we generate a block ourselves
        thr = longpollthread(self.nodes[0])
        thr.start()
        self.nodes[0].generate(1)  # generate a block on another node
        thr.join(5)  # wait 5 seconds or until thread exits
        assert(not thr.is_alive())

        # test 4: test that introducing a new transaction into the mempool will terminate the longpoll
        thr = longpollthread(self.nodes[0])
        thr.start()
        # generate a random transaction and submit it
        (txid, txhex, fee) = random_transaction(self.nodes, decimal("1.1"), decimal("0.0"), decimal("0.001"), 20)
        # after one minute, every 10 seconds the mempool is probed, so in 80 seconds it should have returned
        thr.join(60 + 20)
        assert(not thr.is_alive())

if __name__ == '__main__':
    getblocktemplatelptest().main()

