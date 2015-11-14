#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# exercise the wallet keypool, and interaction with wallet encryption/locking

# add python-moorecoinrpc to module search path:
import os
import sys

import json
import shutil
import subprocess
import tempfile
import traceback

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

def run_test(nodes, tmpdir):
    # encrypt wallet and wait to terminate
    nodes[0].encryptwallet('test')
    moorecoind_processes[0].wait()
    # restart node 0
    nodes[0] = start_node(0, tmpdir)
    # keep creating keys
    addr = nodes[0].getnewaddress()
    try:
        addr = nodes[0].getnewaddress()
        raise assertionerror('keypool should be exhausted after one address')
    except jsonrpcexception,e:
        assert(e.error['code']==-12)

    # put three new keys in the keypool
    nodes[0].walletpassphrase('test', 12000)
    nodes[0].keypoolrefill(3)
    nodes[0].walletlock()

    # drain the keys
    addr = set()
    addr.add(nodes[0].getrawchangeaddress())
    addr.add(nodes[0].getrawchangeaddress())
    addr.add(nodes[0].getrawchangeaddress())
    addr.add(nodes[0].getrawchangeaddress())
    # assert that four unique addresses were returned
    assert(len(addr) == 4)
    # the next one should fail
    try:
        addr = nodes[0].getrawchangeaddress()
        raise assertionerror('keypool should be exhausted after three addresses')
    except jsonrpcexception,e:
        assert(e.error['code']==-12)


def main():
    import optparse

    parser = optparse.optionparser(usage="%prog [options]")
    parser.add_option("--nocleanup", dest="nocleanup", default=false, action="store_true",
                      help="leave moorecoinds and test.* datadir on exit or error")
    parser.add_option("--srcdir", dest="srcdir", default="../../src",
                      help="source directory containing moorecoind/moorecoin-cli (default: %default%)")
    parser.add_option("--tmpdir", dest="tmpdir", default=tempfile.mkdtemp(prefix="test"),
                      help="root directory for datadirs")
    (options, args) = parser.parse_args()

    os.environ['path'] = options.srcdir+":"+os.environ['path']

    check_json_precision()

    success = false
    nodes = []
    try:
        print("initializing test directory "+options.tmpdir)
        if not os.path.isdir(options.tmpdir):
            os.makedirs(options.tmpdir)
        initialize_chain(options.tmpdir)

        nodes = start_nodes(1, options.tmpdir)

        run_test(nodes, options.tmpdir)

        success = true

    except assertionerror as e:
        print("assertion failed: "+e.message)
    except jsonrpcexception as e:
        print("jsonrpc error: "+e.error['message'])
        traceback.print_tb(sys.exc_info()[2])
    except exception as e:
        print("unexpected exception caught during testing: "+str(sys.exc_info()[0]))
        traceback.print_tb(sys.exc_info()[2])

    if not options.nocleanup:
        print("cleaning up")
        stop_nodes(nodes)
        wait_moorecoinds()
        shutil.rmtree(options.tmpdir)

    if success:
        print("tests successful")
        sys.exit(0)
    else:
        print("failed")
        sys.exit(1)

if __name__ == '__main__':
    main()
