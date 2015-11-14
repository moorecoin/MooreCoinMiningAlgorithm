from jsonrpc import serviceproxy
import sys
import string
import getpass

# ===== begin user settings =====
# if you do not set these you will be prompted for a password for every command
rpcuser = ""
rpcpass = ""
# ====== end user settings ======


if rpcpass == "":
    access = serviceproxy("http://127.0.0.1:8332")
else:
    access = serviceproxy("http://"+rpcuser+":"+rpcpass+"@127.0.0.1:8332")
cmd = sys.argv[1].lower()

if cmd == "backupwallet":
    try:
        path = raw_input("enter destination path/filename: ")
        print access.backupwallet(path)
    except exception as inst:
        print inst

elif cmd == "encryptwallet":
    try:
        pwd = getpass.getpass(prompt="enter passphrase: ")
        pwd2 = getpass.getpass(prompt="repeat passphrase: ")
        if pwd == pwd2:
            access.encryptwallet(pwd)
            print "\n---wallet encrypted. server stopping, restart to run with encrypted wallet---\n"
        else:
            print "\n---passphrases do not match---\n"
    except exception as inst:
        print inst

elif cmd == "getaccount":
    try:
        addr = raw_input("enter a moorecoin address: ")
        print access.getaccount(addr)
    except exception as inst:
        print inst

elif cmd == "getaccountaddress":
    try:
        acct = raw_input("enter an account name: ")
        print access.getaccountaddress(acct)
    except exception as inst:
        print inst

elif cmd == "getaddressesbyaccount":
    try:
        acct = raw_input("enter an account name: ")
        print access.getaddressesbyaccount(acct)
    except exception as inst:
        print inst

elif cmd == "getbalance":
    try:
        acct = raw_input("enter an account (optional): ")
        mc = raw_input("minimum confirmations (optional): ")
        try:
            print access.getbalance(acct, mc)
        except:
            print access.getbalance()
    except exception as inst:
        print inst

elif cmd == "getblockbycount":
    try:
        height = raw_input("height: ")
        print access.getblockbycount(height)
    except exception as inst:
        print inst

elif cmd == "getblockcount":
    try:
        print access.getblockcount()
    except exception as inst:
        print inst

elif cmd == "getblocknumber":
    try:
        print access.getblocknumber()
    except exception as inst:
        print inst

elif cmd == "getconnectioncount":
    try:
        print access.getconnectioncount()
    except exception as inst:
        print inst

elif cmd == "getdifficulty":
    try:
        print access.getdifficulty()
    except exception as inst:
        print inst

elif cmd == "getgenerate":
    try:
        print access.getgenerate()
    except exception as inst:
        print inst

elif cmd == "gethashespersec":
    try:
        print access.gethashespersec()
    except exception as inst:
        print inst

elif cmd == "getinfo":
    try:
        print access.getinfo()
    except exception as inst:
        print inst

elif cmd == "getnewaddress":
    try:
        acct = raw_input("enter an account name: ")
        try:
            print access.getnewaddress(acct)
        except:
            print access.getnewaddress()
    except exception as inst:
        print inst

elif cmd == "getreceivedbyaccount":
    try:
        acct = raw_input("enter an account (optional): ")
        mc = raw_input("minimum confirmations (optional): ")
        try:
            print access.getreceivedbyaccount(acct, mc)
        except:
            print access.getreceivedbyaccount()
    except exception as inst:
        print inst

elif cmd == "getreceivedbyaddress":
    try:
        addr = raw_input("enter a moorecoin address (optional): ")
        mc = raw_input("minimum confirmations (optional): ")
        try:
            print access.getreceivedbyaddress(addr, mc)
        except:
            print access.getreceivedbyaddress()
    except exception as inst:
        print inst

elif cmd == "gettransaction":
    try:
        txid = raw_input("enter a transaction id: ")
        print access.gettransaction(txid)
    except exception as inst:
        print inst

elif cmd == "getwork":
    try:
        data = raw_input("data (optional): ")
        try:
            print access.gettransaction(data)
        except:
            print access.gettransaction()
    except exception as inst:
        print inst

elif cmd == "help":
    try:
        cmd = raw_input("command (optional): ")
        try:
            print access.help(cmd)
        except:
            print access.help()
    except exception as inst:
        print inst

elif cmd == "listaccounts":
    try:
        mc = raw_input("minimum confirmations (optional): ")
        try:
            print access.listaccounts(mc)
        except:
            print access.listaccounts()
    except exception as inst:
        print inst

elif cmd == "listreceivedbyaccount":
    try:
        mc = raw_input("minimum confirmations (optional): ")
        incemp = raw_input("include empty? (true/false, optional): ")
        try:
            print access.listreceivedbyaccount(mc, incemp)
        except:
            print access.listreceivedbyaccount()
    except exception as inst:
        print inst

elif cmd == "listreceivedbyaddress":
    try:
        mc = raw_input("minimum confirmations (optional): ")
        incemp = raw_input("include empty? (true/false, optional): ")
        try:
            print access.listreceivedbyaddress(mc, incemp)
        except:
            print access.listreceivedbyaddress()
    except exception as inst:
        print inst

elif cmd == "listtransactions":
    try:
        acct = raw_input("account (optional): ")
        count = raw_input("number of transactions (optional): ")
        frm = raw_input("skip (optional):")
        try:
            print access.listtransactions(acct, count, frm)
        except:
            print access.listtransactions()
    except exception as inst:
        print inst

elif cmd == "move":
    try:
        frm = raw_input("from: ")
        to = raw_input("to: ")
        amt = raw_input("amount:")
        mc = raw_input("minimum confirmations (optional): ")
        comment = raw_input("comment (optional): ")
        try:
            print access.move(frm, to, amt, mc, comment)
        except:
            print access.move(frm, to, amt)
    except exception as inst:
        print inst

elif cmd == "sendfrom":
    try:
        frm = raw_input("from: ")
        to = raw_input("to: ")
        amt = raw_input("amount:")
        mc = raw_input("minimum confirmations (optional): ")
        comment = raw_input("comment (optional): ")
        commentto = raw_input("comment-to (optional): ")
        try:
            print access.sendfrom(frm, to, amt, mc, comment, commentto)
        except:
            print access.sendfrom(frm, to, amt)
    except exception as inst:
        print inst

elif cmd == "sendmany":
    try:
        frm = raw_input("from: ")
        to = raw_input("to (in format address1:amount1,address2:amount2,...): ")
        mc = raw_input("minimum confirmations (optional): ")
        comment = raw_input("comment (optional): ")
        try:
            print access.sendmany(frm,to,mc,comment)
        except:
            print access.sendmany(frm,to)
    except exception as inst:
        print inst

elif cmd == "sendtoaddress":
    try:
        to = raw_input("to (in format address1:amount1,address2:amount2,...): ")
        amt = raw_input("amount:")
        comment = raw_input("comment (optional): ")
        commentto = raw_input("comment-to (optional): ")
        try:
            print access.sendtoaddress(to,amt,comment,commentto)
        except:
            print access.sendtoaddress(to,amt)
    except exception as inst:
        print inst

elif cmd == "setaccount":
    try:
        addr = raw_input("address: ")
        acct = raw_input("account:")
        print access.setaccount(addr,acct)
    except exception as inst:
        print inst

elif cmd == "setgenerate":
    try:
        gen= raw_input("generate? (true/false): ")
        cpus = raw_input("max processors/cores (-1 for unlimited, optional):")
        try:
            print access.setgenerate(gen, cpus)
        except:
            print access.setgenerate(gen)
    except exception as inst:
        print inst

elif cmd == "settxfee":
    try:
        amt = raw_input("amount:")
        print access.settxfee(amt)
    except exception as inst:
        print inst

elif cmd == "stop":
    try:
        print access.stop()
    except exception as inst:
        print inst

elif cmd == "validateaddress":
    try:
        addr = raw_input("address: ")
        print access.validateaddress(addr)
    except exception as inst:
        print inst

elif cmd == "walletpassphrase":
    try:
        pwd = getpass.getpass(prompt="enter wallet passphrase: ")
        access.walletpassphrase(pwd, 60)
        print "\n---wallet unlocked---\n"
    except exception as inst:
        print inst

elif cmd == "walletpassphrasechange":
    try:
        pwd = getpass.getpass(prompt="enter old wallet passphrase: ")
        pwd2 = getpass.getpass(prompt="enter new wallet passphrase: ")
        access.walletpassphrasechange(pwd, pwd2)
        print
        print "\n---passphrase changed---\n"
    except exception as inst:
        print inst

else:
    print "command not found or not supported"
