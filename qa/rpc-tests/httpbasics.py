#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test rpc http basics
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

class httpbasicstest (moorecointestframework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, extra_args=[['-rpckeepalive=1'], ['-rpckeepalive=0'], [], []])

    def run_test(self):

        #################################################
        # lowlevel check for http persistent connection #
        #################################################
        url = urlparse.urlparse(self.nodes[0].url)
        authpair = url.username + ':' + url.password
        headers = {"authorization": "basic " + base64.b64encode(authpair)}

        conn = httplib.httpconnection(url.hostname, url.port)
        conn.connect()
        conn.request('post', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true)
        assert_equal(conn.sock!=none, true) #according to http/1.1 connection must still be open!

        #send 2nd request without closing connection
        conn.request('post', '/', '{"method": "getchaintips"}', headers)
        out2 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true) #must also response with a correct json-rpc message
        assert_equal(conn.sock!=none, true) #according to http/1.1 connection must still be open!
        conn.close()

        #same should be if we add keep-alive because this should be the std. behaviour
        headers = {"authorization": "basic " + base64.b64encode(authpair), "connection": "keep-alive"}

        conn = httplib.httpconnection(url.hostname, url.port)
        conn.connect()
        conn.request('post', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true)
        assert_equal(conn.sock!=none, true) #according to http/1.1 connection must still be open!

        #send 2nd request without closing connection
        conn.request('post', '/', '{"method": "getchaintips"}', headers)
        out2 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true) #must also response with a correct json-rpc message
        assert_equal(conn.sock!=none, true) #according to http/1.1 connection must still be open!
        conn.close()

        #now do the same with "connection: close"
        headers = {"authorization": "basic " + base64.b64encode(authpair), "connection":"close"}

        conn = httplib.httpconnection(url.hostname, url.port)
        conn.connect()
        conn.request('post', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true)
        assert_equal(conn.sock!=none, false) #now the connection must be closed after the response

        #node1 (2nd node) is running with disabled keep-alive option
        urlnode1 = urlparse.urlparse(self.nodes[1].url)
        authpair = urlnode1.username + ':' + urlnode1.password
        headers = {"authorization": "basic " + base64.b64encode(authpair)}

        conn = httplib.httpconnection(urlnode1.hostname, urlnode1.port)
        conn.connect()
        conn.request('post', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true)
        assert_equal(conn.sock!=none, false) #connection must be closed because keep-alive was set to false

        #node2 (third node) is running with standard keep-alive parameters which means keep-alive is off
        urlnode2 = urlparse.urlparse(self.nodes[2].url)
        authpair = urlnode2.username + ':' + urlnode2.password
        headers = {"authorization": "basic " + base64.b64encode(authpair)}

        conn = httplib.httpconnection(urlnode2.hostname, urlnode2.port)
        conn.connect()
        conn.request('post', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read();
        assert_equal('"error":null' in out1, true)
        assert_equal(conn.sock!=none, true) #connection must be closed because moorecoind should use keep-alive by default

if __name__ == '__main__':
    httpbasicstest ().main ()
