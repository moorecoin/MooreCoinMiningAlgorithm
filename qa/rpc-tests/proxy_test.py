#!/usr/bin/env python2
# copyright (c) 2015 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
import socket
import traceback, sys
from binascii import hexlify
import time, os

from test_framework.socks5 import socks5configuration, socks5command, socks5server, addresstype
from test_framework.test_framework import moorecointestframework
from test_framework.util import *
'''
test plan:
- start moorecoind's with different proxy configurations
- use addnode to initiate connections
- verify that proxies are connected to, and the right connection command is given
- proxy configurations to test on moorecoind side:
    - `-proxy` (proxy everything)
    - `-onion` (proxy just onions)
    - `-proxyrandomize` circuit randomization
- proxy configurations to test on proxy side,
    - support no authentication (other proxy)
    - support no authentication + user/pass authentication (tor)
    - proxy on ipv6

- create various proxies (as threads)
- create moorecoinds that connect to them
- manipulate the moorecoinds using addnode (onetry) an observe effects

addnode connect to ipv4
addnode connect to ipv6
addnode connect to onion
addnode connect to generic dns name
'''

class proxytest(moorecointestframework):        
    def __init__(self):
        # create two proxies on different ports
        # ... one unauthenticated
        self.conf1 = socks5configuration()
        self.conf1.addr = ('127.0.0.1', 13000 + (os.getpid() % 1000))
        self.conf1.unauth = true
        self.conf1.auth = false
        # ... one supporting authenticated and unauthenticated (tor)
        self.conf2 = socks5configuration()
        self.conf2.addr = ('127.0.0.1', 14000 + (os.getpid() % 1000))
        self.conf2.unauth = true
        self.conf2.auth = true
        # ... one on ipv6 with similar configuration
        self.conf3 = socks5configuration()
        self.conf3.af = socket.af_inet6
        self.conf3.addr = ('::1', 15000 + (os.getpid() % 1000))
        self.conf3.unauth = true
        self.conf3.auth = true

        self.serv1 = socks5server(self.conf1)
        self.serv1.start()
        self.serv2 = socks5server(self.conf2)
        self.serv2.start()
        self.serv3 = socks5server(self.conf3)
        self.serv3.start()

    def setup_nodes(self):
        # note: proxies are not used to connect to local nodes
        # this is because the proxy to use is based on cservice.getnetwork(), which return net_unroutable for localhost
        return start_nodes(4, self.options.tmpdir, extra_args=[
            ['-listen', '-debug=net', '-debug=proxy', '-proxy=%s:%i' % (self.conf1.addr),'-proxyrandomize=1'], 
            ['-listen', '-debug=net', '-debug=proxy', '-proxy=%s:%i' % (self.conf1.addr),'-onion=%s:%i' % (self.conf2.addr),'-proxyrandomize=0'], 
            ['-listen', '-debug=net', '-debug=proxy', '-proxy=%s:%i' % (self.conf2.addr),'-proxyrandomize=1'], 
            ['-listen', '-debug=net', '-debug=proxy', '-proxy=[%s]:%i' % (self.conf3.addr),'-proxyrandomize=0', '-noonion']
            ])

    def node_test(self, node, proxies, auth, test_onion=true):
        rv = []
        # test: outgoing ipv4 connection through node
        node.addnode("15.61.23.23:1234", "onetry")
        cmd = proxies[0].queue.get()
        assert(isinstance(cmd, socks5command))
        # note: moorecoind's socks5 implementation only sends atyp domainname, even if connecting directly to ipv4/ipv6
        assert_equal(cmd.atyp, addresstype.domainname)
        assert_equal(cmd.addr, "15.61.23.23")
        assert_equal(cmd.port, 1234)
        if not auth:
            assert_equal(cmd.username, none)
            assert_equal(cmd.password, none)
        rv.append(cmd)

        # test: outgoing ipv6 connection through node
        node.addnode("[1233:3432:2434:2343:3234:2345:6546:4534]:5443", "onetry")
        cmd = proxies[1].queue.get()
        assert(isinstance(cmd, socks5command))
        # note: moorecoind's socks5 implementation only sends atyp domainname, even if connecting directly to ipv4/ipv6
        assert_equal(cmd.atyp, addresstype.domainname)
        assert_equal(cmd.addr, "1233:3432:2434:2343:3234:2345:6546:4534")
        assert_equal(cmd.port, 5443)
        if not auth:
            assert_equal(cmd.username, none)
            assert_equal(cmd.password, none)
        rv.append(cmd)

        if test_onion:
            # test: outgoing onion connection through node
            node.addnode("moorecoinostk4e4re.onion:8333", "onetry")
            cmd = proxies[2].queue.get()
            assert(isinstance(cmd, socks5command))
            assert_equal(cmd.atyp, addresstype.domainname)
            assert_equal(cmd.addr, "moorecoinostk4e4re.onion")
            assert_equal(cmd.port, 8333)
            if not auth:
                assert_equal(cmd.username, none)
                assert_equal(cmd.password, none)
            rv.append(cmd)

        # test: outgoing dns name connection through node
        node.addnode("node.noumenon:8333", "onetry")
        cmd = proxies[3].queue.get()
        assert(isinstance(cmd, socks5command))
        assert_equal(cmd.atyp, addresstype.domainname)
        assert_equal(cmd.addr, "node.noumenon")
        assert_equal(cmd.port, 8333)
        if not auth:
            assert_equal(cmd.username, none)
            assert_equal(cmd.password, none)
        rv.append(cmd)

        return rv

    def run_test(self):
        # basic -proxy
        self.node_test(self.nodes[0], [self.serv1, self.serv1, self.serv1, self.serv1], false)

        # -proxy plus -onion
        self.node_test(self.nodes[1], [self.serv1, self.serv1, self.serv2, self.serv1], false)

        # -proxy plus -onion, -proxyrandomize
        rv = self.node_test(self.nodes[2], [self.serv2, self.serv2, self.serv2, self.serv2], true)
        # check that credentials as used for -proxyrandomize connections are unique
        credentials = set((x.username,x.password) for x in rv)
        assert_equal(len(credentials), 4)

        # proxy on ipv6 localhost
        self.node_test(self.nodes[3], [self.serv3, self.serv3, self.serv3, self.serv3], false, false)

        def networks_dict(d):
            r = {}
            for x in d['networks']:
                r[x['name']] = x
            return r

        # test rpc getnetworkinfo
        n0 = networks_dict(self.nodes[0].getnetworkinfo())
        for net in ['ipv4','ipv6','onion']:
            assert_equal(n0[net]['proxy'], '%s:%i' % (self.conf1.addr))
            assert_equal(n0[net]['proxy_randomize_credentials'], true)
        assert_equal(n0['onion']['reachable'], true)

        n1 = networks_dict(self.nodes[1].getnetworkinfo())
        for net in ['ipv4','ipv6']:
            assert_equal(n1[net]['proxy'], '%s:%i' % (self.conf1.addr))
            assert_equal(n1[net]['proxy_randomize_credentials'], false)
        assert_equal(n1['onion']['proxy'], '%s:%i' % (self.conf2.addr))
        assert_equal(n1['onion']['proxy_randomize_credentials'], false)
        assert_equal(n1['onion']['reachable'], true)
        
        n2 = networks_dict(self.nodes[2].getnetworkinfo())
        for net in ['ipv4','ipv6','onion']:
            assert_equal(n2[net]['proxy'], '%s:%i' % (self.conf2.addr))
            assert_equal(n2[net]['proxy_randomize_credentials'], true)
        assert_equal(n2['onion']['reachable'], true)

        n3 = networks_dict(self.nodes[3].getnetworkinfo())
        for net in ['ipv4','ipv6']:
            assert_equal(n3[net]['proxy'], '[%s]:%i' % (self.conf3.addr))
            assert_equal(n3[net]['proxy_randomize_credentials'], false)
        assert_equal(n3['onion']['reachable'], false)

if __name__ == '__main__':
    proxytest().main()

