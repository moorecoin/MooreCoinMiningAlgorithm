#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import moorecointestframework
from test_framework.util import *

from binascii import a2b_hex, b2a_hex
from hashlib import sha256
from struct import pack


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

def b2x(b):
    return b2a_hex(b).decode('ascii')

# note: this does not work for signed numbers (set the high bit) or zero (use b'\0')
def encodeunum(n):
    s = bytearray(b'\1')
    while n > 127:
        s[0] += 1
        s.append(n % 256)
        n //= 256
    s.append(n)
    return bytes(s)

def varlenencode(n):
    if n < 0xfd:
        return pack('<b', n)
    if n <= 0xffff:
        return b'\xfd' + pack('<h', n)
    if n <= 0xffffffff:
        return b'\xfe' + pack('<l', n)
    return b'\xff' + pack('<q', n)

def dblsha(b):
    return sha256(sha256(b).digest()).digest()

def genmrklroot(leaflist):
    cur = leaflist
    while len(cur) > 1:
        n = []
        if len(cur) & 1:
            cur.append(cur[-1])
        for i in range(0, len(cur), 2):
            n.append(dblsha(cur[i] + cur[i+1]))
        cur = n
    return cur[0]

def template_to_bytes(tmpl, txlist):
    blkver = pack('<l', tmpl['version'])
    mrklroot = genmrklroot(list(dblsha(a) for a in txlist))
    timestamp = pack('<l', tmpl['curtime'])
    nonce = b'\0\0\0\0'
    blk = blkver + a2b_hex(tmpl['previousblockhash'])[::-1] + mrklroot + timestamp + a2b_hex(tmpl['bits'])[::-1] + nonce
    blk += varlenencode(len(txlist))
    for tx in txlist:
        blk += tx
    return blk

def template_to_hex(tmpl, txlist):
    return b2x(template_to_bytes(tmpl, txlist))

def assert_template(node, tmpl, txlist, expect):
    rsp = node.getblocktemplate({'data':template_to_hex(tmpl, txlist),'mode':'proposal'})
    if rsp != expect:
        raise assertionerror('unexpected: %s' % (rsp,))

class getblocktemplateproposaltest(moorecointestframework):
    '''
    test block proposals with getblocktemplate.
    '''

    def run_test(self):
        node = self.nodes[0]
        node.generate(1) # mine a block to leave initial block download
        tmpl = node.getblocktemplate()
        if 'coinbasetxn' not in tmpl:
            rawcoinbase = encodeunum(tmpl['height'])
            rawcoinbase += b'\x01-'
            hemoorecoinbase = b2x(rawcoinbase)
            hexoutval = b2x(pack('<q', tmpl['coinbasevalue']))
            tmpl['coinbasetxn'] = {'data': '01000000' + '01' + '0000000000000000000000000000000000000000000000000000000000000000ffffffff' + ('%02x' % (len(rawcoinbase),)) + hemoorecoinbase + 'fffffffe' + '01' + hexoutval + '00' + '00000000'}
        txlist = list(bytearray(a2b_hex(a['data'])) for a in (tmpl['coinbasetxn'],) + tuple(tmpl['transactions']))

        # test 0: capability advertised
        assert('proposal' in tmpl['capabilities'])

        # note: this test currently fails (regtest mode doesn't enforce block height in coinbase)
        ## test 1: bad height in coinbase
        #txlist[0][4+1+36+1+1] += 1
        #assert_template(node, tmpl, txlist, 'fixme')
        #txlist[0][4+1+36+1+1] -= 1

        # test 2: bad input hash for gen tx
        txlist[0][4+1] += 1
        assert_template(node, tmpl, txlist, 'bad-cb-missing')
        txlist[0][4+1] -= 1

        # test 3: truncated final tx
        lastbyte = txlist[-1].pop()
        try:
            assert_template(node, tmpl, txlist, 'n/a')
        except jsonrpcexception:
            pass  # expected
        txlist[-1].append(lastbyte)

        # test 4: add an invalid tx to the end (duplicate of gen tx)
        txlist.append(txlist[0])
        assert_template(node, tmpl, txlist, 'bad-txns-duplicate')
        txlist.pop()

        # test 5: add an invalid tx to the end (non-duplicate)
        txlist.append(bytearray(txlist[0]))
        txlist[-1][4+1] = b'\xff'
        assert_template(node, tmpl, txlist, 'bad-txns-inputs-missingorspent')
        txlist.pop()

        # test 6: future tx lock time
        txlist[0][-4:] = b'\xff\xff\xff\xff'
        assert_template(node, tmpl, txlist, 'bad-txns-nonfinal')
        txlist[0][-4:] = b'\0\0\0\0'

        # test 7: bad tx count
        txlist.append(b'')
        try:
            assert_template(node, tmpl, txlist, 'n/a')
        except jsonrpcexception:
            pass  # expected
        txlist.pop()

        # test 8: bad bits
        realbits = tmpl['bits']
        tmpl['bits'] = '1c0000ff'  # impossible in the real world
        assert_template(node, tmpl, txlist, 'bad-diffbits')
        tmpl['bits'] = realbits

        # test 9: bad merkle root
        rawtmpl = template_to_bytes(tmpl, txlist)
        rawtmpl[4+32] = (rawtmpl[4+32] + 1) % 0x100
        rsp = node.getblocktemplate({'data':b2x(rawtmpl),'mode':'proposal'})
        if rsp != 'bad-txnmrklroot':
            raise assertionerror('unexpected: %s' % (rsp,))

        # test 10: bad timestamps
        realtime = tmpl['curtime']
        tmpl['curtime'] = 0x7fffffff
        assert_template(node, tmpl, txlist, 'time-too-new')
        tmpl['curtime'] = 0
        assert_template(node, tmpl, txlist, 'time-too-old')
        tmpl['curtime'] = realtime

        # test 11: valid block
        assert_template(node, tmpl, txlist, none)

        # test 12: orphan block
        tmpl['previousblockhash'] = 'ff00' * 16
        assert_template(node, tmpl, txlist, 'inconclusive-not-best-prevblk')

if __name__ == '__main__':
    getblocktemplateproposaltest().main()
