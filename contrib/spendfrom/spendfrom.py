#!/usr/bin/env python
#
# use the raw transactions api to spend moorecoins received on particular addresses,
# and send any change back to that same address.
#
# example usage:
#  spendfrom.py  # lists available funds
#  spendfrom.py --from=address --to=address --amount=11.00
#
# assumes it will talk to a moorecoind or moorecoin-qt running
# on localhost.
#
# depends on jsonrpc
#

from decimal import *
import getpass
import math
import os
import os.path
import platform
import sys
import time
from jsonrpc import serviceproxy, json

base_fee=decimal("0.001")

def check_json_precision():
    """make sure json library being used does not lose precision converting btc values"""
    n = decimal("20000000.00000003")
    satoshis = int(json.loads(json.dumps(float(n)))*1.0e8)
    if satoshis != 2000000000000003:
        raise runtimeerror("json encode/decode loses precision")

def determine_db_dir():
    """return the default location of the moorecoin data directory"""
    if platform.system() == "darwin":
        return os.path.expanduser("~/library/application support/moorecoin/")
    elif platform.system() == "windows":
        return os.path.join(os.environ['appdata'], "moorecoin")
    return os.path.expanduser("~/.moorecoin")

def read_moorecoin_config(dbdir):
    """read the moorecoin.conf file from dbdir, returns dictionary of settings"""
    from configparser import safeconfigparser

    class fakesechead(object):
        def __init__(self, fp):
            self.fp = fp
            self.sechead = '[all]\n'
        def readline(self):
            if self.sechead:
                try: return self.sechead
                finally: self.sechead = none
            else:
                s = self.fp.readline()
                if s.find('#') != -1:
                    s = s[0:s.find('#')].strip() +"\n"
                return s

    config_parser = safeconfigparser()
    config_parser.readfp(fakesechead(open(os.path.join(dbdir, "moorecoin.conf"))))
    return dict(config_parser.items("all"))

def connect_json(config):
    """connect to a moorecoin json-rpc server"""
    testnet = config.get('testnet', '0')
    testnet = (int(testnet) > 0)  # 0/1 in config file, convert to true/false
    if not 'rpcport' in config:
        config['rpcport'] = 18332 if testnet else 8332
    connect = "http://%s:%s@127.0.0.1:%s"%(config['rpcuser'], config['rpcpassword'], config['rpcport'])
    try:
        result = serviceproxy(connect)
        # serviceproxy is lazy-connect, so send an rpc command mostly to catch connection errors,
        # but also make sure the moorecoind we're talking to is/isn't testnet:
        if result.getmininginfo()['testnet'] != testnet:
            sys.stderr.write("rpc server at "+connect+" testnet setting mismatch\n")
            sys.exit(1)
        return result
    except:
        sys.stderr.write("error connecting to rpc server at "+connect+"\n")
        sys.exit(1)

def unlock_wallet(moorecoind):
    info = moorecoind.getinfo()
    if 'unlocked_until' not in info:
        return true # wallet is not encrypted
    t = int(info['unlocked_until'])
    if t <= time.time():
        try:
            passphrase = getpass.getpass("wallet is locked; enter passphrase: ")
            moorecoind.walletpassphrase(passphrase, 5)
        except:
            sys.stderr.write("wrong passphrase\n")

    info = moorecoind.getinfo()
    return int(info['unlocked_until']) > time.time()

def list_available(moorecoind):
    address_summary = dict()

    address_to_account = dict()
    for info in moorecoind.listreceivedbyaddress(0):
        address_to_account[info["address"]] = info["account"]

    unspent = moorecoind.listunspent(0)
    for output in unspent:
        # listunspent doesn't give addresses, so:
        rawtx = moorecoind.getrawtransaction(output['txid'], 1)
        vout = rawtx["vout"][output['vout']]
        pk = vout["scriptpubkey"]

        # this code only deals with ordinary pay-to-moorecoin-address
        # or pay-to-script-hash outputs right now; anything exotic is ignored.
        if pk["type"] != "pubkeyhash" and pk["type"] != "scripthash":
            continue
        
        address = pk["addresses"][0]
        if address in address_summary:
            address_summary[address]["total"] += vout["value"]
            address_summary[address]["outputs"].append(output)
        else:
            address_summary[address] = {
                "total" : vout["value"],
                "outputs" : [output],
                "account" : address_to_account.get(address, "")
                }

    return address_summary

def select_coins(needed, inputs):
    # feel free to improve this, this is good enough for my simple needs:
    outputs = []
    have = decimal("0.0")
    n = 0
    while have < needed and n < len(inputs):
        outputs.append({ "txid":inputs[n]["txid"], "vout":inputs[n]["vout"]})
        have += inputs[n]["amount"]
        n += 1
    return (outputs, have-needed)

def create_tx(moorecoind, fromaddresses, toaddress, amount, fee):
    all_coins = list_available(moorecoind)

    total_available = decimal("0.0")
    needed = amount+fee
    potential_inputs = []
    for addr in fromaddresses:
        if addr not in all_coins:
            continue
        potential_inputs.extend(all_coins[addr]["outputs"])
        total_available += all_coins[addr]["total"]

    if total_available < needed:
        sys.stderr.write("error, only %f btc available, need %f\n"%(total_available, needed));
        sys.exit(1)

    #
    # note:
    # python's json/jsonrpc modules have inconsistent support for decimal numbers.
    # instead of wrestling with getting json.dumps() (used by jsonrpc) to encode
    # decimals, i'm casting amounts to float before sending them to moorecoind.
    #  
    outputs = { toaddress : float(amount) }
    (inputs, change_amount) = select_coins(needed, potential_inputs)
    if change_amount > base_fee:  # don't bother with zero or tiny change
        change_address = fromaddresses[-1]
        if change_address in outputs:
            outputs[change_address] += float(change_amount)
        else:
            outputs[change_address] = float(change_amount)

    rawtx = moorecoind.createrawtransaction(inputs, outputs)
    signed_rawtx = moorecoind.signrawtransaction(rawtx)
    if not signed_rawtx["complete"]:
        sys.stderr.write("signrawtransaction failed\n")
        sys.exit(1)
    txdata = signed_rawtx["hex"]

    return txdata

def compute_amount_in(moorecoind, txinfo):
    result = decimal("0.0")
    for vin in txinfo['vin']:
        in_info = moorecoind.getrawtransaction(vin['txid'], 1)
        vout = in_info['vout'][vin['vout']]
        result = result + vout['value']
    return result

def compute_amount_out(txinfo):
    result = decimal("0.0")
    for vout in txinfo['vout']:
        result = result + vout['value']
    return result

def sanity_test_fee(moorecoind, txdata_hex, max_fee):
    class feeerror(runtimeerror):
        pass
    try:
        txinfo = moorecoind.decoderawtransaction(txdata_hex)
        total_in = compute_amount_in(moorecoind, txinfo)
        total_out = compute_amount_out(txinfo)
        if total_in-total_out > max_fee:
            raise feeerror("rejecting transaction, unreasonable fee of "+str(total_in-total_out))

        tx_size = len(txdata_hex)/2
        kb = tx_size/1000  # integer division rounds down
        if kb > 1 and fee < base_fee:
            raise feeerror("rejecting no-fee transaction, larger than 1000 bytes")
        if total_in < 0.01 and fee < base_fee:
            raise feeerror("rejecting no-fee, tiny-amount transaction")
        # exercise for the reader: compute transaction priority, and
        # warn if this is a very-low-priority transaction

    except feeerror as err:
        sys.stderr.write((str(err)+"\n"))
        sys.exit(1)

def main():
    import optparse

    parser = optparse.optionparser(usage="%prog [options]")
    parser.add_option("--from", dest="fromaddresses", default=none,
                      help="addresses to get moorecoins from")
    parser.add_option("--to", dest="to", default=none,
                      help="address to get send moorecoins to")
    parser.add_option("--amount", dest="amount", default=none,
                      help="amount to send")
    parser.add_option("--fee", dest="fee", default="0.0",
                      help="fee to include")
    parser.add_option("--datadir", dest="datadir", default=determine_db_dir(),
                      help="location of moorecoin.conf file with rpc username/password (default: %default)")
    parser.add_option("--testnet", dest="testnet", default=false, action="store_true",
                      help="use the test network")
    parser.add_option("--dry_run", dest="dry_run", default=false, action="store_true",
                      help="don't broadcast the transaction, just create and print the transaction data")

    (options, args) = parser.parse_args()

    check_json_precision()
    config = read_moorecoin_config(options.datadir)
    if options.testnet: config['testnet'] = true
    moorecoind = connect_json(config)

    if options.amount is none:
        address_summary = list_available(moorecoind)
        for address,info in address_summary.iteritems():
            n_transactions = len(info['outputs'])
            if n_transactions > 1:
                print("%s %.8f %s (%d transactions)"%(address, info['total'], info['account'], n_transactions))
            else:
                print("%s %.8f %s"%(address, info['total'], info['account']))
    else:
        fee = decimal(options.fee)
        amount = decimal(options.amount)
        while unlock_wallet(moorecoind) == false:
            pass # keep asking for passphrase until they get it right
        txdata = create_tx(moorecoind, options.fromaddresses.split(","), options.to, amount, fee)
        sanity_test_fee(moorecoind, txdata, amount*decimal("0.01"))
        if options.dry_run:
            print(txdata)
        else:
            txid = moorecoind.sendrawtransaction(txdata)
            print(txid)

if __name__ == '__main__':
    main()
