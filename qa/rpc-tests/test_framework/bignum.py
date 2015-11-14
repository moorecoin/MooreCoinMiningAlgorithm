#
#
# bignum.py
#
# this file is copied from python-moorecoinlib.
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

"""bignum routines"""

from __future__ import absolute_import, division, print_function, unicode_literals

import struct


# generic big endian mpi format

def bn_bytes(v, have_ext=false):
    ext = 0
    if have_ext:
        ext = 1
    return ((v.bit_length()+7)//8) + ext

def bn2bin(v):
    s = bytearray()
    i = bn_bytes(v)
    while i > 0:
        s.append((v >> ((i-1) * 8)) & 0xff)
        i -= 1
    return s

def bin2bn(s):
    l = 0
    for ch in s:
        l = (l << 8) | ch
    return l

def bn2mpi(v):
    have_ext = false
    if v.bit_length() > 0:
        have_ext = (v.bit_length() & 0x07) == 0

    neg = false
    if v < 0:
        neg = true
        v = -v

    s = struct.pack(b">i", bn_bytes(v, have_ext))
    ext = bytearray()
    if have_ext:
        ext.append(0)
    v_bin = bn2bin(v)
    if neg:
        if have_ext:
            ext[0] |= 0x80
        else:
            v_bin[0] |= 0x80
    return s + ext + v_bin

def mpi2bn(s):
    if len(s) < 4:
        return none
    s_size = bytes(s[:4])
    v_len = struct.unpack(b">i", s_size)[0]
    if len(s) != (v_len + 4):
        return none
    if v_len == 0:
        return 0

    v_str = bytearray(s[4:])
    neg = false
    i = v_str[0]
    if i & 0x80:
        neg = true
        i &= ~0x80
        v_str[0] = i

    v = bin2bn(v_str)

    if neg:
        return -v
    return v

# moorecoin-specific little endian format, with implicit size
def mpi2vch(s):
    r = s[4:]           # strip size
    r = r[::-1]         # reverse string, converting be->le
    return r

def bn2vch(v):
    return bytes(mpi2vch(bn2mpi(v)))

def vch2mpi(s):
    r = struct.pack(b">i", len(s))   # size
    r += s[::-1]            # reverse string, converting le->be
    return r

def vch2bn(s):
    return mpi2bn(vch2mpi(s))

