#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test re-org scenarios with a mempool that contains transactions
# that spend (directly or indirectly) coinbase transactions.
#

from test_framework.test_framework import moorecointestframework
from test_framework.util import *
from pprint import pprint
from time import sleep

# create one-input, one-output, no-fee transaction:
class rawtransactionstest(moorecointestframework):

    def setup_chain(self):
        print("initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self, split=false):
        self.nodes = start_nodes(3, self.options.tmpdir)

        #connect to a local machine for debugging
        #url = "http://moorecoinrpc:dp6dvqztqxarpenwyn3lztfchccycuuhwnf7e8px99x1@%s:%d" % ('127.0.0.1', 18332)
        #proxy = authserviceproxy(url)
        #proxy.url = url # store url on proxy for info
        #self.nodes.append(proxy)

        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)

        self.is_network_split=false
        self.sync_all()

    def run_test(self):

        #prepare some coins for multiple *rawtransaction commands
        self.nodes[2].generate(1)
        self.nodes[0].generate(101)
        self.sync_all()
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),1.5);
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),1.0);
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),5.0);
        self.sync_all()
        self.nodes[0].generate(5)
        self.sync_all()

        #########################################
        # sendrawtransaction with missing input #
        #########################################
        inputs  = [ {'txid' : "1d1d4e24ed99057e84c3f80fd8fbec79ed9e1acee37da269356ecea000000000", 'vout' : 1}] #won't exists
        outputs = { self.nodes[0].getnewaddress() : 4.998 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        rawtx   = self.nodes[2].signrawtransaction(rawtx)

        errorstring = ""
        try:
            rawtx   = self.nodes[2].sendrawtransaction(rawtx['hex'])
        except jsonrpcexception,e:
            errorstring = e.error['message']

        assert_equal("missing inputs" in errorstring, true);

        #########################
        # raw tx multisig tests #
        #########################
        # 2of2 test
        addr1 = self.nodes[2].getnewaddress()
        addr2 = self.nodes[2].getnewaddress()

        addr1obj = self.nodes[2].validateaddress(addr1)
        addr2obj = self.nodes[2].validateaddress(addr2)

        msigobj = self.nodes[2].addmultisigaddress(2, [addr1obj['pubkey'], addr2obj['pubkey']])
        msigobjvalid = self.nodes[2].validateaddress(msigobj)

        #use balance deltas instead of absolute values
        bal = self.nodes[2].getbalance()

        # send 1.2 btc to msig adr
        txid       = self.nodes[0].sendtoaddress(msigobj, 1.2);
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[2].getbalance(), bal+decimal('1.20000000')) #node2 has both keys of the 2of2 ms addr., tx should affect the balance




        # 2of3 test from different nodes
        bal = self.nodes[2].getbalance()
        addr1 = self.nodes[1].getnewaddress()
        addr2 = self.nodes[2].getnewaddress()
        addr3 = self.nodes[2].getnewaddress()

        addr1obj = self.nodes[1].validateaddress(addr1)
        addr2obj = self.nodes[2].validateaddress(addr2)
        addr3obj = self.nodes[2].validateaddress(addr3)

        msigobj = self.nodes[2].addmultisigaddress(2, [addr1obj['pubkey'], addr2obj['pubkey'], addr3obj['pubkey']])
        msigobjvalid = self.nodes[2].validateaddress(msigobj)

        txid       = self.nodes[0].sendtoaddress(msigobj, 2.2);
        dectx = self.nodes[0].gettransaction(txid)
        rawtx = self.nodes[0].decoderawtransaction(dectx['hex'])
        spk = rawtx['vout'][0]['scriptpubkey']['hex']
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        #this is a incomplete feature
        #node2 has two of three key and the funds should be spendable and count at balance calculation
        assert_equal(self.nodes[2].getbalance(), bal) #for now, assume the funds of a 2of3 multisig tx are not marked as spendable

        txdetails = self.nodes[0].gettransaction(txid, true)
        rawtx = self.nodes[0].decoderawtransaction(txdetails['hex'])
        vout = false
        for outpoint in rawtx['vout']:
            if outpoint['value'] == decimal('2.20000000'):
                vout = outpoint
                break;

        bal = self.nodes[0].getbalance()
        inputs = [{ "txid" : txid, "vout" : vout['n'], "scriptpubkey" : vout['scriptpubkey']['hex']}]
        outputs = { self.nodes[0].getnewaddress() : 2.19 }
        rawtx = self.nodes[2].createrawtransaction(inputs, outputs)
        rawtxpartialsigned = self.nodes[1].signrawtransaction(rawtx, inputs)
        assert_equal(rawtxpartialsigned['complete'], false) #node1 only has one key, can't comp. sign the tx
        
        rawtxsigned = self.nodes[2].signrawtransaction(rawtx, inputs)
        assert_equal(rawtxsigned['complete'], true) #node2 can sign the tx compl., own two of three keys
        self.nodes[2].sendrawtransaction(rawtxsigned['hex'])
        rawtx = self.nodes[0].decoderawtransaction(rawtxsigned['hex'])
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), bal+decimal('50.00000000')+decimal('2.19000000')) #block reward + tx

if __name__ == '__main__':
    rawtransactionstest().main()
