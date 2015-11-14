// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_version_h
#define moorecoin_version_h

/**
 * network protocol versioning
 */

static const int protocol_version = 70002;

//! initial proto version, to be increased after version/verack negotiation
static const int init_proto_version = 209;

//! in this version, 'getheaders' was introduced.
static const int getheaders_version = 31800;

//! disconnect from peers older than this proto version
static const int min_peer_proto_version = getheaders_version;

//! ntime field added to caddress, starting with this version;
//! if possible, avoid requesting addresses nodes older than this
static const int caddr_time_version = 31402;

//! only request blocks from nodes outside this range of versions
static const int noblks_version_start = 32000;
static const int noblks_version_end = 32400;

//! bip 0031, pong message, is enabled for all versions after this one
static const int bip0031_version = 60000;

//! "mempool" command, enhanced "getdata" behavior starts with this version
static const int mempool_gd_version = 60002;

#endif // moorecoin_version_h
