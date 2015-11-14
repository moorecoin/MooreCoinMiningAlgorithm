#!/usr/bin/env bash
# copyright (c) 2014 the bitcoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# functions used by more than one test

function echoerr {
  echo "$@" 1>&2;
}

# usage: extractkey <key> "<json_object_string>"
# warning: this will only work for the very-well-behaved
# json produced by bitcoind, do not use it to try to
# parse arbitrary/nested/etc json.
function extractkey {
    echo $2 | tr -d ' "{}\n' | awk -v rs=',' -f: "\$1 ~ /$1/ { print \$2}"
}

function createdatadir {
  dir=$1
  mkdir -p $dir
  conf=$dir/bitcoin.conf
  echo "regtest=1" >> $conf
  echo "keypool=2" >> $conf
  echo "rpcuser=rt" >> $conf
  echo "rpcpassword=rt" >> $conf
  echo "rpcwait=1" >> $conf
  echo "walletnotify=${sendandwait} -stop" >> $conf
  shift
  while (( "$#" )); do
      echo $1 >> $conf
      shift
  done
}

function assertequal {
  if (( $( echo "$1 == $2" | bc ) == 0 ))
  then
    echoerr "assertequal: $1 != $2"
    declare -f cleanup > /dev/null 2>&1
    if [[ $? -eq 0 ]] ; then
        cleanup
    fi
    exit 1
  fi
}

# checkbalance -datadir=... amount account minconf
function checkbalance {
  declare -i expect="$2"
  b=$( $cli $1 getbalance $3 $4 )
  if (( $( echo "$b == $expect" | bc ) == 0 ))
  then
    echoerr "bad balance: $b (expected $2)"
    declare -f cleanup > /dev/null 2>&1
    if [[ $? -eq 0 ]] ; then
        cleanup
    fi
    exit 1
  fi
}

# use: address <datadir> [account]
function address {
  $cli $1 getnewaddress $2
}

# send from to amount
function send {
  from=$1
  to=$2
  amount=$3
  address=$(address $to)
  txid=$( ${sendandwait} $cli $from sendtoaddress $address $amount )
}

# use: unspent <datadir> <n'th-last-unspent> <var>
function unspent {
  local r=$( $cli $1 listunspent | awk -f'[ |:,"]+' "\$2 ~ /$3/ { print \$3 }" | tail -n $2 | head -n 1)
  echo $r
}

# use: createtxn1 <datadir> <n'th-last-unspent> <destaddress>
# produces hex from signrawtransaction
function createtxn1 {
  txid=$(unspent $1 $2 txid)
  amount=$(unspent $1 $2 amount)
  vout=$(unspent $1 $2 vout)
  rawtxn=$( $cli $1 createrawtransaction "[{\"txid\":\"$txid\",\"vout\":$vout}]" "{\"$3\":$amount}")
  extractkey hex "$( $cli $1 signrawtransaction $rawtxn )"
}

# use: sendrawtxn <datadir> <hex_txn_data>
function sendrawtxn {
  ${sendandwait} $cli $1 sendrawtransaction $2
}

# use: getblocks <datadir>
# returns number of blocks from getinfo
function getblocks {
    $cli $1 getblockcount
}
