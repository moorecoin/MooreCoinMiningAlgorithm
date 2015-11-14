# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.
#
# helpful routines for regression testing
#

# add python-moorecoinrpc to module search path:
import os
import sys

from decimal import decimal, round_down
import json
import random
import shutil
import subprocess
import time
import re

from authproxy import authserviceproxy, jsonrpcexception
from util import *

def p2p_port(n):
    return 11000 + n + os.getpid()%999
def rpc_port(n):
    return 12000 + n + os.getpid()%999

def check_json_precision():
    """make sure json library being used does not lose precision converting btc values"""
    n = decimal("20000000.00000003")
    satoshis = int(json.loads(json.dumps(float(n)))*1.0e8)
    if satoshis != 2000000000000003:
        raise runtimeerror("json encode/decode loses precision")

def sync_blocks(rpc_connections, wait=1):
    """
    wait until everybody has the same block count
    """
    while true:
        counts = [ x.getblockcount() for x in rpc_connections ]
        if counts == [ counts[0] ]*len(counts):
            break
        time.sleep(wait)

def sync_mempools(rpc_connections, wait=1):
    """
    wait until everybody has the same transactions in their memory
    pools
    """
    while true:
        pool = set(rpc_connections[0].getrawmempool())
        num_match = 1
        for i in range(1, len(rpc_connections)):
            if set(rpc_connections[i].getrawmempool()) == pool:
                num_match = num_match+1
        if num_match == len(rpc_connections):
            break
        time.sleep(wait)

moorecoind_processes = {}

def initialize_datadir(dirname, n):
    datadir = os.path.join(dirname, "node"+str(n))
    if not os.path.isdir(datadir):
        os.makedirs(datadir)
    with open(os.path.join(datadir, "moorecoin.conf"), 'w') as f:
        f.write("regtest=1\n");
        f.write("rpcuser=rt\n");
        f.write("rpcpassword=rt\n");
        f.write("port="+str(p2p_port(n))+"\n");
        f.write("rpcport="+str(rpc_port(n))+"\n");
    return datadir

def initialize_chain(test_dir):
    """
    create (or copy from cache) a 200-block-long chain and
    4 wallets.
    moorecoind and moorecoin-cli must be in search path.
    """

    if not os.path.isdir(os.path.join("cache", "node0")):
        devnull = open("/dev/null", "w+")
        # create cache directories, run moorecoinds:
        for i in range(4):
            datadir=initialize_datadir("cache", i)
            args = [ os.getenv("moorecoind", "moorecoind"), "-keypool=1", "-datadir="+datadir, "-discover=0" ]
            if i > 0:
                args.append("-connect=127.0.0.1:"+str(p2p_port(0)))
            moorecoind_processes[i] = subprocess.popen(args)
            if os.getenv("python_debug", ""):
                print "initialize_chain: moorecoind started, calling moorecoin-cli -rpcwait getblockcount"
            subprocess.check_call([ os.getenv("moorecoincli", "moorecoin-cli"), "-datadir="+datadir,
                                    "-rpcwait", "getblockcount"], stdout=devnull)
            if os.getenv("python_debug", ""):
                print "initialize_chain: moorecoin-cli -rpcwait getblockcount completed"
        devnull.close()
        rpcs = []
        for i in range(4):
            try:
                url = "http://rt:rt@127.0.0.1:%d"%(rpc_port(i),)
                rpcs.append(authserviceproxy(url))
            except:
                sys.stderr.write("error connecting to "+url+"\n")
                sys.exit(1)

        # create a 200-block-long chain; each of the 4 nodes
        # gets 25 mature blocks and 25 immature.
        # blocks are created with timestamps 10 minutes apart, starting
        # at 1 jan 2014
        block_time = 1388534400
        for i in range(2):
            for peer in range(4):
                for j in range(25):
                    set_node_times(rpcs, block_time)
                    rpcs[peer].generate(1)
                    block_time += 10*60
                # must sync before next peer starts generating blocks
                sync_blocks(rpcs)

        # shut them down, and clean up cache directories:
        stop_nodes(rpcs)
        wait_moorecoinds()
        for i in range(4):
            os.remove(log_filename("cache", i, "debug.log"))
            os.remove(log_filename("cache", i, "db.log"))
            os.remove(log_filename("cache", i, "peers.dat"))
            os.remove(log_filename("cache", i, "fee_estimates.dat"))

    for i in range(4):
        from_dir = os.path.join("cache", "node"+str(i))
        to_dir = os.path.join(test_dir,  "node"+str(i))
        shutil.copytree(from_dir, to_dir)
        initialize_datadir(test_dir, i) # overwrite port/rpcport in moorecoin.conf

def initialize_chain_clean(test_dir, num_nodes):
    """
    create an empty blockchain and num_nodes wallets.
    useful if a test case wants complete control over initialization.
    """
    for i in range(num_nodes):
        datadir=initialize_datadir(test_dir, i)


def _rpchost_to_args(rpchost):
    '''convert optional ip:port spec to rpcconnect/rpcport args'''
    if rpchost is none:
        return []

    match = re.match('(\[[0-9a-fa-f:]+\]|[^:]+)(?::([0-9]+))?$', rpchost)
    if not match:
        raise valueerror('invalid rpc host spec ' + rpchost)

    rpcconnect = match.group(1)
    rpcport = match.group(2)

    if rpcconnect.startswith('['): # remove ipv6 [...] wrapping
        rpcconnect = rpcconnect[1:-1]

    rv = ['-rpcconnect=' + rpcconnect]
    if rpcport:
        rv += ['-rpcport=' + rpcport]
    return rv

def start_node(i, dirname, extra_args=none, rpchost=none, timewait=none, binary=none):
    """
    start a moorecoind and return rpc connection to it
    """
    datadir = os.path.join(dirname, "node"+str(i))
    if binary is none:
        binary = os.getenv("moorecoind", "moorecoind")
    args = [ binary, "-datadir="+datadir, "-keypool=1", "-discover=0", "-rest" ]
    if extra_args is not none: args.extend(extra_args)
    moorecoind_processes[i] = subprocess.popen(args)
    devnull = open("/dev/null", "w+")
    if os.getenv("python_debug", ""):
        print "start_node: moorecoind started, calling moorecoin-cli -rpcwait getblockcount"
    subprocess.check_call([ os.getenv("moorecoincli", "moorecoin-cli"), "-datadir="+datadir] +
                          _rpchost_to_args(rpchost)  +
                          ["-rpcwait", "getblockcount"], stdout=devnull)
    if os.getenv("python_debug", ""):
        print "start_node: calling moorecoin-cli -rpcwait getblockcount returned"
    devnull.close()
    url = "http://rt:rt@%s:%d" % (rpchost or '127.0.0.1', rpc_port(i))
    if timewait is not none:
        proxy = authserviceproxy(url, timeout=timewait)
    else:
        proxy = authserviceproxy(url)
    proxy.url = url # store url on proxy for info
    return proxy

def start_nodes(num_nodes, dirname, extra_args=none, rpchost=none, binary=none):
    """
    start multiple moorecoinds, return rpc connections to them
    """
    if extra_args is none: extra_args = [ none for i in range(num_nodes) ]
    if binary is none: binary = [ none for i in range(num_nodes) ]
    return [ start_node(i, dirname, extra_args[i], rpchost, binary=binary[i]) for i in range(num_nodes) ]

def log_filename(dirname, n_node, logname):
    return os.path.join(dirname, "node"+str(n_node), "regtest", logname)

def stop_node(node, i):
    node.stop()
    moorecoind_processes[i].wait()
    del moorecoind_processes[i]

def stop_nodes(nodes):
    for node in nodes:
        node.stop()
    del nodes[:] # emptying array closes connections as a side effect

def set_node_times(nodes, t):
    for node in nodes:
        node.setmocktime(t)

def wait_moorecoinds():
    # wait for all moorecoinds to cleanly exit
    for moorecoind in moorecoind_processes.values():
        moorecoind.wait()
    moorecoind_processes.clear()

def connect_nodes(from_connection, node_num):
    ip_port = "127.0.0.1:"+str(p2p_port(node_num))
    from_connection.addnode(ip_port, "onetry")
    # poll until version handshake complete to avoid race conditions
    # with transaction relaying
    while any(peer['version'] == 0 for peer in from_connection.getpeerinfo()):
        time.sleep(0.1)

def connect_nodes_bi(nodes, a, b):
    connect_nodes(nodes[a], b)
    connect_nodes(nodes[b], a)

def find_output(node, txid, amount):
    """
    return index to output of txid with value amount
    raises exception if there is none.
    """
    txdata = node.getrawtransaction(txid, 1)
    for i in range(len(txdata["vout"])):
        if txdata["vout"][i]["value"] == amount:
            return i
    raise runtimeerror("find_output txid %s : %s not found"%(txid,str(amount)))


def gather_inputs(from_node, amount_needed, confirmations_required=1):
    """
    return a random set of unspent txouts that are enough to pay amount_needed
    """
    assert(confirmations_required >=0)
    utxo = from_node.listunspent(confirmations_required)
    random.shuffle(utxo)
    inputs = []
    total_in = decimal("0.00000000")
    while total_in < amount_needed and len(utxo) > 0:
        t = utxo.pop()
        total_in += t["amount"]
        inputs.append({ "txid" : t["txid"], "vout" : t["vout"], "address" : t["address"] } )
    if total_in < amount_needed:
        raise runtimeerror("insufficient funds: need %d, have %d"%(amount_needed, total_in))
    return (total_in, inputs)

def make_change(from_node, amount_in, amount_out, fee):
    """
    create change output(s), return them
    """
    outputs = {}
    amount = amount_out+fee
    change = amount_in - amount
    if change > amount*2:
        # create an extra change output to break up big inputs
        change_address = from_node.getnewaddress()
        # split change in two, being careful of rounding:
        outputs[change_address] = decimal(change/2).quantize(decimal('0.00000001'), rounding=round_down)
        change = amount_in - amount - outputs[change_address]
    if change > 0:
        outputs[from_node.getnewaddress()] = change
    return outputs

def send_zeropri_transaction(from_node, to_node, amount, fee):
    """
    create&broadcast a zero-priority transaction.
    returns (txid, hex-encoded-txdata)
    ensures transaction is zero-priority by first creating a send-to-self,
    then using its output
    """

    # create a send-to-self with confirmed inputs:
    self_address = from_node.getnewaddress()
    (total_in, inputs) = gather_inputs(from_node, amount+fee*2)
    outputs = make_change(from_node, total_in, amount+fee, fee)
    outputs[self_address] = float(amount+fee)

    self_rawtx = from_node.createrawtransaction(inputs, outputs)
    self_signresult = from_node.signrawtransaction(self_rawtx)
    self_txid = from_node.sendrawtransaction(self_signresult["hex"], true)

    vout = find_output(from_node, self_txid, amount+fee)
    # now immediately spend the output to create a 1-input, 1-output
    # zero-priority transaction:
    inputs = [ { "txid" : self_txid, "vout" : vout } ]
    outputs = { to_node.getnewaddress() : float(amount) }

    rawtx = from_node.createrawtransaction(inputs, outputs)
    signresult = from_node.signrawtransaction(rawtx)
    txid = from_node.sendrawtransaction(signresult["hex"], true)

    return (txid, signresult["hex"])

def random_zeropri_transaction(nodes, amount, min_fee, fee_increment, fee_variants):
    """
    create a random zero-priority transaction.
    returns (txid, hex-encoded-transaction-data, fee)
    """
    from_node = random.choice(nodes)
    to_node = random.choice(nodes)
    fee = min_fee + fee_increment*random.randint(0,fee_variants)
    (txid, txhex) = send_zeropri_transaction(from_node, to_node, amount, fee)
    return (txid, txhex, fee)

def random_transaction(nodes, amount, min_fee, fee_increment, fee_variants):
    """
    create a random transaction.
    returns (txid, hex-encoded-transaction-data, fee)
    """
    from_node = random.choice(nodes)
    to_node = random.choice(nodes)
    fee = min_fee + fee_increment*random.randint(0,fee_variants)

    (total_in, inputs) = gather_inputs(from_node, amount+fee)
    outputs = make_change(from_node, total_in, amount, fee)
    outputs[to_node.getnewaddress()] = float(amount)

    rawtx = from_node.createrawtransaction(inputs, outputs)
    signresult = from_node.signrawtransaction(rawtx)
    txid = from_node.sendrawtransaction(signresult["hex"], true)

    return (txid, signresult["hex"], fee)

def assert_equal(thing1, thing2):
    if thing1 != thing2:
        raise assertionerror("%s != %s"%(str(thing1),str(thing2)))

def assert_greater_than(thing1, thing2):
    if thing1 <= thing2:
        raise assertionerror("%s <= %s"%(str(thing1),str(thing2)))

def assert_raises(exc, fun, *args, **kwds):
    try:
        fun(*args, **kwds)
    except exc:
        pass
    except exception as e:
        raise assertionerror("unexpected exception raised: "+type(e).__name__)
    else:
        raise assertionerror("no exception raised")
