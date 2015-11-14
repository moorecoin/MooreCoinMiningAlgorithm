#!/usr/bin/python
# copyright 2014 bitpay, inc.
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

import os
import bctest
import buildenv

if __name__ == '__main__':
	bctest.bctester(os.environ["srcdir"] + "/test/data",
			"moorecoin-util-test.json",buildenv)

