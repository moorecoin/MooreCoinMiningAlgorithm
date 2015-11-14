### verify sf binaries ###
this script attempts to download the signature file `sha256sums.asc` from https://bitcoin.org.

it first checks if the signature passes, and then downloads the files specified in the file, and checks if the hashes of these files match those that are specified in the signature file.

the script returns 0 if everything passes the checks. it returns 1 if either the signature check or the hash check doesn't pass. if an error occurs the return value is 2.