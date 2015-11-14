// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_main_h
#define moorecoin_main_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "net.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "sync.h"
#include "tinyformat.h"
#include "txmempool.h"
#include "uint256.h"

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/unordered_map.hpp>

class cblockindex;
class cblocktreedb;
class cbloomfilter;
class cinv;
class cscriptcheck;
class cvalidationinterface;
class cvalidationstate;

struct cnodestatestats;

/** default for -blockmaxsize and -blockminsize, which control the range of sizes the mining code will create **/
static const unsigned int default_block_max_size = 750000;
static const unsigned int default_block_min_size = 0;
/** default for -blockprioritysize, maximum space for zero/low-fee transactions **/
static const unsigned int default_block_priority_size = 50000;
/** default for accepting alerts from the p2p network. */
static const bool default_alerts = true;
/** the maximum size for transactions we're willing to relay/mine */
static const unsigned int max_standard_tx_size = 100000;
/** maximum number of signature check operations in an isstandard() p2sh script */
static const unsigned int max_p2sh_sigops = 15;
/** the maximum number of sigops we're willing to relay/mine in a single tx */
static const unsigned int max_standard_tx_sigops = max_block_sigops/5;
/** default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int default_max_orphan_transactions = 100;
/** the maximum size of a blk?????.dat file (since 0.8) */
static const unsigned int max_blockfile_size = 0x8000000; // 128 mib
/** the pre-allocation chunk size for blk?????.dat files (since 0.8) */
static const unsigned int blockfile_chunk_size = 0x1000000; // 16 mib
/** the pre-allocation chunk size for rev?????.dat files (since 0.8) */
static const unsigned int undofile_chunk_size = 0x100000; // 1 mib
/** maximum number of script-checking threads allowed */
static const int max_scriptcheck_threads = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int default_scriptcheck_threads = 0;
/** number of blocks that can be requested at any given time from a single peer. */
static const int max_blocks_in_transit_per_peer = 16;
/** timeout in seconds during which a peer must stall block download progress before being disconnected. */
static const unsigned int block_stalling_timeout = 2;
/** number of headers sent in one getheaders result. we rely on the assumption that if a peer sends
 *  less than this number, we reached its tip. changing this value is a protocol upgrade. */
static const unsigned int max_headers_results = 2000;
/** size of the "block download window": how far ahead of our current height do we fetch?
 *  larger windows tolerate larger download speed differences between peer, but increase the potential
 *  degree of disordering of blocks on disk (which make reindexing and in the future perhaps pruning
 *  harder). we'll probably want to make this a per-peer adaptive value at some point. */
static const unsigned int block_download_window = 1024;
/** time to wait (in seconds) between writing blocks/block index to disk. */
static const unsigned int database_write_interval = 60 * 60;
/** time to wait (in seconds) between flushing chainstate to disk. */
static const unsigned int database_flush_interval = 24 * 60 * 60;
/** maximum length of reject messages. */
static const unsigned int max_reject_message_length = 111;

struct blockhasher
{
    size_t operator()(const uint256& hash) const { return hash.getcheaphash(); }
};

extern cscript coinbase_flags;
extern ccriticalsection cs_main;
extern ctxmempool mempool;
typedef boost::unordered_map<uint256, cblockindex*, blockhasher> blockmap;
extern blockmap mapblockindex;
extern uint64_t nlastblocktx;
extern uint64_t nlastblocksize;
extern const std::string strmessagemagic;
extern cwaitablecriticalsection csbestblock;
extern cconditionvariable cvblockchange;
extern bool fimporting;
extern bool freindex;
extern int nscriptcheckthreads;
extern bool ftxindex;
extern bool fisbaremultisigstd;
extern bool fcheckblockindex;
extern bool fcheckpointsenabled;
extern size_t ncoincacheusage;
extern cfeerate minrelaytxfee;
extern bool falerts;

/** best header we've seen so far (used for getheaders queries' starting points). */
extern cblockindex *pindexbestheader;

/** minimum disk space required - used in checkdiskspace() */
static const uint64_t nmindiskspace = 52428800;

/** pruning-related variables and constants */
/** true if any block files have ever been pruned. */
extern bool fhavepruned;
/** true if we're running in -prune mode. */
extern bool fprunemode;
/** number of mib of block files that we're trying to stay below. */
extern uint64_t nprunetarget;
/** block files containing a block-height within min_blocks_to_keep of chainactive.tip() will not be pruned. */
static const signed int min_blocks_to_keep = 288;

// require that user allocate at least 550mb for block & undo files (blk???.dat and rev???.dat)
// at 1mb per block, 288 blocks = 288mb.
// add 15% for undo data = 331mb
// add 20% for orphan block rate = 397mb
// we want the low water mark after pruning to be at least 397 mb and since we prune in
// full block file chunks, we need the high water mark which triggers the prune to be
// one 128mb block file + added 15% undo data = 147mb greater for a total of 545mb
// setting the target to > than 550mb will make it likely we can respect the target.
static const signed int min_disk_space_for_block_files = 550 * 1024 * 1024;

/** register with a network node to receive its signals */
void registernodesignals(cnodesignals& nodesignals);
/** unregister a network node */
void unregisternodesignals(cnodesignals& nodesignals);

/** 
 * process an incoming block. this only returns after the best known valid
 * block is made active. note that it does not, however, guarantee that the
 * specific block passed to it has been checked for validity!
 * 
 * @param[out]  state   this may be set to an error state if any error occurred processing it, including during validation/connection/etc of otherwise unrelated blocks during reorganisation; or it may be set to an invalid state if pblock is itself invalid (but this is not guaranteed even when the block is checked). if you want to *possibly* get feedback on whether pblock is valid, you must also install a cvalidationinterface (see validationinterface.h) - this will have its blockchecked method called whenever *any* block completes validation.
 * @param[in]   pfrom   the node which we are receiving the block from; it is added to mapblocksource and may be penalised if the block is invalid.
 * @param[in]   pblock  the block we want to process.
 * @param[in]   fforceprocessing process this block even if unrequested; used for non-network block sources and whitelisted peers.
 * @param[out]  dbp     if pblock is stored to disk (or already there), this will be set to its location.
 * @return true if state.isvalid()
 */
bool processnewblock(cvalidationstate &state, cnode* pfrom, cblock* pblock, bool fforceprocessing, cdiskblockpos *dbp);
/** check whether enough disk space is available for an incoming block */
bool checkdiskspace(uint64_t nadditionalbytes = 0);
/** open a block file (blk?????.dat) */
file* openblockfile(const cdiskblockpos &pos, bool freadonly = false);
/** open an undo file (rev?????.dat) */
file* openundofile(const cdiskblockpos &pos, bool freadonly = false);
/** translation to a filesystem path */
boost::filesystem::path getblockposfilename(const cdiskblockpos &pos, const char *prefix);
/** import blocks from an external file */
bool loadexternalblockfile(file* filein, cdiskblockpos *dbp = null);
/** initialize a new block tree database + block data on disk */
bool initblockindex();
/** load the block tree and coins database from disk */
bool loadblockindex();
/** unload database information */
void unloadblockindex();
/** process protocol messages received from a given node */
bool processmessages(cnode* pfrom);
/**
 * send queued protocol messages to be sent to a give node.
 *
 * @param[in]   pto             the node which we are sending messages to.
 * @param[in]   fsendtrickle    when true send the trickled data, otherwise trickle the data until true.
 */
bool sendmessages(cnode* pto, bool fsendtrickle);
/** run an instance of the script checking thread */
void threadscriptcheck();
/** try to detect partition (network isolation) attacks against us */
void partitioncheck(bool (*initialdownloadcheck)(), ccriticalsection& cs, const cblockindex *const &bestheader, int64_t npowtargetspacing);
/** check whether we are doing an initial block download (synchronizing from disk or network) */
bool isinitialblockdownload();
/** format a string that describes several potential problems detected by the core */
std::string getwarnings(const std::string& strfor);
/** retrieve a transaction (from memory pool, or from disk, if possible) */
bool gettransaction(const uint256 &hash, ctransaction &tx, uint256 &hashblock, bool fallowslow = false);
/** find the best known block, and make it the tip of the block chain */
bool activatebestchain(cvalidationstate &state, cblock *pblock = null);
camount getblocksubsidy(int nheight, const consensus::params& consensusparams);

/**
 * prune block and undo files (blk???.dat and undo???.dat) so that the disk space used is less than a user-defined target.
 * the user sets the target (in mb) on the command line or in config file.  this will be run on startup and whenever new
 * space is allocated in a block or undo file, staying below the target. changing back to unpruned requires a reindex
 * (which in this case means the blockchain must be re-downloaded.)
 *
 * pruning functions are called from flushstatetodisk when the global fcheckforpruning flag has been set.
 * block and undo files are deleted in lock-step (when blk00003.dat is deleted, so is rev00003.dat.)
 * pruning cannot take place until the longest chain is at least a certain length (100000 on mainnet, 1000 on testnet, 10 on regtest).
 * pruning will never delete a block within a defined distance (currently 288) from the active chain's tip.
 * the block index is updated by unsetting have_data and have_undo for any blocks that were stored in the deleted files.
 * a db flag records the fact that at least some block files have been pruned.
 *
 * @param[out]   setfilestoprune   the set of file indices that can be unlinked will be returned
 */
void findfilestoprune(std::set<int>& setfilestoprune);

/**
 *  actually unlink the specified files
 */
void unlinkprunedfiles(std::set<int>& setfilestoprune);

/** create a new block index entry for a given block hash */
cblockindex * insertblockindex(uint256 hash);
/** get statistics from node state */
bool getnodestatestats(nodeid nodeid, cnodestatestats &stats);
/** increase a node's misbehavior score. */
void misbehaving(nodeid nodeid, int howmuch);
/** flush all state, indexes and buffers to disk. */
void flushstatetodisk();
/** prune block files and flush state to disk. */
void pruneandflush();

/** (try to) add transaction to memory pool **/
bool accepttomemorypool(ctxmempool& pool, cvalidationstate &state, const ctransaction &tx, bool flimitfree,
                        bool* pfmissinginputs, bool frejectabsurdfee=false);


struct cnodestatestats {
    int nmisbehavior;
    int nsyncheight;
    int ncommonheight;
    std::vector<int> vheightinflight;
};

struct cdisktxpos : public cdiskblockpos
{
    unsigned int ntxoffset; // after header

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(*(cdiskblockpos*)this);
        readwrite(varint(ntxoffset));
    }

    cdisktxpos(const cdiskblockpos &blockin, unsigned int ntxoffsetin) : cdiskblockpos(blockin.nfile, blockin.npos), ntxoffset(ntxoffsetin) {
    }

    cdisktxpos() {
        setnull();
    }

    void setnull() {
        cdiskblockpos::setnull();
        ntxoffset = 0;
    }
};


camount getminrelayfee(const ctransaction& tx, unsigned int nbytes, bool fallowfree);

/**
 * check transaction inputs, and make sure any
 * pay-to-script-hash transactions are evaluating isstandard scripts
 * 
 * why bother? to avoid denial-of-service attacks; an attacker
 * can submit a standard hash... op_equal transaction,
 * which will get accepted into blocks. the redemption
 * script can be anything; an attacker could use a very
 * expensive-to-check-upon-redemption script like:
 *   dup checksig drop ... repeated 100 times... op_1
 */

/** 
 * check for standard transaction types
 * @param[in] mapinputs    map of previous transactions that have outputs we're spending
 * @return true if all inputs (scriptsigs) use only standard transaction forms
 */
bool areinputsstandard(const ctransaction& tx, const ccoinsviewcache& mapinputs);

/** 
 * count ecdsa signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see ctransaction::fetchinputs
 */
unsigned int getlegacysigopcount(const ctransaction& tx);

/**
 * count ecdsa signature operations in pay-to-script-hash inputs.
 * 
 * @param[in] mapinputs map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see ctransaction::fetchinputs
 */
unsigned int getp2shsigopcount(const ctransaction& tx, const ccoinsviewcache& mapinputs);


/**
 * check whether all inputs of this transaction are valid (no double spends, scripts & sigs, amounts)
 * this does not modify the utxo set. if pvchecks is not null, script checks are pushed onto it
 * instead of being performed inline.
 */
bool checkinputs(const ctransaction& tx, cvalidationstate &state, const ccoinsviewcache &view, bool fscriptchecks,
                 unsigned int flags, bool cachestore, std::vector<cscriptcheck> *pvchecks = null);

/** apply the effects of this transaction on the utxo set represented by view */
void updatecoins(const ctransaction& tx, cvalidationstate &state, ccoinsviewcache &inputs, int nheight);

/** context-independent validity checks */
bool checktransaction(const ctransaction& tx, cvalidationstate& state);

/** check for standard transaction types
 * @return true if all outputs (scriptpubkeys) use only standard transaction forms
 */
bool isstandardtx(const ctransaction& tx, std::string& reason);

/**
 * check if transaction is final and can be included in a block with the
 * specified height and time. consensus critical.
 */
bool isfinaltx(const ctransaction &tx, int nblockheight, int64_t nblocktime);

/**
 * check if transaction will be final in the next block to be created.
 *
 * calls isfinaltx() with current block height and appropriate block time.
 */
bool checkfinaltx(const ctransaction &tx);

/** 
 * closure representing one script verification
 * note that this stores references to the spending transaction 
 */
class cscriptcheck
{
private:
    cscript scriptpubkey;
    const ctransaction *ptxto;
    unsigned int nin;
    unsigned int nflags;
    bool cachestore;
    scripterror error;

public:
    cscriptcheck(): ptxto(0), nin(0), nflags(0), cachestore(false), error(script_err_unknown_error) {}
    cscriptcheck(const ccoins& txfromin, const ctransaction& txtoin, unsigned int ninin, unsigned int nflagsin, bool cachein) :
        scriptpubkey(txfromin.vout[txtoin.vin[ninin].prevout.n].scriptpubkey),
        ptxto(&txtoin), nin(ninin), nflags(nflagsin), cachestore(cachein), error(script_err_unknown_error) { }

    bool operator()();

    void swap(cscriptcheck &check) {
        scriptpubkey.swap(check.scriptpubkey);
        std::swap(ptxto, check.ptxto);
        std::swap(nin, check.nin);
        std::swap(nflags, check.nflags);
        std::swap(cachestore, check.cachestore);
        std::swap(error, check.error);
    }

    scripterror getscripterror() const { return error; }
};


/** functions for disk access for blocks */
bool writeblocktodisk(cblock& block, cdiskblockpos& pos, const cmessageheader::messagestartchars& messagestart);
bool readblockfromdisk(cblock& block, const cdiskblockpos& pos);
bool readblockfromdisk(cblock& block, const cblockindex* pindex);


/** functions for validating blocks and updating the block tree */

/** undo the effects of this block (with given index) on the utxo set represented by coins.
 *  in case pfclean is provided, operation will try to be tolerant about errors, and *pfclean
 *  will be true if no problems were found. otherwise, the return value will be false in case
 *  of problems. note that in any case, coins may be modified. */
bool disconnectblock(cblock& block, cvalidationstate& state, cblockindex* pindex, ccoinsviewcache& coins, bool* pfclean = null);

/** apply the effects of this block (with given index) on the utxo set represented by coins */
bool connectblock(const cblock& block, cvalidationstate& state, cblockindex* pindex, ccoinsviewcache& coins, bool fjustcheck = false);

/** context-independent validity checks */
bool checkblockheader(const cblockheader& block, cvalidationstate& state, bool fcheckpow = true);
bool checkblock(const cblock& block, cvalidationstate& state, bool fcheckpow = true, bool fcheckmerkleroot = true);

/** context-dependent validity checks */
bool contextualcheckblockheader(const cblockheader& block, cvalidationstate& state, cblockindex *pindexprev);
bool contextualcheckblock(const cblock& block, cvalidationstate& state, cblockindex *pindexprev);

/** check a block is completely valid from start to finish (only works on top of our current best block, with cs_main held) */
bool testblockvalidity(cvalidationstate &state, const cblock& block, cblockindex *pindexprev, bool fcheckpow = true, bool fcheckmerkleroot = true);

/** store block on disk. if dbp is non-null, the file is known to already reside on disk */
bool acceptblock(cblock& block, cvalidationstate& state, cblockindex **pindex, bool frequested, cdiskblockpos* dbp);
bool acceptblockheader(const cblockheader& block, cvalidationstate& state, cblockindex **ppindex= null);



class cblockfileinfo
{
public:
    unsigned int nblocks;      //! number of blocks stored in file
    unsigned int nsize;        //! number of used bytes of block file
    unsigned int nundosize;    //! number of used bytes in the undo file
    unsigned int nheightfirst; //! lowest height of block in file
    unsigned int nheightlast;  //! highest height of block in file
    uint64_t ntimefirst;         //! earliest time of block in file
    uint64_t ntimelast;          //! latest time of block in file

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(varint(nblocks));
        readwrite(varint(nsize));
        readwrite(varint(nundosize));
        readwrite(varint(nheightfirst));
        readwrite(varint(nheightlast));
        readwrite(varint(ntimefirst));
        readwrite(varint(ntimelast));
    }

     void setnull() {
         nblocks = 0;
         nsize = 0;
         nundosize = 0;
         nheightfirst = 0;
         nheightlast = 0;
         ntimefirst = 0;
         ntimelast = 0;
     }

     cblockfileinfo() {
         setnull();
     }

     std::string tostring() const;

     /** update statistics (does not update nsize) */
     void addblock(unsigned int nheightin, uint64_t ntimein) {
         if (nblocks==0 || nheightfirst > nheightin)
             nheightfirst = nheightin;
         if (nblocks==0 || ntimefirst > ntimein)
             ntimefirst = ntimein;
         nblocks++;
         if (nheightin > nheightlast)
             nheightlast = nheightin;
         if (ntimein > ntimelast)
             ntimelast = ntimein;
     }
};

/** raii wrapper for verifydb: verify consistency of the block and coin databases */
class cverifydb {
public:
    cverifydb();
    ~cverifydb();
    bool verifydb(ccoinsview *coinsview, int nchecklevel, int ncheckdepth);
};

/** find the last common block between the parameter chain and a locator. */
cblockindex* findforkinglobalindex(const cchain& chain, const cblocklocator& locator);

/** mark a block as invalid. */
bool invalidateblock(cvalidationstate& state, cblockindex *pindex);

/** remove invalidity status from a block and its descendants. */
bool reconsiderblock(cvalidationstate& state, cblockindex *pindex);

/** the currently-connected chain of blocks. */
extern cchain chainactive;

/** global variable that points to the active ccoinsview (protected by cs_main) */
extern ccoinsviewcache *pcoinstip;

/** global variable that points to the active block tree (protected by cs_main) */
extern cblocktreedb *pblocktree;

/**
 * return the spend height, which is one more than the inputs.getbestblock().
 * while checking, getbestblock() refers to the parent block. (protected by cs_main)
 * this is also true for mempool checks.
 */
int getspendheight(const ccoinsviewcache& inputs);

#endif // moorecoin_main_h
