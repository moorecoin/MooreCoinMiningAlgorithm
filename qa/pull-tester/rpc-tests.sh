#!/bin/bash
set -e

curdir=$(cd $(dirname "$0"); pwd)
# get builddir and real_bitcoind
. "${curdir}/tests-config.sh"

export bitcoincli=${builddir}/qa/pull-tester/run-bitcoin-cli
export bitcoind=${real_bitcoind}

if [ "x${exeext}" = "x.exe" ]; then
  echo "win tests currently disabled"
  exit 0
fi

#run the tests

testscripts=(
    'wallet.py'
    'listtransactions.py'
    'mempool_resurrect_test.py'
    'txn_doublespend.py'
    'txn_doublespend.py --mineblock'
    'getchaintips.py'
    'rawtransactions.py'
    'rest.py'
    'mempool_spendcoinbase.py'
    'mempool_coinbase_spends.py'
    'httpbasics.py'
    'zapwallettxes.py'
    'proxy_test.py'
    'merkle_blocks.py'
    'signrawtransactions.py'
    'walletbackup.py'
    'nodehandling.py'
);
testscriptsext=(
    'bipdersig-p2p.py'
    'bipdersig.py'
    'getblocktemplate_longpoll.py'
    'getblocktemplate_proposals.py'
    'pruning.py'
    'forknotify.py'
    'invalidateblock.py'
    'keypool.py'
    'receivedby.py'
    'reindex.py'
    'rpcbind_test.py'
#   'script_test.py'
    'smartfees.py'
    'maxblocksinflight.py'
    'invalidblockrequest.py'
    'rawtransactions.py'
#    'forknotify.py'
    'p2p-acceptblock.py'
);

extarg="-extended"
passon=${@#$extarg}

if [ "x${enable_bitcoind}${enable_utils}${enable_wallet}" = "x111" ]; then
    for (( i = 0; i < ${#testscripts[@]}; i++ ))
    do
        if [ -z "$1" ] || [ "${1:0:1}" == "-" ] || [ "$1" == "${testscripts[$i]}" ] || [ "$1.py" == "${testscripts[$i]}" ]
        then
            echo -e "running testscript \033[1m${testscripts[$i]}...\033[0m"
            ${builddir}/qa/rpc-tests/${testscripts[$i]} --srcdir "${builddir}/src" ${passon}
        fi
    done
    for (( i = 0; i < ${#testscriptsext[@]}; i++ ))
    do
        if [ "$1" == $extarg ] || [ "$1" == "${testscriptsext[$i]}" ] || [ "$1.py" == "${testscriptsext[$i]}" ]
        then
            echo -e "running \033[1m2nd level\033[0m testscript \033[1m${testscriptsext[$i]}...\033[0m"
            ${builddir}/qa/rpc-tests/${testscriptsext[$i]} --srcdir "${builddir}/src" ${passon}
        fi
    done
else
  echo "no rpc tests to run. wallet, utils, and bitcoind must all be enabled"
fi
