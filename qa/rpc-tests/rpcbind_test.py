#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# test for -rpcbind, as well as -rpcallowip and -rpcconnect

# add python-moorecoinrpc to module search path:
import os
import sys

import json
import shutil
import subprocess
import tempfile
import traceback

from test_framework.util import *
from test_framework.netutil import *

def run_bind_test(tmpdir, allow_ips, connect_to, addresses, expected):
    '''
    start a node with requested rpcallowip and rpcbind parameters,
    then try to connect, and check if the set of bound addresses
    matches the expected set.
    '''
    expected = [(addr_to_hex(addr), port) for (addr, port) in expected]
    base_args = ['-disablewallet', '-nolisten']
    if allow_ips:
        base_args += ['-rpcallowip=' + x for x in allow_ips]
    binds = ['-rpcbind='+addr for addr in addresses]
    nodes = start_nodes(1, tmpdir, [base_args + binds], connect_to)
    try:
        pid = moorecoind_processes[0].pid
        assert_equal(set(get_bind_addrs(pid)), set(expected))
    finally:
        stop_nodes(nodes)
        wait_moorecoinds()

def run_allowip_test(tmpdir, allow_ips, rpchost, rpcport):
    '''
    start a node with rpcwallow ip, and request getinfo
    at a non-localhost ip.
    '''
    base_args = ['-disablewallet', '-nolisten'] + ['-rpcallowip='+x for x in allow_ips]
    nodes = start_nodes(1, tmpdir, [base_args])
    try:
        # connect to node through non-loopback interface
        url = "http://rt:rt@%s:%d" % (rpchost, rpcport,)
        node = authserviceproxy(url)
        node.getinfo()
    finally:
        node = none # make sure connection will be garbage collected and closed
        stop_nodes(nodes)
        wait_moorecoinds()


def run_test(tmpdir):
    assert(sys.platform == 'linux2') # due to os-specific network stats queries, this test works only on linux
    # find the first non-loopback interface for testing
    non_loopback_ip = none
    for name,ip in all_interfaces():
        if ip != '127.0.0.1':
            non_loopback_ip = ip
            break
    if non_loopback_ip is none:
        assert(not 'this test requires at least one non-loopback ipv4 interface')
    print("using interface %s for testing" % non_loopback_ip)

    defaultport = rpc_port(0)

    # check default without rpcallowip (ipv4 and ipv6 localhost)
    run_bind_test(tmpdir, none, '127.0.0.1', [],
        [('127.0.0.1', defaultport), ('::1', defaultport)])
    # check default with rpcallowip (ipv6 any)
    run_bind_test(tmpdir, ['127.0.0.1'], '127.0.0.1', [],
        [('::0', defaultport)])
    # check only ipv4 localhost (explicit)
    run_bind_test(tmpdir, ['127.0.0.1'], '127.0.0.1', ['127.0.0.1'],
        [('127.0.0.1', defaultport)])
    # check only ipv4 localhost (explicit) with alternative port
    run_bind_test(tmpdir, ['127.0.0.1'], '127.0.0.1:32171', ['127.0.0.1:32171'],
        [('127.0.0.1', 32171)])
    # check only ipv4 localhost (explicit) with multiple alternative ports on same host
    run_bind_test(tmpdir, ['127.0.0.1'], '127.0.0.1:32171', ['127.0.0.1:32171', '127.0.0.1:32172'],
        [('127.0.0.1', 32171), ('127.0.0.1', 32172)])
    # check only ipv6 localhost (explicit)
    run_bind_test(tmpdir, ['[::1]'], '[::1]', ['[::1]'],
        [('::1', defaultport)])
    # check both ipv4 and ipv6 localhost (explicit)
    run_bind_test(tmpdir, ['127.0.0.1'], '127.0.0.1', ['127.0.0.1', '[::1]'],
        [('127.0.0.1', defaultport), ('::1', defaultport)])
    # check only non-loopback interface
    run_bind_test(tmpdir, [non_loopback_ip], non_loopback_ip, [non_loopback_ip],
        [(non_loopback_ip, defaultport)])

    # check that with invalid rpcallowip, we are denied
    run_allowip_test(tmpdir, [non_loopback_ip], non_loopback_ip, defaultport)
    try:
        run_allowip_test(tmpdir, ['1.1.1.1'], non_loopback_ip, defaultport)
        assert(not 'connection not denied by rpcallowip as expected')
    except valueerror:
        pass

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

        run_test(options.tmpdir)

        success = true

    except assertionerror as e:
        print("assertion failed: "+e.message)
    except exception as e:
        print("unexpected exception caught during testing: "+str(e))
        traceback.print_tb(sys.exc_info()[2])

    if not options.nocleanup:
        print("cleaning up")
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
