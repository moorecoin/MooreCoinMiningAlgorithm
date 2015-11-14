wallet tools
---------------------

### [bitrpc](/contrib/bitrpc) ###
allows for sending of all standard bitcoin commands via rpc rather than as command line args.

### [spendfrom](/contrib/spendfrom) ###

use the raw transactions api to send coins received on a particular
address (or addresses).

repository tools
---------------------

### [developer tools](/contrib/devtools) ###
specific tools for developers working on this repository.
contains the script `github-merge.sh` for merging github pull requests securely and signing them using gpg.

### [verify-commits](/contrib/verify-commits) ###
tool to verify that every merge commit was signed by a developer using the above `github-merge.sh` script.

### [linearize](/contrib/linearize) ###
construct a linear, no-fork, best version of the blockchain.

### [qos](/contrib/qos) ###

a linux bash script that will set up traffic control (tc) to limit the outgoing bandwidth for connections to the bitcoin network. this means one can have an always-on bitcoind instance running, and another local bitcoind/bitcoin-qt instance which connects to this node and receives blocks from it.

### [seeds](/contrib/seeds) ###
utility to generate the pnseed[] array that is compiled into the client.

build tools and keys
---------------------

### [debian](/contrib/debian) ###
contains files used to package bitcoind/bitcoin-qt
for debian-based linux systems. if you compile bitcoind/bitcoin-qt yourself, there are some useful files here.

### [gitian-descriptors](/contrib/gitian-descriptors) ###
gavin's notes on getting gitian builds up and running using kvm.

### [gitian-downloader](/contrib/gitian-downloader)
various pgp files of core developers. 

### [macdeploy](/contrib/macdeploy) ###
scripts and notes for mac builds. 

test and verify tools 
---------------------

### [testgen](/contrib/testgen) ###
utilities to generate test vectors for the data-driven bitcoin tests.

### [test patches](/contrib/test-patches) ###
these patches are applied when the automated pull-tester
tests each pull and when master is tested using jenkins.

### [verify sf binaries](/contrib/verifysfbinaries) ###
this script attempts to download and verify the signature file sha256sums.asc from sourceforge.
