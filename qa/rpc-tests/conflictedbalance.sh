#!/usr/bin/env bash
# copyright (c) 2014 the bitcoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# test marking of spent outputs

# create a transaction graph with four transactions,
# a/b/c/d
# c spends a
# d spends b and c

# then simulate c being mutated, to create c'
#  that is mined.
# a is still (correctly) considered spent.
# b should be treated as unspent

if [ $# -lt 1 ]; then
        echo "usage: $0 path_to_binaries"
        echo "e.g. $0 ../../src"
        echo "env vars bitcoind and bitcoincli may be used to specify the exact binaries used"
        exit 1
fi

set -f

bitcoind=${bitcoind:-${1}/bitcoind}
cli=${bitcoincli:-${1}/bitcoin-cli}

dir="${bash_source%/*}"
sendandwait="${dir}/send.sh"
if [[ ! -d "$dir" ]]; then dir="$pwd"; fi
. "$dir/util.sh"

d=$(mktemp -d test.xxxxx)

# two nodes; one will play the part of merchant, the
# other an evil transaction-mutating miner.

d1=${d}/node1
createdatadir $d1 port=11000 rpcport=11001
b1args="-datadir=$d1 -debug=mempool"
$bitcoind $b1args &
b1pid=$!

d2=${d}/node2
createdatadir $d2 port=11010 rpcport=11011
b2args="-datadir=$d2 -debug=mempool"
$bitcoind $b2args &
b2pid=$!

# wait until both nodes are at the same block number
function waitblocks {
    while :
    do
        sleep 1
        declare -i blocks1=$( getblocks $b1args )
        declare -i blocks2=$( getblocks $b2args )
        if (( blocks1 == blocks2 ))
        then
            break
        fi
    done
}

# wait until node has $n peers
function waitpeers {
    while :
    do
        declare -i peers=$( $cli $1 getconnectioncount )
        if (( peers == "$2" ))
        then
            break
        fi
        sleep 1
    done
}

echo "generating test blockchain..."

# start with b2 connected to b1:
$cli $b2args addnode 127.0.0.1:11000 onetry
waitpeers "$b1args" 1

# 2 block, 50 xbt each == 100 xbt
# these will be transactions "a" and "b"
$cli $b1args generate 2

waitblocks
# 100 blocks, 0 mature == 0 xbt
$cli $b2args generate 100
waitblocks

checkbalance "$b1args" 100
checkbalance "$b2args" 0

# restart b2 with no connection
$cli $b2args stop > /dev/null 2>&1
wait $b2pid
$bitcoind $b2args &
b2pid=$!

b1address=$( $cli $b1args getnewaddress )
b2address=$( $cli $b2args getnewaddress )

# transaction c: send-to-self, spend a
txid_c=$( $cli $b1args sendtoaddress $b1address 50.0)

# transaction d: spends b and c
txid_d=$( $cli $b1args sendtoaddress $b2address 100.0)

checkbalance "$b1args" 0

# mutate txid_c and add it to b2's memory pool:
rawtx_c=$( $cli $b1args getrawtransaction $txid_c )

# ... mutate c to create c'
l=${rawtx_c:82:2}
newlen=$( printf "%x" $(( 16#$l + 1 )) )
mutatedtx_c=${rawtx_c:0:82}${newlen}4c${rawtx_c:84}
# ... give mutated tx1 to b2:
mutatedtxid=$( $cli $b2args sendrawtransaction $mutatedtx_c )

echo "txid_c: " $txid_c
echo "mutated: " $mutatedtxid

# re-connect nodes, and have both nodes mine some blocks:
$cli $b2args addnode 127.0.0.1:11000 onetry
waitpeers "$b1args" 1

# having b2 mine the next block puts the mutated
# transaction c in the chain:
$cli $b2args generate 1
waitblocks

# b1 should still be able to spend 100, because d is conflicted
# so does not count as a spend of b
checkbalance "$b1args" 100

$cli $b2args stop > /dev/null 2>&1
wait $b2pid
$cli $b1args stop > /dev/null 2>&1
wait $b1pid

echo "tests successful, cleaning up"
rm -rf $d
exit 0
