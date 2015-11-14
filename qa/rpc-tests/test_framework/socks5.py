# copyright (c) 2015 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
'''
dummy socks5 server for testing.
'''
from __future__ import print_function, division, unicode_literals
import socket, threading, queue
import traceback, sys

### protocol constants
class command:
    connect = 0x01

class addresstype:
    ipv4 = 0x01
    domainname = 0x03
    ipv6 = 0x04

### utility functions
def recvall(s, n):
    '''receive n bytes from a socket, or fail'''
    rv = bytearray()
    while n > 0:
        d = s.recv(n)
        if not d:
            raise ioerror('unexpected end of stream')
        rv.extend(d)
        n -= len(d)
    return rv

### implementation classes
class socks5configuration(object):
    '''proxy configuration'''
    def __init__(self):
        self.addr = none # bind address (must be set)
        self.af = socket.af_inet # bind address family
        self.unauth = false  # support unauthenticated
        self.auth = false  # support authentication

class socks5command(object):
    '''information about an incoming socks5 command'''
    def __init__(self, cmd, atyp, addr, port, username, password):
        self.cmd = cmd # command (one of command.*)
        self.atyp = atyp # address type (one of addresstype.*)
        self.addr = addr # address
        self.port = port # port to connect to
        self.username = username
        self.password = password
    def __repr__(self):
        return 'socks5command(%s,%s,%s,%s,%s,%s)' % (self.cmd, self.atyp, self.addr, self.port, self.username, self.password)

class socks5connection(object):
    def __init__(self, serv, conn, peer):
        self.serv = serv
        self.conn = conn
        self.peer = peer

    def handle(self):
        '''
        handle socks5 request according to rfc1928
        '''
        try:
            # verify socks version
            ver = recvall(self.conn, 1)[0]
            if ver != 0x05:
                raise ioerror('invalid socks version %i' % ver)
            # choose authentication method
            nmethods = recvall(self.conn, 1)[0]
            methods = bytearray(recvall(self.conn, nmethods))
            method = none
            if 0x02 in methods and self.serv.conf.auth:
                method = 0x02 # username/password
            elif 0x00 in methods and self.serv.conf.unauth:
                method = 0x00 # unauthenticated
            if method is none:
                raise ioerror('no supported authentication method was offered')
            # send response
            self.conn.sendall(bytearray([0x05, method]))
            # read authentication (optional)
            username = none
            password = none
            if method == 0x02:
                ver = recvall(self.conn, 1)[0]
                if ver != 0x01:
                    raise ioerror('invalid auth packet version %i' % ver)
                ulen = recvall(self.conn, 1)[0]
                username = str(recvall(self.conn, ulen))
                plen = recvall(self.conn, 1)[0]
                password = str(recvall(self.conn, plen))
                # send authentication response
                self.conn.sendall(bytearray([0x01, 0x00]))

            # read connect request
            (ver,cmd,rsv,atyp) = recvall(self.conn, 4)
            if ver != 0x05:
                raise ioerror('invalid socks version %i in connect request' % ver)
            if cmd != command.connect:
                raise ioerror('unhandled command %i in connect request' % cmd)

            if atyp == addresstype.ipv4:
                addr = recvall(self.conn, 4)
            elif atyp == addresstype.domainname:
                n = recvall(self.conn, 1)[0]
                addr = str(recvall(self.conn, n))
            elif atyp == addresstype.ipv6:
                addr = recvall(self.conn, 16)
            else:
                raise ioerror('unknown address type %i' % atyp)
            port_hi,port_lo = recvall(self.conn, 2)
            port = (port_hi << 8) | port_lo

            # send dummy response
            self.conn.sendall(bytearray([0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]))

            cmdin = socks5command(cmd, atyp, addr, port, username, password)
            self.serv.queue.put(cmdin)
            print('proxy: ', cmdin)
            # fall through to disconnect
        except exception,e:
            traceback.print_exc(file=sys.stderr)
            self.serv.queue.put(e)
        finally:
            self.conn.close()

class socks5server(object):
    def __init__(self, conf):
        self.conf = conf
        self.s = socket.socket(conf.af)
        self.s.setsockopt(socket.sol_socket, socket.so_reuseaddr, 1)
        self.s.bind(conf.addr)
        self.s.listen(5)
        self.running = false
        self.thread = none
        self.queue = queue.queue() # report connections and exceptions to client

    def run(self):
        while self.running:
            (sockconn, peer) = self.s.accept()
            if self.running:
                conn = socks5connection(self, sockconn, peer)
                thread = threading.thread(none, conn.handle)
                thread.daemon = true
                thread.start()
    
    def start(self):
        assert(not self.running)
        self.running = true
        self.thread = threading.thread(none, self.run)
        self.thread.daemon = true
        self.thread.start()

    def stop(self):
        self.running = false
        # connect to self to end run loop
        s = socket.socket(self.conf.af)
        s.connect(self.conf.addr)
        s.close()
        self.thread.join()

