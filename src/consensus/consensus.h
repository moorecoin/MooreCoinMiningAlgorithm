// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_consensus_consensus_h
#define moorecoin_consensus_consensus_h

/** the maximum allowed size for a serialized block, in bytes (network rule) */
static const unsigned int max_block_size = 1000000;
/** the maximum allowed number of signature check operations in a block (network rule) */
static const unsigned int max_block_sigops = max_block_size/50;
/** coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int coinbase_maturity = 100;
/** threshold for nlocktime: below this value it is interpreted as block number, otherwise as unix timestamp. */
static const unsigned int locktime_threshold = 500000000; // tue nov  5 00:53:20 1985 utc

#endif // moorecoin_consensus_consensus_h
