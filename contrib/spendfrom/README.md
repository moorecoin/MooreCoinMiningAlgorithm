### spendfrom ###

use the raw transactions api to send coins received on a particular
address (or addresses). 

### usage: ###
depends on [jsonrpc](http://json-rpc.org/).

	spendfrom.py --from=fromaddress1[,fromaddress2] --to=toaddress --amount=amount \
	             --fee=fee --datadir=/path/to/.bitcoin --testnet --dry_run

with no arguments, outputs a list of amounts associated with addresses.

with arguments, sends coins received by the `fromaddress` addresses to the `toaddress`.

### notes ###

- you may explicitly specify how much fee to pay (a fee more than 1% of the amount
will fail,  though, to prevent bitcoin-losing accidents). spendfrom may fail if
it thinks the transaction would never be confirmed (if the amount being sent is
too small, or if the transaction is too many bytes for the fee).

- if a change output needs to be created, the change will be sent to the last
`fromaddress` (if you specify just one `fromaddress`, change will go back to it).

- if `--datadir` is not specified, the default datadir is used.

- the `--dry_run` option will just create and sign the transaction and print
the transaction data (as hexadecimal), instead of broadcasting it.

- if the transaction is created and broadcast successfully, a transaction id
is printed.

- if this was a tool for end-users and not programmers, it would have much friendlier
error-handling.
