regression tests of rpc interface
=================================

### [python-bitcoinrpc](https://github.com/jgarzik/python-bitcoinrpc)
git subtree of [https://github.com/jgarzik/python-bitcoinrpc](https://github.com/jgarzik/python-bitcoinrpc).
changes to python-bitcoinrpc should be made upstream, and then
pulled here using git subtree.

### [test_framework/test_framework.py](test_framework/test_framework.py)
base class for new regression tests.

### [test_framework/util.py](test_framework/util.py)
generally useful functions.

bash-based tests, to be ported to python:
-----------------------------------------
- conflictedbalance.sh : more testing of malleable transaction handling

notes
=====

you can run a single test by calling `qa/pull-tester/rpc-tests.sh <testname>`.

run all possible tests with `qa/pull-tester/rpc-tests.sh -extended`.

possible options:

```
-h, --help       show this help message and exit
  --nocleanup      leave bitcoinds and test.* datadir on exit or error
  --noshutdown     don't stop bitcoinds after the test execution
  --srcdir=srcdir  source directory containing bitcoind/bitcoin-cli (default:
                   ../../src)
  --tmpdir=tmpdir  root directory for datadirs
  --tracerpc       print out all rpc calls as they are made
```

if you set the environment variable `python_debug=1` you will get some debug output (example: `python_debug=1 qa/pull-tester/rpc-tests.sh wallet`). 

a 200-block -regtest blockchain and wallets for four nodes
is created the first time a regression test is run and
is stored in the cache/ directory. each node has 25 mature
blocks (25*50=1250 btc) in its wallet.

after the first run, the cache/ blockchain and wallets are
copied into a temporary directory and used as the initial
test state.

if you get into a bad state, you should be able
to recover with:

```bash
rm -rf cache
killall bitcoind
```
