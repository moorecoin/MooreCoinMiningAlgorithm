#!/usr/bin/env python
'''
generate valid and invalid base58 address and private key test vectors.

usage: 
    gen_base58_test_vectors.py valid 50 > ../../src/test/data/base58_keys_valid.json
    gen_base58_test_vectors.py invalid 50 > ../../src/test/data/base58_keys_invalid.json
'''
# 2012 wladimir j. van der laan
# released under mit license
import os
from itertools import islice
from base58 import b58encode, b58decode, b58encode_chk, b58decode_chk, b58chars
import random
from binascii import b2a_hex

# key types
pubkey_address = 0
script_address = 5
pubkey_address_test = 111
script_address_test = 196
privkey = 128
privkey_test = 239

metadata_keys = ['isprivkey', 'istestnet', 'addrtype', 'iscompressed']
# templates for valid sequences
templates = [
  # prefix, payload_size, suffix, metadata
  #                                  none = n/a
  ((pubkey_address,),      20, (),   (false, false, 'pubkey', none)),
  ((script_address,),      20, (),   (false, false, 'script',  none)),
  ((pubkey_address_test,), 20, (),   (false, true,  'pubkey', none)),
  ((script_address_test,), 20, (),   (false, true,  'script',  none)),
  ((privkey,),             32, (),   (true,  false, none,  false)),
  ((privkey,),             32, (1,), (true,  false, none,  true)),
  ((privkey_test,),        32, (),   (true,  true,  none,  false)),
  ((privkey_test,),        32, (1,), (true,  true,  none,  true))
]

def is_valid(v):
    '''check vector v for validity'''
    result = b58decode_chk(v)
    if result is none:
        return false
    valid = false
    for template in templates:
        prefix = str(bytearray(template[0]))
        suffix = str(bytearray(template[2]))
        if result.startswith(prefix) and result.endswith(suffix):
            if (len(result) - len(prefix) - len(suffix)) == template[1]:
                return true
    return false

def gen_valid_vectors():
    '''generate valid test vectors'''
    while true:
        for template in templates:
            prefix = str(bytearray(template[0]))
            payload = os.urandom(template[1]) 
            suffix = str(bytearray(template[2]))
            rv = b58encode_chk(prefix + payload + suffix)
            assert is_valid(rv)
            metadata = dict([(x,y) for (x,y) in zip(metadata_keys,template[3]) if y is not none])
            yield (rv, b2a_hex(payload), metadata)

def gen_invalid_vector(template, corrupt_prefix, randomize_payload_size, corrupt_suffix):
    '''generate possibly invalid vector'''
    if corrupt_prefix:
        prefix = os.urandom(1)
    else:
        prefix = str(bytearray(template[0]))
    
    if randomize_payload_size:
        payload = os.urandom(max(int(random.expovariate(0.5)), 50))
    else:
        payload = os.urandom(template[1])
    
    if corrupt_suffix:
        suffix = os.urandom(len(template[2]))
    else:
        suffix = str(bytearray(template[2]))

    return b58encode_chk(prefix + payload + suffix)

def randbool(p = 0.5):
    '''return true with p(p)'''
    return random.random() < p

def gen_invalid_vectors():
    '''generate invalid test vectors'''
    # start with some manual edge-cases
    yield "",
    yield "x",
    while true:
        # kinds of invalid vectors:
        #   invalid prefix
        #   invalid payload length
        #   invalid (randomized) suffix (add random data)
        #   corrupt checksum
        for template in templates:
            val = gen_invalid_vector(template, randbool(0.2), randbool(0.2), randbool(0.2))
            if random.randint(0,10)<1: # line corruption
                if randbool(): # add random character to end
                    val += random.choice(b58chars)
                else: # replace random character in the middle
                    n = random.randint(0, len(val))
                    val = val[0:n] + random.choice(b58chars) + val[n+1:]
            if not is_valid(val):
                yield val,

if __name__ == '__main__':
    import sys, json
    iters = {'valid':gen_valid_vectors, 'invalid':gen_invalid_vectors}
    try:
        uiter = iters[sys.argv[1]]
    except indexerror:
        uiter = gen_valid_vectors
    try:
        count = int(sys.argv[2])
    except indexerror:
        count = 0
   
    data = list(islice(uiter(), count))
    json.dump(data, sys.stdout, sort_keys=true, indent=4)
    sys.stdout.write('\n')

