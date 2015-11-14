# mininode.py - moorecoin p2p network half-a-node
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#
# this python code was modified from artforz' public domain  half-a-node, as
# found in the mini-node branch of http://github.com/jgarzik/pynode.
#
# nodeconn: an object which manages p2p connectivity to a moorecoin node
# nodeconncb: a base class that describes the interface for receiving
#             callbacks with network messages from a nodeconn
# cblock, ctransaction, cblockheader, ctxin, ctxout, etc....:
#     data structures that should map to corresponding structures in
#     moorecoin/primitives
# msg_block, msg_tx, msg_headers, etc.:
#     data structures that represent network messages
# ser_*, deser_*: functions that handle serialization/deserialization


import struct
import socket
import asyncore
import binascii
import time
import sys
import random
import cstringio
import hashlib
from threading import rlock
from threading import thread
import logging
import copy

bip0031_version = 60000
my_version = 60001  # past bip-31 for ping/pong
my_subversion = "/python-mininode-tester:0.0.1/"

max_inv_sz = 50000

# keep our own socket map for asyncore, so that we can track disconnects
# ourselves (to workaround an issue with closing an asyncore socket when 
# using select)
mininode_socket_map = dict()

# one lock for synchronizing all data access between the networking thread (see
# networkthread below) and the thread running the test logic.  for simplicity,
# nodeconn acquires this lock whenever delivering a message to to a nodeconncb,
# and whenever adding anything to the send buffer (in send_message()).  this
# lock should be acquired in the thread running the test logic to synchronize
# access to any data shared with the nodeconncb or nodeconn.
mininode_lock = rlock()

# serialization/deserialization tools
def sha256(s):
    return hashlib.new('sha256', s).digest()


def hash256(s):
    return sha256(sha256(s))


def deser_string(f):
    nit = struct.unpack("<b", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<h", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<i", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<q", f.read(8))[0]
    return f.read(nit)


def ser_string(s):
    if len(s) < 253:
        return chr(len(s)) + s
    elif len(s) < 0x10000:
        return chr(253) + struct.pack("<h", len(s)) + s
    elif len(s) < 0x100000000l:
        return chr(254) + struct.pack("<i", len(s)) + s
    return chr(255) + struct.pack("<q", len(s)) + s


def deser_uint256(f):
    r = 0l
    for i in xrange(8):
        t = struct.unpack("<i", f.read(4))[0]
        r += t << (i * 32)
    return r


def ser_uint256(u):
    rs = ""
    for i in xrange(8):
        rs += struct.pack("<i", u & 0xffffffffl)
        u >>= 32
    return rs


def uint256_from_str(s):
    r = 0l
    t = struct.unpack("<iiiiiiii", s[:32])
    for i in xrange(8):
        r += t[i] << (i * 32)
    return r


def uint256_from_compact(c):
    nbytes = (c >> 24) & 0xff
    v = (c & 0xffffffl) << (8 * (nbytes - 3))
    return v


def deser_vector(f, c):
    nit = struct.unpack("<b", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<h", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<i", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = c()
        t.deserialize(f)
        r.append(t)
    return r


def ser_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<h", len(l))
    elif len(l) < 0x100000000l:
        r = chr(254) + struct.pack("<i", len(l))
    else:
        r = chr(255) + struct.pack("<q", len(l))
    for i in l:
        r += i.serialize()
    return r


def deser_uint256_vector(f):
    nit = struct.unpack("<b", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<h", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<i", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = deser_uint256(f)
        r.append(t)
    return r


def ser_uint256_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<h", len(l))
    elif len(l) < 0x100000000l:
        r = chr(254) + struct.pack("<i", len(l))
    else:
        r = chr(255) + struct.pack("<q", len(l))
    for i in l:
        r += ser_uint256(i)
    return r


def deser_string_vector(f):
    nit = struct.unpack("<b", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<h", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<i", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = deser_string(f)
        r.append(t)
    return r


def ser_string_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<h", len(l))
    elif len(l) < 0x100000000l:
        r = chr(254) + struct.pack("<i", len(l))
    else:
        r = chr(255) + struct.pack("<q", len(l))
    for sv in l:
        r += ser_string(sv)
    return r


def deser_int_vector(f):
    nit = struct.unpack("<b", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<h", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<i", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<q", f.read(8))[0]
    r = []
    for i in xrange(nit):
        t = struct.unpack("<i", f.read(4))[0]
        r.append(t)
    return r


def ser_int_vector(l):
    r = ""
    if len(l) < 253:
        r = chr(len(l))
    elif len(l) < 0x10000:
        r = chr(253) + struct.pack("<h", len(l))
    elif len(l) < 0x100000000l:
        r = chr(254) + struct.pack("<i", len(l))
    else:
        r = chr(255) + struct.pack("<q", len(l))
    for i in l:
        r += struct.pack("<i", i)
    return r


# objects that map to moorecoind objects, which can be serialized/deserialized

class caddress(object):
    def __init__(self):
        self.nservices = 1
        self.pchreserved = "\x00" * 10 + "\xff" * 2
        self.ip = "0.0.0.0"
        self.port = 0

    def deserialize(self, f):
        self.nservices = struct.unpack("<q", f.read(8))[0]
        self.pchreserved = f.read(12)
        self.ip = socket.inet_ntoa(f.read(4))
        self.port = struct.unpack(">h", f.read(2))[0]

    def serialize(self):
        r = ""
        r += struct.pack("<q", self.nservices)
        r += self.pchreserved
        r += socket.inet_aton(self.ip)
        r += struct.pack(">h", self.port)
        return r

    def __repr__(self):
        return "caddress(nservices=%i ip=%s port=%i)" % (self.nservices,
                                                         self.ip, self.port)


class cinv(object):
    typemap = {
        0: "error",
        1: "tx",
        2: "block"}

    def __init__(self, t=0, h=0l):
        self.type = t
        self.hash = h

    def deserialize(self, f):
        self.type = struct.unpack("<i", f.read(4))[0]
        self.hash = deser_uint256(f)

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.type)
        r += ser_uint256(self.hash)
        return r

    def __repr__(self):
        return "cinv(type=%s hash=%064x)" \
            % (self.typemap[self.type], self.hash)


class cblocklocator(object):
    def __init__(self):
        self.nversion = my_version
        self.vhave = []

    def deserialize(self, f):
        self.nversion = struct.unpack("<i", f.read(4))[0]
        self.vhave = deser_uint256_vector(f)

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nversion)
        r += ser_uint256_vector(self.vhave)
        return r

    def __repr__(self):
        return "cblocklocator(nversion=%i vhave=%s)" \
            % (self.nversion, repr(self.vhave))


class coutpoint(object):
    def __init__(self, hash=0, n=0):
        self.hash = hash
        self.n = n

    def deserialize(self, f):
        self.hash = deser_uint256(f)
        self.n = struct.unpack("<i", f.read(4))[0]

    def serialize(self):
        r = ""
        r += ser_uint256(self.hash)
        r += struct.pack("<i", self.n)
        return r

    def __repr__(self):
        return "coutpoint(hash=%064x n=%i)" % (self.hash, self.n)


class ctxin(object):
    def __init__(self, outpoint=none, scriptsig="", nsequence=0):
        if outpoint is none:
            self.prevout = coutpoint()
        else:
            self.prevout = outpoint
        self.scriptsig = scriptsig
        self.nsequence = nsequence

    def deserialize(self, f):
        self.prevout = coutpoint()
        self.prevout.deserialize(f)
        self.scriptsig = deser_string(f)
        self.nsequence = struct.unpack("<i", f.read(4))[0]

    def serialize(self):
        r = ""
        r += self.prevout.serialize()
        r += ser_string(self.scriptsig)
        r += struct.pack("<i", self.nsequence)
        return r

    def __repr__(self):
        return "ctxin(prevout=%s scriptsig=%s nsequence=%i)" \
            % (repr(self.prevout), binascii.hexlify(self.scriptsig),
               self.nsequence)


class ctxout(object):
    def __init__(self, nvalue=0, scriptpubkey=""):
        self.nvalue = nvalue
        self.scriptpubkey = scriptpubkey

    def deserialize(self, f):
        self.nvalue = struct.unpack("<q", f.read(8))[0]
        self.scriptpubkey = deser_string(f)

    def serialize(self):
        r = ""
        r += struct.pack("<q", self.nvalue)
        r += ser_string(self.scriptpubkey)
        return r

    def __repr__(self):
        return "ctxout(nvalue=%i.%08i scriptpubkey=%s)" \
            % (self.nvalue // 100000000, self.nvalue % 100000000,
               binascii.hexlify(self.scriptpubkey))


class ctransaction(object):
    def __init__(self, tx=none):
        if tx is none:
            self.nversion = 1
            self.vin = []
            self.vout = []
            self.nlocktime = 0
            self.sha256 = none
            self.hash = none
        else:
            self.nversion = tx.nversion
            self.vin = copy.deepcopy(tx.vin)
            self.vout = copy.deepcopy(tx.vout)
            self.nlocktime = tx.nlocktime
            self.sha256 = none
            self.hash = none

    def deserialize(self, f):
        self.nversion = struct.unpack("<i", f.read(4))[0]
        self.vin = deser_vector(f, ctxin)
        self.vout = deser_vector(f, ctxout)
        self.nlocktime = struct.unpack("<i", f.read(4))[0]
        self.sha256 = none
        self.hash = none

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nversion)
        r += ser_vector(self.vin)
        r += ser_vector(self.vout)
        r += struct.pack("<i", self.nlocktime)
        return r

    def rehash(self):
        self.sha256 = none
        self.calc_sha256()

    def calc_sha256(self):
        if self.sha256 is none:
            self.sha256 = uint256_from_str(hash256(self.serialize()))
        self.hash = hash256(self.serialize())[::-1].encode('hex_codec')

    def is_valid(self):
        self.calc_sha256()
        for tout in self.vout:
            if tout.nvalue < 0 or tout.nvalue > 21000000l * 100000000l:
                return false
        return true

    def __repr__(self):
        return "ctransaction(nversion=%i vin=%s vout=%s nlocktime=%i)" \
            % (self.nversion, repr(self.vin), repr(self.vout), self.nlocktime)


class cblockheader(object):
    def __init__(self, header=none):
        if header is none:
            self.set_null()
        else:
            self.nversion = header.nversion
            self.hashprevblock = header.hashprevblock
            self.hashmerkleroot = header.hashmerkleroot
            self.ntime = header.ntime
            self.nbits = header.nbits
            self.nnonce = header.nnonce
            self.sha256 = header.sha256
            self.hash = header.hash
            self.calc_sha256()

    def set_null(self):
        self.nversion = 1
        self.hashprevblock = 0
        self.hashmerkleroot = 0
        self.ntime = 0
        self.nbits = 0
        self.nnonce = 0
        self.sha256 = none
        self.hash = none

    def deserialize(self, f):
        self.nversion = struct.unpack("<i", f.read(4))[0]
        self.hashprevblock = deser_uint256(f)
        self.hashmerkleroot = deser_uint256(f)
        self.ntime = struct.unpack("<i", f.read(4))[0]
        self.nbits = struct.unpack("<i", f.read(4))[0]
        self.nnonce = struct.unpack("<i", f.read(4))[0]
        self.sha256 = none
        self.hash = none

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nversion)
        r += ser_uint256(self.hashprevblock)
        r += ser_uint256(self.hashmerkleroot)
        r += struct.pack("<i", self.ntime)
        r += struct.pack("<i", self.nbits)
        r += struct.pack("<i", self.nnonce)
        return r

    def calc_sha256(self):
        if self.sha256 is none:
            r = ""
            r += struct.pack("<i", self.nversion)
            r += ser_uint256(self.hashprevblock)
            r += ser_uint256(self.hashmerkleroot)
            r += struct.pack("<i", self.ntime)
            r += struct.pack("<i", self.nbits)
            r += struct.pack("<i", self.nnonce)
            self.sha256 = uint256_from_str(hash256(r))
            self.hash = hash256(r)[::-1].encode('hex_codec')

    def rehash(self):
        self.sha256 = none
        self.calc_sha256()
        return self.sha256

    def __repr__(self):
        return "cblockheader(nversion=%i hashprevblock=%064x hashmerkleroot=%064x ntime=%s nbits=%08x nnonce=%08x)" \
            % (self.nversion, self.hashprevblock, self.hashmerkleroot,
               time.ctime(self.ntime), self.nbits, self.nnonce)


class cblock(cblockheader):
    def __init__(self, header=none):
        super(cblock, self).__init__(header)
        self.vtx = []

    def deserialize(self, f):
        super(cblock, self).deserialize(f)
        self.vtx = deser_vector(f, ctransaction)

    def serialize(self):
        r = ""
        r += super(cblock, self).serialize()
        r += ser_vector(self.vtx)
        return r

    def calc_merkle_root(self):
        hashes = []
        for tx in self.vtx:
            tx.calc_sha256()
            hashes.append(ser_uint256(tx.sha256))
        while len(hashes) > 1:
            newhashes = []
            for i in xrange(0, len(hashes), 2):
                i2 = min(i+1, len(hashes)-1)
                newhashes.append(hash256(hashes[i] + hashes[i2]))
            hashes = newhashes
        return uint256_from_str(hashes[0])

    def is_valid(self):
        self.calc_sha256()
        target = uint256_from_compact(self.nbits)
        if self.sha256 > target:
            return false
        for tx in self.vtx:
            if not tx.is_valid():
                return false
        if self.calc_merkle_root() != self.hashmerkleroot:
            return false
        return true

    def solve(self):
        self.calc_sha256()
        target = uint256_from_compact(self.nbits)
        while self.sha256 > target:
            self.nnonce += 1
            self.rehash()

    def __repr__(self):
        return "cblock(nversion=%i hashprevblock=%064x hashmerkleroot=%064x ntime=%s nbits=%08x nnonce=%08x vtx=%s)" \
            % (self.nversion, self.hashprevblock, self.hashmerkleroot,
               time.ctime(self.ntime), self.nbits, self.nnonce, repr(self.vtx))


class cunsignedalert(object):
    def __init__(self):
        self.nversion = 1
        self.nrelayuntil = 0
        self.nexpiration = 0
        self.nid = 0
        self.ncancel = 0
        self.setcancel = []
        self.nminver = 0
        self.nmaxver = 0
        self.setsubver = []
        self.npriority = 0
        self.strcomment = ""
        self.strstatusbar = ""
        self.strreserved = ""

    def deserialize(self, f):
        self.nversion = struct.unpack("<i", f.read(4))[0]
        self.nrelayuntil = struct.unpack("<q", f.read(8))[0]
        self.nexpiration = struct.unpack("<q", f.read(8))[0]
        self.nid = struct.unpack("<i", f.read(4))[0]
        self.ncancel = struct.unpack("<i", f.read(4))[0]
        self.setcancel = deser_int_vector(f)
        self.nminver = struct.unpack("<i", f.read(4))[0]
        self.nmaxver = struct.unpack("<i", f.read(4))[0]
        self.setsubver = deser_string_vector(f)
        self.npriority = struct.unpack("<i", f.read(4))[0]
        self.strcomment = deser_string(f)
        self.strstatusbar = deser_string(f)
        self.strreserved = deser_string(f)

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nversion)
        r += struct.pack("<q", self.nrelayuntil)
        r += struct.pack("<q", self.nexpiration)
        r += struct.pack("<i", self.nid)
        r += struct.pack("<i", self.ncancel)
        r += ser_int_vector(self.setcancel)
        r += struct.pack("<i", self.nminver)
        r += struct.pack("<i", self.nmaxver)
        r += ser_string_vector(self.setsubver)
        r += struct.pack("<i", self.npriority)
        r += ser_string(self.strcomment)
        r += ser_string(self.strstatusbar)
        r += ser_string(self.strreserved)
        return r

    def __repr__(self):
        return "cunsignedalert(nversion %d, nrelayuntil %d, nexpiration %d, nid %d, ncancel %d, nminver %d, nmaxver %d, npriority %d, strcomment %s, strstatusbar %s, strreserved %s)" \
            % (self.nversion, self.nrelayuntil, self.nexpiration, self.nid,
               self.ncancel, self.nminver, self.nmaxver, self.npriority,
               self.strcomment, self.strstatusbar, self.strreserved)


class calert(object):
    def __init__(self):
        self.vchmsg = ""
        self.vchsig = ""

    def deserialize(self, f):
        self.vchmsg = deser_string(f)
        self.vchsig = deser_string(f)

    def serialize(self):
        r = ""
        r += ser_string(self.vchmsg)
        r += ser_string(self.vchsig)
        return r

    def __repr__(self):
        return "calert(vchmsg.sz %d, vchsig.sz %d)" \
            % (len(self.vchmsg), len(self.vchsig))


# objects that correspond to messages on the wire
class msg_version(object):
    command = "version"

    def __init__(self):
        self.nversion = my_version
        self.nservices = 1
        self.ntime = time.time()
        self.addrto = caddress()
        self.addrfrom = caddress()
        self.nnonce = random.getrandbits(64)
        self.strsubver = my_subversion
        self.nstartingheight = -1

    def deserialize(self, f):
        self.nversion = struct.unpack("<i", f.read(4))[0]
        if self.nversion == 10300:
            self.nversion = 300
        self.nservices = struct.unpack("<q", f.read(8))[0]
        self.ntime = struct.unpack("<q", f.read(8))[0]
        self.addrto = caddress()
        self.addrto.deserialize(f)
        if self.nversion >= 106:
            self.addrfrom = caddress()
            self.addrfrom.deserialize(f)
            self.nnonce = struct.unpack("<q", f.read(8))[0]
            self.strsubver = deser_string(f)
            if self.nversion >= 209:
                self.nstartingheight = struct.unpack("<i", f.read(4))[0]
            else:
                self.nstartingheight = none
        else:
            self.addrfrom = none
            self.nnonce = none
            self.strsubver = none
            self.nstartingheight = none

    def serialize(self):
        r = ""
        r += struct.pack("<i", self.nversion)
        r += struct.pack("<q", self.nservices)
        r += struct.pack("<q", self.ntime)
        r += self.addrto.serialize()
        r += self.addrfrom.serialize()
        r += struct.pack("<q", self.nnonce)
        r += ser_string(self.strsubver)
        r += struct.pack("<i", self.nstartingheight)
        return r

    def __repr__(self):
        return 'msg_version(nversion=%i nservices=%i ntime=%s addrto=%s addrfrom=%s nnonce=0x%016x strsubver=%s nstartingheight=%i)' \
            % (self.nversion, self.nservices, time.ctime(self.ntime),
               repr(self.addrto), repr(self.addrfrom), self.nnonce,
               self.strsubver, self.nstartingheight)


class msg_verack(object):
    command = "verack"

    def __init__(self):
        pass

    def deserialize(self, f):
        pass

    def serialize(self):
        return ""

    def __repr__(self):
        return "msg_verack()"


class msg_addr(object):
    command = "addr"

    def __init__(self):
        self.addrs = []

    def deserialize(self, f):
        self.addrs = deser_vector(f, caddress)

    def serialize(self):
        return ser_vector(self.addrs)

    def __repr__(self):
        return "msg_addr(addrs=%s)" % (repr(self.addrs))


class msg_alert(object):
    command = "alert"

    def __init__(self):
        self.alert = calert()

    def deserialize(self, f):
        self.alert = calert()
        self.alert.deserialize(f)

    def serialize(self):
        r = ""
        r += self.alert.serialize()
        return r

    def __repr__(self):
        return "msg_alert(alert=%s)" % (repr(self.alert), )


class msg_inv(object):
    command = "inv"

    def __init__(self, inv=none):
        if inv is none:
            self.inv = []
        else:
            self.inv = inv

    def deserialize(self, f):
        self.inv = deser_vector(f, cinv)

    def serialize(self):
        return ser_vector(self.inv)

    def __repr__(self):
        return "msg_inv(inv=%s)" % (repr(self.inv))


class msg_getdata(object):
    command = "getdata"

    def __init__(self):
        self.inv = []

    def deserialize(self, f):
        self.inv = deser_vector(f, cinv)

    def serialize(self):
        return ser_vector(self.inv)

    def __repr__(self):
        return "msg_getdata(inv=%s)" % (repr(self.inv))


class msg_getblocks(object):
    command = "getblocks"

    def __init__(self):
        self.locator = cblocklocator()
        self.hashstop = 0l

    def deserialize(self, f):
        self.locator = cblocklocator()
        self.locator.deserialize(f)
        self.hashstop = deser_uint256(f)

    def serialize(self):
        r = ""
        r += self.locator.serialize()
        r += ser_uint256(self.hashstop)
        return r

    def __repr__(self):
        return "msg_getblocks(locator=%s hashstop=%064x)" \
            % (repr(self.locator), self.hashstop)


class msg_tx(object):
    command = "tx"

    def __init__(self, tx=ctransaction()):
        self.tx = tx

    def deserialize(self, f):
        self.tx.deserialize(f)

    def serialize(self):
        return self.tx.serialize()

    def __repr__(self):
        return "msg_tx(tx=%s)" % (repr(self.tx))


class msg_block(object):
    command = "block"

    def __init__(self, block=none):
        if block is none:
            self.block = cblock()
        else:
            self.block = block

    def deserialize(self, f):
        self.block.deserialize(f)

    def serialize(self):
        return self.block.serialize()

    def __repr__(self):
        return "msg_block(block=%s)" % (repr(self.block))


class msg_getaddr(object):
    command = "getaddr"

    def __init__(self):
        pass

    def deserialize(self, f):
        pass

    def serialize(self):
        return ""

    def __repr__(self):
        return "msg_getaddr()"


class msg_ping_prebip31(object):
    command = "ping"

    def __init__(self):
        pass

    def deserialize(self, f):
        pass

    def serialize(self):
        return ""

    def __repr__(self):
        return "msg_ping() (pre-bip31)"


class msg_ping(object):
    command = "ping"

    def __init__(self, nonce=0l):
        self.nonce = nonce

    def deserialize(self, f):
        self.nonce = struct.unpack("<q", f.read(8))[0]

    def serialize(self):
        r = ""
        r += struct.pack("<q", self.nonce)
        return r

    def __repr__(self):
        return "msg_ping(nonce=%08x)" % self.nonce


class msg_pong(object):
    command = "pong"

    def __init__(self, nonce=0l):
        self.nonce = nonce

    def deserialize(self, f):
        self.nonce = struct.unpack("<q", f.read(8))[0]

    def serialize(self):
        r = ""
        r += struct.pack("<q", self.nonce)
        return r

    def __repr__(self):
        return "msg_pong(nonce=%08x)" % self.nonce


class msg_mempool(object):
    command = "mempool"

    def __init__(self):
        pass

    def deserialize(self, f):
        pass

    def serialize(self):
        return ""

    def __repr__(self):
        return "msg_mempool()"


# getheaders message has
# number of entries
# vector of hashes
# hash_stop (hash of last desired block header, 0 to get as many as possible)
class msg_getheaders(object):
    command = "getheaders"

    def __init__(self):
        self.locator = cblocklocator()
        self.hashstop = 0l

    def deserialize(self, f):
        self.locator = cblocklocator()
        self.locator.deserialize(f)
        self.hashstop = deser_uint256(f)

    def serialize(self):
        r = ""
        r += self.locator.serialize()
        r += ser_uint256(self.hashstop)
        return r

    def __repr__(self):
        return "msg_getheaders(locator=%s, stop=%064x)" \
            % (repr(self.locator), self.hashstop)


# headers message has
# <count> <vector of block headers>
class msg_headers(object):
    command = "headers"

    def __init__(self):
        self.headers = []

    def deserialize(self, f):
        # comment in moorecoind indicates these should be deserialized as blocks
        blocks = deser_vector(f, cblock)
        for x in blocks:
            self.headers.append(cblockheader(x))

    def serialize(self):
        blocks = [cblock(x) for x in self.headers]
        return ser_vector(blocks)

    def __repr__(self):
        return "msg_headers(headers=%s)" % repr(self.headers)


class msg_reject(object):
    command = "reject"

    def __init__(self):
        self.message = ""
        self.code = ""
        self.reason = ""
        self.data = 0l

    def deserialize(self, f):
        self.message = deser_string(f)
        self.code = struct.unpack("<b", f.read(1))[0]
        self.reason = deser_string(f)
        if (self.message == "block" or self.message == "tx"):
            self.data = deser_uint256(f)

    def serialize(self):
        r = ser_string(self.message)
        r += struct.pack("<b", self.code)
        r += ser_string(self.reason)
        if (self.message == "block" or self.message == "tx"):
            r += ser_uint256(self.data)
        return r

    def __repr__(self):
        return "msg_reject: %s %d %s [%064x]" \
            % (self.message, self.code, self.reason, self.data)


# this is what a callback should look like for nodeconn
# reimplement the on_* functions to provide handling for events
class nodeconncb(object):
    def __init__(self):
        self.verack_received = false

    # derived classes should call this function once to set the message map
    # which associates the derived classes' functions to incoming messages
    def create_callback_map(self):
        self.cbmap = {
            "version": self.on_version,
            "verack": self.on_verack,
            "addr": self.on_addr,
            "alert": self.on_alert,
            "inv": self.on_inv,
            "getdata": self.on_getdata,
            "getblocks": self.on_getblocks,
            "tx": self.on_tx,
            "block": self.on_block,
            "getaddr": self.on_getaddr,
            "ping": self.on_ping,
            "pong": self.on_pong,
            "headers": self.on_headers,
            "getheaders": self.on_getheaders,
            "reject": self.on_reject,
            "mempool": self.on_mempool
        }

    def deliver(self, conn, message):
        with mininode_lock:
            try:
                self.cbmap[message.command](conn, message)
            except:
                print "error delivering %s (%s)" % (repr(message),
                                                    sys.exc_info()[0])

    def on_version(self, conn, message):
        if message.nversion >= 209:
            conn.send_message(msg_verack())
        conn.ver_send = min(my_version, message.nversion)
        if message.nversion < 209:
            conn.ver_recv = conn.ver_send

    def on_verack(self, conn, message):
        conn.ver_recv = conn.ver_send
        self.verack_received = true

    def on_inv(self, conn, message):
        want = msg_getdata()
        for i in message.inv:
            if i.type != 0:
                want.inv.append(i)
        if len(want.inv):
            conn.send_message(want)

    def on_addr(self, conn, message): pass
    def on_alert(self, conn, message): pass
    def on_getdata(self, conn, message): pass
    def on_getblocks(self, conn, message): pass
    def on_tx(self, conn, message): pass
    def on_block(self, conn, message): pass
    def on_getaddr(self, conn, message): pass
    def on_headers(self, conn, message): pass
    def on_getheaders(self, conn, message): pass
    def on_ping(self, conn, message):
        if conn.ver_send > bip0031_version:
            conn.send_message(msg_pong(message.nonce))
    def on_reject(self, conn, message): pass
    def on_close(self, conn): pass
    def on_mempool(self, conn): pass
    def on_pong(self, conn, message): pass


# the actual nodeconn class
# this class provides an interface for a p2p connection to a specified node
class nodeconn(asyncore.dispatcher):
    messagemap = {
        "version": msg_version,
        "verack": msg_verack,
        "addr": msg_addr,
        "alert": msg_alert,
        "inv": msg_inv,
        "getdata": msg_getdata,
        "getblocks": msg_getblocks,
        "tx": msg_tx,
        "block": msg_block,
        "getaddr": msg_getaddr,
        "ping": msg_ping,
        "pong": msg_pong,
        "headers": msg_headers,
        "getheaders": msg_getheaders,
        "reject": msg_reject,
        "mempool": msg_mempool
    }
    magic_bytes = {
        "mainnet": "\xf9\xbe\xb4\xd9",   # mainnet
        "testnet3": "\x0b\x11\x09\x07",  # testnet3
        "regtest": "\xfa\xbf\xb5\xda"    # regtest
    }

    def __init__(self, dstaddr, dstport, rpc, callback, net="regtest"):
        asyncore.dispatcher.__init__(self, map=mininode_socket_map)
        self.log = logging.getlogger("nodeconn(%s:%d)" % (dstaddr, dstport))
        self.dstaddr = dstaddr
        self.dstport = dstport
        self.create_socket(socket.af_inet, socket.sock_stream)
        self.sendbuf = ""
        self.recvbuf = ""
        self.ver_send = 209
        self.ver_recv = 209
        self.last_sent = 0
        self.state = "connecting"
        self.network = net
        self.cb = callback
        self.disconnect = false

        # stuff version msg into sendbuf
        vt = msg_version()
        vt.addrto.ip = self.dstaddr
        vt.addrto.port = self.dstport
        vt.addrfrom.ip = "0.0.0.0"
        vt.addrfrom.port = 0
        self.send_message(vt, true)
        print 'mininode: connecting to moorecoin node ip # ' + dstaddr + ':' \
            + str(dstport)

        try:
            self.connect((dstaddr, dstport))
        except:
            self.handle_close()
        self.rpc = rpc

    def show_debug_msg(self, msg):
        self.log.debug(msg)

    def handle_connect(self):
        self.show_debug_msg("mininode: connected & listening: \n")
        self.state = "connected"

    def handle_close(self):
        self.show_debug_msg("mininode: closing connection to %s:%d... "
                            % (self.dstaddr, self.dstport))
        self.state = "closed"
        self.recvbuf = ""
        self.sendbuf = ""
        try:
            self.close()
        except:
            pass
        self.cb.on_close(self)

    def handle_read(self):
        try:
            t = self.recv(8192)
            if len(t) > 0:
                self.recvbuf += t
                self.got_data()
        except:
            pass

    def readable(self):
        return true

    def writable(self):
        with mininode_lock:
            length = len(self.sendbuf)
        return (length > 0)

    def handle_write(self):
        with mininode_lock:
            try:
                sent = self.send(self.sendbuf)
            except:
                self.handle_close()
                return
            self.sendbuf = self.sendbuf[sent:]

    def got_data(self):
        while true:
            if len(self.recvbuf) < 4:
                return
            if self.recvbuf[:4] != self.magic_bytes[self.network]:
                raise valueerror("got garbage %s" % repr(self.recvbuf))
            if self.ver_recv < 209:
                if len(self.recvbuf) < 4 + 12 + 4:
                    return
                command = self.recvbuf[4:4+12].split("\x00", 1)[0]
                msglen = struct.unpack("<i", self.recvbuf[4+12:4+12+4])[0]
                checksum = none
                if len(self.recvbuf) < 4 + 12 + 4 + msglen:
                    return
                msg = self.recvbuf[4+12+4:4+12+4+msglen]
                self.recvbuf = self.recvbuf[4+12+4+msglen:]
            else:
                if len(self.recvbuf) < 4 + 12 + 4 + 4:
                    return
                command = self.recvbuf[4:4+12].split("\x00", 1)[0]
                msglen = struct.unpack("<i", self.recvbuf[4+12:4+12+4])[0]
                checksum = self.recvbuf[4+12+4:4+12+4+4]
                if len(self.recvbuf) < 4 + 12 + 4 + 4 + msglen:
                    return
                msg = self.recvbuf[4+12+4+4:4+12+4+4+msglen]
                th = sha256(msg)
                h = sha256(th)
                if checksum != h[:4]:
                    raise valueerror("got bad checksum " + repr(self.recvbuf))
                self.recvbuf = self.recvbuf[4+12+4+4+msglen:]
            if command in self.messagemap:
                f = cstringio.stringio(msg)
                t = self.messagemap[command]()
                t.deserialize(f)
                self.got_message(t)
            else:
                self.show_debug_msg("unknown command: '" + command + "' " +
                                    repr(msg))

    def send_message(self, message, pushbuf=false):
        if self.state != "connected" and not pushbuf:
            return
        self.show_debug_msg("send %s" % repr(message))
        command = message.command
        data = message.serialize()
        tmsg = self.magic_bytes[self.network]
        tmsg += command
        tmsg += "\x00" * (12 - len(command))
        tmsg += struct.pack("<i", len(data))
        if self.ver_send >= 209:
            th = sha256(data)
            h = sha256(th)
            tmsg += h[:4]
        tmsg += data
        with mininode_lock:
            self.sendbuf += tmsg
            self.last_sent = time.time()

    def got_message(self, message):
        if message.command == "version":
            if message.nversion <= bip0031_version:
                self.messagemap['ping'] = msg_ping_prebip31
        if self.last_sent + 30 * 60 < time.time():
            self.send_message(self.messagemap['ping']())
        self.show_debug_msg("recv %s" % repr(message))
        self.cb.deliver(self, message)

    def disconnect_node(self):
        self.disconnect = true


class networkthread(thread):
    def run(self):
        while mininode_socket_map:
            # we check for whether to disconnect outside of the asyncore
            # loop to workaround the behavior of asyncore when using
            # select
            disconnected = []
            for fd, obj in mininode_socket_map.items():
                if obj.disconnect:
                    disconnected.append(obj)
            [ obj.handle_close() for obj in disconnected ]
            asyncore.loop(0.1, use_poll=true, map=mininode_socket_map, count=1)


# an exception we can raise if we detect a potential disconnect
# (p2p or rpc) before the test is complete
class earlydisconnecterror(exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)
