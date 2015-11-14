#!/usr/bin/env python2
#
# distributed under the mit/x11 software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#

'''
test notes:
this test uses the script_valid and script_invalid tests from the unittest
framework to do end-to-end testing where we compare that two nodes agree on
whether blocks containing a given test script are valid.

we generally ignore the script flags associated with each test (since we lack
the precision to test each script using those flags in this framework), but
for tests with script_verify_p2sh, we can use a block time after the bip16 
switchover date to try to test with that flag enabled (and for tests without
that flag, we use a block time before the switchover date).

note: this test is very slow and may take more than 40 minutes to run.
'''

from test_framework.test_framework import comparisontestframework
from test_framework.util import *
from test_framework.comptool import testinstance, testmanager
from test_framework.mininode import *
from test_framework.blocktools import *
from test_framework.script import *
import logging
import copy
import json

script_valid_file   = "../../src/test/data/script_valid.json"
script_invalid_file = "../../src/test/data/script_invalid.json"

# pass in a set of json files to open. 
class scripttestfile(object):

    def __init__(self, files):
        self.files = files
        self.index = -1
        self.data = []

    def load_files(self):
        for f in self.files:
            self.data.extend(json.loads(open(os.path.dirname(os.path.abspath(__file__))+"/"+f).read()))

    # skip over records that are not long enough to be tests
    def get_records(self):
        while (self.index < len(self.data)):
            if len(self.data[self.index]) >= 3:
                yield self.data[self.index]
            self.index += 1


# helper for parsing the flags specified in the .json files
script_verify_none = 0
script_verify_p2sh = 1 
script_verify_strictenc = 1 << 1
script_verify_dersig = 1 << 2
script_verify_low_s = 1 << 3
script_verify_nulldummy = 1 << 4
script_verify_sigpushonly = 1 << 5
script_verify_minimaldata = 1 << 6
script_verify_discourage_upgradable_nops = 1 << 7
script_verify_cleanstack = 1 << 8

flag_map = { 
    "": script_verify_none,
    "none": script_verify_none, 
    "p2sh": script_verify_p2sh,
    "strictenc": script_verify_strictenc,
    "dersig": script_verify_dersig,
    "low_s": script_verify_low_s,
    "nulldummy": script_verify_nulldummy,
    "sigpushonly": script_verify_sigpushonly,
    "minimaldata": script_verify_minimaldata,
    "discourage_upgradable_nops": script_verify_discourage_upgradable_nops,
    "cleanstack": script_verify_cleanstack,
}

def parsescriptflags(flag_string):
    flags = 0
    for x in flag_string.split(","):
        if x in flag_map:
            flags |= flag_map[x]
        else:
            print "error: unrecognized script flag: ", x
    return flags

'''
given a string that is a scriptsig or scriptpubkey from the .json files above,
convert it to a cscript()
'''
# replicates behavior from core_read.cpp
def parsescript(json_script):
    script = json_script.split(" ")
    parsed_script = cscript()
    for x in script:
        if len(x) == 0:
            # empty string, ignore.
            pass
        elif x.isdigit() or (len(x) >= 1 and x[0] == "-" and x[1:].isdigit()):
            # number
            n = int(x, 0)
            if (n == -1) or (n >= 1 and n <= 16):
                parsed_script = cscript(bytes(parsed_script) + bytes(cscript([n])))
            else:
                parsed_script += cscriptnum(int(x, 0))
        elif x.startswith("0x"):
            # raw hex data, inserted not pushed onto stack:
            for i in xrange(2, len(x), 2):
                parsed_script = cscript(bytes(parsed_script) + bytes(chr(int(x[i:i+2],16))))
        elif x.startswith("'") and x.endswith("'") and len(x) >= 2:
            # single-quoted string, pushed as data.
            parsed_script += cscript([x[1:-1]])
        else:
            # opcode, e.g. op_add or add:
            tryopname = "op_" + x
            if tryopname in opcodes_by_name:
                parsed_script += cscriptop(opcodes_by_name["op_" + x])
            else:
                print "parsescript: error parsing '%s'" % x
                return ""
    return parsed_script
            
class testbuilder(object):
    def create_credit_tx(self, scriptpubkey):
        # self.tx1 is a coinbase transaction, modeled after the one created by script_tests.cpp
        # this allows us to reuse signatures created in the unit test framework.
        self.tx1 = create_coinbase()                 # this has a bip34 scriptsig,
        self.tx1.vin[0].scriptsig = cscript([0, 0])  # but this matches the unit tests
        self.tx1.vout[0].nvalue = 0
        self.tx1.vout[0].scriptpubkey = scriptpubkey
        self.tx1.rehash()
    def create_spend_tx(self, scriptsig):
        self.tx2 = create_transaction(self.tx1, 0, cscript(), 0)
        self.tx2.vin[0].scriptsig = scriptsig
        self.tx2.vout[0].scriptpubkey = cscript()
        self.tx2.rehash()
    def rehash(self):
        self.tx1.rehash()
        self.tx2.rehash()

# this test uses the (default) two nodes provided by comparisontestframework,
# specified on the command line with --testbinary and --refbinary.
# see comptool.py
class scripttest(comparisontestframework):

    def run_test(self):
        # set up the comparison tool testmanager
        test = testmanager(self, self.options.tmpdir)
        test.add_all_connections(self.nodes)

        # load scripts
        self.scripts = scripttestfile([script_valid_file, script_invalid_file])
        self.scripts.load_files()

        # some variables we re-use between test instances (to build blocks)
        self.tip = none
        self.block_time = none

        networkthread().start()  # start up network handling in another thread
        test.run()

    def generate_test_instance(self, pubkeystring, scriptsigstring):
        scriptpubkey = parsescript(pubkeystring)
        scriptsig = parsescript(scriptsigstring)

        test = testinstance(sync_every_block=false)
        test_build = testbuilder()
        test_build.create_credit_tx(scriptpubkey)
        test_build.create_spend_tx(scriptsig)
        test_build.rehash()

        block = create_block(self.tip, test_build.tx1, self.block_time)
        self.block_time += 1
        block.solve()
        self.tip = block.sha256
        test.blocks_and_transactions = [[block, true]]

        for i in xrange(100):
            block = create_block(self.tip, create_coinbase(), self.block_time)
            self.block_time += 1
            block.solve()
            self.tip = block.sha256
            test.blocks_and_transactions.append([block, true])

        block = create_block(self.tip, create_coinbase(), self.block_time)
        self.block_time += 1
        block.vtx.append(test_build.tx2)
        block.hashmerkleroot = block.calc_merkle_root()
        block.rehash()
        block.solve()
        test.blocks_and_transactions.append([block, none])
        return test   

    # this generates the tests for testmanager.
    def get_tests(self):
        self.tip = int ("0x" + self.nodes[0].getbestblockhash() + "l", 0)
        self.block_time = 1333230000  # before the bip16 switchover

        '''
        create a new block with an anyone-can-spend coinbase
        '''
        block = create_block(self.tip, create_coinbase(), self.block_time)
        self.block_time += 1
        block.solve()
        self.tip = block.sha256
        yield testinstance(objects=[[block, true]])

        '''
        build out to 100 blocks total, maturing the coinbase.
        '''
        test = testinstance(objects=[], sync_every_block=false, sync_every_tx=false)
        for i in xrange(100):
            b = create_block(self.tip, create_coinbase(), self.block_time)
            b.solve()
            test.blocks_and_transactions.append([b, true])
            self.tip = b.sha256
            self.block_time += 1
        yield test
 
        ''' iterate through script tests. '''
        counter = 0
        for script_test in self.scripts.get_records():
            ''' reset the blockchain to genesis block + 100 blocks. '''
            if self.nodes[0].getblockcount() > 101:
                self.nodes[0].invalidateblock(self.nodes[0].getblockhash(102))
                self.nodes[1].invalidateblock(self.nodes[1].getblockhash(102))

            self.tip = int ("0x" + self.nodes[0].getbestblockhash() + "l", 0)

            [scriptsig, scriptpubkey, flags] = script_test[0:3]
            flags = parsescriptflags(flags)

            # we can use block time to determine whether the nodes should be
            # enforcing bip16.
            #
            # we intentionally let the block time grow by 1 each time.
            # this forces the block hashes to differ between tests, so that
            # a call to invalidateblock doesn't interfere with a later test.
            if (flags & script_verify_p2sh):
                self.block_time = 1333238400 + counter # advance to enforcing bip16
            else:
                self.block_time = 1333230000 + counter # before the bip16 switchover

            print "script test: [%s]" % script_test

            yield self.generate_test_instance(scriptpubkey, scriptsig)
            counter += 1

if __name__ == '__main__':
    scripttest().main()
