#!/usr/bin/env python2
# copyright (c) 2015 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import moorecointestframework
from test_framework.util import *


class signrawtransactionstest(moorecointestframework):
    """tests transaction signing via rpc command "signrawtransaction"."""

    def setup_chain(self):
        print('initializing test directory ' + self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 1)

    def setup_network(self, split=false):
        self.nodes = start_nodes(1, self.options.tmpdir)
        self.is_network_split = false

    def successful_signing_test(self):
        """creates and signs a valid raw transaction with one input.

        expected results:

        1) the transaction has a complete set of signatures
        2) no script verification error occurred"""
        privkeys = ['cuekhd5orzt3mz8p9pxyrehfswtvfgsfdjizzbcjubaagk1btj7n']

        inputs = [
            # valid pay-to-pubkey script
            {'txid': '9b907ef1e3c26fc71fe4a4b3580bc75264112f95050014157059c736f0202e71', 'vout': 0,
             'scriptpubkey': '76a91460baa0f494b38ce3c940dea67f3804dc52d1fb9488ac'}
        ]

        outputs = {'mplqjfk79b7ccv4vmjwewaj5mpx8up5zxb': 0.1}

        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        rawtxsigned = self.nodes[0].signrawtransaction(rawtx, inputs, privkeys)

        # 1) the transaction has a complete set of signatures
        assert 'complete' in rawtxsigned
        assert_equal(rawtxsigned['complete'], true)

        # 2) no script verification error occurred
        assert 'errors' not in rawtxsigned

    def script_verification_error_test(self):
        """creates and signs a raw transaction with valid (vin 0), invalid (vin 1) and one missing (vin 2) input script.

        expected results:

        3) the transaction has no complete set of signatures
        4) two script verification errors occurred
        5) script verification errors have certain properties ("txid", "vout", "scriptsig", "sequence", "error")
        6) the verification errors refer to the invalid (vin 1) and missing input (vin 2)"""
        privkeys = ['cuekhd5orzt3mz8p9pxyrehfswtvfgsfdjizzbcjubaagk1btj7n']

        inputs = [
            # valid pay-to-pubkey script
            {'txid': '9b907ef1e3c26fc71fe4a4b3580bc75264112f95050014157059c736f0202e71', 'vout': 0},
            # invalid script
            {'txid': '5b8673686910442c644b1f4993d8f7753c7c8fcb5c87ee40d56eaeef25204547', 'vout': 7},
            # missing scriptpubkey
            {'txid': '9b907ef1e3c26fc71fe4a4b3580bc75264112f95050014157059c736f0202e71', 'vout': 1},
        ]

        scripts = [
            # valid pay-to-pubkey script
            {'txid': '9b907ef1e3c26fc71fe4a4b3580bc75264112f95050014157059c736f0202e71', 'vout': 0,
             'scriptpubkey': '76a91460baa0f494b38ce3c940dea67f3804dc52d1fb9488ac'},
            # invalid script
            {'txid': '5b8673686910442c644b1f4993d8f7753c7c8fcb5c87ee40d56eaeef25204547', 'vout': 7,
             'scriptpubkey': 'badbadbadbad'}
        ]

        outputs = {'mplqjfk79b7ccv4vmjwewaj5mpx8up5zxb': 0.1}

        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        rawtxsigned = self.nodes[0].signrawtransaction(rawtx, scripts, privkeys)

        # 3) the transaction has no complete set of signatures
        assert 'complete' in rawtxsigned
        assert_equal(rawtxsigned['complete'], false)

        # 4) two script verification errors occurred
        assert 'errors' in rawtxsigned
        assert_equal(len(rawtxsigned['errors']), 2)

        # 5) script verification errors have certain properties
        assert 'txid' in rawtxsigned['errors'][0]
        assert 'vout' in rawtxsigned['errors'][0]
        assert 'scriptsig' in rawtxsigned['errors'][0]
        assert 'sequence' in rawtxsigned['errors'][0]
        assert 'error' in rawtxsigned['errors'][0]

        # 6) the verification errors refer to the invalid (vin 1) and missing input (vin 2)
        assert_equal(rawtxsigned['errors'][0]['txid'], inputs[1]['txid'])
        assert_equal(rawtxsigned['errors'][0]['vout'], inputs[1]['vout'])
        assert_equal(rawtxsigned['errors'][1]['txid'], inputs[2]['txid'])
        assert_equal(rawtxsigned['errors'][1]['vout'], inputs[2]['vout'])

    def run_test(self):
        self.successful_signing_test()
        self.script_verification_error_test()


if __name__ == '__main__':
    signrawtransactionstest().main()
