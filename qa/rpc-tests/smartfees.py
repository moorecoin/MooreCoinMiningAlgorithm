#!/usr/bin/env python2
# copyright (c) 2014-2015 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

#
# test fee estimation code
#

from test_framework.test_framework import moorecointestframework
from test_framework.util import *

# construct 2 trivial p2sh's and the scriptsigs that spend them
# so we can create many many transactions without needing to spend
# time signing.
p2sh_1 = "2mysexegvzzprgnq1jdjdp5bretznm3roq2" # p2sh of "op_1 op_drop"
p2sh_2 = "2nbdpwq8aoo1eekexpnrkvr5xqr3m9ufcza" # p2sh of "op_2 op_drop"
# associated scriptsig's to spend satisfy p2sh_1 and p2sh_2
# 4 bytes of op_true and push 2-byte redeem script of "op_1 op_drop" or "op_2 op_drop"
script_sig = ["0451025175", "0451025275"]

def satoshi_round(amount):
    return  decimal(amount).quantize(decimal('0.00000001'), rounding=round_down)

def small_txpuzzle_randfee(from_node, conflist, unconflist, amount, min_fee, fee_increment):
    '''
    create and send a transaction with a random fee.
    the transaction pays to a trival p2sh script, and assumes that its inputs
    are of the same form.
    the function takes a list of confirmed outputs and unconfirmed outputs
    and attempts to use the confirmed list first for its inputs.
    it adds the newly created outputs to the unconfirmed list.
    returns (raw transaction, fee)
    '''
    # it's best to exponentially distribute our random fees
    # because the buckets are exponentially spaced.
    # exponentially distributed from 1-128 * fee_increment
    rand_fee = float(fee_increment)*(1.1892**random.randint(0,28))
    # total fee ranges from min_fee to min_fee + 127*fee_increment
    fee = min_fee - fee_increment + satoshi_round(rand_fee)
    inputs = []
    total_in = decimal("0.00000000")
    while total_in <= (amount + fee) and len(conflist) > 0:
        t = conflist.pop(0)
        total_in += t["amount"]
        inputs.append({ "txid" : t["txid"], "vout" : t["vout"]} )
    if total_in <= amount + fee:
        while total_in <= (amount + fee) and len(unconflist) > 0:
            t = unconflist.pop(0)
            total_in += t["amount"]
            inputs.append({ "txid" : t["txid"], "vout" : t["vout"]} )
        if total_in <= amount + fee:
            raise runtimeerror("insufficient funds: need %d, have %d"%(amount+fee, total_in))
    outputs = {}
    outputs[p2sh_1] = total_in - amount - fee
    outputs[p2sh_2] = amount
    rawtx = from_node.createrawtransaction(inputs, outputs)
    # createrawtransaction constructions a transaction that is ready to be signed
    # these transactions don't need to be signed, but we still have to insert the scriptsig
    # that will satisfy the scriptpubkey.
    completetx = rawtx[0:10]
    inputnum = 0
    for inp in inputs:
        completetx += rawtx[10+82*inputnum:82+82*inputnum]
        completetx += script_sig[inp["vout"]]
        completetx += rawtx[84+82*inputnum:92+82*inputnum]
        inputnum += 1
    completetx += rawtx[10+82*inputnum:]
    txid = from_node.sendrawtransaction(completetx, true)
    unconflist.append({ "txid" : txid, "vout" : 0 , "amount" : total_in - amount - fee})
    unconflist.append({ "txid" : txid, "vout" : 1 , "amount" : amount})

    return (completetx, fee)

def split_inputs(from_node, txins, txouts, initial_split = false):
    '''
    we need to generate a lot of very small inputs so we can generate a ton of transactions
    and they will have low priority.
    this function takes an input from txins, and creates and sends a transaction
    which splits the value into 2 outputs which are appended to txouts.
    '''
    prevtxout = txins.pop()
    inputs = []
    outputs = {}
    inputs.append({ "txid" : prevtxout["txid"], "vout" : prevtxout["vout"] })
    half_change = satoshi_round(prevtxout["amount"]/2)
    rem_change = prevtxout["amount"] - half_change  - decimal("0.00001000")
    outputs[p2sh_1] = half_change
    outputs[p2sh_2] = rem_change
    rawtx = from_node.createrawtransaction(inputs, outputs)
    # if this is the initial split we actually need to sign the transaction
    # otherwise we just need to insert the property scriptsig
    if (initial_split) :
        completetx = from_node.signrawtransaction(rawtx)["hex"]
    else :
        completetx = rawtx[0:82] + script_sig[prevtxout["vout"]] + rawtx[84:]
    txid = from_node.sendrawtransaction(completetx, true)
    txouts.append({ "txid" : txid, "vout" : 0 , "amount" : half_change})
    txouts.append({ "txid" : txid, "vout" : 1 , "amount" : rem_change})

def check_estimates(node, fees_seen, max_invalid, print_estimates = true):
    '''
    this function calls estimatefee and verifies that the estimates
    meet certain invariants.
    '''
    all_estimates = [ node.estimatefee(i) for i in range(1,26) ]
    if print_estimates:
        print([str(all_estimates[e-1]) for e in [1,2,3,6,15,25]])
    delta = 1.0e-6 # account for rounding error
    last_e = max(fees_seen)
    for e in filter(lambda x: x >= 0, all_estimates):
        # estimates should be within the bounds of what transactions fees actually were:
        if float(e)+delta < min(fees_seen) or float(e)-delta > max(fees_seen):
            raise assertionerror("estimated fee (%f) out of range (%f,%f)"
                                 %(float(e), min(fees_seen), max(fees_seen)))
        # estimates should be monotonically decreasing
        if float(e)-delta > last_e:
            raise assertionerror("estimated fee (%f) larger than last fee (%f) for lower number of confirms"
                                 %(float(e),float(last_e)))
        last_e = e
    valid_estimate = false
    invalid_estimates = 0
    for e in all_estimates:
        if e >= 0:
            valid_estimate = true
        else:
            invalid_estimates += 1
        # once we're at a high enough confirmation count that we can give an estimate
        # we should have estimates for all higher confirmation counts
        if valid_estimate and e < 0:
            raise assertionerror("invalid estimate appears at higher confirm count than valid estimate")
    # check on the expected number of different confirmation counts
    # that we might not have valid estimates for
    if invalid_estimates > max_invalid:
        raise assertionerror("more than (%d) invalid estimates"%(max_invalid))
    return all_estimates


class estimatefeetest(moorecointestframework):

    def setup_network(self):
        '''
        we'll setup the network to have 3 nodes that all mine with different parameters.
        but first we need to use one node to create a lot of small low priority outputs
        which we will use to generate our transactions.
        '''
        self.nodes = []
        # use node0 to mine blocks for input splitting
        self.nodes.append(start_node(0, self.options.tmpdir, ["-maxorphantx=1000",
                                                              "-relaypriority=0", "-whitelist=127.0.0.1"]))

        print("this test is time consuming, please be patient")
        print("splitting inputs to small size so we can generate low priority tx's")
        self.txouts = []
        self.txouts2 = []
        # split a coinbase into two transaction puzzle outputs
        split_inputs(self.nodes[0], self.nodes[0].listunspent(0), self.txouts, true)

        # mine
        while (len(self.nodes[0].getrawmempool()) > 0):
            self.nodes[0].generate(1)

        # repeatedly split those 2 outputs, doubling twice for each rep
        # use txouts to monitor the available utxo, since these won't be tracked in wallet
        reps = 0
        while (reps < 5):
            #double txouts to txouts2
            while (len(self.txouts)>0):
                split_inputs(self.nodes[0], self.txouts, self.txouts2)
            while (len(self.nodes[0].getrawmempool()) > 0):
                self.nodes[0].generate(1)
            #double txouts2 to txouts
            while (len(self.txouts2)>0):
                split_inputs(self.nodes[0], self.txouts2, self.txouts)
            while (len(self.nodes[0].getrawmempool()) > 0):
                self.nodes[0].generate(1)
            reps += 1
        print("finished splitting")

        # now we can connect the other nodes, didn't want to connect them earlier
        # so the estimates would not be affected by the splitting transactions
        # node1 mines small blocks but that are bigger than the expected transaction rate,
        # and allows free transactions.
        # note: the createnewblock code starts counting block size at 1,000 bytes,
        # (17k is room enough for 110 or so transactions)
        self.nodes.append(start_node(1, self.options.tmpdir,
                                     ["-blockprioritysize=1500", "-blockmaxsize=18000",
                                      "-maxorphantx=1000", "-relaypriority=0", "-debug=estimatefee"]))
        connect_nodes(self.nodes[1], 0)

        # node2 is a stingy miner, that
        # produces too small blocks (room for only 70 or so transactions)
        node2args = ["-blockprioritysize=0", "-blockmaxsize=12000", "-maxorphantx=1000", "-relaypriority=0"]

        self.nodes.append(start_node(2, self.options.tmpdir, node2args))
        connect_nodes(self.nodes[0], 2)
        connect_nodes(self.nodes[2], 1)

        self.is_network_split = false
        self.sync_all()

    def transact_and_mine(self, numblocks, mining_node):
        min_fee = decimal("0.00001")
        # we will now mine numblocks blocks generating on average 100 transactions between each block
        # we shuffle our confirmed txout set before each set of transactions
        # small_txpuzzle_randfee will use the transactions that have inputs already in the chain when possible
        # resorting to tx's that depend on the mempool when those run out
        for i in range(numblocks):
            random.shuffle(self.confutxo)
            for j in range(random.randrange(100-50,100+50)):
                from_index = random.randint(1,2)
                (txhex, fee) = small_txpuzzle_randfee(self.nodes[from_index], self.confutxo,
                                                      self.memutxo, decimal("0.005"), min_fee, min_fee)
                tx_kbytes = (len(txhex)/2)/1000.0
                self.fees_per_kb.append(float(fee)/tx_kbytes)
            sync_mempools(self.nodes[0:3],.1)
            mined = mining_node.getblock(mining_node.generate(1)[0],true)["tx"]
            sync_blocks(self.nodes[0:3],.1)
            #update which txouts are confirmed
            newmem = []
            for utx in self.memutxo:
                if utx["txid"] in mined:
                    self.confutxo.append(utx)
                else:
                    newmem.append(utx)
            self.memutxo = newmem

    def run_test(self):
        self.fees_per_kb = []
        self.memutxo = []
        self.confutxo = self.txouts # start with the set of confirmed txouts after splitting
        print("checking estimates for 1/2/3/6/15/25 blocks")
        print("creating transactions and mining them with a huge block size")
        # create transactions and mine 20 big blocks with node 0 such that the mempool is always emptied
        self.transact_and_mine(30, self.nodes[0])
        check_estimates(self.nodes[1], self.fees_per_kb, 1)

        print("creating transactions and mining them with a block size that can't keep up")
        # create transactions and mine 30 small blocks with node 2, but create txs faster than we can mine
        self.transact_and_mine(20, self.nodes[2])
        check_estimates(self.nodes[1], self.fees_per_kb, 3)

        print("creating transactions and mining them at a block size that is just big enough")
        # generate transactions while mining 40 more blocks, this time with node1
        # which mines blocks with capacity just above the rate that transactions are being created
        self.transact_and_mine(40, self.nodes[1])
        check_estimates(self.nodes[1], self.fees_per_kb, 2)

        # finish by mining a normal-sized block:
        while len(self.nodes[1].getrawmempool()) > 0:
            self.nodes[1].generate(1)

        sync_blocks(self.nodes[0:3],.1)
        print("final estimates after emptying mempools")
        check_estimates(self.nodes[1], self.fees_per_kb, 2)

if __name__ == '__main__':
    estimatefeetest().main()
