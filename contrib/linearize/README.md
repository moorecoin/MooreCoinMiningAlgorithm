# linearize
construct a linear, no-fork, best version of the blockchain.

## step 1: download hash list

   $ ./linearize-hashes.py linearize.cfg > hashlist.txt

required configuration file settings for linearize-hashes:
* rpc: rpcuser, rpcpassword

optional config file setting for linearize-hashes:
* rpc: host, port
* block chain: min_height, max_height

## step 2: copy local block data

   $ ./linearize-data.py linearize.cfg

required configuration file settings:
* "input": bitcoind blocks/ directory containing blknnnnn.dat
* "hashlist": text file containing list of block hashes, linearized-hashes.py
output.
* "output_file": bootstrap.dat
      or
* "output": output directory for linearized blocks/blknnnnn.dat output

optional config file setting for linearize-data:
* "netmagic": network magic number
* "max_out_sz": maximum output file size (default 1000*1000*1000)
* "split_timestamp": split files when a new month is first seen, in addition to
reaching a maximum file size.
* "file_timestamp": set each file's last-modified time to that of the
most recent block in that file.
