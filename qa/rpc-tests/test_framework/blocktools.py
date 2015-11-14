# blocktools.py - utilities for manipulating blocks and transactions
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

from mininode import *
from script import cscript, cscriptop

# create a block (with regtest difficulty)
def create_block(hashprev, coinbase, ntime=none):
    block = cblock()
    if ntime is none:
        import time
        block.ntime = int(time.time()+600)
    else:
        block.ntime = ntime
    block.hashprevblock = hashprev
    block.nbits = 0x207fffff # will break after a difficulty adjustment...
    block.vtx.append(coinbase)
    block.hashmerkleroot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(chr(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

counter=1
# create an anyone-can-spend coinbase transaction, assuming no miner fees
def create_coinbase(heightadjust = 0):
    global counter
    coinbase = ctransaction()
    coinbase.vin.append(ctxin(coutpoint(0, 0xffffffff), 
                ser_string(serialize_script_num(counter+heightadjust)), 0xffffffff))
    counter += 1
    coinbaseoutput = ctxout()
    coinbaseoutput.nvalue = 50*100000000
    halvings = int((counter+heightadjust)/150) # regtest
    coinbaseoutput.nvalue >>= halvings
    coinbaseoutput.scriptpubkey = ""
    coinbase.vout = [ coinbaseoutput ]
    coinbase.calc_sha256()
    return coinbase

# create a transaction with an anyone-can-spend output, that spends the
# nth output of prevtx.
def create_transaction(prevtx, n, sig, value):
    tx = ctransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(ctxin(coutpoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(ctxout(value, ""))
    tx.calc_sha256()
    return tx
