#
# script.py
#
# this file is modified from python-moorecoinlib.
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

"""scripts

functionality to build scripts, as well as signaturehash().
"""

from __future__ import absolute_import, division, print_function, unicode_literals

from test_framework.mininode import ctransaction, ctxout, hash256

import sys
bchr = chr
bord = ord
if sys.version > '3':
    long = int
    bchr = lambda x: bytes([x])
    bord = lambda x: x

import copy
import struct

import test_framework.bignum

max_script_size = 10000
max_script_element_size = 520
max_script_opcodes = 201

opcode_names = {}

_opcode_instances = []
class cscriptop(int):
    """a single script opcode"""
    __slots__ = []

    @staticmethod
    def encode_op_pushdata(d):
        """encode a pushdata op, returning bytes"""
        if len(d) < 0x4c:
            return b'' + bchr(len(d)) + d # op_pushdata
        elif len(d) <= 0xff:
            return b'\x4c' + bchr(len(d)) + d # op_pushdata1
        elif len(d) <= 0xffff:
            return b'\x4d' + struct.pack(b'<h', len(d)) + d # op_pushdata2
        elif len(d) <= 0xffffffff:
            return b'\x4e' + struct.pack(b'<i', len(d)) + d # op_pushdata4
        else:
            raise valueerror("data too long to encode in a pushdata op")

    @staticmethod
    def encode_op_n(n):
        """encode a small integer op, returning an opcode"""
        if not (0 <= n <= 16):
            raise valueerror('integer must be in range 0 <= n <= 16, got %d' % n)

        if n == 0:
            return op_0
        else:
            return cscriptop(op_1 + n-1)

    def decode_op_n(self):
        """decode a small integer opcode, returning an integer"""
        if self == op_0:
            return 0

        if not (self == op_0 or op_1 <= self <= op_16):
            raise valueerror('op %r is not an op_n' % self)

        return int(self - op_1+1)

    def is_small_int(self):
        """return true if the op pushes a small integer to the stack"""
        if 0x51 <= self <= 0x60 or self == 0:
            return true
        else:
            return false

    def __str__(self):
        return repr(self)

    def __repr__(self):
        if self in opcode_names:
            return opcode_names[self]
        else:
            return 'cscriptop(0x%x)' % self

    def __new__(cls, n):
        try:
            return _opcode_instances[n]
        except indexerror:
            assert len(_opcode_instances) == n
            _opcode_instances.append(super(cscriptop, cls).__new__(cls, n))
            return _opcode_instances[n]

# populate opcode instance table
for n in range(0xff+1):
    cscriptop(n)


# push value
op_0 = cscriptop(0x00)
op_false = op_0
op_pushdata1 = cscriptop(0x4c)
op_pushdata2 = cscriptop(0x4d)
op_pushdata4 = cscriptop(0x4e)
op_1negate = cscriptop(0x4f)
op_reserved = cscriptop(0x50)
op_1 = cscriptop(0x51)
op_true=op_1
op_2 = cscriptop(0x52)
op_3 = cscriptop(0x53)
op_4 = cscriptop(0x54)
op_5 = cscriptop(0x55)
op_6 = cscriptop(0x56)
op_7 = cscriptop(0x57)
op_8 = cscriptop(0x58)
op_9 = cscriptop(0x59)
op_10 = cscriptop(0x5a)
op_11 = cscriptop(0x5b)
op_12 = cscriptop(0x5c)
op_13 = cscriptop(0x5d)
op_14 = cscriptop(0x5e)
op_15 = cscriptop(0x5f)
op_16 = cscriptop(0x60)

# control
op_nop = cscriptop(0x61)
op_ver = cscriptop(0x62)
op_if = cscriptop(0x63)
op_notif = cscriptop(0x64)
op_verif = cscriptop(0x65)
op_vernotif = cscriptop(0x66)
op_else = cscriptop(0x67)
op_endif = cscriptop(0x68)
op_verify = cscriptop(0x69)
op_return = cscriptop(0x6a)

# stack ops
op_toaltstack = cscriptop(0x6b)
op_fromaltstack = cscriptop(0x6c)
op_2drop = cscriptop(0x6d)
op_2dup = cscriptop(0x6e)
op_3dup = cscriptop(0x6f)
op_2over = cscriptop(0x70)
op_2rot = cscriptop(0x71)
op_2swap = cscriptop(0x72)
op_ifdup = cscriptop(0x73)
op_depth = cscriptop(0x74)
op_drop = cscriptop(0x75)
op_dup = cscriptop(0x76)
op_nip = cscriptop(0x77)
op_over = cscriptop(0x78)
op_pick = cscriptop(0x79)
op_roll = cscriptop(0x7a)
op_rot = cscriptop(0x7b)
op_swap = cscriptop(0x7c)
op_tuck = cscriptop(0x7d)

# splice ops
op_cat = cscriptop(0x7e)
op_substr = cscriptop(0x7f)
op_left = cscriptop(0x80)
op_right = cscriptop(0x81)
op_size = cscriptop(0x82)

# bit logic
op_invert = cscriptop(0x83)
op_and = cscriptop(0x84)
op_or = cscriptop(0x85)
op_xor = cscriptop(0x86)
op_equal = cscriptop(0x87)
op_equalverify = cscriptop(0x88)
op_reserved1 = cscriptop(0x89)
op_reserved2 = cscriptop(0x8a)

# numeric
op_1add = cscriptop(0x8b)
op_1sub = cscriptop(0x8c)
op_2mul = cscriptop(0x8d)
op_2div = cscriptop(0x8e)
op_negate = cscriptop(0x8f)
op_abs = cscriptop(0x90)
op_not = cscriptop(0x91)
op_0notequal = cscriptop(0x92)

op_add = cscriptop(0x93)
op_sub = cscriptop(0x94)
op_mul = cscriptop(0x95)
op_div = cscriptop(0x96)
op_mod = cscriptop(0x97)
op_lshift = cscriptop(0x98)
op_rshift = cscriptop(0x99)

op_booland = cscriptop(0x9a)
op_boolor = cscriptop(0x9b)
op_numequal = cscriptop(0x9c)
op_numequalverify = cscriptop(0x9d)
op_numnotequal = cscriptop(0x9e)
op_lessthan = cscriptop(0x9f)
op_greaterthan = cscriptop(0xa0)
op_lessthanorequal = cscriptop(0xa1)
op_greaterthanorequal = cscriptop(0xa2)
op_min = cscriptop(0xa3)
op_max = cscriptop(0xa4)

op_within = cscriptop(0xa5)

# crypto
op_ripemd160 = cscriptop(0xa6)
op_sha1 = cscriptop(0xa7)
op_sha256 = cscriptop(0xa8)
op_hash160 = cscriptop(0xa9)
op_hash256 = cscriptop(0xaa)
op_codeseparator = cscriptop(0xab)
op_checksig = cscriptop(0xac)
op_checksigverify = cscriptop(0xad)
op_checkmultisig = cscriptop(0xae)
op_checkmultisigverify = cscriptop(0xaf)

# expansion
op_nop1 = cscriptop(0xb0)
op_nop2 = cscriptop(0xb1)
op_nop3 = cscriptop(0xb2)
op_nop4 = cscriptop(0xb3)
op_nop5 = cscriptop(0xb4)
op_nop6 = cscriptop(0xb5)
op_nop7 = cscriptop(0xb6)
op_nop8 = cscriptop(0xb7)
op_nop9 = cscriptop(0xb8)
op_nop10 = cscriptop(0xb9)

# template matching params
op_smallinteger = cscriptop(0xfa)
op_pubkeys = cscriptop(0xfb)
op_pubkeyhash = cscriptop(0xfd)
op_pubkey = cscriptop(0xfe)

op_invalidopcode = cscriptop(0xff)

valid_opcodes = {
    op_1negate,
    op_reserved,
    op_1,
    op_2,
    op_3,
    op_4,
    op_5,
    op_6,
    op_7,
    op_8,
    op_9,
    op_10,
    op_11,
    op_12,
    op_13,
    op_14,
    op_15,
    op_16,

    op_nop,
    op_ver,
    op_if,
    op_notif,
    op_verif,
    op_vernotif,
    op_else,
    op_endif,
    op_verify,
    op_return,

    op_toaltstack,
    op_fromaltstack,
    op_2drop,
    op_2dup,
    op_3dup,
    op_2over,
    op_2rot,
    op_2swap,
    op_ifdup,
    op_depth,
    op_drop,
    op_dup,
    op_nip,
    op_over,
    op_pick,
    op_roll,
    op_rot,
    op_swap,
    op_tuck,

    op_cat,
    op_substr,
    op_left,
    op_right,
    op_size,

    op_invert,
    op_and,
    op_or,
    op_xor,
    op_equal,
    op_equalverify,
    op_reserved1,
    op_reserved2,

    op_1add,
    op_1sub,
    op_2mul,
    op_2div,
    op_negate,
    op_abs,
    op_not,
    op_0notequal,

    op_add,
    op_sub,
    op_mul,
    op_div,
    op_mod,
    op_lshift,
    op_rshift,

    op_booland,
    op_boolor,
    op_numequal,
    op_numequalverify,
    op_numnotequal,
    op_lessthan,
    op_greaterthan,
    op_lessthanorequal,
    op_greaterthanorequal,
    op_min,
    op_max,

    op_within,

    op_ripemd160,
    op_sha1,
    op_sha256,
    op_hash160,
    op_hash256,
    op_codeseparator,
    op_checksig,
    op_checksigverify,
    op_checkmultisig,
    op_checkmultisigverify,

    op_nop1,
    op_nop2,
    op_nop3,
    op_nop4,
    op_nop5,
    op_nop6,
    op_nop7,
    op_nop8,
    op_nop9,
    op_nop10,

    op_smallinteger,
    op_pubkeys,
    op_pubkeyhash,
    op_pubkey,
}

opcode_names.update({
    op_0 : 'op_0',
    op_pushdata1 : 'op_pushdata1',
    op_pushdata2 : 'op_pushdata2',
    op_pushdata4 : 'op_pushdata4',
    op_1negate : 'op_1negate',
    op_reserved : 'op_reserved',
    op_1 : 'op_1',
    op_2 : 'op_2',
    op_3 : 'op_3',
    op_4 : 'op_4',
    op_5 : 'op_5',
    op_6 : 'op_6',
    op_7 : 'op_7',
    op_8 : 'op_8',
    op_9 : 'op_9',
    op_10 : 'op_10',
    op_11 : 'op_11',
    op_12 : 'op_12',
    op_13 : 'op_13',
    op_14 : 'op_14',
    op_15 : 'op_15',
    op_16 : 'op_16',
    op_nop : 'op_nop',
    op_ver : 'op_ver',
    op_if : 'op_if',
    op_notif : 'op_notif',
    op_verif : 'op_verif',
    op_vernotif : 'op_vernotif',
    op_else : 'op_else',
    op_endif : 'op_endif',
    op_verify : 'op_verify',
    op_return : 'op_return',
    op_toaltstack : 'op_toaltstack',
    op_fromaltstack : 'op_fromaltstack',
    op_2drop : 'op_2drop',
    op_2dup : 'op_2dup',
    op_3dup : 'op_3dup',
    op_2over : 'op_2over',
    op_2rot : 'op_2rot',
    op_2swap : 'op_2swap',
    op_ifdup : 'op_ifdup',
    op_depth : 'op_depth',
    op_drop : 'op_drop',
    op_dup : 'op_dup',
    op_nip : 'op_nip',
    op_over : 'op_over',
    op_pick : 'op_pick',
    op_roll : 'op_roll',
    op_rot : 'op_rot',
    op_swap : 'op_swap',
    op_tuck : 'op_tuck',
    op_cat : 'op_cat',
    op_substr : 'op_substr',
    op_left : 'op_left',
    op_right : 'op_right',
    op_size : 'op_size',
    op_invert : 'op_invert',
    op_and : 'op_and',
    op_or : 'op_or',
    op_xor : 'op_xor',
    op_equal : 'op_equal',
    op_equalverify : 'op_equalverify',
    op_reserved1 : 'op_reserved1',
    op_reserved2 : 'op_reserved2',
    op_1add : 'op_1add',
    op_1sub : 'op_1sub',
    op_2mul : 'op_2mul',
    op_2div : 'op_2div',
    op_negate : 'op_negate',
    op_abs : 'op_abs',
    op_not : 'op_not',
    op_0notequal : 'op_0notequal',
    op_add : 'op_add',
    op_sub : 'op_sub',
    op_mul : 'op_mul',
    op_div : 'op_div',
    op_mod : 'op_mod',
    op_lshift : 'op_lshift',
    op_rshift : 'op_rshift',
    op_booland : 'op_booland',
    op_boolor : 'op_boolor',
    op_numequal : 'op_numequal',
    op_numequalverify : 'op_numequalverify',
    op_numnotequal : 'op_numnotequal',
    op_lessthan : 'op_lessthan',
    op_greaterthan : 'op_greaterthan',
    op_lessthanorequal : 'op_lessthanorequal',
    op_greaterthanorequal : 'op_greaterthanorequal',
    op_min : 'op_min',
    op_max : 'op_max',
    op_within : 'op_within',
    op_ripemd160 : 'op_ripemd160',
    op_sha1 : 'op_sha1',
    op_sha256 : 'op_sha256',
    op_hash160 : 'op_hash160',
    op_hash256 : 'op_hash256',
    op_codeseparator : 'op_codeseparator',
    op_checksig : 'op_checksig',
    op_checksigverify : 'op_checksigverify',
    op_checkmultisig : 'op_checkmultisig',
    op_checkmultisigverify : 'op_checkmultisigverify',
    op_nop1 : 'op_nop1',
    op_nop2 : 'op_nop2',
    op_nop3 : 'op_nop3',
    op_nop4 : 'op_nop4',
    op_nop5 : 'op_nop5',
    op_nop6 : 'op_nop6',
    op_nop7 : 'op_nop7',
    op_nop8 : 'op_nop8',
    op_nop9 : 'op_nop9',
    op_nop10 : 'op_nop10',
    op_smallinteger : 'op_smallinteger',
    op_pubkeys : 'op_pubkeys',
    op_pubkeyhash : 'op_pubkeyhash',
    op_pubkey : 'op_pubkey',
    op_invalidopcode : 'op_invalidopcode',
})

opcodes_by_name = {
    'op_0' : op_0,
    'op_pushdata1' : op_pushdata1,
    'op_pushdata2' : op_pushdata2,
    'op_pushdata4' : op_pushdata4,
    'op_1negate' : op_1negate,
    'op_reserved' : op_reserved,
    'op_1' : op_1,
    'op_2' : op_2,
    'op_3' : op_3,
    'op_4' : op_4,
    'op_5' : op_5,
    'op_6' : op_6,
    'op_7' : op_7,
    'op_8' : op_8,
    'op_9' : op_9,
    'op_10' : op_10,
    'op_11' : op_11,
    'op_12' : op_12,
    'op_13' : op_13,
    'op_14' : op_14,
    'op_15' : op_15,
    'op_16' : op_16,
    'op_nop' : op_nop,
    'op_ver' : op_ver,
    'op_if' : op_if,
    'op_notif' : op_notif,
    'op_verif' : op_verif,
    'op_vernotif' : op_vernotif,
    'op_else' : op_else,
    'op_endif' : op_endif,
    'op_verify' : op_verify,
    'op_return' : op_return,
    'op_toaltstack' : op_toaltstack,
    'op_fromaltstack' : op_fromaltstack,
    'op_2drop' : op_2drop,
    'op_2dup' : op_2dup,
    'op_3dup' : op_3dup,
    'op_2over' : op_2over,
    'op_2rot' : op_2rot,
    'op_2swap' : op_2swap,
    'op_ifdup' : op_ifdup,
    'op_depth' : op_depth,
    'op_drop' : op_drop,
    'op_dup' : op_dup,
    'op_nip' : op_nip,
    'op_over' : op_over,
    'op_pick' : op_pick,
    'op_roll' : op_roll,
    'op_rot' : op_rot,
    'op_swap' : op_swap,
    'op_tuck' : op_tuck,
    'op_cat' : op_cat,
    'op_substr' : op_substr,
    'op_left' : op_left,
    'op_right' : op_right,
    'op_size' : op_size,
    'op_invert' : op_invert,
    'op_and' : op_and,
    'op_or' : op_or,
    'op_xor' : op_xor,
    'op_equal' : op_equal,
    'op_equalverify' : op_equalverify,
    'op_reserved1' : op_reserved1,
    'op_reserved2' : op_reserved2,
    'op_1add' : op_1add,
    'op_1sub' : op_1sub,
    'op_2mul' : op_2mul,
    'op_2div' : op_2div,
    'op_negate' : op_negate,
    'op_abs' : op_abs,
    'op_not' : op_not,
    'op_0notequal' : op_0notequal,
    'op_add' : op_add,
    'op_sub' : op_sub,
    'op_mul' : op_mul,
    'op_div' : op_div,
    'op_mod' : op_mod,
    'op_lshift' : op_lshift,
    'op_rshift' : op_rshift,
    'op_booland' : op_booland,
    'op_boolor' : op_boolor,
    'op_numequal' : op_numequal,
    'op_numequalverify' : op_numequalverify,
    'op_numnotequal' : op_numnotequal,
    'op_lessthan' : op_lessthan,
    'op_greaterthan' : op_greaterthan,
    'op_lessthanorequal' : op_lessthanorequal,
    'op_greaterthanorequal' : op_greaterthanorequal,
    'op_min' : op_min,
    'op_max' : op_max,
    'op_within' : op_within,
    'op_ripemd160' : op_ripemd160,
    'op_sha1' : op_sha1,
    'op_sha256' : op_sha256,
    'op_hash160' : op_hash160,
    'op_hash256' : op_hash256,
    'op_codeseparator' : op_codeseparator,
    'op_checksig' : op_checksig,
    'op_checksigverify' : op_checksigverify,
    'op_checkmultisig' : op_checkmultisig,
    'op_checkmultisigverify' : op_checkmultisigverify,
    'op_nop1' : op_nop1,
    'op_nop2' : op_nop2,
    'op_nop3' : op_nop3,
    'op_nop4' : op_nop4,
    'op_nop5' : op_nop5,
    'op_nop6' : op_nop6,
    'op_nop7' : op_nop7,
    'op_nop8' : op_nop8,
    'op_nop9' : op_nop9,
    'op_nop10' : op_nop10,
    'op_smallinteger' : op_smallinteger,
    'op_pubkeys' : op_pubkeys,
    'op_pubkeyhash' : op_pubkeyhash,
    'op_pubkey' : op_pubkey,
}

class cscriptinvaliderror(exception):
    """base class for cscript exceptions"""
    pass

class cscripttruncatedpushdataerror(cscriptinvaliderror):
    """invalid pushdata due to truncation"""
    def __init__(self, msg, data):
        self.data = data
        super(cscripttruncatedpushdataerror, self).__init__(msg)

# this is used, eg, for blockchain heights in coinbase scripts (bip34)
class cscriptnum(object):
    def __init__(self, d=0):
        self.value = d

    @staticmethod
    def encode(obj):
        r = bytearray(0)
        if obj.value == 0:
            return bytes(r)
        neg = obj.value < 0
        absvalue = -obj.value if neg else obj.value
        while (absvalue):
            r.append(chr(absvalue & 0xff))
            absvalue >>= 8
        if r[-1] & 0x80:
            r.append(0x80 if neg else 0)
        elif neg:
            r[-1] |= 0x80
        return bytes(bchr(len(r)) + r)


class cscript(bytes):
    """serialized script

    a bytes subclass, so you can use this directly whenever bytes are accepted.
    note that this means that indexing does *not* work - you'll get an index by
    byte rather than opcode. this format was chosen for efficiency so that the
    general case would not require creating a lot of little cscriptop objects.

    iter(script) however does iterate by opcode.
    """
    @classmethod
    def __coerce_instance(cls, other):
        # coerce other into bytes
        if isinstance(other, cscriptop):
            other = bchr(other)
        elif isinstance(other, cscriptnum):
            if (other.value == 0):
                other = bchr(cscriptop(op_0))
            else:
                other = cscriptnum.encode(other)
        elif isinstance(other, (int, long)):
            if 0 <= other <= 16:
                other = bytes(bchr(cscriptop.encode_op_n(other)))
            elif other == -1:
                other = bytes(bchr(op_1negate))
            else:
                other = cscriptop.encode_op_pushdata(bignum.bn2vch(other))
        elif isinstance(other, (bytes, bytearray)):
            other = cscriptop.encode_op_pushdata(other)
        return other

    def __add__(self, other):
        # do the coercion outside of the try block so that errors in it are
        # noticed.
        other = self.__coerce_instance(other)

        try:
            # bytes.__add__ always returns bytes instances unfortunately
            return cscript(super(cscript, self).__add__(other))
        except typeerror:
            raise typeerror('can not add a %r instance to a cscript' % other.__class__)

    def join(self, iterable):
        # join makes no sense for a cscript()
        raise notimplementederror

    def __new__(cls, value=b''):
        if isinstance(value, bytes) or isinstance(value, bytearray):
            return super(cscript, cls).__new__(cls, value)
        else:
            def coerce_iterable(iterable):
                for instance in iterable:
                    yield cls.__coerce_instance(instance)
            # annoyingly on both python2 and python3 bytes.join() always
            # returns a bytes instance even when subclassed.
            return super(cscript, cls).__new__(cls, b''.join(coerce_iterable(value)))

    def raw_iter(self):
        """raw iteration

        yields tuples of (opcode, data, sop_idx) so that the different possible
        pushdata encodings can be accurately distinguished, as well as
        determining the exact opcode byte indexes. (sop_idx)
        """
        i = 0
        while i < len(self):
            sop_idx = i
            opcode = bord(self[i])
            i += 1

            if opcode > op_pushdata4:
                yield (opcode, none, sop_idx)
            else:
                datasize = none
                pushdata_type = none
                if opcode < op_pushdata1:
                    pushdata_type = 'pushdata(%d)' % opcode
                    datasize = opcode

                elif opcode == op_pushdata1:
                    pushdata_type = 'pushdata1'
                    if i >= len(self):
                        raise cscriptinvaliderror('pushdata1: missing data length')
                    datasize = bord(self[i])
                    i += 1

                elif opcode == op_pushdata2:
                    pushdata_type = 'pushdata2'
                    if i + 1 >= len(self):
                        raise cscriptinvaliderror('pushdata2: missing data length')
                    datasize = bord(self[i]) + (bord(self[i+1]) << 8)
                    i += 2

                elif opcode == op_pushdata4:
                    pushdata_type = 'pushdata4'
                    if i + 3 >= len(self):
                        raise cscriptinvaliderror('pushdata4: missing data length')
                    datasize = bord(self[i]) + (bord(self[i+1]) << 8) + (bord(self[i+2]) << 16) + (bord(self[i+3]) << 24)
                    i += 4

                else:
                    assert false # shouldn't happen


                data = bytes(self[i:i+datasize])

                # check for truncation
                if len(data) < datasize:
                    raise cscripttruncatedpushdataerror('%s: truncated data' % pushdata_type, data)

                i += datasize

                yield (opcode, data, sop_idx)

    def __iter__(self):
        """'cooked' iteration

        returns either a cscriptop instance, an integer, or bytes, as
        appropriate.

        see raw_iter() if you need to distinguish the different possible
        pushdata encodings.
        """
        for (opcode, data, sop_idx) in self.raw_iter():
            if data is not none:
                yield data
            else:
                opcode = cscriptop(opcode)

                if opcode.is_small_int():
                    yield opcode.decode_op_n()
                else:
                    yield cscriptop(opcode)

    def __repr__(self):
        # for python3 compatibility add b before strings so testcases don't
        # need to change
        def _repr(o):
            if isinstance(o, bytes):
                return "x('%s')" % binascii.hexlify(o).decode('utf8')
            else:
                return repr(o)

        ops = []
        i = iter(self)
        while true:
            op = none
            try:
                op = _repr(next(i))
            except cscripttruncatedpushdataerror as err:
                op = '%s...<error: %s>' % (_repr(err.data), err)
                break
            except cscriptinvaliderror as err:
                op = '<error: %s>' % err
                break
            except stopiteration:
                break
            finally:
                if op is not none:
                    ops.append(op)

        return "cscript([%s])" % ', '.join(ops)

    def getsigopcount(self, faccurate):
        """get the sigop count.

        faccurate - accurately count checkmultisig, see bip16 for details.

        note that this is consensus-critical.
        """
        n = 0
        lastopcode = op_invalidopcode
        for (opcode, data, sop_idx) in self.raw_iter():
            if opcode in (op_checksig, op_checksigverify):
                n += 1
            elif opcode in (op_checkmultisig, op_checkmultisigverify):
                if faccurate and (op_1 <= lastopcode <= op_16):
                    n += opcode.decode_op_n()
                else:
                    n += 20
            lastopcode = opcode
        return n


sighash_all = 1
sighash_none = 2
sighash_single = 3
sighash_anyonecanpay = 0x80

def findanddelete(script, sig):
    """consensus critical, see findanddelete() in satoshi codebase"""
    r = b''
    last_sop_idx = sop_idx = 0
    skip = true
    for (opcode, data, sop_idx) in script.raw_iter():
        if not skip:
            r += script[last_sop_idx:sop_idx]
        last_sop_idx = sop_idx
        if script[sop_idx:sop_idx + len(sig)] == sig:
            skip = true
        else:
            skip = false
    if not skip:
        r += script[last_sop_idx:]
    return cscript(r)


def signaturehash(script, txto, inidx, hashtype):
    """consensus-correct signaturehash

    returns (hash, err) to precisely match the consensus-critical behavior of
    the sighash_single bug. (inidx is *not* checked for validity)
    """
    hash_one = b'\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'

    if inidx >= len(txto.vin):
        return (hash_one, "inidx %d out of range (%d)" % (inidx, len(txto.vin)))
    txtmp = ctransaction(txto)

    for txin in txtmp.vin:
        txin.scriptsig = b''
    txtmp.vin[inidx].scriptsig = findanddelete(script, cscript([op_codeseparator]))

    if (hashtype & 0x1f) == sighash_none:
        txtmp.vout = []

        for i in range(len(txtmp.vin)):
            if i != inidx:
                txtmp.vin[i].nsequence = 0

    elif (hashtype & 0x1f) == sighash_single:
        outidx = inidx
        if outidx >= len(txtmp.vout):
            return (hash_one, "outidx %d out of range (%d)" % (outidx, len(txtmp.vout)))

        tmp = txtmp.vout[outidx]
        txtmp.vout = []
        for i in range(outidx):
            txtmp.vout.append(ctxout())
        txtmp.vout.append(tmp)

        for i in range(len(txtmp.vin)):
            if i != inidx:
                txtmp.vin[i].nsequence = 0

    if hashtype & sighash_anyonecanpay:
        tmp = txtmp.vin[inidx]
        txtmp.vin = []
        txtmp.vin.append(tmp)

    s = txtmp.serialize()
    s += struct.pack(b"<i", hashtype)

    hash = hash256(s)

    return (hash, none)
