'''
moorecoin base58 encoding and decoding.

based on https://moorecointalk.org/index.php?topic=1026.0 (public domain)
'''
import hashlib

# for compatibility with following code...
class sha256:
    new = hashlib.sha256

if str != bytes:
    # python 3.x
    def ord(c):
        return c
    def chr(n):
        return bytes( (n,) )

__b58chars = '123456789abcdefghjklmnpqrstuvwxyzabcdefghijkmnopqrstuvwxyz'
__b58base = len(__b58chars)
b58chars = __b58chars

def b58encode(v):
    """ encode v, which is a string of bytes, to base58.
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        long_value += (256**i) * ord(c)

    result = ''
    while long_value >= __b58base:
        div, mod = divmod(long_value, __b58base)
        result = __b58chars[mod] + result
        long_value = div
    result = __b58chars[long_value] + result

    # moorecoin does a little leading-zero-compression:
    # leading 0-bytes in the input become leading-1s
    npad = 0
    for c in v:
        if c == '\0': npad += 1
        else: break

    return (__b58chars[0]*npad) + result

def b58decode(v, length = none):
    """ decode v into a string of len bytes
    """
    long_value = 0
    for (i, c) in enumerate(v[::-1]):
        long_value += __b58chars.find(c) * (__b58base**i)

    result = bytes()
    while long_value >= 256:
        div, mod = divmod(long_value, 256)
        result = chr(mod) + result
        long_value = div
    result = chr(long_value) + result

    npad = 0
    for c in v:
        if c == __b58chars[0]: npad += 1
        else: break

    result = chr(0)*npad + result
    if length is not none and len(result) != length:
        return none

    return result

def checksum(v):
    """return 32-bit checksum based on sha256"""
    return sha256.new(sha256.new(v).digest()).digest()[0:4]

def b58encode_chk(v):
    """b58encode a string, with 32-bit checksum"""
    return b58encode(v + checksum(v))

def b58decode_chk(v):
    """decode a base58 string, check and remove checksum"""
    result = b58decode(v)
    if result is none:
        return none
    h3 = checksum(result[:-4])
    if result[-4:] == checksum(result[:-4]):
        return result[:-4]
    else:
        return none

def get_bcaddress_version(straddress):
    """ returns none if straddress is invalid.  otherwise returns integer version of address. """
    addr = b58decode_chk(straddress)
    if addr is none or len(addr)!=21: return none
    version = addr[0]
    return ord(version)

if __name__ == '__main__':
    # test case (from http://gitorious.org/moorecoin/python-base58.git)
    assert get_bcaddress_version('15vjradx9zpba8lvnbrcafzrvzn7ixhnsc') is 0
    _ohai = 'o hai'.encode('ascii')
    _tmp = b58encode(_ohai)
    assert _tmp == 'dyb3oms'
    assert b58decode(_tmp, 5) == _ohai
    print("tests passed")
