// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"

#include "addrman.h"
#include "alert.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "consensus/validation.h"
#include "init.h"
#include "merkleblock.h"
#include "net.h"
#include "pow.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "undo.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"

#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/thread.hpp>

using namespace std;

#if defined(ndebug)
# error "moorecoin cannot be compiled without assertions."
#endif

/**
 * global state
 */

ccriticalsection cs_main;

blockmap mapblockindex;
cchain chainactive;
cblockindex *pindexbestheader = null;
int64_t ntimebestreceived = 0;
cwaitablecriticalsection csbestblock;
cconditionvariable cvblockchange;
int nscriptcheckthreads = 0;
bool fimporting = false;
bool freindex = false;
bool ftxindex = false;
bool fhavepruned = false;
bool fprunemode = false;
bool fisbaremultisigstd = true;
bool fcheckblockindex = false;
bool fcheckpointsenabled = true;
size_t ncoincacheusage = 5000 * 300;
uint64_t nprunetarget = 0;
bool falerts = default_alerts;

/** fees smaller than this (in satoshi) are considered zero fee (for relaying and mining) */
cfeerate minrelaytxfee = cfeerate(1000);

ctxmempool mempool(::minrelaytxfee);

struct corphantx {
    ctransaction tx;
    nodeid frompeer;
};
map<uint256, corphantx> maporphantransactions;
map<uint256, set<uint256> > maporphantransactionsbyprev;
void eraseorphansfor(nodeid peer);

/**
 * returns true if there are nrequired or more blocks of minversion or above
 * in the last consensus::params::nmajoritywindow blocks, starting at pstart and going backwards.
 */
static bool issupermajority(int minversion, const cblockindex* pstart, unsigned nrequired, const consensus::params& consensusparams);
static void checkblockindex();

/** constant stuff for coinbase transactions we create: */
cscript coinbase_flags;

const string strmessagemagic = "moorecoin signed message:\n";

// internal stuff
namespace {

    struct cblockindexworkcomparator
    {
        bool operator()(cblockindex *pa, cblockindex *pb) const {
            // first sort by most total work, ...
            if (pa->nchainwork > pb->nchainwork) return false;
            if (pa->nchainwork < pb->nchainwork) return true;

            // ... then by earliest time received, ...
            if (pa->nsequenceid < pb->nsequenceid) return false;
            if (pa->nsequenceid > pb->nsequenceid) return true;

            // use pointer address as tie breaker (should only happen with blocks
            // loaded from disk, as those all have id 0).
            if (pa < pb) return false;
            if (pa > pb) return true;

            // identical blocks.
            return false;
        }
    };

    cblockindex *pindexbestinvalid;

    /**
     * the set of all cblockindex entries with block_valid_transactions (for itself and all ancestors) and
     * as good as our current tip or better. entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
    set<cblockindex*, cblockindexworkcomparator> setblockindexcandidates;
    /** number of nodes with fsyncstarted. */
    int nsyncstarted = 0;
    /** all pairs a->b, where a (or one if its ancestors) misses transactions, but b has transactions.
      * pruned nodes may have entries where b is missing data.
      */
    multimap<cblockindex*, cblockindex*> mapblocksunlinked;

    ccriticalsection cs_lastblockfile;
    std::vector<cblockfileinfo> vinfoblockfile;
    int nlastblockfile = 0;
    /** global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  set on startup
     *  or if we allocate more file space when we're in prune mode
     */
    bool fcheckforpruning = false;

    /**
     * every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    ccriticalsection cs_nblocksequenceid;
    /** blocks loaded from disk are assigned id 0, so start the counter at 1. */
    uint32_t nblocksequenceid = 1;

    /**
     * sources of received blocks, saved to be able to send them reject
     * messages or ban them when processing happens afterwards. protected by
     * cs_main.
     */
    map<uint256, nodeid> mapblocksource;

    /** blocks that are in flight, and that are in the queue to be downloaded. protected by cs_main. */
    struct queuedblock {
        uint256 hash;
        cblockindex *pindex;  //! optional.
        int64_t ntime;  //! time of "getdata" request in microseconds.
        bool fvalidatedheaders;  //! whether this block has validated headers at the time of request.
        int64_t ntimedisconnect; //! the timeout for this block request (for disconnecting a slow peer)
    };
    map<uint256, pair<nodeid, list<queuedblock>::iterator> > mapblocksinflight;

    /** number of blocks in flight with validated headers. */
    int nqueuedvalidatedheaders = 0;

    /** number of preferable block download peers. */
    int npreferreddownload = 0;

    /** dirty block index entries. */
    set<cblockindex*> setdirtyblockindex;

    /** dirty block file entries. */
    set<int> setdirtyfileinfo;
} // anon namespace

//////////////////////////////////////////////////////////////////////////////
//
// registration of network node signals.
//

namespace {

struct cblockreject {
    unsigned char chrejectcode;
    string strrejectreason;
    uint256 hashblock;
};

/**
 * maintain validation-specific state about nodes, protected by cs_main, instead
 * by cnode's own locks. this simplifies asynchronous operation, where
 * processing of incoming data is done after the processmessage call returns,
 * and we're no longer holding the node's locks.
 */
struct cnodestate {
    //! the peer's address
    cservice address;
    //! whether we have a fully established connection.
    bool fcurrentlyconnected;
    //! accumulated misbehaviour score for this peer.
    int nmisbehavior;
    //! whether this peer should be disconnected and banned (unless whitelisted).
    bool fshouldban;
    //! string name of this peer (debugging/logging purposes).
    std::string name;
    //! list of asynchronously-determined block rejections to notify this peer about.
    std::vector<cblockreject> rejects;
    //! the best known block we know this peer has announced.
    cblockindex *pindexbestknownblock;
    //! the hash of the last unknown block this peer has announced.
    uint256 hashlastunknownblock;
    //! the last full block we both have.
    cblockindex *pindexlastcommonblock;
    //! whether we've started headers synchronization with this peer.
    bool fsyncstarted;
    //! since when we're stalling block download progress (in microseconds), or 0.
    int64_t nstallingsince;
    list<queuedblock> vblocksinflight;
    int nblocksinflight;
    int nblocksinflightvalidheaders;
    //! whether we consider this a preferred download peer.
    bool fpreferreddownload;

    cnodestate() {
        fcurrentlyconnected = false;
        nmisbehavior = 0;
        fshouldban = false;
        pindexbestknownblock = null;
        hashlastunknownblock.setnull();
        pindexlastcommonblock = null;
        fsyncstarted = false;
        nstallingsince = 0;
        nblocksinflight = 0;
        nblocksinflightvalidheaders = 0;
        fpreferreddownload = false;
    }
};

/** map maintaining per-node state. requires cs_main. */
map<nodeid, cnodestate> mapnodestate;

// requires cs_main.
cnodestate *state(nodeid pnode) {
    map<nodeid, cnodestate>::iterator it = mapnodestate.find(pnode);
    if (it == mapnodestate.end())
        return null;
    return &it->second;
}

int getheight()
{
    lock(cs_main);
    return chainactive.height();
}

void updatepreferreddownload(cnode* node, cnodestate* state)
{
    npreferreddownload -= state->fpreferreddownload;

    // whether this node should be marked as a preferred download node.
    state->fpreferreddownload = (!node->finbound || node->fwhitelisted) && !node->foneshot && !node->fclient;

    npreferreddownload += state->fpreferreddownload;
}

// returns time at which to timeout block request (ntime in microseconds)
int64_t getblocktimeout(int64_t ntime, int nvalidatedqueuedbefore, const consensus::params &consensusparams)
{
    return ntime + 500000 * consensusparams.npowtargetspacing * (4 + nvalidatedqueuedbefore);
}

void initializenode(nodeid nodeid, const cnode *pnode) {
    lock(cs_main);
    cnodestate &state = mapnodestate.insert(std::make_pair(nodeid, cnodestate())).first->second;
    state.name = pnode->addrname;
    state.address = pnode->addr;
}

void finalizenode(nodeid nodeid) {
    lock(cs_main);
    cnodestate *state = state(nodeid);

    if (state->fsyncstarted)
        nsyncstarted--;

    if (state->nmisbehavior == 0 && state->fcurrentlyconnected) {
        addresscurrentlyconnected(state->address);
    }

    boost_foreach(const queuedblock& entry, state->vblocksinflight)
        mapblocksinflight.erase(entry.hash);
    eraseorphansfor(nodeid);
    npreferreddownload -= state->fpreferreddownload;

    mapnodestate.erase(nodeid);
}

// requires cs_main.
// returns a bool indicating whether we requested this block.
bool markblockasreceived(const uint256& hash) {
    map<uint256, pair<nodeid, list<queuedblock>::iterator> >::iterator itinflight = mapblocksinflight.find(hash);
    if (itinflight != mapblocksinflight.end()) {
        cnodestate *state = state(itinflight->second.first);
        nqueuedvalidatedheaders -= itinflight->second.second->fvalidatedheaders;
        state->nblocksinflightvalidheaders -= itinflight->second.second->fvalidatedheaders;
        state->vblocksinflight.erase(itinflight->second.second);
        state->nblocksinflight--;
        state->nstallingsince = 0;
        mapblocksinflight.erase(itinflight);
        return true;
    }
    return false;
}

// requires cs_main.
void markblockasinflight(nodeid nodeid, const uint256& hash, const consensus::params& consensusparams, cblockindex *pindex = null) {
    cnodestate *state = state(nodeid);
    assert(state != null);

    // make sure it's not listed somewhere already.
    markblockasreceived(hash);

    int64_t nnow = gettimemicros();
    queuedblock newentry = {hash, pindex, nnow, pindex != null, getblocktimeout(nnow, nqueuedvalidatedheaders, consensusparams)};
    nqueuedvalidatedheaders += newentry.fvalidatedheaders;
    list<queuedblock>::iterator it = state->vblocksinflight.insert(state->vblocksinflight.end(), newentry);
    state->nblocksinflight++;
    state->nblocksinflightvalidheaders += newentry.fvalidatedheaders;
    mapblocksinflight[hash] = std::make_pair(nodeid, it);
}

/** check whether the last unknown block a peer advertized is not yet known. */
void processblockavailability(nodeid nodeid) {
    cnodestate *state = state(nodeid);
    assert(state != null);

    if (!state->hashlastunknownblock.isnull()) {
        blockmap::iterator itold = mapblockindex.find(state->hashlastunknownblock);
        if (itold != mapblockindex.end() && itold->second->nchainwork > 0) {
            if (state->pindexbestknownblock == null || itold->second->nchainwork >= state->pindexbestknownblock->nchainwork)
                state->pindexbestknownblock = itold->second;
            state->hashlastunknownblock.setnull();
        }
    }
}

/** update tracking information about which blocks a peer is assumed to have. */
void updateblockavailability(nodeid nodeid, const uint256 &hash) {
    cnodestate *state = state(nodeid);
    assert(state != null);

    processblockavailability(nodeid);

    blockmap::iterator it = mapblockindex.find(hash);
    if (it != mapblockindex.end() && it->second->nchainwork > 0) {
        // an actually better block was announced.
        if (state->pindexbestknownblock == null || it->second->nchainwork >= state->pindexbestknownblock->nchainwork)
            state->pindexbestknownblock = it->second;
    } else {
        // an unknown block was announced; just assume that the latest one is the best one.
        state->hashlastunknownblock = hash;
    }
}

/** find the last common ancestor two blocks have.
 *  both pa and pb must be non-null. */
cblockindex* lastcommonancestor(cblockindex* pa, cblockindex* pb) {
    if (pa->nheight > pb->nheight) {
        pa = pa->getancestor(pb->nheight);
    } else if (pb->nheight > pa->nheight) {
        pb = pb->getancestor(pa->nheight);
    }

    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }

    // eventually all chain branches meet at the genesis block.
    assert(pa == pb);
    return pa;
}

/** update pindexlastcommonblock and add not-in-flight missing successors to vblocks, until it has
 *  at most count entries. */
void findnextblockstodownload(nodeid nodeid, unsigned int count, std::vector<cblockindex*>& vblocks, nodeid& nodestaller) {
    if (count == 0)
        return;

    vblocks.reserve(vblocks.size() + count);
    cnodestate *state = state(nodeid);
    assert(state != null);

    // make sure pindexbestknownblock is up to date, we'll need it.
    processblockavailability(nodeid);

    if (state->pindexbestknownblock == null || state->pindexbestknownblock->nchainwork < chainactive.tip()->nchainwork) {
        // this peer has nothing interesting.
        return;
    }

    if (state->pindexlastcommonblock == null) {
        // bootstrap quickly by guessing a parent of our best tip is the forking point.
        // guessing wrong in either direction is not a problem.
        state->pindexlastcommonblock = chainactive[std::min(state->pindexbestknownblock->nheight, chainactive.height())];
    }

    // if the peer reorganized, our previous pindexlastcommonblock may not be an ancestor
    // of its current tip anymore. go back enough to fix that.
    state->pindexlastcommonblock = lastcommonancestor(state->pindexlastcommonblock, state->pindexbestknownblock);
    if (state->pindexlastcommonblock == state->pindexbestknownblock)
        return;

    std::vector<cblockindex*> vtofetch;
    cblockindex *pindexwalk = state->pindexlastcommonblock;
    // never fetch further than the best block we know the peer has, or more than block_download_window + 1 beyond the last
    // linked block we have in common with this peer. the +1 is so we can detect stalling, namely if we would be able to
    // download that next block if the window were 1 larger.
    int nwindowend = state->pindexlastcommonblock->nheight + block_download_window;
    int nmaxheight = std::min<int>(state->pindexbestknownblock->nheight, nwindowend + 1);
    nodeid waitingfor = -1;
    while (pindexwalk->nheight < nmaxheight) {
        // read up to 128 (or more, if more blocks than that are needed) successors of pindexwalk (towards
        // pindexbestknownblock) into vtofetch. we fetch 128, because cblockindex::getancestor may be as expensive
        // as iterating over ~100 cblockindex* entries anyway.
        int ntofetch = std::min(nmaxheight - pindexwalk->nheight, std::max<int>(count - vblocks.size(), 128));
        vtofetch.resize(ntofetch);
        pindexwalk = state->pindexbestknownblock->getancestor(pindexwalk->nheight + ntofetch);
        vtofetch[ntofetch - 1] = pindexwalk;
        for (unsigned int i = ntofetch - 1; i > 0; i--) {
            vtofetch[i - 1] = vtofetch[i]->pprev;
        }

        // iterate over those blocks in vtofetch (in forward direction), adding the ones that
        // are not yet downloaded and not in flight to vblocks. in the mean time, update
        // pindexlastcommonblock as long as all ancestors are already downloaded.
        boost_foreach(cblockindex* pindex, vtofetch) {
            if (!pindex->isvalid(block_valid_tree)) {
                // we consider the chain that this peer is on invalid.
                return;
            }
            if (pindex->nstatus & block_have_data) {
                if (pindex->nchaintx)
                    state->pindexlastcommonblock = pindex;
            } else if (mapblocksinflight.count(pindex->getblockhash()) == 0) {
                // the block is not already downloaded, and not yet in flight.
                if (pindex->nheight > nwindowend) {
                    // we reached the end of the window.
                    if (vblocks.size() == 0 && waitingfor != nodeid) {
                        // we aren't able to fetch anything, but we would be if the download window was one larger.
                        nodestaller = waitingfor;
                    }
                    return;
                }
                vblocks.push_back(pindex);
                if (vblocks.size() == count) {
                    return;
                }
            } else if (waitingfor == -1) {
                // this is the first already-in-flight block.
                waitingfor = mapblocksinflight[pindex->getblockhash()].first;
            }
        }
    }
}

} // anon namespace

bool getnodestatestats(nodeid nodeid, cnodestatestats &stats) {
    lock(cs_main);
    cnodestate *state = state(nodeid);
    if (state == null)
        return false;
    stats.nmisbehavior = state->nmisbehavior;
    stats.nsyncheight = state->pindexbestknownblock ? state->pindexbestknownblock->nheight : -1;
    stats.ncommonheight = state->pindexlastcommonblock ? state->pindexlastcommonblock->nheight : -1;
    boost_foreach(const queuedblock& queue, state->vblocksinflight) {
        if (queue.pindex)
            stats.vheightinflight.push_back(queue.pindex->nheight);
    }
    return true;
}

void registernodesignals(cnodesignals& nodesignals)
{
    nodesignals.getheight.connect(&getheight);
    nodesignals.processmessages.connect(&processmessages);
    nodesignals.sendmessages.connect(&sendmessages);
    nodesignals.initializenode.connect(&initializenode);
    nodesignals.finalizenode.connect(&finalizenode);
}

void unregisternodesignals(cnodesignals& nodesignals)
{
    nodesignals.getheight.disconnect(&getheight);
    nodesignals.processmessages.disconnect(&processmessages);
    nodesignals.sendmessages.disconnect(&sendmessages);
    nodesignals.initializenode.disconnect(&initializenode);
    nodesignals.finalizenode.disconnect(&finalizenode);
}

cblockindex* findforkinglobalindex(const cchain& chain, const cblocklocator& locator)
{
    // find the first block the caller has in the main chain
    boost_foreach(const uint256& hash, locator.vhave) {
        blockmap::iterator mi = mapblockindex.find(hash);
        if (mi != mapblockindex.end())
        {
            cblockindex* pindex = (*mi).second;
            if (chain.contains(pindex))
                return pindex;
        }
    }
    return chain.genesis();
}

ccoinsviewcache *pcoinstip = null;
cblocktreedb *pblocktree = null;

//////////////////////////////////////////////////////////////////////////////
//
// maporphantransactions
//

bool addorphantx(const ctransaction& tx, nodeid peer)
{
    uint256 hash = tx.gethash();
    if (maporphantransactions.count(hash))
        return false;

    // ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. if a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:
    unsigned int sz = tx.getserializesize(ser_network, ctransaction::current_version);
    if (sz > 5000)
    {
        logprint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.tostring());
        return false;
    }

    maporphantransactions[hash].tx = tx;
    maporphantransactions[hash].frompeer = peer;
    boost_foreach(const ctxin& txin, tx.vin)
        maporphantransactionsbyprev[txin.prevout.hash].insert(hash);

    logprint("mempool", "stored orphan tx %s (mapsz %u prevsz %u)\n", hash.tostring(),
             maporphantransactions.size(), maporphantransactionsbyprev.size());
    return true;
}

void static eraseorphantx(uint256 hash)
{
    map<uint256, corphantx>::iterator it = maporphantransactions.find(hash);
    if (it == maporphantransactions.end())
        return;
    boost_foreach(const ctxin& txin, it->second.tx.vin)
    {
        map<uint256, set<uint256> >::iterator itprev = maporphantransactionsbyprev.find(txin.prevout.hash);
        if (itprev == maporphantransactionsbyprev.end())
            continue;
        itprev->second.erase(hash);
        if (itprev->second.empty())
            maporphantransactionsbyprev.erase(itprev);
    }
    maporphantransactions.erase(it);
}

void eraseorphansfor(nodeid peer)
{
    int nerased = 0;
    map<uint256, corphantx>::iterator iter = maporphantransactions.begin();
    while (iter != maporphantransactions.end())
    {
        map<uint256, corphantx>::iterator maybeerase = iter++; // increment to avoid iterator becoming invalid
        if (maybeerase->second.frompeer == peer)
        {
            eraseorphantx(maybeerase->second.tx.gethash());
            ++nerased;
        }
    }
    if (nerased > 0) logprint("mempool", "erased %d orphan tx from peer %d\n", nerased, peer);
}


unsigned int limitorphantxsize(unsigned int nmaxorphans)
{
    unsigned int nevicted = 0;
    while (maporphantransactions.size() > nmaxorphans)
    {
        // evict a random orphan:
        uint256 randomhash = getrandhash();
        map<uint256, corphantx>::iterator it = maporphantransactions.lower_bound(randomhash);
        if (it == maporphantransactions.end())
            it = maporphantransactions.begin();
        eraseorphantx(it->first);
        ++nevicted;
    }
    return nevicted;
}







bool isstandardtx(const ctransaction& tx, string& reason)
{
    if (tx.nversion > ctransaction::current_version || tx.nversion < 1) {
        reason = "version";
        return false;
    }

    // extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is o(ninputs*txsize). limiting transactions
    // to max_standard_tx_size mitigates cpu exhaustion attacks.
    unsigned int sz = tx.getserializesize(ser_network, ctransaction::current_version);
    if (sz >= max_standard_tx_size) {
        reason = "tx-size";
        return false;
    }

    boost_foreach(const ctxin& txin, tx.vin)
    {
        // biggest 'standard' txin is a 15-of-15 p2sh multisig with compressed
        // keys. (remember the 520 byte limit on redeemscript size) that works
        // out to a (15*(33+1))+3=513 byte redeemscript, 513+1+15*(73+1)+3=1627
        // bytes of scriptsig, which we round off to 1650 bytes for some minor
        // future-proofing. that's also enough to spend a 20-of-20
        // checkmultisig scriptpubkey, though such a scriptpubkey is not
        // considered standard)
        if (txin.scriptsig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptsig.ispushonly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int ndataout = 0;
    txnouttype whichtype;
    boost_foreach(const ctxout& txout, tx.vout) {
        if (!::isstandard(txout.scriptpubkey, whichtype)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichtype == tx_null_data)
            ndataout++;
        else if ((whichtype == tx_multisig) && (!fisbaremultisigstd)) {
            reason = "bare-multisig";
            return false;
        } else if (txout.isdust(::minrelaytxfee)) {
            reason = "dust";
            return false;
        }
    }

    // only one op_return txout is permitted
    if (ndataout > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool isfinaltx(const ctransaction &tx, int nblockheight, int64_t nblocktime)
{
    if (tx.nlocktime == 0)
        return true;
    if ((int64_t)tx.nlocktime < ((int64_t)tx.nlocktime < locktime_threshold ? (int64_t)nblockheight : nblocktime))
        return true;
    boost_foreach(const ctxin& txin, tx.vin)
        if (!txin.isfinal())
            return false;
    return true;
}

bool checkfinaltx(const ctransaction &tx)
{
    assertlockheld(cs_main);
    return isfinaltx(tx, chainactive.height() + 1, getadjustedtime());
}

/**
 * check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptsigs with extra data stuffed into them,
 *    not consumed by scriptpubkey (or p2sh script)
 * 2. p2sh scripts with a crazy number of expensive
 *    checksig/checkmultisig operations
 */
bool areinputsstandard(const ctransaction& tx, const ccoinsviewcache& mapinputs)
{
    if (tx.iscoinbase())
        return true; // coinbases don't use vin normally

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const ctxout& prev = mapinputs.getoutputfor(tx.vin[i]);

        vector<vector<unsigned char> > vsolutions;
        txnouttype whichtype;
        // get the scriptpubkey corresponding to this input:
        const cscript& prevscript = prev.scriptpubkey;
        if (!solver(prevscript, whichtype, vsolutions))
            return false;
        int nargsexpected = scriptsigargsexpected(whichtype, vsolutions);
        if (nargsexpected < 0)
            return false;

        // transactions with extra stuff in their scriptsigs are
        // non-standard. note that this evalscript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptsig
        // isstandardtx() will have already returned false
        // and this method isn't called.
        vector<vector<unsigned char> > stack;
        if (!evalscript(stack, tx.vin[i].scriptsig, script_verify_none, basesignaturechecker()))
            return false;

        if (whichtype == tx_scripthash)
        {
            if (stack.empty())
                return false;
            cscript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vsolutions2;
            txnouttype whichtype2;
            if (solver(subscript, whichtype2, vsolutions2))
            {
                int tmpexpected = scriptsigargsexpected(whichtype2, vsolutions2);
                if (tmpexpected < 0)
                    return false;
                nargsexpected += tmpexpected;
            }
            else
            {
                // any other script with less than 15 sigops ok:
                unsigned int sigops = subscript.getsigopcount(true);
                // ... extra data left on the stack after execution is ok, too:
                return (sigops <= max_p2sh_sigops);
            }
        }

        if (stack.size() != (unsigned int)nargsexpected)
            return false;
    }

    return true;
}

unsigned int getlegacysigopcount(const ctransaction& tx)
{
    unsigned int nsigops = 0;
    boost_foreach(const ctxin& txin, tx.vin)
    {
        nsigops += txin.scriptsig.getsigopcount(false);
    }
    boost_foreach(const ctxout& txout, tx.vout)
    {
        nsigops += txout.scriptpubkey.getsigopcount(false);
    }
    return nsigops;
}

unsigned int getp2shsigopcount(const ctransaction& tx, const ccoinsviewcache& inputs)
{
    if (tx.iscoinbase())
        return 0;

    unsigned int nsigops = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const ctxout &prevout = inputs.getoutputfor(tx.vin[i]);
        if (prevout.scriptpubkey.ispaytoscripthash())
            nsigops += prevout.scriptpubkey.getsigopcount(tx.vin[i].scriptsig);
    }
    return nsigops;
}








bool checktransaction(const ctransaction& tx, cvalidationstate &state)
{
    // basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.dos(10, error("checktransaction(): vin empty"),
                         reject_invalid, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.dos(10, error("checktransaction(): vout empty"),
                         reject_invalid, "bad-txns-vout-empty");
    // size limits
    if (::getserializesize(tx, ser_network, protocol_version) > max_block_size)
        return state.dos(100, error("checktransaction(): size limits failed"),
                         reject_invalid, "bad-txns-oversize");

    // check for negative or overflow output values
    camount nvalueout = 0;
    boost_foreach(const ctxout& txout, tx.vout)
    {
        if (txout.nvalue < 0)
            return state.dos(100, error("checktransaction(): txout.nvalue negative"),
                             reject_invalid, "bad-txns-vout-negative");
        if (txout.nvalue > max_money)
            return state.dos(100, error("checktransaction(): txout.nvalue too high"),
                             reject_invalid, "bad-txns-vout-toolarge");
        nvalueout += txout.nvalue;
        if (!moneyrange(nvalueout))
            return state.dos(100, error("checktransaction(): txout total out of range"),
                             reject_invalid, "bad-txns-txouttotal-toolarge");
    }

    // check for duplicate inputs
    set<coutpoint> vinoutpoints;
    boost_foreach(const ctxin& txin, tx.vin)
    {
        if (vinoutpoints.count(txin.prevout))
            return state.dos(100, error("checktransaction(): duplicate inputs"),
                             reject_invalid, "bad-txns-inputs-duplicate");
        vinoutpoints.insert(txin.prevout);
    }

    if (tx.iscoinbase())
    {
        if (tx.vin[0].scriptsig.size() < 2 || tx.vin[0].scriptsig.size() > 100)
            return state.dos(100, error("checktransaction(): coinbase script size"),
                             reject_invalid, "bad-cb-length");
    }
    else
    {
        boost_foreach(const ctxin& txin, tx.vin)
            if (txin.prevout.isnull())
                return state.dos(10, error("checktransaction(): prevout is null"),
                                 reject_invalid, "bad-txns-prevout-null");
    }

    return true;
}

camount getminrelayfee(const ctransaction& tx, unsigned int nbytes, bool fallowfree)
{
    {
        lock(mempool.cs);
        uint256 hash = tx.gethash();
        double dprioritydelta = 0;
        camount nfeedelta = 0;
        mempool.applydeltas(hash, dprioritydelta, nfeedelta);
        if (dprioritydelta > 0 || nfeedelta > 0)
            return 0;
    }

    camount nminfee = ::minrelaytxfee.getfee(nbytes);

    if (fallowfree)
    {
        // there is a free transaction area in blocks created by most miners,
        // * if we are relaying we allow transactions up to default_block_priority_size - 1000
        //   to be considered to fall into this category. we don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nbytes < (default_block_priority_size - 1000))
            nminfee = 0;
    }

    if (!moneyrange(nminfee))
        nminfee = max_money;
    return nminfee;
}


bool accepttomemorypool(ctxmempool& pool, cvalidationstate &state, const ctransaction &tx, bool flimitfree,
                        bool* pfmissinginputs, bool frejectabsurdfee)
{
    assertlockheld(cs_main);
    if (pfmissinginputs)
        *pfmissinginputs = false;

    if (!checktransaction(tx, state))
        return error("accepttomemorypool: checktransaction failed");

    // coinbase is only valid in a block, not as a loose transaction
    if (tx.iscoinbase())
        return state.dos(100, error("accepttomemorypool: coinbase as individual tx"),
                         reject_invalid, "coinbase");

    // rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (params().requirestandard() && !isstandardtx(tx, reason))
        return state.dos(0,
                         error("accepttomemorypool: nonstandard transaction: %s", reason),
                         reject_nonstandard, reason);

    // only accept nlocktime-using transactions that can be mined in the next
    // block; we don't want our mempool filled up with transactions that can't
    // be mined yet.
    if (!checkfinaltx(tx))
        return state.dos(0, error("accepttomemorypool: non-final"),
                         reject_nonstandard, "non-final");

    // is it already in the memory pool?
    uint256 hash = tx.gethash();
    if (pool.exists(hash))
        return false;

    // check for conflicts with in-memory transactions
    {
    lock(pool.cs); // protect pool.mapnexttx
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        coutpoint outpoint = tx.vin[i].prevout;
        if (pool.mapnexttx.count(outpoint))
        {
            // disable replacement feature for now
            return false;
        }
    }
    }

    {
        ccoinsview dummy;
        ccoinsviewcache view(&dummy);

        camount nvaluein = 0;
        {
        lock(pool.cs);
        ccoinsviewmempool viewmempool(pcoinstip, pool);
        view.setbackend(viewmempool);

        // do we already have it?
        if (view.havecoins(hash))
            return false;

        // do all inputs exist?
        // note that this does not check for the presence of actual outputs (see the next check for that),
        // and only helps with filling in pfmissinginputs (to determine missing vs spent).
        boost_foreach(const ctxin txin, tx.vin) {
            if (!view.havecoins(txin.prevout.hash)) {
                if (pfmissinginputs)
                    *pfmissinginputs = true;
                return false;
            }
        }

        // are the actual inputs available?
        if (!view.haveinputs(tx))
            return state.invalid(error("accepttomemorypool: inputs already spent"),
                                 reject_duplicate, "bad-txns-inputs-spent");

        // bring the best block into scope
        view.getbestblock();

        nvaluein = view.getvaluein(tx);

        // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
        view.setbackend(dummy);
        }

        // check for non-standard pay-to-script-hash in inputs
        if (params().requirestandard() && !areinputsstandard(tx, view))
            return error("accepttomemorypool: nonstandard transaction input");

        // check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. since the coinbase transaction
        // itself can contain sigops max_standard_tx_sigops is less than
        // max_block_sigops; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nsigops = getlegacysigopcount(tx);
        nsigops += getp2shsigopcount(tx, view);
        if (nsigops > max_standard_tx_sigops)
            return state.dos(0,
                             error("accepttomemorypool: too many sigops %s, %d > %d",
                                   hash.tostring(), nsigops, max_standard_tx_sigops),
                             reject_nonstandard, "bad-txns-too-many-sigops");

        camount nvalueout = tx.getvalueout();
        camount nfees = nvaluein-nvalueout;
        double dpriority = view.getpriority(tx, chainactive.height());

        ctxmempoolentry entry(tx, nfees, gettime(), dpriority, chainactive.height(), mempool.hasnoinputsof(tx));
        unsigned int nsize = entry.gettxsize();

        // don't accept it if it can't get into a block
        camount txminfee = getminrelayfee(tx, nsize, true);
        if (flimitfree && nfees < txminfee)
            return state.dos(0, error("accepttomemorypool: not enough fees %s, %d < %d",
                                      hash.tostring(), nfees, txminfee),
                             reject_insufficientfee, "insufficient fee");

        // require that free transactions have sufficient priority to be mined in the next block.
        if (getboolarg("-relaypriority", true) && nfees < ::minrelaytxfee.getfee(nsize) && !allowfree(view.getpriority(tx, chainactive.height() + 1))) {
            return state.dos(0, false, reject_insufficientfee, "insufficient priority");
        }

        // continuously rate-limit free (really, very-low-fee) transactions
        // this mitigates 'penny-flooding' -- sending thousands of free transactions just to
        // be annoying or make others' transactions take longer to confirm.
        if (flimitfree && nfees < ::minrelaytxfee.getfee(nsize))
        {
            static ccriticalsection csfreelimiter;
            static double dfreecount;
            static int64_t nlasttime;
            int64_t nnow = gettime();

            lock(csfreelimiter);

            // use an exponentially decaying ~10-minute window:
            dfreecount *= pow(1.0 - 1.0/600.0, (double)(nnow - nlasttime));
            nlasttime = nnow;
            // -limitfreerelay unit is thousand-bytes-per-minute
            // at default rate it would take over a month to fill 1gb
            if (dfreecount >= getarg("-limitfreerelay", 15)*10*1000)
                return state.dos(0, error("accepttomemorypool: free transaction rejected by rate limiter"),
                                 reject_insufficientfee, "rate limited free transaction");
            logprint("mempool", "rate limit dfreecount: %g => %g\n", dfreecount, dfreecount+nsize);
            dfreecount += nsize;
        }

        if (frejectabsurdfee && nfees > ::minrelaytxfee.getfee(nsize) * 10000)
            return error("accepttomemorypool: absurdly high fees %s, %d > %d",
                         hash.tostring(),
                         nfees, ::minrelaytxfee.getfee(nsize) * 10000);

        // check against previous transactions
        // this is done last to help prevent cpu exhaustion denial-of-service attacks.
        if (!checkinputs(tx, state, view, true, standard_script_verify_flags, true))
        {
            return error("accepttomemorypool: connectinputs failed %s", hash.tostring());
        }

        // check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. for
        // instance the strictenc flag was incorrectly allowing certain
        // checksig not scripts to pass, even though they were invalid.
        //
        // there is a similar check in createnewblock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a dos attack.
        if (!checkinputs(tx, state, view, true, mandatory_script_verify_flags, true))
        {
            return error("accepttomemorypool: bug! please report this! connectinputs failed against mandatory but not standard flags %s", hash.tostring());
        }

        // store transaction in memory
        pool.addunchecked(hash, entry, !isinitialblockdownload());
    }

    syncwithwallets(tx, null);

    return true;
}

/** return transaction in tx, and if it was found inside a block, its hash is placed in hashblock */
bool gettransaction(const uint256 &hash, ctransaction &txout, uint256 &hashblock, bool fallowslow)
{
    cblockindex *pindexslow = null;
    {
        lock(cs_main);
        {
            if (mempool.lookup(hash, txout))
            {
                return true;
            }
        }

        if (ftxindex) {
            cdisktxpos postx;
            if (pblocktree->readtxindex(hash, postx)) {
                cautofile file(openblockfile(postx, true), ser_disk, client_version);
                if (file.isnull())
                    return error("%s: openblockfile failed", __func__);
                cblockheader header;
                try {
                    file >> header;
                    fseek(file.get(), postx.ntxoffset, seek_cur);
                    file >> txout;
                } catch (const std::exception& e) {
                    return error("%s: deserialize or i/o error - %s", __func__, e.what());
                }
                hashblock = header.gethash();
                if (txout.gethash() != hash)
                    return error("%s: txid mismatch", __func__);
                return true;
            }
        }

        if (fallowslow) { // use coin database to locate block that contains transaction, and scan it
            int nheight = -1;
            {
                ccoinsviewcache &view = *pcoinstip;
                const ccoins* coins = view.accesscoins(hash);
                if (coins)
                    nheight = coins->nheight;
            }
            if (nheight > 0)
                pindexslow = chainactive[nheight];
        }
    }

    if (pindexslow) {
        cblock block;
        if (readblockfromdisk(block, pindexslow)) {
            boost_foreach(const ctransaction &tx, block.vtx) {
                if (tx.gethash() == hash) {
                    txout = tx;
                    hashblock = pindexslow->getblockhash();
                    return true;
                }
            }
        }
    }

    return false;
}






//////////////////////////////////////////////////////////////////////////////
//
// cblock and cblockindex
//

bool writeblocktodisk(cblock& block, cdiskblockpos& pos, const cmessageheader::messagestartchars& messagestart)
{
    // open history file to append
    cautofile fileout(openblockfile(pos), ser_disk, client_version);
    if (fileout.isnull())
        return error("writeblocktodisk: openblockfile failed");

    // write index header
    unsigned int nsize = fileout.getserializesize(block);
    fileout << flatdata(messagestart) << nsize;

    // write block
    long fileoutpos = ftell(fileout.get());
    if (fileoutpos < 0)
        return error("writeblocktodisk: ftell failed");
    pos.npos = (unsigned int)fileoutpos;
    fileout << block;

    return true;
}

bool readblockfromdisk(cblock& block, const cdiskblockpos& pos)
{
    block.setnull();

    // open history file to read
    cautofile filein(openblockfile(pos, true), ser_disk, client_version);
    if (filein.isnull())
        return error("readblockfromdisk: openblockfile failed for %s", pos.tostring());

    // read block
    try {
        filein >> block;
    }
    catch (const std::exception& e) {
        return error("%s: deserialize or i/o error - %s at %s", __func__, e.what(), pos.tostring());
    }

    // check the header
    if (!checkproofofwork(block.gethash(), block.nbits, params().getconsensus()))
        return error("readblockfromdisk: errors in block header at %s", pos.tostring());

    return true;
}

bool readblockfromdisk(cblock& block, const cblockindex* pindex)
{
    if (!readblockfromdisk(block, pindex->getblockpos()))
        return false;
    if (block.gethash() != pindex->getblockhash())
        return error("readblockfromdisk(cblock&, cblockindex*): gethash() doesn't match index for %s at %s",
                pindex->tostring(), pindex->getblockpos().tostring());
    return true;
}

camount getblocksubsidy(int nheight, const consensus::params& consensusparams)
{
    int halvings = nheight / consensusparams.nsubsidyhalvinginterval;
    // force block reward to zero when right shift is undefined.
    if (halvings >= 64)
        return 0;

    camount nsubsidy = 50 * coin;
    // subsidy is cut in half every 210,000 blocks which will occur approximately every 4 years.
    nsubsidy >>= halvings;
    return nsubsidy;
}

bool isinitialblockdownload()
{
    const cchainparams& chainparams = params();
    lock(cs_main);
    if (fimporting || freindex)
        return true;
    if (fcheckpointsenabled && chainactive.height() < checkpoints::gettotalblocksestimate(chainparams.checkpoints()))
        return true;
    static bool lockibdstate = false;
    if (lockibdstate)
        return false;
    bool state = (chainactive.height() < pindexbestheader->nheight - 24 * 6 ||
            pindexbestheader->getblocktime() < gettime() - 24 * 60 * 60);
    if (!state)
        lockibdstate = true;
    return state;
}

bool flargeworkforkfound = false;
bool flargeworkinvalidchainfound = false;
cblockindex *pindexbestforktip = null, *pindexbestforkbase = null;

void checkforkwarningconditions()
{
    assertlockheld(cs_main);
    // before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before the last checkpoint)
    if (isinitialblockdownload())
        return;

    // if our best fork is no longer within 72 blocks (+/- 12 hours if no one mines it)
    // of our head, drop it
    if (pindexbestforktip && chainactive.height() - pindexbestforktip->nheight >= 72)
        pindexbestforktip = null;

    if (pindexbestforktip || (pindexbestinvalid && pindexbestinvalid->nchainwork > chainactive.tip()->nchainwork + (getblockproof(*chainactive.tip()) * 6)))
    {
        if (!flargeworkforkfound && pindexbestforkbase)
        {
            std::string warning = std::string("'warning: large-work fork detected, forking after block ") +
                pindexbestforkbase->phashblock->tostring() + std::string("'");
            calert::notify(warning, true);
        }
        if (pindexbestforktip && pindexbestforkbase)
        {
            logprintf("%s: warning: large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nchain state database corruption likely.\n", __func__,
                   pindexbestforkbase->nheight, pindexbestforkbase->phashblock->tostring(),
                   pindexbestforktip->nheight, pindexbestforktip->phashblock->tostring());
            flargeworkforkfound = true;
        }
        else
        {
            logprintf("%s: warning: found invalid chain at least ~6 blocks longer than our best chain.\nchain state database corruption likely.\n", __func__);
            flargeworkinvalidchainfound = true;
        }
    }
    else
    {
        flargeworkforkfound = false;
        flargeworkinvalidchainfound = false;
    }
}

void checkforkwarningconditionsonnewfork(cblockindex* pindexnewforktip)
{
    assertlockheld(cs_main);
    // if we are on a fork that is sufficiently large, set a warning flag
    cblockindex* pfork = pindexnewforktip;
    cblockindex* plonger = chainactive.tip();
    while (pfork && pfork != plonger)
    {
        while (plonger && plonger->nheight > pfork->nheight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // we define a condition where we should warn the user about as a fork of at least 7 blocks
    // with a tip within 72 blocks (+/- 12 hours if no one mines it) of ours
    // we use 7 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // we define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 7-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexbestforktip || (pindexbestforktip && pindexnewforktip->nheight > pindexbestforktip->nheight)) &&
            pindexnewforktip->nchainwork - pfork->nchainwork > (getblockproof(*pfork) * 7) &&
            chainactive.height() - pindexnewforktip->nheight < 72)
    {
        pindexbestforktip = pindexnewforktip;
        pindexbestforkbase = pfork;
    }

    checkforkwarningconditions();
}

// requires cs_main.
void misbehaving(nodeid pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    cnodestate *state = state(pnode);
    if (state == null)
        return;

    state->nmisbehavior += howmuch;
    int banscore = getarg("-banscore", 100);
    if (state->nmisbehavior >= banscore && state->nmisbehavior - howmuch < banscore)
    {
        logprintf("%s: %s (%d -> %d) ban threshold exceeded\n", __func__, state->name, state->nmisbehavior-howmuch, state->nmisbehavior);
        state->fshouldban = true;
    } else
        logprintf("%s: %s (%d -> %d)\n", __func__, state->name, state->nmisbehavior-howmuch, state->nmisbehavior);
}

void static invalidchainfound(cblockindex* pindexnew)
{
    if (!pindexbestinvalid || pindexnew->nchainwork > pindexbestinvalid->nchainwork)
        pindexbestinvalid = pindexnew;

    logprintf("%s: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
      pindexnew->getblockhash().tostring(), pindexnew->nheight,
      log(pindexnew->nchainwork.getdouble())/log(2.0), datetimestrformat("%y-%m-%d %h:%m:%s",
      pindexnew->getblocktime()));
    logprintf("%s:  current best=%s  height=%d  log2_work=%.8g  date=%s\n", __func__,
      chainactive.tip()->getblockhash().tostring(), chainactive.height(), log(chainactive.tip()->nchainwork.getdouble())/log(2.0),
      datetimestrformat("%y-%m-%d %h:%m:%s", chainactive.tip()->getblocktime()));
    checkforkwarningconditions();
}

void static invalidblockfound(cblockindex *pindex, const cvalidationstate &state) {
    int ndos = 0;
    if (state.isinvalid(ndos)) {
        std::map<uint256, nodeid>::iterator it = mapblocksource.find(pindex->getblockhash());
        if (it != mapblocksource.end() && state(it->second)) {
            cblockreject reject = {state.getrejectcode(), state.getrejectreason().substr(0, max_reject_message_length), pindex->getblockhash()};
            state(it->second)->rejects.push_back(reject);
            if (ndos > 0)
                misbehaving(it->second, ndos);
        }
    }
    if (!state.corruptionpossible()) {
        pindex->nstatus |= block_failed_valid;
        setdirtyblockindex.insert(pindex);
        setblockindexcandidates.erase(pindex);
        invalidchainfound(pindex);
    }
}

void updatecoins(const ctransaction& tx, cvalidationstate &state, ccoinsviewcache &inputs, ctxundo &txundo, int nheight)
{
    // mark inputs spent
    if (!tx.iscoinbase()) {
        txundo.vprevout.reserve(tx.vin.size());
        boost_foreach(const ctxin &txin, tx.vin) {
            ccoinsmodifier coins = inputs.modifycoins(txin.prevout.hash);
            unsigned npos = txin.prevout.n;

            if (npos >= coins->vout.size() || coins->vout[npos].isnull())
                assert(false);
            // mark an outpoint spent, and construct undo information
            txundo.vprevout.push_back(ctxinundo(coins->vout[npos]));
            coins->spend(npos);
            if (coins->vout.size() == 0) {
                ctxinundo& undo = txundo.vprevout.back();
                undo.nheight = coins->nheight;
                undo.fcoinbase = coins->fcoinbase;
                undo.nversion = coins->nversion;
            }
        }
    }

    // add outputs
    inputs.modifycoins(tx.gethash())->fromtx(tx, nheight);
}

void updatecoins(const ctransaction& tx, cvalidationstate &state, ccoinsviewcache &inputs, int nheight)
{
    ctxundo txundo;
    updatecoins(tx, state, inputs, txundo, nheight);
}

bool cscriptcheck::operator()() {
    const cscript &scriptsig = ptxto->vin[nin].scriptsig;
    if (!verifyscript(scriptsig, scriptpubkey, nflags, cachingtransactionsignaturechecker(ptxto, nin, cachestore), &error)) {
        return ::error("cscriptcheck(): %s:%d verifysignature failed: %s", ptxto->gethash().tostring(), nin, scripterrorstring(error));
    }
    return true;
}

int getspendheight(const ccoinsviewcache& inputs)
{
    lock(cs_main);
    cblockindex* pindexprev = mapblockindex.find(inputs.getbestblock())->second;
    return pindexprev->nheight + 1;
}

namespace consensus {
bool checktxinputs(const ctransaction& tx, cvalidationstate& state, const ccoinsviewcache& inputs, int nspendheight)
{
        // this doesn't trigger the dos code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (!inputs.haveinputs(tx))
            return state.invalid(error("checkinputs(): %s inputs unavailable", tx.gethash().tostring()));

        camount nvaluein = 0;
        camount nfees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++)
        {
            const coutpoint &prevout = tx.vin[i].prevout;
            const ccoins *coins = inputs.accesscoins(prevout.hash);
            assert(coins);

            // if prev is coinbase, check that it's matured
            if (coins->iscoinbase()) {
                if (nspendheight - coins->nheight < coinbase_maturity)
                    return state.invalid(
                        error("checkinputs(): tried to spend coinbase at depth %d", nspendheight - coins->nheight),
                        reject_invalid, "bad-txns-premature-spend-of-coinbase");
            }

            // check for negative or overflow input values
            nvaluein += coins->vout[prevout.n].nvalue;
            if (!moneyrange(coins->vout[prevout.n].nvalue) || !moneyrange(nvaluein))
                return state.dos(100, error("checkinputs(): txin values out of range"),
                                 reject_invalid, "bad-txns-inputvalues-outofrange");

        }

        if (nvaluein < tx.getvalueout())
            return state.dos(100, error("checkinputs(): %s value in (%s) < value out (%s)",
                                        tx.gethash().tostring(), formatmoney(nvaluein), formatmoney(tx.getvalueout())),
                             reject_invalid, "bad-txns-in-belowout");

        // tally transaction fees
        camount ntxfee = nvaluein - tx.getvalueout();
        if (ntxfee < 0)
            return state.dos(100, error("checkinputs(): %s ntxfee < 0", tx.gethash().tostring()),
                             reject_invalid, "bad-txns-fee-negative");
        nfees += ntxfee;
        if (!moneyrange(nfees))
            return state.dos(100, error("checkinputs(): nfees out of range"),
                             reject_invalid, "bad-txns-fee-outofrange");
    return true;
}
}// namespace consensus

bool checkinputs(const ctransaction& tx, cvalidationstate &state, const ccoinsviewcache &inputs, bool fscriptchecks, unsigned int flags, bool cachestore, std::vector<cscriptcheck> *pvchecks)
{
    if (!tx.iscoinbase())
    {
        if (!consensus::checktxinputs(tx, state, inputs, getspendheight(inputs)))
            return false;

        if (pvchecks)
            pvchecks->reserve(tx.vin.size());

        // the first loop above does all the inexpensive checks.
        // only if all inputs pass do we perform expensive ecdsa signature checks.
        // helps prevent cpu exhaustion attacks.

        // skip ecdsa signature verification when connecting blocks
        // before the last block chain checkpoint. this is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fscriptchecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const coutpoint &prevout = tx.vin[i].prevout;
                const ccoins* coins = inputs.accesscoins(prevout.hash);
                assert(coins);

                // verify signature
                cscriptcheck check(*coins, tx, i, flags, cachestore);
                if (pvchecks) {
                    pvchecks->push_back(cscriptcheck());
                    check.swap(pvchecks->back());
                } else if (!check()) {
                    if (flags & standard_not_mandatory_verify_flags) {
                        // check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard der encodings or non-null dummy
                        // arguments; if so, don't trigger dos protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        cscriptcheck check(*coins, tx, i,
                                flags & ~standard_not_mandatory_verify_flags, cachestore);
                        if (check())
                            return state.invalid(false, reject_nonstandard, strprintf("non-mandatory-script-verify-flag (%s)", scripterrorstring(check.getscripterror())));
                    }
                    // failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid p2sh. we dos ban
                    // such nodes as they are not following the protocol. that
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
                    return state.dos(100,false, reject_invalid, strprintf("mandatory-script-verify-flag-failed (%s)", scripterrorstring(check.getscripterror())));
                }
            }
        }
    }

    return true;
}

namespace {

bool undowritetodisk(const cblockundo& blockundo, cdiskblockpos& pos, const uint256& hashblock, const cmessageheader::messagestartchars& messagestart)
{
    // open history file to append
    cautofile fileout(openundofile(pos), ser_disk, client_version);
    if (fileout.isnull())
        return error("%s: openundofile failed", __func__);

    // write index header
    unsigned int nsize = fileout.getserializesize(blockundo);
    fileout << flatdata(messagestart) << nsize;

    // write undo data
    long fileoutpos = ftell(fileout.get());
    if (fileoutpos < 0)
        return error("%s: ftell failed", __func__);
    pos.npos = (unsigned int)fileoutpos;
    fileout << blockundo;

    // calculate & write checksum
    chashwriter hasher(ser_gethash, protocol_version);
    hasher << hashblock;
    hasher << blockundo;
    fileout << hasher.gethash();

    return true;
}

bool undoreadfromdisk(cblockundo& blockundo, const cdiskblockpos& pos, const uint256& hashblock)
{
    // open history file to read
    cautofile filein(openundofile(pos, true), ser_disk, client_version);
    if (filein.isnull())
        return error("%s: openblockfile failed", __func__);

    // read block
    uint256 hashchecksum;
    try {
        filein >> blockundo;
        filein >> hashchecksum;
    }
    catch (const std::exception& e) {
        return error("%s: deserialize or i/o error - %s", __func__, e.what());
    }

    // verify checksum
    chashwriter hasher(ser_gethash, protocol_version);
    hasher << hashblock;
    hasher << blockundo;
    if (hashchecksum != hasher.gethash())
        return error("%s: checksum mismatch", __func__);

    return true;
}

/** abort with a message */
bool abortnode(const std::string& strmessage, const std::string& usermessage="")
{
    strmiscwarning = strmessage;
    logprintf("*** %s\n", strmessage);
    uiinterface.threadsafemessagebox(
        usermessage.empty() ? _("error: a fatal internal error occured, see debug.log for details") : usermessage,
        "", cclientuiinterface::msg_error);
    startshutdown();
    return false;
}

bool abortnode(cvalidationstate& state, const std::string& strmessage, const std::string& usermessage="")
{
    abortnode(strmessage, usermessage);
    return state.error(strmessage);
}

} // anon namespace

/**
 * apply the undo operation of a ctxinundo to the given chain state.
 * @param undo the undo object.
 * @param view the coins view to which to apply the changes.
 * @param out the out point that corresponds to the tx input.
 * @return true on success.
 */
static bool applytxinundo(const ctxinundo& undo, ccoinsviewcache& view, const coutpoint& out)
{
    bool fclean = true;

    ccoinsmodifier coins = view.modifycoins(out.hash);
    if (undo.nheight != 0) {
        // undo data contains height: this is the last output of the prevout tx being spent
        if (!coins->ispruned())
            fclean = fclean && error("%s: undo data overwriting existing transaction", __func__);
        coins->clear();
        coins->fcoinbase = undo.fcoinbase;
        coins->nheight = undo.nheight;
        coins->nversion = undo.nversion;
    } else {
        if (coins->ispruned())
            fclean = fclean && error("%s: undo data adding output to missing transaction", __func__);
    }
    if (coins->isavailable(out.n))
        fclean = fclean && error("%s: undo data overwriting existing output", __func__);
    if (coins->vout.size() < out.n+1)
        coins->vout.resize(out.n+1);
    coins->vout[out.n] = undo.txout;

    return fclean;
}

bool disconnectblock(cblock& block, cvalidationstate& state, cblockindex* pindex, ccoinsviewcache& view, bool* pfclean)
{
    assert(pindex->getblockhash() == view.getbestblock());

    if (pfclean)
        *pfclean = false;

    bool fclean = true;

    cblockundo blockundo;
    cdiskblockpos pos = pindex->getundopos();
    if (pos.isnull())
        return error("disconnectblock(): no undo data available");
    if (!undoreadfromdisk(blockundo, pos, pindex->pprev->getblockhash()))
        return error("disconnectblock(): failure reading undo data");

    if (blockundo.vtxundo.size() + 1 != block.vtx.size())
        return error("disconnectblock(): block and undo data inconsistent");

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const ctransaction &tx = block.vtx[i];
        uint256 hash = tx.gethash();

        // check that all outputs are available and match the outputs in the block itself
        // exactly.
        {
        ccoinsmodifier outs = view.modifycoins(hash);
        outs->clearunspendable();

        ccoins outsblock(tx, pindex->nheight);
        // the ccoins serialization does not serialize negative numbers.
        // no network rules currently depend on the version here, so an inconsistency is harmless
        // but it must be corrected before txout nversion ever influences a network rule.
        if (outsblock.nversion < 0)
            outs->nversion = outsblock.nversion;
        if (*outs != outsblock)
            fclean = fclean && error("disconnectblock(): added transaction mismatch? database corrupted");

        // remove outputs
        outs->clear();
        }

        // restore inputs
        if (i > 0) { // not coinbases
            const ctxundo &txundo = blockundo.vtxundo[i-1];
            if (txundo.vprevout.size() != tx.vin.size())
                return error("disconnectblock(): transaction and undo data inconsistent");
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const coutpoint &out = tx.vin[j].prevout;
                const ctxinundo &undo = txundo.vprevout[j];
                if (!applytxinundo(undo, view, out))
                    fclean = false;
            }
        }
    }

    // move best block pointer to prevout block
    view.setbestblock(pindex->pprev->getblockhash());

    if (pfclean) {
        *pfclean = fclean;
        return true;
    }

    return fclean;
}

void static flushblockfile(bool ffinalize = false)
{
    lock(cs_lastblockfile);

    cdiskblockpos posold(nlastblockfile, 0);

    file *fileold = openblockfile(posold);
    if (fileold) {
        if (ffinalize)
            truncatefile(fileold, vinfoblockfile[nlastblockfile].nsize);
        filecommit(fileold);
        fclose(fileold);
    }

    fileold = openundofile(posold);
    if (fileold) {
        if (ffinalize)
            truncatefile(fileold, vinfoblockfile[nlastblockfile].nundosize);
        filecommit(fileold);
        fclose(fileold);
    }
}

bool findundopos(cvalidationstate &state, int nfile, cdiskblockpos &pos, unsigned int naddsize);

static ccheckqueue<cscriptcheck> scriptcheckqueue(128);

void threadscriptcheck() {
    renamethread("moorecoin-scriptch");
    scriptcheckqueue.thread();
}

//
// called periodically asynchronously; alerts if it smells like
// we're being fed a bad chain (blocks being generated much
// too slowly or too quickly).
//
void partitioncheck(bool (*initialdownloadcheck)(), ccriticalsection& cs, const cblockindex *const &bestheader,
                    int64_t npowtargetspacing)
{
    if (bestheader == null || initialdownloadcheck()) return;

    static int64_t lastalerttime = 0;
    int64_t now = getadjustedtime();
    if (lastalerttime > now-60*60*24) return; // alert at most once per day

    const int span_hours=4;
    const int span_seconds=span_hours*60*60;
    int blocks_expected = span_seconds / npowtargetspacing;

    boost::math::poisson_distribution<double> poisson(blocks_expected);

    std::string strwarning;
    int64_t starttime = getadjustedtime()-span_seconds;

    lock(cs);
    const cblockindex* i = bestheader;
    int nblocks = 0;
    while (i->getblocktime() >= starttime) {
        ++nblocks;
        i = i->pprev;
        if (i == null) return; // ran out of chain, we must not be fully sync'ed
    }

    // how likely is it to find that many by chance?
    double p = boost::math::pdf(poisson, nblocks);

    logprint("partitioncheck", "%s : found %d blocks in the last %d hours\n", __func__, nblocks, span_hours);
    logprint("partitioncheck", "%s : likelihood: %g\n", __func__, p);

    // aim for one false-positive about every fifty years of normal running:
    const int fifty_years = 50*365*24*60*60;
    double alertthreshold = 1.0 / (fifty_years / span_seconds);

    if (p <= alertthreshold && nblocks < blocks_expected)
    {
        // many fewer blocks than expected: alert!
        strwarning = strprintf(_("warning: check your network connection, %d blocks received in the last %d hours (%d expected)"),
                               nblocks, span_hours, blocks_expected);
    }
    else if (p <= alertthreshold && nblocks > blocks_expected)
    {
        // many more blocks than expected: alert!
        strwarning = strprintf(_("warning: abnormally high number of blocks generated, %d blocks received in the last %d hours (%d expected)"),
                               nblocks, span_hours, blocks_expected);
    }
    if (!strwarning.empty())
    {
        strmiscwarning = strwarning;
        calert::notify(strwarning, true);
        lastalerttime = now;
    }
}

static int64_t ntimeverify = 0;
static int64_t ntimeconnect = 0;
static int64_t ntimeindex = 0;
static int64_t ntimecallbacks = 0;
static int64_t ntimetotal = 0;

bool connectblock(const cblock& block, cvalidationstate& state, cblockindex* pindex, ccoinsviewcache& view, bool fjustcheck)
{
    const cchainparams& chainparams = params();
    assertlockheld(cs_main);
    // check it again in case a previous version let a bad block in
    if (!checkblock(block, state, !fjustcheck, !fjustcheck))
        return false;

    // verify that the view's current state corresponds to the previous block
    uint256 hashprevblock = pindex->pprev == null ? uint256() : pindex->pprev->getblockhash();
    assert(hashprevblock == view.getbestblock());

    // special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.gethash() == chainparams.getconsensus().hashgenesisblock) {
        if (!fjustcheck)
            view.setbestblock(pindex->getblockhash());
        return true;
    }

    bool fscriptchecks = true;
    if (fcheckpointsenabled) {
        cblockindex *pindexlastcheckpoint = checkpoints::getlastcheckpoint(chainparams.checkpoints());
        if (pindexlastcheckpoint && pindexlastcheckpoint->getancestor(pindex->nheight) == pindex) {
            // this block is an ancestor of a checkpoint: disable script checks
            fscriptchecks = false;
        }
    }

    // do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // if such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // see bip30 and http://r6.ca/blog/20120206t005236z.html for more information.
    // this logic is not necessary for memory pool transactions, as accepttomemorypool
    // already refuses previously-known transaction ids entirely.
    // this rule was originally applied to all blocks with a timestamp after march 15, 2012, 0:00 utc.
    // now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. this prevents exploiting the issue against nodes during their
    // initial block download.
    bool fenforcebip30 = (!pindex->phashblock) || // enforce on createnewblock invocations which don't have a hash.
                          !((pindex->nheight==91842 && pindex->getblockhash() == uint256s("0x00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec")) ||
                           (pindex->nheight==91880 && pindex->getblockhash() == uint256s("0x00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721")));
    if (fenforcebip30) {
        boost_foreach(const ctransaction& tx, block.vtx) {
            const ccoins* coins = view.accesscoins(tx.gethash());
            if (coins && !coins->ispruned())
                return state.dos(100, error("connectblock(): tried to overwrite transaction"),
                                 reject_invalid, "bad-txns-bip30");
        }
    }

    // bip16 didn't become active until apr 1 2012
    int64_t nbip16switchtime = 1333238400;
    bool fstrictpaytoscripthash = (pindex->getblocktime() >= nbip16switchtime);

    unsigned int flags = fstrictpaytoscripthash ? script_verify_p2sh : script_verify_none;

    // start enforcing the dersig (bip66) rules, for block.nversion=3 blocks, when 75% of the network has upgraded:
    if (block.nversion >= 3 && issupermajority(3, pindex->pprev, chainparams.getconsensus().nmajorityenforceblockupgrade, chainparams.getconsensus())) {
        flags |= script_verify_dersig;
    }

    cblockundo blockundo;

    ccheckqueuecontrol<cscriptcheck> control(fscriptchecks && nscriptcheckthreads ? &scriptcheckqueue : null);

    int64_t ntimestart = gettimemicros();
    camount nfees = 0;
    int ninputs = 0;
    unsigned int nsigops = 0;
    cdisktxpos pos(pindex->getblockpos(), getsizeofcompactsize(block.vtx.size()));
    std::vector<std::pair<uint256, cdisktxpos> > vpos;
    vpos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    for (unsigned int i = 0; i < block.vtx.size(); i++)
    {
        const ctransaction &tx = block.vtx[i];

        ninputs += tx.vin.size();
        nsigops += getlegacysigopcount(tx);
        if (nsigops > max_block_sigops)
            return state.dos(100, error("connectblock(): too many sigops"),
                             reject_invalid, "bad-blk-sigops");

        if (!tx.iscoinbase())
        {
            if (!view.haveinputs(tx))
                return state.dos(100, error("connectblock(): inputs missing/spent"),
                                 reject_invalid, "bad-txns-inputs-missingorspent");

            if (fstrictpaytoscripthash)
            {
                // add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nsigops += getp2shsigopcount(tx, view);
                if (nsigops > max_block_sigops)
                    return state.dos(100, error("connectblock(): too many sigops"),
                                     reject_invalid, "bad-blk-sigops");
            }

            nfees += view.getvaluein(tx)-tx.getvalueout();

            std::vector<cscriptcheck> vchecks;
            if (!checkinputs(tx, state, view, fscriptchecks, flags, false, nscriptcheckthreads ? &vchecks : null))
                return false;
            control.add(vchecks);
        }

        ctxundo undodummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(ctxundo());
        }
        updatecoins(tx, state, view, i == 0 ? undodummy : blockundo.vtxundo.back(), pindex->nheight);

        vpos.push_back(std::make_pair(tx.gethash(), pos));
        pos.ntxoffset += ::getserializesize(tx, ser_disk, client_version);
    }
    int64_t ntime1 = gettimemicros(); ntimeconnect += ntime1 - ntimestart;
    logprint("bench", "      - connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (ntime1 - ntimestart), 0.001 * (ntime1 - ntimestart) / block.vtx.size(), ninputs <= 1 ? 0 : 0.001 * (ntime1 - ntimestart) / (ninputs-1), ntimeconnect * 0.000001);

    camount blockreward = nfees + getblocksubsidy(pindex->nheight, chainparams.getconsensus());
    if (block.vtx[0].getvalueout() > blockreward)
        return state.dos(100,
                         error("connectblock(): coinbase pays too much (actual=%d vs limit=%d)",
                               block.vtx[0].getvalueout(), blockreward),
                               reject_invalid, "bad-cb-amount");

    if (!control.wait())
        return state.dos(100, false);
    int64_t ntime2 = gettimemicros(); ntimeverify += ntime2 - ntimestart;
    logprint("bench", "    - verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", ninputs - 1, 0.001 * (ntime2 - ntimestart), ninputs <= 1 ? 0 : 0.001 * (ntime2 - ntimestart) / (ninputs-1), ntimeverify * 0.000001);

    if (fjustcheck)
        return true;

    // write undo information to disk
    if (pindex->getundopos().isnull() || !pindex->isvalid(block_valid_scripts))
    {
        if (pindex->getundopos().isnull()) {
            cdiskblockpos pos;
            if (!findundopos(state, pindex->nfile, pos, ::getserializesize(blockundo, ser_disk, client_version) + 40))
                return error("connectblock(): findundopos failed");
            if (!undowritetodisk(blockundo, pos, pindex->pprev->getblockhash(), chainparams.messagestart()))
                return abortnode(state, "failed to write undo data");

            // update nundopos in block index
            pindex->nundopos = pos.npos;
            pindex->nstatus |= block_have_undo;
        }

        pindex->raisevalidity(block_valid_scripts);
        setdirtyblockindex.insert(pindex);
    }

    if (ftxindex)
        if (!pblocktree->writetxindex(vpos))
            return abortnode(state, "failed to write transaction index");

    // add this block to the view's block chain
    view.setbestblock(pindex->getblockhash());

    int64_t ntime3 = gettimemicros(); ntimeindex += ntime3 - ntime2;
    logprint("bench", "    - index writing: %.2fms [%.2fs]\n", 0.001 * (ntime3 - ntime2), ntimeindex * 0.000001);

    // watch for changes to the previous coinbase transaction.
    static uint256 hashprevbestcoinbase;
    getmainsignals().updatedtransaction(hashprevbestcoinbase);
    hashprevbestcoinbase = block.vtx[0].gethash();

    int64_t ntime4 = gettimemicros(); ntimecallbacks += ntime4 - ntime3;
    logprint("bench", "    - callbacks: %.2fms [%.2fs]\n", 0.001 * (ntime4 - ntime3), ntimecallbacks * 0.000001);

    return true;
}

enum flushstatemode {
    flush_state_none,
    flush_state_if_needed,
    flush_state_periodic,
    flush_state_always
};

/**
 * update the on-disk chain state.
 * the caches and indexes are flushed depending on the mode we're called with
 * if they're too large, if it's been a while since the last write,
 * or always and in all cases if we're in prune mode and are deleting files.
 */
bool static flushstatetodisk(cvalidationstate &state, flushstatemode mode) {
    lock2(cs_main, cs_lastblockfile);
    static int64_t nlastwrite = 0;
    static int64_t nlastflush = 0;
    static int64_t nlastsetchain = 0;
    std::set<int> setfilestoprune;
    bool fflushforprune = false;
    try {
    if (fprunemode && fcheckforpruning) {
        findfilestoprune(setfilestoprune);
        fcheckforpruning = false;
        if (!setfilestoprune.empty()) {
            fflushforprune = true;
            if (!fhavepruned) {
                pblocktree->writeflag("prunedblockfiles", true);
                fhavepruned = true;
            }
        }
    }
    int64_t nnow = gettimemicros();
    // avoid writing/flushing immediately after startup.
    if (nlastwrite == 0) {
        nlastwrite = nnow;
    }
    if (nlastflush == 0) {
        nlastflush = nnow;
    }
    if (nlastsetchain == 0) {
        nlastsetchain = nnow;
    }
    size_t cachesize = pcoinstip->dynamicmemoryusage();
    // the cache is large and close to the limit, but we have time now (not in the middle of a block processing).
    bool fcachelarge = mode == flush_state_periodic && cachesize * (10.0/9) > ncoincacheusage;
    // the cache is over the limit, we have to write now.
    bool fcachecritical = mode == flush_state_if_needed && cachesize > ncoincacheusage;
    // it's been a while since we wrote the block index to disk. do this frequently, so we don't need to redownload after a crash.
    bool fperiodicwrite = mode == flush_state_periodic && nnow > nlastwrite + (int64_t)database_write_interval * 1000000;
    // it's been very long since we flushed the cache. do this infrequently, to optimize cache usage.
    bool fperiodicflush = mode == flush_state_periodic && nnow > nlastflush + (int64_t)database_flush_interval * 1000000;
    // combine all conditions that result in a full cache flush.
    bool fdofullflush = (mode == flush_state_always) || fcachelarge || fcachecritical || fperiodicflush || fflushforprune;
    // write blocks and block index to disk.
    if (fdofullflush || fperiodicwrite) {
        // depend on nmindiskspace to ensure we can write block index
        if (!checkdiskspace(0))
            return state.error("out of disk space");
        // first make sure all block and undo data is flushed to disk.
        flushblockfile();
        // then update all block file information (which may refer to block and undo files).
        {
            std::vector<std::pair<int, const cblockfileinfo*> > vfiles;
            vfiles.reserve(setdirtyfileinfo.size());
            for (set<int>::iterator it = setdirtyfileinfo.begin(); it != setdirtyfileinfo.end(); ) {
                vfiles.push_back(make_pair(*it, &vinfoblockfile[*it]));
                setdirtyfileinfo.erase(it++);
            }
            std::vector<const cblockindex*> vblocks;
            vblocks.reserve(setdirtyblockindex.size());
            for (set<cblockindex*>::iterator it = setdirtyblockindex.begin(); it != setdirtyblockindex.end(); ) {
                vblocks.push_back(*it);
                setdirtyblockindex.erase(it++);
            }
            if (!pblocktree->writebatchsync(vfiles, nlastblockfile, vblocks)) {
                return abortnode(state, "files to write to block index database");
            }
        }
        // finally remove any pruned files
        if (fflushforprune)
            unlinkprunedfiles(setfilestoprune);
        nlastwrite = nnow;
    }
    // flush best chain related state. this can only be done if the blocks / block index write was also done.
    if (fdofullflush) {
        // typical ccoins structures on disk are around 128 bytes in size.
        // pushing a new one to the database can cause it to be written
        // twice (once in the log, and once in the tables). this is already
        // an overestimation, as most will delete an existing entry or
        // overwrite one. still, use a conservative safety factor of 2.
        if (!checkdiskspace(128 * 2 * 2 * pcoinstip->getcachesize()))
            return state.error("out of disk space");
        // flush the chainstate (which may refer to block index entries).
        if (!pcoinstip->flush())
            return abortnode(state, "failed to write to coin database");
        nlastflush = nnow;
    }
    if ((mode == flush_state_always || mode == flush_state_periodic) && nnow > nlastsetchain + (int64_t)database_write_interval * 1000000) {
        // update best block in wallet (so we can detect restored wallets).
        getmainsignals().setbestchain(chainactive.getlocator());
        nlastsetchain = nnow;
    }
    } catch (const std::runtime_error& e) {
        return abortnode(state, std::string("system error while flushing: ") + e.what());
    }
    return true;
}

void flushstatetodisk() {
    cvalidationstate state;
    flushstatetodisk(state, flush_state_always);
}

void pruneandflush() {
    cvalidationstate state;
    fcheckforpruning = true;
    flushstatetodisk(state, flush_state_none);
}

/** update chainactive and related internal data structures. */
void static updatetip(cblockindex *pindexnew) {
    const cchainparams& chainparams = params();
    chainactive.settip(pindexnew);

    // new best block
    ntimebestreceived = gettime();
    mempool.addtransactionsupdated(1);

    logprintf("%s: new best=%s  height=%d  log2_work=%.8g  tx=%lu  date=%s progress=%f  cache=%.1fmib(%utx)\n", __func__,
      chainactive.tip()->getblockhash().tostring(), chainactive.height(), log(chainactive.tip()->nchainwork.getdouble())/log(2.0), (unsigned long)chainactive.tip()->nchaintx,
      datetimestrformat("%y-%m-%d %h:%m:%s", chainactive.tip()->getblocktime()),
      checkpoints::guessverificationprogress(chainparams.checkpoints(), chainactive.tip()), pcoinstip->dynamicmemoryusage() * (1.0 / (1<<20)), pcoinstip->getcachesize());

    cvblockchange.notify_all();

    // check the version of the last 100 blocks to see if we need to upgrade:
    static bool fwarned = false;
    if (!isinitialblockdownload() && !fwarned)
    {
        int nupgraded = 0;
        const cblockindex* pindex = chainactive.tip();
        for (int i = 0; i < 100 && pindex != null; i++)
        {
            if (pindex->nversion > cblock::current_version)
                ++nupgraded;
            pindex = pindex->pprev;
        }
        if (nupgraded > 0)
            logprintf("%s: %d of last 100 blocks above version %d\n", __func__, nupgraded, (int)cblock::current_version);
        if (nupgraded > 100/2)
        {
            // strmiscwarning is read by getwarnings(), called by qt and the json-rpc code to warn the user:
            strmiscwarning = _("warning: this version is obsolete; upgrade required!");
            calert::notify(strmiscwarning, true);
            fwarned = true;
        }
    }
}

/** disconnect chainactive's tip. */
bool static disconnecttip(cvalidationstate &state) {
    cblockindex *pindexdelete = chainactive.tip();
    assert(pindexdelete);
    mempool.check(pcoinstip);
    // read block from disk.
    cblock block;
    if (!readblockfromdisk(block, pindexdelete))
        return abortnode(state, "failed to read block");
    // apply the block atomically to the chain state.
    int64_t nstart = gettimemicros();
    {
        ccoinsviewcache view(pcoinstip);
        if (!disconnectblock(block, state, pindexdelete, view))
            return error("disconnecttip(): disconnectblock %s failed", pindexdelete->getblockhash().tostring());
        assert(view.flush());
    }
    logprint("bench", "- disconnect block: %.2fms\n", (gettimemicros() - nstart) * 0.001);
    // write the chain state to disk, if necessary.
    if (!flushstatetodisk(state, flush_state_if_needed))
        return false;
    // resurrect mempool transactions from the disconnected block.
    boost_foreach(const ctransaction &tx, block.vtx) {
        // ignore validation errors in resurrected transactions
        list<ctransaction> removed;
        cvalidationstate statedummy;
        if (tx.iscoinbase() || !accepttomemorypool(mempool, statedummy, tx, false, null))
            mempool.remove(tx, removed, true);
    }
    mempool.removecoinbasespends(pcoinstip, pindexdelete->nheight);
    mempool.check(pcoinstip);
    // update chainactive and related variables.
    updatetip(pindexdelete->pprev);
    // let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    boost_foreach(const ctransaction &tx, block.vtx) {
        syncwithwallets(tx, null);
    }
    return true;
}

static int64_t ntimereadfromdisk = 0;
static int64_t ntimeconnecttotal = 0;
static int64_t ntimeflush = 0;
static int64_t ntimechainstate = 0;
static int64_t ntimepostconnect = 0;

/**
 * connect a new block to chainactive. pblock is either null or a pointer to a cblock
 * corresponding to pindexnew, to bypass loading it again from disk.
 */
bool static connecttip(cvalidationstate &state, cblockindex *pindexnew, cblock *pblock) {
    assert(pindexnew->pprev == chainactive.tip());
    mempool.check(pcoinstip);
    // read block from disk.
    int64_t ntime1 = gettimemicros();
    cblock block;
    if (!pblock) {
        if (!readblockfromdisk(block, pindexnew))
            return abortnode(state, "failed to read block");
        pblock = &block;
    }
    // apply the block atomically to the chain state.
    int64_t ntime2 = gettimemicros(); ntimereadfromdisk += ntime2 - ntime1;
    int64_t ntime3;
    logprint("bench", "  - load block from disk: %.2fms [%.2fs]\n", (ntime2 - ntime1) * 0.001, ntimereadfromdisk * 0.000001);
    {
        ccoinsviewcache view(pcoinstip);
        cinv inv(msg_block, pindexnew->getblockhash());
        bool rv = connectblock(*pblock, state, pindexnew, view);
        getmainsignals().blockchecked(*pblock, state);
        if (!rv) {
            if (state.isinvalid())
                invalidblockfound(pindexnew, state);
            return error("connecttip(): connectblock %s failed", pindexnew->getblockhash().tostring());
        }
        mapblocksource.erase(inv.hash);
        ntime3 = gettimemicros(); ntimeconnecttotal += ntime3 - ntime2;
        logprint("bench", "  - connect total: %.2fms [%.2fs]\n", (ntime3 - ntime2) * 0.001, ntimeconnecttotal * 0.000001);
        assert(view.flush());
    }
    int64_t ntime4 = gettimemicros(); ntimeflush += ntime4 - ntime3;
    logprint("bench", "  - flush: %.2fms [%.2fs]\n", (ntime4 - ntime3) * 0.001, ntimeflush * 0.000001);
    // write the chain state to disk, if necessary.
    if (!flushstatetodisk(state, flush_state_if_needed))
        return false;
    int64_t ntime5 = gettimemicros(); ntimechainstate += ntime5 - ntime4;
    logprint("bench", "  - writing chainstate: %.2fms [%.2fs]\n", (ntime5 - ntime4) * 0.001, ntimechainstate * 0.000001);
    // remove conflicting transactions from the mempool.
    list<ctransaction> txconflicted;
    mempool.removeforblock(pblock->vtx, pindexnew->nheight, txconflicted, !isinitialblockdownload());
    mempool.check(pcoinstip);
    // update chainactive & related variables.
    updatetip(pindexnew);
    // tell wallet about transactions that went from mempool
    // to conflicted:
    boost_foreach(const ctransaction &tx, txconflicted) {
        syncwithwallets(tx, null);
    }
    // ... and about transactions that got confirmed:
    boost_foreach(const ctransaction &tx, pblock->vtx) {
        syncwithwallets(tx, pblock);
    }

    int64_t ntime6 = gettimemicros(); ntimepostconnect += ntime6 - ntime5; ntimetotal += ntime6 - ntime1;
    logprint("bench", "  - connect postprocess: %.2fms [%.2fs]\n", (ntime6 - ntime5) * 0.001, ntimepostconnect * 0.000001);
    logprint("bench", "- connect block: %.2fms [%.2fs]\n", (ntime6 - ntime1) * 0.001, ntimetotal * 0.000001);
    return true;
}

/**
 * return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static cblockindex* findmostworkchain() {
    do {
        cblockindex *pindexnew = null;

        // find the best candidate header.
        {
            std::set<cblockindex*, cblockindexworkcomparator>::reverse_iterator it = setblockindexcandidates.rbegin();
            if (it == setblockindexcandidates.rend())
                return null;
            pindexnew = *it;
        }

        // check whether all blocks on the path between the currently active chain and the candidate are valid.
        // just going until the active chain is an optimization, as we know all blocks in it are valid already.
        cblockindex *pindextest = pindexnew;
        bool finvalidancestor = false;
        while (pindextest && !chainactive.contains(pindextest)) {
            assert(pindextest->nchaintx || pindextest->nheight == 0);

            // pruned nodes may have entries in setblockindexcandidates for
            // which block files have been deleted.  remove those as candidates
            // for the most work chain if we come across them; we can't switch
            // to a chain unless we have all the non-active-chain parent blocks.
            bool ffailedchain = pindextest->nstatus & block_failed_mask;
            bool fmissingdata = !(pindextest->nstatus & block_have_data);
            if (ffailedchain || fmissingdata) {
                // candidate chain is not usable (either invalid or missing data)
                if (ffailedchain && (pindexbestinvalid == null || pindexnew->nchainwork > pindexbestinvalid->nchainwork))
                    pindexbestinvalid = pindexnew;
                cblockindex *pindexfailed = pindexnew;
                // remove the entire chain from the set.
                while (pindextest != pindexfailed) {
                    if (ffailedchain) {
                        pindexfailed->nstatus |= block_failed_child;
                    } else if (fmissingdata) {
                        // if we're missing data, then add back to mapblocksunlinked,
                        // so that if the block arrives in the future we can try adding
                        // to setblockindexcandidates again.
                        mapblocksunlinked.insert(std::make_pair(pindexfailed->pprev, pindexfailed));
                    }
                    setblockindexcandidates.erase(pindexfailed);
                    pindexfailed = pindexfailed->pprev;
                }
                setblockindexcandidates.erase(pindextest);
                finvalidancestor = true;
                break;
            }
            pindextest = pindextest->pprev;
        }
        if (!finvalidancestor)
            return pindexnew;
    } while(true);
}

/** delete all entries in setblockindexcandidates that are worse than the current tip. */
static void pruneblockindexcandidates() {
    // note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<cblockindex*, cblockindexworkcomparator>::iterator it = setblockindexcandidates.begin();
    while (it != setblockindexcandidates.end() && setblockindexcandidates.value_comp()(*it, chainactive.tip())) {
        setblockindexcandidates.erase(it++);
    }
    // either the current tip or a successor of it we're working towards is left in setblockindexcandidates.
    assert(!setblockindexcandidates.empty());
}

/**
 * try to make some progress towards making pindexmostwork the active block.
 * pblock is either null or a pointer to a cblock corresponding to pindexmostwork.
 */
static bool activatebestchainstep(cvalidationstate &state, cblockindex *pindexmostwork, cblock *pblock) {
    assertlockheld(cs_main);
    bool finvalidfound = false;
    const cblockindex *pindexoldtip = chainactive.tip();
    const cblockindex *pindexfork = chainactive.findfork(pindexmostwork);

    // disconnect active blocks which are no longer in the best chain.
    while (chainactive.tip() && chainactive.tip() != pindexfork) {
        if (!disconnecttip(state))
            return false;
    }

    // build list of new blocks to connect.
    std::vector<cblockindex*> vpindextoconnect;
    bool fcontinue = true;
    int nheight = pindexfork ? pindexfork->nheight : -1;
    while (fcontinue && nheight != pindexmostwork->nheight) {
    // don't iterate the entire list of potential improvements toward the best tip, as we likely only need
    // a few blocks along the way.
    int ntargetheight = std::min(nheight + 32, pindexmostwork->nheight);
    vpindextoconnect.clear();
    vpindextoconnect.reserve(ntargetheight - nheight);
    cblockindex *pindexiter = pindexmostwork->getancestor(ntargetheight);
    while (pindexiter && pindexiter->nheight != nheight) {
        vpindextoconnect.push_back(pindexiter);
        pindexiter = pindexiter->pprev;
    }
    nheight = ntargetheight;

    // connect new blocks.
    boost_reverse_foreach(cblockindex *pindexconnect, vpindextoconnect) {
        if (!connecttip(state, pindexconnect, pindexconnect == pindexmostwork ? pblock : null)) {
            if (state.isinvalid()) {
                // the block violates a consensus rule.
                if (!state.corruptionpossible())
                    invalidchainfound(vpindextoconnect.back());
                state = cvalidationstate();
                finvalidfound = true;
                fcontinue = false;
                break;
            } else {
                // a system error occurred (disk space, database error, ...).
                return false;
            }
        } else {
            pruneblockindexcandidates();
            if (!pindexoldtip || chainactive.tip()->nchainwork > pindexoldtip->nchainwork) {
                // we're in a better position than we were. return temporarily to release the lock.
                fcontinue = false;
                break;
            }
        }
    }
    }

    // callbacks/notifications for a new best chain.
    if (finvalidfound)
        checkforkwarningconditionsonnewfork(vpindextoconnect.back());
    else
        checkforkwarningconditions();

    return true;
}

/**
 * make the best chain active, in multiple steps. the result is either failure
 * or an activated best chain. pblock is either null or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool activatebestchain(cvalidationstate &state, cblock *pblock) {
    cblockindex *pindexnewtip = null;
    cblockindex *pindexmostwork = null;
    const cchainparams& chainparams = params();
    do {
        boost::this_thread::interruption_point();

        bool finitialdownload;
        {
            lock(cs_main);
            pindexmostwork = findmostworkchain();

            // whether we have anything to do at all.
            if (pindexmostwork == null || pindexmostwork == chainactive.tip())
                return true;

            if (!activatebestchainstep(state, pindexmostwork, pblock && pblock->gethash() == pindexmostwork->getblockhash() ? pblock : null))
                return false;

            pindexnewtip = chainactive.tip();
            finitialdownload = isinitialblockdownload();
        }
        // when we reach this point, we switched to a new tip (stored in pindexnewtip).

        // notifications/callbacks that can run without cs_main
        if (!finitialdownload) {
            uint256 hashnewtip = pindexnewtip->getblockhash();
            // relay inventory, but don't relay old inventory during initial block download.
            int nblockestimate = 0;
            if (fcheckpointsenabled)
                nblockestimate = checkpoints::gettotalblocksestimate(chainparams.checkpoints());
            // don't relay blocks if pruning -- could cause a peer to try to download, resulting
            // in a stalled download if the block file is pruned before the request.
            if (nlocalservices & node_network) {
                lock(cs_vnodes);
                boost_foreach(cnode* pnode, vnodes)
                    if (chainactive.height() > (pnode->nstartingheight != -1 ? pnode->nstartingheight - 2000 : nblockestimate))
                        pnode->pushinventory(cinv(msg_block, hashnewtip));
            }
            // notify external listeners about the new tip.
            uiinterface.notifyblocktip(hashnewtip);
        }
    } while(pindexmostwork != chainactive.tip());
    checkblockindex();

    // write changes periodically to disk, after relay.
    if (!flushstatetodisk(state, flush_state_periodic)) {
        return false;
    }

    return true;
}

bool invalidateblock(cvalidationstate& state, cblockindex *pindex) {
    assertlockheld(cs_main);

    // mark the block itself as invalid.
    pindex->nstatus |= block_failed_valid;
    setdirtyblockindex.insert(pindex);
    setblockindexcandidates.erase(pindex);

    while (chainactive.contains(pindex)) {
        cblockindex *pindexwalk = chainactive.tip();
        pindexwalk->nstatus |= block_failed_child;
        setdirtyblockindex.insert(pindexwalk);
        setblockindexcandidates.erase(pindexwalk);
        // activatebestchain considers blocks already in chainactive
        // unconditionally valid already, so force disconnect away from it.
        if (!disconnecttip(state)) {
            return false;
        }
    }

    // the resulting new best tip may not be in setblockindexcandidates anymore, so
    // add it again.
    blockmap::iterator it = mapblockindex.begin();
    while (it != mapblockindex.end()) {
        if (it->second->isvalid(block_valid_transactions) && it->second->nchaintx && !setblockindexcandidates.value_comp()(it->second, chainactive.tip())) {
            setblockindexcandidates.insert(it->second);
        }
        it++;
    }

    invalidchainfound(pindex);
    return true;
}

bool reconsiderblock(cvalidationstate& state, cblockindex *pindex) {
    assertlockheld(cs_main);

    int nheight = pindex->nheight;

    // remove the invalidity flag from this block and all its descendants.
    blockmap::iterator it = mapblockindex.begin();
    while (it != mapblockindex.end()) {
        if (!it->second->isvalid() && it->second->getancestor(nheight) == pindex) {
            it->second->nstatus &= ~block_failed_mask;
            setdirtyblockindex.insert(it->second);
            if (it->second->isvalid(block_valid_transactions) && it->second->nchaintx && setblockindexcandidates.value_comp()(chainactive.tip(), it->second)) {
                setblockindexcandidates.insert(it->second);
            }
            if (it->second == pindexbestinvalid) {
                // reset invalid block marker if it was pointing to one of those.
                pindexbestinvalid = null;
            }
        }
        it++;
    }

    // remove the invalidity flag from all ancestors too.
    while (pindex != null) {
        if (pindex->nstatus & block_failed_mask) {
            pindex->nstatus &= ~block_failed_mask;
            setdirtyblockindex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

cblockindex* addtoblockindex(const cblockheader& block)
{
    // check for duplicate
    uint256 hash = block.gethash();
    blockmap::iterator it = mapblockindex.find(hash);
    if (it != mapblockindex.end())
        return it->second;

    // construct new block index object
    cblockindex* pindexnew = new cblockindex(block);
    assert(pindexnew);
    // we assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexnew->nsequenceid = 0;
    blockmap::iterator mi = mapblockindex.insert(make_pair(hash, pindexnew)).first;
    pindexnew->phashblock = &((*mi).first);
    blockmap::iterator miprev = mapblockindex.find(block.hashprevblock);
    if (miprev != mapblockindex.end())
    {
        pindexnew->pprev = (*miprev).second;
        pindexnew->nheight = pindexnew->pprev->nheight + 1;
        pindexnew->buildskip();
    }
    pindexnew->nchainwork = (pindexnew->pprev ? pindexnew->pprev->nchainwork : 0) + getblockproof(*pindexnew);
    pindexnew->raisevalidity(block_valid_tree);
    if (pindexbestheader == null || pindexbestheader->nchainwork < pindexnew->nchainwork)
        pindexbestheader = pindexnew;

    setdirtyblockindex.insert(pindexnew);

    return pindexnew;
}

/** mark a block as having its data received and checked (up to block_valid_transactions). */
bool receivedblocktransactions(const cblock &block, cvalidationstate& state, cblockindex *pindexnew, const cdiskblockpos& pos)
{
    pindexnew->ntx = block.vtx.size();
    pindexnew->nchaintx = 0;
    pindexnew->nfile = pos.nfile;
    pindexnew->ndatapos = pos.npos;
    pindexnew->nundopos = 0;
    pindexnew->nstatus |= block_have_data;
    pindexnew->raisevalidity(block_valid_transactions);
    setdirtyblockindex.insert(pindexnew);

    if (pindexnew->pprev == null || pindexnew->pprev->nchaintx) {
        // if pindexnew is the genesis block or all parents are block_valid_transactions.
        deque<cblockindex*> queue;
        queue.push_back(pindexnew);

        // recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            cblockindex *pindex = queue.front();
            queue.pop_front();
            pindex->nchaintx = (pindex->pprev ? pindex->pprev->nchaintx : 0) + pindex->ntx;
            {
                lock(cs_nblocksequenceid);
                pindex->nsequenceid = nblocksequenceid++;
            }
            if (chainactive.tip() == null || !setblockindexcandidates.value_comp()(pindex, chainactive.tip())) {
                setblockindexcandidates.insert(pindex);
            }
            std::pair<std::multimap<cblockindex*, cblockindex*>::iterator, std::multimap<cblockindex*, cblockindex*>::iterator> range = mapblocksunlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<cblockindex*, cblockindex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapblocksunlinked.erase(it);
            }
        }
    } else {
        if (pindexnew->pprev && pindexnew->pprev->isvalid(block_valid_tree)) {
            mapblocksunlinked.insert(std::make_pair(pindexnew->pprev, pindexnew));
        }
    }

    return true;
}

bool findblockpos(cvalidationstate &state, cdiskblockpos &pos, unsigned int naddsize, unsigned int nheight, uint64_t ntime, bool fknown = false)
{
    lock(cs_lastblockfile);

    unsigned int nfile = fknown ? pos.nfile : nlastblockfile;
    if (vinfoblockfile.size() <= nfile) {
        vinfoblockfile.resize(nfile + 1);
    }

    if (!fknown) {
        while (vinfoblockfile[nfile].nsize + naddsize >= max_blockfile_size) {
            logprintf("leaving block file %i: %s\n", nfile, vinfoblockfile[nfile].tostring());
            flushblockfile(true);
            nfile++;
            if (vinfoblockfile.size() <= nfile) {
                vinfoblockfile.resize(nfile + 1);
            }
        }
        pos.nfile = nfile;
        pos.npos = vinfoblockfile[nfile].nsize;
    }

    nlastblockfile = nfile;
    vinfoblockfile[nfile].addblock(nheight, ntime);
    if (fknown)
        vinfoblockfile[nfile].nsize = std::max(pos.npos + naddsize, vinfoblockfile[nfile].nsize);
    else
        vinfoblockfile[nfile].nsize += naddsize;

    if (!fknown) {
        unsigned int noldchunks = (pos.npos + blockfile_chunk_size - 1) / blockfile_chunk_size;
        unsigned int nnewchunks = (vinfoblockfile[nfile].nsize + blockfile_chunk_size - 1) / blockfile_chunk_size;
        if (nnewchunks > noldchunks) {
            if (fprunemode)
                fcheckforpruning = true;
            if (checkdiskspace(nnewchunks * blockfile_chunk_size - pos.npos)) {
                file *file = openblockfile(pos);
                if (file) {
                    logprintf("pre-allocating up to position 0x%x in blk%05u.dat\n", nnewchunks * blockfile_chunk_size, pos.nfile);
                    allocatefilerange(file, pos.npos, nnewchunks * blockfile_chunk_size - pos.npos);
                    fclose(file);
                }
            }
            else
                return state.error("out of disk space");
        }
    }

    setdirtyfileinfo.insert(nfile);
    return true;
}

bool findundopos(cvalidationstate &state, int nfile, cdiskblockpos &pos, unsigned int naddsize)
{
    pos.nfile = nfile;

    lock(cs_lastblockfile);

    unsigned int nnewsize;
    pos.npos = vinfoblockfile[nfile].nundosize;
    nnewsize = vinfoblockfile[nfile].nundosize += naddsize;
    setdirtyfileinfo.insert(nfile);

    unsigned int noldchunks = (pos.npos + undofile_chunk_size - 1) / undofile_chunk_size;
    unsigned int nnewchunks = (nnewsize + undofile_chunk_size - 1) / undofile_chunk_size;
    if (nnewchunks > noldchunks) {
        if (fprunemode)
            fcheckforpruning = true;
        if (checkdiskspace(nnewchunks * undofile_chunk_size - pos.npos)) {
            file *file = openundofile(pos);
            if (file) {
                logprintf("pre-allocating up to position 0x%x in rev%05u.dat\n", nnewchunks * undofile_chunk_size, pos.nfile);
                allocatefilerange(file, pos.npos, nnewchunks * undofile_chunk_size - pos.npos);
                fclose(file);
            }
        }
        else
            return state.error("out of disk space");
    }

    return true;
}

bool checkblockheader(const cblockheader& block, cvalidationstate& state, bool fcheckpow)
{
    // check proof of work matches claimed amount
    if (fcheckpow && !checkproofofwork(block.gethash(), block.nbits, params().getconsensus()))
        return state.dos(50, error("checkblockheader(): proof of work failed"),
                         reject_invalid, "high-hash");

    // check timestamp
    if (block.getblocktime() > getadjustedtime() + 2 * 60 * 60)
        return state.invalid(error("checkblockheader(): block timestamp too far in the future"),
                             reject_invalid, "time-too-new");

    return true;
}

bool checkblock(const cblock& block, cvalidationstate& state, bool fcheckpow, bool fcheckmerkleroot)
{
    // these are checks that are independent of context.

    // check that the header is valid (particularly pow).  this is mostly
    // redundant with the call in acceptblockheader.
    if (!checkblockheader(block, state, fcheckpow))
        return false;

    // check the merkle root.
    if (fcheckmerkleroot) {
        bool mutated;
        uint256 hashmerkleroot2 = block.buildmerkletree(&mutated);
        if (block.hashmerkleroot != hashmerkleroot2)
            return state.dos(100, error("checkblock(): hashmerkleroot mismatch"),
                             reject_invalid, "bad-txnmrklroot", true);

        // check for merkle tree malleability (cve-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.dos(100, error("checkblock(): duplicate transaction"),
                             reject_invalid, "bad-txns-duplicate", true);
    }

    // all potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.

    // size limits
    if (block.vtx.empty() || block.vtx.size() > max_block_size || ::getserializesize(block, ser_network, protocol_version) > max_block_size)
        return state.dos(100, error("checkblock(): size limits failed"),
                         reject_invalid, "bad-blk-length");

    // first transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].iscoinbase())
        return state.dos(100, error("checkblock(): first tx is not coinbase"),
                         reject_invalid, "bad-cb-missing");
    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].iscoinbase())
            return state.dos(100, error("checkblock(): more than one coinbase"),
                             reject_invalid, "bad-cb-multiple");

    // check transactions
    boost_foreach(const ctransaction& tx, block.vtx)
        if (!checktransaction(tx, state))
            return error("checkblock(): checktransaction failed");

    unsigned int nsigops = 0;
    boost_foreach(const ctransaction& tx, block.vtx)
    {
        nsigops += getlegacysigopcount(tx);
    }
    if (nsigops > max_block_sigops)
        return state.dos(100, error("checkblock(): out-of-bounds sigopcount"),
                         reject_invalid, "bad-blk-sigops", true);

    return true;
}

static bool checkindexagainstcheckpoint(const cblockindex* pindexprev, cvalidationstate& state, const cchainparams& chainparams, const uint256& hash)
{
    if (*pindexprev->phashblock == chainparams.getconsensus().hashgenesisblock)
        return true;

    int nheight = pindexprev->nheight+1;
    // don't accept any forks from the main chain prior to last checkpoint
    cblockindex* pcheckpoint = checkpoints::getlastcheckpoint(chainparams.checkpoints());
    if (pcheckpoint && nheight < pcheckpoint->nheight)
        return state.dos(100, error("%s: forked chain older than last checkpoint (height %d)", __func__, nheight));

    return true;
}

bool contextualcheckblockheader(const cblockheader& block, cvalidationstate& state, cblockindex * const pindexprev)
{
    const consensus::params& consensusparams = params().getconsensus();
    // check proof of work
    if (block.nbits != getnextworkrequired(pindexprev, &block, consensusparams))
        return state.dos(100, error("%s: incorrect proof of work", __func__),
                         reject_invalid, "bad-diffbits");

    // check timestamp against prev
    if (block.getblocktime() <= pindexprev->getmediantimepast())
        return state.invalid(error("%s: block's timestamp is too early", __func__),
                             reject_invalid, "time-too-old");

    // reject block.nversion=1 blocks when 95% (75% on testnet) of the network has upgraded:
    if (block.nversion < 2 && issupermajority(2, pindexprev, consensusparams.nmajorityrejectblockoutdated, consensusparams))
        return state.invalid(error("%s: rejected nversion=1 block", __func__),
                             reject_obsolete, "bad-version");

    // reject block.nversion=2 blocks when 95% (75% on testnet) of the network has upgraded:
    if (block.nversion < 3 && issupermajority(3, pindexprev, consensusparams.nmajorityrejectblockoutdated, consensusparams))
        return state.invalid(error("%s : rejected nversion=2 block", __func__),
                             reject_obsolete, "bad-version");

    return true;
}

bool contextualcheckblock(const cblock& block, cvalidationstate& state, cblockindex * const pindexprev)
{
    const int nheight = pindexprev == null ? 0 : pindexprev->nheight + 1;
    const consensus::params& consensusparams = params().getconsensus();

    // check that all transactions are finalized
    boost_foreach(const ctransaction& tx, block.vtx)
        if (!isfinaltx(tx, nheight, block.getblocktime())) {
            return state.dos(10, error("%s: contains a non-final transaction", __func__), reject_invalid, "bad-txns-nonfinal");
        }

    // enforce block.nversion=2 rule that the coinbase starts with serialized block height
    // if 750 of the last 1,000 blocks are version 2 or greater (51/100 if testnet):
    if (block.nversion >= 2 && issupermajority(2, pindexprev, consensusparams.nmajorityenforceblockupgrade, consensusparams))
    {
        cscript expect = cscript() << nheight;
        if (block.vtx[0].vin[0].scriptsig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptsig.begin())) {
            return state.dos(100, error("%s: block height mismatch in coinbase", __func__), reject_invalid, "bad-cb-height");
        }
    }

    return true;
}

bool acceptblockheader(const cblockheader& block, cvalidationstate& state, cblockindex** ppindex)
{
    const cchainparams& chainparams = params();
    assertlockheld(cs_main);
    // check for duplicate
    uint256 hash = block.gethash();
    blockmap::iterator miself = mapblockindex.find(hash);
    cblockindex *pindex = null;
    if (miself != mapblockindex.end()) {
        // block header is already known.
        pindex = miself->second;
        if (ppindex)
            *ppindex = pindex;
        if (pindex->nstatus & block_failed_mask)
            return state.invalid(error("%s: block is marked invalid", __func__), 0, "duplicate");
        return true;
    }

    if (!checkblockheader(block, state))
        return false;

    // get prev block index
    cblockindex* pindexprev = null;
    if (hash != chainparams.getconsensus().hashgenesisblock) {
        blockmap::iterator mi = mapblockindex.find(block.hashprevblock);
        if (mi == mapblockindex.end())
            return state.dos(10, error("%s: prev block not found", __func__), 0, "bad-prevblk");
        pindexprev = (*mi).second;
        if (pindexprev->nstatus & block_failed_mask)
            return state.dos(100, error("%s: prev block invalid", __func__), reject_invalid, "bad-prevblk");
    }
    assert(pindexprev);
    if (fcheckpointsenabled && !checkindexagainstcheckpoint(pindexprev, state, chainparams, hash))
        return error("%s: checkindexagainstcheckpoint(): %s", __func__, state.getrejectreason().c_str());

    if (!contextualcheckblockheader(block, state, pindexprev))
        return false;

    if (pindex == null)
        pindex = addtoblockindex(block);

    if (ppindex)
        *ppindex = pindex;

    return true;
}

bool acceptblock(cblock& block, cvalidationstate& state, cblockindex** ppindex, bool frequested, cdiskblockpos* dbp)
{
    const cchainparams& chainparams = params();
    assertlockheld(cs_main);

    cblockindex *&pindex = *ppindex;

    if (!acceptblockheader(block, state, &pindex))
        return false;

    // try to process all requested blocks that we don't have, but only
    // process an unrequested block if it's new and has enough work to
    // advance our tip.
    bool falreadyhave = pindex->nstatus & block_have_data;
    bool fhasmorework = (chainactive.tip() ? pindex->nchainwork > chainactive.tip()->nchainwork : true);

    // todo: deal better with return value and error conditions for duplicate
    // and unrequested blocks.
    if (falreadyhave) return true;
    if (!frequested) {  // if we didn't ask for it:
        if (pindex->ntx != 0) return true;  // this is a previously-processed block that was pruned
        if (!fhasmorework) return true;     // don't process less-work chains
    }

    if ((!checkblock(block, state)) || !contextualcheckblock(block, state, pindex->pprev)) {
        if (state.isinvalid() && !state.corruptionpossible()) {
            pindex->nstatus |= block_failed_valid;
            setdirtyblockindex.insert(pindex);
        }
        return false;
    }

    int nheight = pindex->nheight;

    // write block to history file
    try {
        unsigned int nblocksize = ::getserializesize(block, ser_disk, client_version);
        cdiskblockpos blockpos;
        if (dbp != null)
            blockpos = *dbp;
        if (!findblockpos(state, blockpos, nblocksize+8, nheight, block.getblocktime(), dbp != null))
            return error("acceptblock(): findblockpos failed");
        if (dbp == null)
            if (!writeblocktodisk(block, blockpos, chainparams.messagestart()))
                abortnode(state, "failed to write block");
        if (!receivedblocktransactions(block, state, pindex, blockpos))
            return error("acceptblock(): receivedblocktransactions failed");
    } catch (const std::runtime_error& e) {
        return abortnode(state, std::string("system error: ") + e.what());
    }

    if (fcheckforpruning)
        flushstatetodisk(state, flush_state_none); // we just allocated more disk space for block files

    return true;
}

static bool issupermajority(int minversion, const cblockindex* pstart, unsigned nrequired, const consensus::params& consensusparams)
{
    unsigned int nfound = 0;
    for (int i = 0; i < consensusparams.nmajoritywindow && nfound < nrequired && pstart != null; i++)
    {
        if (pstart->nversion >= minversion)
            ++nfound;
        pstart = pstart->pprev;
    }
    return (nfound >= nrequired);
}


bool processnewblock(cvalidationstate &state, cnode* pfrom, cblock* pblock, bool fforceprocessing, cdiskblockpos *dbp)
{
    // preliminary checks
    bool checked = checkblock(*pblock, state);

    {
        lock(cs_main);
        bool frequested = markblockasreceived(pblock->gethash());
        frequested |= fforceprocessing;
        if (!checked) {
            return error("%s: checkblock failed", __func__);
        }

        // store to disk
        cblockindex *pindex = null;
        bool ret = acceptblock(*pblock, state, &pindex, frequested, dbp);
        if (pindex && pfrom) {
            mapblocksource[pindex->getblockhash()] = pfrom->getid();
        }
        checkblockindex();
        if (!ret)
            return error("%s: acceptblock failed", __func__);
    }

    if (!activatebestchain(state, pblock))
        return error("%s: activatebestchain failed", __func__);

    return true;
}

bool testblockvalidity(cvalidationstate &state, const cblock& block, cblockindex * const pindexprev, bool fcheckpow, bool fcheckmerkleroot)
{
    const cchainparams& chainparams = params();
    assertlockheld(cs_main);
    assert(pindexprev && pindexprev == chainactive.tip());
    if (fcheckpointsenabled && !checkindexagainstcheckpoint(pindexprev, state, chainparams, block.gethash()))
        return error("%s: checkindexagainstcheckpoint(): %s", __func__, state.getrejectreason().c_str());

    ccoinsviewcache viewnew(pcoinstip);
    cblockindex indexdummy(block);
    indexdummy.pprev = pindexprev;
    indexdummy.nheight = pindexprev->nheight + 1;

    // note: checkblockheader is called by checkblock
    if (!contextualcheckblockheader(block, state, pindexprev))
        return false;
    if (!checkblock(block, state, fcheckpow, fcheckmerkleroot))
        return false;
    if (!contextualcheckblock(block, state, pindexprev))
        return false;
    if (!connectblock(block, state, &indexdummy, viewnew, true))
        return false;
    assert(state.isvalid());

    return true;
}

/**
 * block pruning code
 */

/* calculate the amount of disk space the block & undo files currently use */
uint64_t calculatecurrentusage()
{
    uint64_t retval = 0;
    boost_foreach(const cblockfileinfo &file, vinfoblockfile) {
        retval += file.nsize + file.nundosize;
    }
    return retval;
}

/* prune a block file (modify associated database entries)*/
void pruneoneblockfile(const int filenumber)
{
    for (blockmap::iterator it = mapblockindex.begin(); it != mapblockindex.end(); ++it) {
        cblockindex* pindex = it->second;
        if (pindex->nfile == filenumber) {
            pindex->nstatus &= ~block_have_data;
            pindex->nstatus &= ~block_have_undo;
            pindex->nfile = 0;
            pindex->ndatapos = 0;
            pindex->nundopos = 0;
            setdirtyblockindex.insert(pindex);

            // prune from mapblocksunlinked -- any block we prune would have
            // to be downloaded again in order to consider its chain, at which
            // point it would be considered as a candidate for
            // mapblocksunlinked or setblockindexcandidates.
            std::pair<std::multimap<cblockindex*, cblockindex*>::iterator, std::multimap<cblockindex*, cblockindex*>::iterator> range = mapblocksunlinked.equal_range(pindex->pprev);
            while (range.first != range.second) {
                std::multimap<cblockindex *, cblockindex *>::iterator it = range.first;
                range.first++;
                if (it->second == pindex) {
                    mapblocksunlinked.erase(it);
                }
            }
        }
    }

    vinfoblockfile[filenumber].setnull();
    setdirtyfileinfo.insert(filenumber);
}


void unlinkprunedfiles(std::set<int>& setfilestoprune)
{
    for (set<int>::iterator it = setfilestoprune.begin(); it != setfilestoprune.end(); ++it) {
        cdiskblockpos pos(*it, 0);
        boost::filesystem::remove(getblockposfilename(pos, "blk"));
        boost::filesystem::remove(getblockposfilename(pos, "rev"));
        logprintf("prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}

/* calculate the block/rev files that should be deleted to remain under target*/
void findfilestoprune(std::set<int>& setfilestoprune)
{
    lock2(cs_main, cs_lastblockfile);
    if (chainactive.tip() == null || nprunetarget == 0) {
        return;
    }
    if (chainactive.tip()->nheight <= params().pruneafterheight()) {
        return;
    }

    unsigned int nlastblockwecanprune = chainactive.tip()->nheight - min_blocks_to_keep;
    uint64_t ncurrentusage = calculatecurrentusage();
    // we don't check to prune until after we've allocated new space for files
    // so we should leave a buffer under our target to account for another allocation
    // before the next pruning.
    uint64_t nbuffer = blockfile_chunk_size + undofile_chunk_size;
    uint64_t nbytestoprune;
    int count=0;

    if (ncurrentusage + nbuffer >= nprunetarget) {
        for (int filenumber = 0; filenumber < nlastblockfile; filenumber++) {
            nbytestoprune = vinfoblockfile[filenumber].nsize + vinfoblockfile[filenumber].nundosize;

            if (vinfoblockfile[filenumber].nsize == 0)
                continue;

            if (ncurrentusage + nbuffer < nprunetarget)  // are we below our target?
                break;

            // don't prune files that could have a block within min_blocks_to_keep of the main chain's tip but keep scanning
            if (vinfoblockfile[filenumber].nheightlast > nlastblockwecanprune)
                continue;

            pruneoneblockfile(filenumber);
            // queue up the files for removal
            setfilestoprune.insert(filenumber);
            ncurrentusage -= nbytestoprune;
            count++;
        }
    }

    logprint("prune", "prune: target=%dmib actual=%dmib diff=%dmib max_prune_height=%d removed %d blk/rev pairs\n",
           nprunetarget/1024/1024, ncurrentusage/1024/1024,
           ((int64_t)nprunetarget - (int64_t)ncurrentusage)/1024/1024,
           nlastblockwecanprune, count);
}

bool checkdiskspace(uint64_t nadditionalbytes)
{
    uint64_t nfreebytesavailable = boost::filesystem::space(getdatadir()).available;

    // check for nmindiskspace bytes (currently 50mb)
    if (nfreebytesavailable < nmindiskspace + nadditionalbytes)
        return abortnode("disk space is low!", _("error: disk space is low!"));

    return true;
}

file* opendiskfile(const cdiskblockpos &pos, const char *prefix, bool freadonly)
{
    if (pos.isnull())
        return null;
    boost::filesystem::path path = getblockposfilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    file* file = fopen(path.string().c_str(), "rb+");
    if (!file && !freadonly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        logprintf("unable to open file %s\n", path.string());
        return null;
    }
    if (pos.npos) {
        if (fseek(file, pos.npos, seek_set)) {
            logprintf("unable to seek to position %u of %s\n", pos.npos, path.string());
            fclose(file);
            return null;
        }
    }
    return file;
}

file* openblockfile(const cdiskblockpos &pos, bool freadonly) {
    return opendiskfile(pos, "blk", freadonly);
}

file* openundofile(const cdiskblockpos &pos, bool freadonly) {
    return opendiskfile(pos, "rev", freadonly);
}

boost::filesystem::path getblockposfilename(const cdiskblockpos &pos, const char *prefix)
{
    return getdatadir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nfile);
}

cblockindex * insertblockindex(uint256 hash)
{
    if (hash.isnull())
        return null;

    // return existing
    blockmap::iterator mi = mapblockindex.find(hash);
    if (mi != mapblockindex.end())
        return (*mi).second;

    // create new
    cblockindex* pindexnew = new cblockindex();
    if (!pindexnew)
        throw runtime_error("loadblockindex(): new cblockindex failed");
    mi = mapblockindex.insert(make_pair(hash, pindexnew)).first;
    pindexnew->phashblock = &((*mi).first);

    return pindexnew;
}

bool static loadblockindexdb()
{
    const cchainparams& chainparams = params();
    if (!pblocktree->loadblockindexguts())
        return false;

    boost::this_thread::interruption_point();

    // calculate nchainwork
    vector<pair<int, cblockindex*> > vsortedbyheight;
    vsortedbyheight.reserve(mapblockindex.size());
    boost_foreach(const pairtype(uint256, cblockindex*)& item, mapblockindex)
    {
        cblockindex* pindex = item.second;
        vsortedbyheight.push_back(make_pair(pindex->nheight, pindex));
    }
    sort(vsortedbyheight.begin(), vsortedbyheight.end());
    boost_foreach(const pairtype(int, cblockindex*)& item, vsortedbyheight)
    {
        cblockindex* pindex = item.second;
        pindex->nchainwork = (pindex->pprev ? pindex->pprev->nchainwork : 0) + getblockproof(*pindex);
        // we can link the chain of blocks for which we've received transactions at some point.
        // pruned nodes may have deleted the block.
        if (pindex->ntx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->nchaintx) {
                    pindex->nchaintx = pindex->pprev->nchaintx + pindex->ntx;
                } else {
                    pindex->nchaintx = 0;
                    mapblocksunlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nchaintx = pindex->ntx;
            }
        }
        if (pindex->isvalid(block_valid_transactions) && (pindex->nchaintx || pindex->pprev == null))
            setblockindexcandidates.insert(pindex);
        if (pindex->nstatus & block_failed_mask && (!pindexbestinvalid || pindex->nchainwork > pindexbestinvalid->nchainwork))
            pindexbestinvalid = pindex;
        if (pindex->pprev)
            pindex->buildskip();
        if (pindex->isvalid(block_valid_tree) && (pindexbestheader == null || cblockindexworkcomparator()(pindexbestheader, pindex)))
            pindexbestheader = pindex;
    }

    // load block file info
    pblocktree->readlastblockfile(nlastblockfile);
    vinfoblockfile.resize(nlastblockfile + 1);
    logprintf("%s: last block file = %i\n", __func__, nlastblockfile);
    for (int nfile = 0; nfile <= nlastblockfile; nfile++) {
        pblocktree->readblockfileinfo(nfile, vinfoblockfile[nfile]);
    }
    logprintf("%s: last block file info: %s\n", __func__, vinfoblockfile[nlastblockfile].tostring());
    for (int nfile = nlastblockfile + 1; true; nfile++) {
        cblockfileinfo info;
        if (pblocktree->readblockfileinfo(nfile, info)) {
            vinfoblockfile.push_back(info);
        } else {
            break;
        }
    }

    // check presence of blk files
    logprintf("checking all blk files are present...\n");
    set<int> setblkdatafiles;
    boost_foreach(const pairtype(uint256, cblockindex*)& item, mapblockindex)
    {
        cblockindex* pindex = item.second;
        if (pindex->nstatus & block_have_data) {
            setblkdatafiles.insert(pindex->nfile);
        }
    }
    for (std::set<int>::iterator it = setblkdatafiles.begin(); it != setblkdatafiles.end(); it++)
    {
        cdiskblockpos pos(*it, 0);
        if (cautofile(openblockfile(pos, true), ser_disk, client_version).isnull()) {
            return false;
        }
    }

    // check whether we have ever pruned block & undo files
    pblocktree->readflag("prunedblockfiles", fhavepruned);
    if (fhavepruned)
        logprintf("loadblockindexdb(): block files have previously been pruned\n");

    // check whether we need to continue reindexing
    bool freindexing = false;
    pblocktree->readreindexing(freindexing);
    freindex |= freindexing;

    // check whether we have a transaction index
    pblocktree->readflag("txindex", ftxindex);
    logprintf("%s: transaction index %s\n", __func__, ftxindex ? "enabled" : "disabled");

    // load pointer to end of best chain
    blockmap::iterator it = mapblockindex.find(pcoinstip->getbestblock());
    if (it == mapblockindex.end())
        return true;
    chainactive.settip(it->second);

    pruneblockindexcandidates();

    logprintf("%s: hashbestchain=%s height=%d date=%s progress=%f\n", __func__,
        chainactive.tip()->getblockhash().tostring(), chainactive.height(),
        datetimestrformat("%y-%m-%d %h:%m:%s", chainactive.tip()->getblocktime()),
        checkpoints::guessverificationprogress(chainparams.checkpoints(), chainactive.tip()));

    return true;
}

cverifydb::cverifydb()
{
    uiinterface.showprogress(_("verifying blocks..."), 0);
}

cverifydb::~cverifydb()
{
    uiinterface.showprogress("", 100);
}

bool cverifydb::verifydb(ccoinsview *coinsview, int nchecklevel, int ncheckdepth)
{
    lock(cs_main);
    if (chainactive.tip() == null || chainactive.tip()->pprev == null)
        return true;

    // verify blocks in the best chain
    if (ncheckdepth <= 0)
        ncheckdepth = 1000000000; // suffices until the year 19000
    if (ncheckdepth > chainactive.height())
        ncheckdepth = chainactive.height();
    nchecklevel = std::max(0, std::min(4, nchecklevel));
    logprintf("verifying last %i blocks at level %i\n", ncheckdepth, nchecklevel);
    ccoinsviewcache coins(coinsview);
    cblockindex* pindexstate = chainactive.tip();
    cblockindex* pindexfailure = null;
    int ngoodtransactions = 0;
    cvalidationstate state;
    for (cblockindex* pindex = chainactive.tip(); pindex && pindex->pprev; pindex = pindex->pprev)
    {
        boost::this_thread::interruption_point();
        uiinterface.showprogress(_("verifying blocks..."), std::max(1, std::min(99, (int)(((double)(chainactive.height() - pindex->nheight)) / (double)ncheckdepth * (nchecklevel >= 4 ? 50 : 100)))));
        if (pindex->nheight < chainactive.height()-ncheckdepth)
            break;
        cblock block;
        // check level 0: read from disk
        if (!readblockfromdisk(block, pindex))
            return error("verifydb(): *** readblockfromdisk failed at %d, hash=%s", pindex->nheight, pindex->getblockhash().tostring());
        // check level 1: verify block validity
        if (nchecklevel >= 1 && !checkblock(block, state))
            return error("verifydb(): *** found bad block at %d, hash=%s\n", pindex->nheight, pindex->getblockhash().tostring());
        // check level 2: verify undo validity
        if (nchecklevel >= 2 && pindex) {
            cblockundo undo;
            cdiskblockpos pos = pindex->getundopos();
            if (!pos.isnull()) {
                if (!undoreadfromdisk(undo, pos, pindex->pprev->getblockhash()))
                    return error("verifydb(): *** found bad undo data at %d, hash=%s\n", pindex->nheight, pindex->getblockhash().tostring());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nchecklevel >= 3 && pindex == pindexstate && (coins.dynamicmemoryusage() + pcoinstip->dynamicmemoryusage()) <= ncoincacheusage) {
            bool fclean = true;
            if (!disconnectblock(block, state, pindex, coins, &fclean))
                return error("verifydb(): *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nheight, pindex->getblockhash().tostring());
            pindexstate = pindex->pprev;
            if (!fclean) {
                ngoodtransactions = 0;
                pindexfailure = pindex;
            } else
                ngoodtransactions += block.vtx.size();
        }
        if (shutdownrequested())
            return true;
    }
    if (pindexfailure)
        return error("verifydb(): *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainactive.height() - pindexfailure->nheight + 1, ngoodtransactions);

    // check level 4: try reconnecting blocks
    if (nchecklevel >= 4) {
        cblockindex *pindex = pindexstate;
        while (pindex != chainactive.tip()) {
            boost::this_thread::interruption_point();
            uiinterface.showprogress(_("verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainactive.height() - pindex->nheight)) / (double)ncheckdepth * 50))));
            pindex = chainactive.next(pindex);
            cblock block;
            if (!readblockfromdisk(block, pindex))
                return error("verifydb(): *** readblockfromdisk failed at %d, hash=%s", pindex->nheight, pindex->getblockhash().tostring());
            if (!connectblock(block, state, pindex, coins))
                return error("verifydb(): *** found unconnectable block at %d, hash=%s", pindex->nheight, pindex->getblockhash().tostring());
        }
    }

    logprintf("no coin database inconsistencies in last %i blocks (%i transactions)\n", chainactive.height() - pindexstate->nheight, ngoodtransactions);

    return true;
}

void unloadblockindex()
{
    lock(cs_main);
    setblockindexcandidates.clear();
    chainactive.settip(null);
    pindexbestinvalid = null;
    pindexbestheader = null;
    mempool.clear();
    maporphantransactions.clear();
    maporphantransactionsbyprev.clear();
    nsyncstarted = 0;
    mapblocksunlinked.clear();
    vinfoblockfile.clear();
    nlastblockfile = 0;
    nblocksequenceid = 1;
    mapblocksource.clear();
    mapblocksinflight.clear();
    nqueuedvalidatedheaders = 0;
    npreferreddownload = 0;
    setdirtyblockindex.clear();
    setdirtyfileinfo.clear();
    mapnodestate.clear();

    boost_foreach(blockmap::value_type& entry, mapblockindex) {
        delete entry.second;
    }
    mapblockindex.clear();
    fhavepruned = false;
}

bool loadblockindex()
{
    // load block index from databases
    if (!freindex && !loadblockindexdb())
        return false;
    return true;
}


bool initblockindex() {
    const cchainparams& chainparams = params();
    lock(cs_main);
    // check whether we're already initialized
    if (chainactive.genesis() != null)
        return true;

    // use the provided setting for -txindex in the new database
    ftxindex = getboolarg("-txindex", false);
    pblocktree->writeflag("txindex", ftxindex);
    logprintf("initializing databases...\n");

    // only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!freindex) {
        try {
            cblock &block = const_cast<cblock&>(params().genesisblock());
            // start new block file
            unsigned int nblocksize = ::getserializesize(block, ser_disk, client_version);
            cdiskblockpos blockpos;
            cvalidationstate state;
            if (!findblockpos(state, blockpos, nblocksize+8, 0, block.getblocktime()))
                return error("loadblockindex(): findblockpos failed");
            if (!writeblocktodisk(block, blockpos, chainparams.messagestart()))
                return error("loadblockindex(): writing genesis block to disk failed");
            cblockindex *pindex = addtoblockindex(block);
            if (!receivedblocktransactions(block, state, pindex, blockpos))
                return error("loadblockindex(): genesis block not accepted");
            if (!activatebestchain(state, &block))
                return error("loadblockindex(): genesis block cannot be activated");
            // force a chainstate write so that when we verifydb in a moment, it doesn't check stale data
            return flushstatetodisk(state, flush_state_always);
        } catch (const std::runtime_error& e) {
            return error("loadblockindex(): failed to initialize block database: %s", e.what());
        }
    }

    return true;
}



bool loadexternalblockfile(file* filein, cdiskblockpos *dbp)
{
    const cchainparams& chainparams = params();
    // map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, cdiskblockpos> mapblocksunknownparent;
    int64_t nstart = gettimemillis();

    int nloaded = 0;
    try {
        // this takes over filein and calls fclose() on it in the cbufferedfile destructor
        cbufferedfile blkdat(filein, 2*max_block_size, max_block_size+8, ser_disk, client_version);
        uint64_t nrewind = blkdat.getpos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.setpos(nrewind);
            nrewind++; // start one byte further next time, in case of failure
            blkdat.setlimit(); // remove former limit
            unsigned int nsize = 0;
            try {
                // locate a header
                unsigned char buf[message_start_size];
                blkdat.findbyte(params().messagestart()[0]);
                nrewind = blkdat.getpos()+1;
                blkdat >> flatdata(buf);
                if (memcmp(buf, params().messagestart(), message_start_size))
                    continue;
                // read size
                blkdat >> nsize;
                if (nsize < 80 || nsize > max_block_size)
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nblockpos = blkdat.getpos();
                if (dbp)
                    dbp->npos = nblockpos;
                blkdat.setlimit(nblockpos + nsize);
                blkdat.setpos(nblockpos);
                cblock block;
                blkdat >> block;
                nrewind = blkdat.getpos();

                // detect out of order blocks, and store them for later
                uint256 hash = block.gethash();
                if (hash != chainparams.getconsensus().hashgenesisblock && mapblockindex.find(block.hashprevblock) == mapblockindex.end()) {
                    logprint("reindex", "%s: out of order block %s, parent %s not known\n", __func__, hash.tostring(),
                            block.hashprevblock.tostring());
                    if (dbp)
                        mapblocksunknownparent.insert(std::make_pair(block.hashprevblock, *dbp));
                    continue;
                }

                // process in case the block isn't known yet
                if (mapblockindex.count(hash) == 0 || (mapblockindex[hash]->nstatus & block_have_data) == 0) {
                    cvalidationstate state;
                    if (processnewblock(state, null, &block, true, dbp))
                        nloaded++;
                    if (state.iserror())
                        break;
                } else if (hash != chainparams.getconsensus().hashgenesisblock && mapblockindex[hash]->nheight % 1000 == 0) {
                    logprintf("block import: already had block %s at height %d\n", hash.tostring(), mapblockindex[hash]->nheight);
                }

                // recursively process earlier encountered successors of this block
                deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, cdiskblockpos>::iterator, std::multimap<uint256, cdiskblockpos>::iterator> range = mapblocksunknownparent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, cdiskblockpos>::iterator it = range.first;
                        if (readblockfromdisk(block, it->second))
                        {
                            logprintf("%s: processing out of order child %s of %s\n", __func__, block.gethash().tostring(),
                                    head.tostring());
                            cvalidationstate dummy;
                            if (processnewblock(dummy, null, &block, true, &it->second))
                            {
                                nloaded++;
                                queue.push_back(block.gethash());
                            }
                        }
                        range.first++;
                        mapblocksunknownparent.erase(it);
                    }
                }
            } catch (const std::exception& e) {
                logprintf("%s: deserialize or i/o error - %s", __func__, e.what());
            }
        }
    } catch (const std::runtime_error& e) {
        abortnode(std::string("system error: ") + e.what());
    }
    if (nloaded > 0)
        logprintf("loaded %i blocks from external file in %dms\n", nloaded, gettimemillis() - nstart);
    return nloaded > 0;
}

void static checkblockindex()
{
    const consensus::params& consensusparams = params().getconsensus();
    if (!fcheckblockindex) {
        return;
    }

    lock(cs_main);

    // during a reindex, we read the genesis block and call checkblockindex before activatebestchain,
    // so we have the genesis block in mapblockindex but no active chain.  (a few of the tests when
    // iterating the block tree require that chainactive has been initialized.)
    if (chainactive.height() < 0) {
        assert(mapblockindex.size() <= 1);
        return;
    }

    // build forward-pointing map of the entire block tree.
    std::multimap<cblockindex*,cblockindex*> forward;
    for (blockmap::iterator it = mapblockindex.begin(); it != mapblockindex.end(); it++) {
        forward.insert(std::make_pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapblockindex.size());

    std::pair<std::multimap<cblockindex*,cblockindex*>::iterator,std::multimap<cblockindex*,cblockindex*>::iterator> rangegenesis = forward.equal_range(null);
    cblockindex *pindex = rangegenesis.first->second;
    rangegenesis.first++;
    assert(rangegenesis.first == rangegenesis.second); // there is only one index entry with parent null.

    // iterate over the entire block tree, using depth-first search.
    // along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nnodes = 0;
    int nheight = 0;
    cblockindex* pindexfirstinvalid = null; // oldest ancestor of pindex which is invalid.
    cblockindex* pindexfirstmissing = null; // oldest ancestor of pindex which does not have block_have_data.
    cblockindex* pindexfirstneverprocessed = null; // oldest ancestor of pindex for which ntx == 0.
    cblockindex* pindexfirstnottreevalid = null; // oldest ancestor of pindex which does not have block_valid_tree (regardless of being valid or not).
    cblockindex* pindexfirstnottransactionsvalid = null; // oldest ancestor of pindex which does not have block_valid_transactions (regardless of being valid or not).
    cblockindex* pindexfirstnotchainvalid = null; // oldest ancestor of pindex which does not have block_valid_chain (regardless of being valid or not).
    cblockindex* pindexfirstnotscriptsvalid = null; // oldest ancestor of pindex which does not have block_valid_scripts (regardless of being valid or not).
    while (pindex != null) {
        nnodes++;
        if (pindexfirstinvalid == null && pindex->nstatus & block_failed_valid) pindexfirstinvalid = pindex;
        if (pindexfirstmissing == null && !(pindex->nstatus & block_have_data)) pindexfirstmissing = pindex;
        if (pindexfirstneverprocessed == null && pindex->ntx == 0) pindexfirstneverprocessed = pindex;
        if (pindex->pprev != null && pindexfirstnottreevalid == null && (pindex->nstatus & block_valid_mask) < block_valid_tree) pindexfirstnottreevalid = pindex;
        if (pindex->pprev != null && pindexfirstnottransactionsvalid == null && (pindex->nstatus & block_valid_mask) < block_valid_transactions) pindexfirstnottransactionsvalid = pindex;
        if (pindex->pprev != null && pindexfirstnotchainvalid == null && (pindex->nstatus & block_valid_mask) < block_valid_chain) pindexfirstnotchainvalid = pindex;
        if (pindex->pprev != null && pindexfirstnotscriptsvalid == null && (pindex->nstatus & block_valid_mask) < block_valid_scripts) pindexfirstnotscriptsvalid = pindex;

        // begin: actual consistency checks.
        if (pindex->pprev == null) {
            // genesis block checks.
            assert(pindex->getblockhash() == consensusparams.hashgenesisblock); // genesis block's hash must match.
            assert(pindex == chainactive.genesis()); // the current active chain's genesis block must be this block.
        }
        if (pindex->nchaintx == 0) assert(pindex->nsequenceid == 0);  // nsequenceid can't be set for blocks that aren't linked
        // valid_transactions is equivalent to ntx > 0 for all nodes (whether or not pruning has occurred).
        // have_data is only equivalent to ntx > 0 (or valid_transactions) if no pruning has occurred.
        if (!fhavepruned) {
            // if we've never pruned, then have_data should be equivalent to ntx > 0
            assert(!(pindex->nstatus & block_have_data) == (pindex->ntx == 0));
            assert(pindexfirstmissing == pindexfirstneverprocessed);
        } else {
            // if we have pruned, then we can only say that have_data implies ntx > 0
            if (pindex->nstatus & block_have_data) assert(pindex->ntx > 0);
        }
        if (pindex->nstatus & block_have_undo) assert(pindex->nstatus & block_have_data);
        assert(((pindex->nstatus & block_valid_mask) >= block_valid_transactions) == (pindex->ntx > 0)); // this is pruning-independent.
        // all parents having had data (at some point) is equivalent to all parents being valid_transactions, which is equivalent to nchaintx being set.
        assert((pindexfirstneverprocessed != null) == (pindex->nchaintx == 0)); // nchaintx != 0 is used to signal that all parent blocks have been processed (but may have been pruned).
        assert((pindexfirstnottransactionsvalid != null) == (pindex->nchaintx == 0));
        assert(pindex->nheight == nheight); // nheight must be consistent.
        assert(pindex->pprev == null || pindex->nchainwork >= pindex->pprev->nchainwork); // for every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nheight < 2 || (pindex->pskip && (pindex->pskip->nheight < nheight))); // the pskip pointer must point back for all but the first 2 blocks.
        assert(pindexfirstnottreevalid == null); // all mapblockindex entries must at least be tree valid
        if ((pindex->nstatus & block_valid_mask) >= block_valid_tree) assert(pindexfirstnottreevalid == null); // tree valid implies all parents are tree valid
        if ((pindex->nstatus & block_valid_mask) >= block_valid_chain) assert(pindexfirstnotchainvalid == null); // chain valid implies all parents are chain valid
        if ((pindex->nstatus & block_valid_mask) >= block_valid_scripts) assert(pindexfirstnotscriptsvalid == null); // scripts valid implies all parents are scripts valid
        if (pindexfirstinvalid == null) {
            // checks for not-invalid blocks.
            assert((pindex->nstatus & block_failed_mask) == 0); // the failed mask cannot be set for blocks without invalid parents.
        }
        if (!cblockindexworkcomparator()(pindex, chainactive.tip()) && pindexfirstneverprocessed == null) {
            if (pindexfirstinvalid == null) {
                // if this block sorts at least as good as the current tip and
                // is valid and we have all data for its parents, it must be in
                // setblockindexcandidates.  chainactive.tip() must also be there
                // even if some data has been pruned.
                if (pindexfirstmissing == null || pindex == chainactive.tip()) {
                    assert(setblockindexcandidates.count(pindex));
                }
                // if some parent is missing, then it could be that this block was in
                // setblockindexcandidates but had to be removed because of the missing data.
                // in this case it must be in mapblocksunlinked -- see test below.
            }
        } else { // if this block sorts worse than the current tip or some ancestor's block has never been seen, it cannot be in setblockindexcandidates.
            assert(setblockindexcandidates.count(pindex) == 0);
        }
        // check whether this block is in mapblocksunlinked.
        std::pair<std::multimap<cblockindex*,cblockindex*>::iterator,std::multimap<cblockindex*,cblockindex*>::iterator> rangeunlinked = mapblocksunlinked.equal_range(pindex->pprev);
        bool foundinunlinked = false;
        while (rangeunlinked.first != rangeunlinked.second) {
            assert(rangeunlinked.first->first == pindex->pprev);
            if (rangeunlinked.first->second == pindex) {
                foundinunlinked = true;
                break;
            }
            rangeunlinked.first++;
        }
        if (pindex->pprev && (pindex->nstatus & block_have_data) && pindexfirstneverprocessed != null && pindexfirstinvalid == null) {
            // if this block has block data available, some parent was never received, and has no invalid parents, it must be in mapblocksunlinked.
            assert(foundinunlinked);
        }
        if (!(pindex->nstatus & block_have_data)) assert(!foundinunlinked); // can't be in mapblocksunlinked if we don't have_data
        if (pindexfirstmissing == null) assert(!foundinunlinked); // we aren't missing data for any parent -- cannot be in mapblocksunlinked.
        if (pindex->pprev && (pindex->nstatus & block_have_data) && pindexfirstneverprocessed == null && pindexfirstmissing != null) {
            // we have_data for this block, have received data for all parents at some point, but we're currently missing data for some parent.
            assert(fhavepruned); // we must have pruned.
            // this block may have entered mapblocksunlinked if:
            //  - it has a descendant that at some point had more work than the
            //    tip, and
            //  - we tried switching to that descendant but were missing
            //    data for some intermediate block between chainactive and the
            //    tip.
            // so if this block is itself better than chainactive.tip() and it wasn't in
            // setblockindexcandidates, then it must be in mapblocksunlinked.
            if (!cblockindexworkcomparator()(pindex, chainactive.tip()) && setblockindexcandidates.count(pindex) == 0) {
                if (pindexfirstinvalid == null) {
                    assert(foundinunlinked);
                }
            }
        }
        // assert(pindex->getblockhash() == pindex->getblockheader().gethash()); // perhaps too slow
        // end: actual consistency checks.

        // try descending into the first subnode.
        std::pair<std::multimap<cblockindex*,cblockindex*>::iterator,std::multimap<cblockindex*,cblockindex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // a subnode was found.
            pindex = range.first->second;
            nheight++;
            continue;
        }
        // this is a leaf node.
        // move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // we are going to either move to a parent or a sibling of pindex.
            // if pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexfirstinvalid) pindexfirstinvalid = null;
            if (pindex == pindexfirstmissing) pindexfirstmissing = null;
            if (pindex == pindexfirstneverprocessed) pindexfirstneverprocessed = null;
            if (pindex == pindexfirstnottreevalid) pindexfirstnottreevalid = null;
            if (pindex == pindexfirstnottransactionsvalid) pindexfirstnottransactionsvalid = null;
            if (pindex == pindexfirstnotchainvalid) pindexfirstnotchainvalid = null;
            if (pindex == pindexfirstnotscriptsvalid) pindexfirstnotscriptsvalid = null;
            // find our parent.
            cblockindex* pindexpar = pindex->pprev;
            // find which child we just visited.
            std::pair<std::multimap<cblockindex*,cblockindex*>::iterator,std::multimap<cblockindex*,cblockindex*>::iterator> rangepar = forward.equal_range(pindexpar);
            while (rangepar.first->second != pindex) {
                assert(rangepar.first != rangepar.second); // our parent must have at least the node we're coming from as child.
                rangepar.first++;
            }
            // proceed to the next one.
            rangepar.first++;
            if (rangepar.first != rangepar.second) {
                // move to the sibling.
                pindex = rangepar.first->second;
                break;
            } else {
                // move up further.
                pindex = pindexpar;
                nheight--;
                continue;
            }
        }
    }

    // check that we actually traversed the entire map.
    assert(nnodes == forward.size());
}

//////////////////////////////////////////////////////////////////////////////
//
// calert
//

std::string getwarnings(const std::string& strfor)
{
    int npriority = 0;
    string strstatusbar;
    string strrpc;

    if (!client_version_is_release)
        strstatusbar = _("this is a pre-release test build - use at your own risk - do not use for mining or merchant applications");

    if (getboolarg("-testsafemode", false))
        strstatusbar = strrpc = "testsafemode enabled";

    // misc warnings like out of disk space and clock is wrong
    if (strmiscwarning != "")
    {
        npriority = 1000;
        strstatusbar = strmiscwarning;
    }

    if (flargeworkforkfound)
    {
        npriority = 2000;
        strstatusbar = strrpc = _("warning: the network does not appear to fully agree! some miners appear to be experiencing issues.");
    }
    else if (flargeworkinvalidchainfound)
    {
        npriority = 2000;
        strstatusbar = strrpc = _("warning: we do not appear to fully agree with our peers! you may need to upgrade, or other nodes may need to upgrade.");
    }

    // alerts
    {
        lock(cs_mapalerts);
        boost_foreach(pairtype(const uint256, calert)& item, mapalerts)
        {
            const calert& alert = item.second;
            if (alert.appliestome() && alert.npriority > npriority)
            {
                npriority = alert.npriority;
                strstatusbar = alert.strstatusbar;
            }
        }
    }

    if (strfor == "statusbar")
        return strstatusbar;
    else if (strfor == "rpc")
        return strrpc;
    assert(!"getwarnings(): invalid parameter");
    return "error";
}








//////////////////////////////////////////////////////////////////////////////
//
// messages
//


bool static alreadyhave(const cinv& inv)
{
    switch (inv.type)
    {
    case msg_tx:
        {
            bool txinmap = false;
            txinmap = mempool.exists(inv.hash);
            return txinmap || maporphantransactions.count(inv.hash) ||
                pcoinstip->havecoins(inv.hash);
        }
    case msg_block:
        return mapblockindex.count(inv.hash);
    }
    // don't know what it is, just say we already got one
    return true;
}

void static processgetdata(cnode* pfrom)
{
    std::deque<cinv>::iterator it = pfrom->vrecvgetdata.begin();

    vector<cinv> vnotfound;

    lock(cs_main);

    while (it != pfrom->vrecvgetdata.end()) {
        // don't bother if send buffer is too full to respond anyway
        if (pfrom->nsendsize >= sendbuffersize())
            break;

        const cinv &inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == msg_block || inv.type == msg_filtered_block)
            {
                bool send = false;
                blockmap::iterator mi = mapblockindex.find(inv.hash);
                if (mi != mapblockindex.end())
                {
                    if (chainactive.contains(mi->second)) {
                        send = true;
                    } else {
                        static const int nonemonth = 30 * 24 * 60 * 60;
                        // to prevent fingerprinting attacks, only send blocks outside of the active
                        // chain if they are valid, and no more than a month older (both in time, and in
                        // best equivalent proof of work) than the best header chain we know about.
                        send = mi->second->isvalid(block_valid_scripts) && (pindexbestheader != null) &&
                            (pindexbestheader->getblocktime() - mi->second->getblocktime() < nonemonth) &&
                            (getblockproofequivalenttime(*pindexbestheader, *mi->second, *pindexbestheader, params().getconsensus()) < nonemonth);
                        if (!send) {
                            logprintf("%s: ignoring request from peer=%i for old block that isn't in the main chain\n", __func__, pfrom->getid());
                        }
                    }
                }
                // pruned nodes may have deleted the block, so check whether
                // it's available before trying to send.
                if (send && (mi->second->nstatus & block_have_data))
                {
                    // send block from disk
                    cblock block;
                    if (!readblockfromdisk(block, (*mi).second))
                        assert(!"cannot load block from disk");
                    if (inv.type == msg_block)
                        pfrom->pushmessage("block", block);
                    else // msg_filtered_block)
                    {
                        lock(pfrom->cs_filter);
                        if (pfrom->pfilter)
                        {
                            cmerkleblock merkleblock(block, *pfrom->pfilter);
                            pfrom->pushmessage("merkleblock", merkleblock);
                            // cmerkleblock just contains hashes, so also push any transactions in the block the client did not see
                            // this avoids hurting performance by pointlessly requiring a round-trip
                            // note that there is currently no way for a node to request any single transactions we didn't send here -
                            // they must either disconnect and retry or request the full block.
                            // thus, the protocol spec specified allows for us to provide duplicate txn here,
                            // however we must always provide at least what the remote peer needs
                            typedef std::pair<unsigned int, uint256> pairtype;
                            boost_foreach(pairtype& pair, merkleblock.vmatchedtxn)
                                if (!pfrom->setinventoryknown.count(cinv(msg_tx, pair.second)))
                                    pfrom->pushmessage("tx", block.vtx[pair.first]);
                        }
                        // else
                            // no response
                    }

                    // trigger the peer node to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashcontinue)
                    {
                        // bypass pushinventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<cinv> vinv;
                        vinv.push_back(cinv(msg_block, chainactive.tip()->getblockhash()));
                        pfrom->pushmessage("inv", vinv);
                        pfrom->hashcontinue.setnull();
                    }
                }
            }
            else if (inv.isknowntype())
            {
                // send stream from relay memory
                bool pushed = false;
                {
                    lock(cs_maprelay);
                    map<cinv, cdatastream>::iterator mi = maprelay.find(inv);
                    if (mi != maprelay.end()) {
                        pfrom->pushmessage(inv.getcommand(), (*mi).second);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == msg_tx) {
                    ctransaction tx;
                    if (mempool.lookup(inv.hash, tx)) {
                        cdatastream ss(ser_network, protocol_version);
                        ss.reserve(1000);
                        ss << tx;
                        pfrom->pushmessage("tx", ss);
                        pushed = true;
                    }
                }
                if (!pushed) {
                    vnotfound.push_back(inv);
                }
            }

            // track requests for our stuff.
            getmainsignals().inventory(inv.hash);

            if (inv.type == msg_block || inv.type == msg_filtered_block)
                break;
        }
    }

    pfrom->vrecvgetdata.erase(pfrom->vrecvgetdata.begin(), it);

    if (!vnotfound.empty()) {
        // let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. currently only spv clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. spv clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.
        pfrom->pushmessage("notfound", vnotfound);
    }
}

bool static processmessage(cnode* pfrom, string strcommand, cdatastream& vrecv, int64_t ntimereceived)
{
    const cchainparams& chainparams = params();
    randaddseedperfmon();
    logprint("net", "received: %s (%u bytes) peer=%d\n", sanitizestring(strcommand), vrecv.size(), pfrom->id);
    if (mapargs.count("-dropmessagestest") && getrand(atoi(mapargs["-dropmessagestest"])) == 0)
    {
        logprintf("dropmessagestest dropping recv message\n");
        return true;
    }




    if (strcommand == "version")
    {
        // each connection can only send one version message
        if (pfrom->nversion != 0)
        {
            pfrom->pushmessage("reject", strcommand, reject_duplicate, string("duplicate version message"));
            misbehaving(pfrom->getid(), 1);
            return false;
        }

        int64_t ntime;
        caddress addrme;
        caddress addrfrom;
        uint64_t nnonce = 1;
        vrecv >> pfrom->nversion >> pfrom->nservices >> ntime >> addrme;
        if (pfrom->nversion < min_peer_proto_version)
        {
            // disconnect from peers older than this proto version
            logprintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, pfrom->nversion);
            pfrom->pushmessage("reject", strcommand, reject_obsolete,
                               strprintf("version must be %d or greater", min_peer_proto_version));
            pfrom->fdisconnect = true;
            return false;
        }

        if (pfrom->nversion == 10300)
            pfrom->nversion = 300;
        if (!vrecv.empty())
            vrecv >> addrfrom >> nnonce;
        if (!vrecv.empty()) {
            vrecv >> limited_string(pfrom->strsubver, 256);
            pfrom->cleansubver = sanitizestring(pfrom->strsubver);
        }
        if (!vrecv.empty())
            vrecv >> pfrom->nstartingheight;
        if (!vrecv.empty())
            vrecv >> pfrom->frelaytxes; // set to true after we get the first filter* message
        else
            pfrom->frelaytxes = true;

        // disconnect if we connected to ourself
        if (nnonce == nlocalhostnonce && nnonce > 1)
        {
            logprintf("connected to self at %s, disconnecting\n", pfrom->addr.tostring());
            pfrom->fdisconnect = true;
            return true;
        }

        pfrom->addrlocal = addrme;
        if (pfrom->finbound && addrme.isroutable())
        {
            seenlocal(addrme);
        }

        // be shy and don't send version until we hear
        if (pfrom->finbound)
            pfrom->pushversion();

        pfrom->fclient = !(pfrom->nservices & node_network);

        // potentially mark this peer as a preferred download peer.
        updatepreferreddownload(pfrom, state(pfrom->getid()));

        // change version
        pfrom->pushmessage("verack");
        pfrom->sssend.setversion(min(pfrom->nversion, protocol_version));

        if (!pfrom->finbound)
        {
            // advertise our address
            if (flisten && !isinitialblockdownload())
            {
                caddress addr = getlocaladdress(&pfrom->addr);
                if (addr.isroutable())
                {
                    pfrom->pushaddress(addr);
                } else if (ispeeraddrlocalgood(pfrom)) {
                    addr.setip(pfrom->addrlocal);
                    pfrom->pushaddress(addr);
                }
            }

            // get recent addresses
            if (pfrom->foneshot || pfrom->nversion >= caddr_time_version || addrman.size() < 1000)
            {
                pfrom->pushmessage("getaddr");
                pfrom->fgetaddr = true;
            }
            addrman.good(pfrom->addr);
        } else {
            if (((cnetaddr)pfrom->addr) == (cnetaddr)addrfrom)
            {
                addrman.add(addrfrom, addrfrom);
                addrman.good(addrfrom);
            }
        }

        // relay alerts
        {
            lock(cs_mapalerts);
            boost_foreach(pairtype(const uint256, calert)& item, mapalerts)
                item.second.relayto(pfrom);
        }

        pfrom->fsuccessfullyconnected = true;

        string remoteaddr;
        if (flogips)
            remoteaddr = ", peeraddr=" + pfrom->addr.tostring();

        logprintf("receive version message: %s: version %d, blocks=%d, us=%s, peer=%d%s\n",
                  pfrom->cleansubver, pfrom->nversion,
                  pfrom->nstartingheight, addrme.tostring(), pfrom->id,
                  remoteaddr);

        int64_t ntimeoffset = ntime - gettime();
        pfrom->ntimeoffset = ntimeoffset;
        addtimedata(pfrom->addr, ntimeoffset);
    }


    else if (pfrom->nversion == 0)
    {
        // must have a version message before anything else
        misbehaving(pfrom->getid(), 1);
        return false;
    }


    else if (strcommand == "verack")
    {
        pfrom->setrecvversion(min(pfrom->nversion, protocol_version));

        // mark this node as currently connected, so we update its timestamp later.
        if (pfrom->fnetworknode) {
            lock(cs_main);
            state(pfrom->getid())->fcurrentlyconnected = true;
        }
    }


    else if (strcommand == "addr")
    {
        vector<caddress> vaddr;
        vrecv >> vaddr;

        // don't want addr from older versions unless seeding
        if (pfrom->nversion < caddr_time_version && addrman.size() > 1000)
            return true;
        if (vaddr.size() > 1000)
        {
            misbehaving(pfrom->getid(), 20);
            return error("message addr size() = %u", vaddr.size());
        }

        // store the new addresses
        vector<caddress> vaddrok;
        int64_t nnow = getadjustedtime();
        int64_t nsince = nnow - 10 * 60;
        boost_foreach(caddress& addr, vaddr)
        {
            boost::this_thread::interruption_point();

            if (addr.ntime <= 100000000 || addr.ntime > nnow + 10 * 60)
                addr.ntime = nnow - 5 * 24 * 60 * 60;
            pfrom->addaddressknown(addr);
            bool freachable = isreachable(addr);
            if (addr.ntime > nsince && !pfrom->fgetaddr && vaddr.size() <= 10 && addr.isroutable())
            {
                // relay to a limited number of other nodes
                {
                    lock(cs_vnodes);
                    // use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the addrknowns of the chosen nodes prevent repeats
                    static uint256 hashsalt;
                    if (hashsalt.isnull())
                        hashsalt = getrandhash();
                    uint64_t hashaddr = addr.gethash();
                    uint256 hashrand = arithtouint256(uinttoarith256(hashsalt) ^ (hashaddr<<32) ^ ((gettime()+hashaddr)/(24*60*60)));
                    hashrand = hash(begin(hashrand), end(hashrand));
                    multimap<uint256, cnode*> mapmix;
                    boost_foreach(cnode* pnode, vnodes)
                    {
                        if (pnode->nversion < caddr_time_version)
                            continue;
                        unsigned int npointer;
                        memcpy(&npointer, &pnode, sizeof(npointer));
                        uint256 hashkey = arithtouint256(uinttoarith256(hashrand) ^ npointer);
                        hashkey = hash(begin(hashkey), end(hashkey));
                        mapmix.insert(make_pair(hashkey, pnode));
                    }
                    int nrelaynodes = freachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, cnode*>::iterator mi = mapmix.begin(); mi != mapmix.end() && nrelaynodes-- > 0; ++mi)
                        ((*mi).second)->pushaddress(addr);
                }
            }
            // do not store addresses outside our network
            if (freachable)
                vaddrok.push_back(addr);
        }
        addrman.add(vaddrok, pfrom->addr, 2 * 60 * 60);
        if (vaddr.size() < 1000)
            pfrom->fgetaddr = false;
        if (pfrom->foneshot)
            pfrom->fdisconnect = true;
    }


    else if (strcommand == "inv")
    {
        vector<cinv> vinv;
        vrecv >> vinv;
        if (vinv.size() > max_inv_sz)
        {
            misbehaving(pfrom->getid(), 20);
            return error("message inv size() = %u", vinv.size());
        }

        lock(cs_main);

        std::vector<cinv> vtofetch;

        for (unsigned int ninv = 0; ninv < vinv.size(); ninv++)
        {
            const cinv &inv = vinv[ninv];

            boost::this_thread::interruption_point();
            pfrom->addinventoryknown(inv);

            bool falreadyhave = alreadyhave(inv);
            logprint("net", "got inv: %s  %s peer=%d\n", inv.tostring(), falreadyhave ? "have" : "new", pfrom->id);

            if (!falreadyhave && !fimporting && !freindex && inv.type != msg_block)
                pfrom->askfor(inv);

            if (inv.type == msg_block) {
                updateblockavailability(pfrom->getid(), inv.hash);
                if (!falreadyhave && !fimporting && !freindex && !mapblocksinflight.count(inv.hash)) {
                    // first request the headers preceding the announced block. in the normal fully-synced
                    // case where a new block is announced that succeeds the current tip (no reorganization),
                    // there are no such headers.
                    // secondly, and only when we are close to being synced, we request the announced block directly,
                    // to avoid an extra round-trip. note that we must *first* ask for the headers, so by the
                    // time the block arrives, the header chain leading up to it is already validated. not
                    // doing this will result in the received block being rejected as an orphan in case it is
                    // not a direct successor.
                    pfrom->pushmessage("getheaders", chainactive.getlocator(pindexbestheader), inv.hash);
                    cnodestate *nodestate = state(pfrom->getid());
                    if (chainactive.tip()->getblocktime() > getadjustedtime() - chainparams.getconsensus().npowtargetspacing * 20 &&
                        nodestate->nblocksinflight < max_blocks_in_transit_per_peer) {
                        vtofetch.push_back(inv);
                        // mark block as in flight already, even though the actual "getdata" message only goes out
                        // later (within the same cs_main lock, though).
                        markblockasinflight(pfrom->getid(), inv.hash, chainparams.getconsensus());
                    }
                    logprint("net", "getheaders (%d) %s to peer=%d\n", pindexbestheader->nheight, inv.hash.tostring(), pfrom->id);
                }
            }

            // track requests for our stuff
            getmainsignals().inventory(inv.hash);

            if (pfrom->nsendsize > (sendbuffersize() * 2)) {
                misbehaving(pfrom->getid(), 50);
                return error("send buffer size() = %u", pfrom->nsendsize);
            }
        }

        if (!vtofetch.empty())
            pfrom->pushmessage("getdata", vtofetch);
    }


    else if (strcommand == "getdata")
    {
        vector<cinv> vinv;
        vrecv >> vinv;
        if (vinv.size() > max_inv_sz)
        {
            misbehaving(pfrom->getid(), 20);
            return error("message getdata size() = %u", vinv.size());
        }

        if (fdebug || (vinv.size() != 1))
            logprint("net", "received getdata (%u invsz) peer=%d\n", vinv.size(), pfrom->id);

        if ((fdebug && vinv.size() > 0) || (vinv.size() == 1))
            logprint("net", "received getdata for: %s peer=%d\n", vinv[0].tostring(), pfrom->id);

        pfrom->vrecvgetdata.insert(pfrom->vrecvgetdata.end(), vinv.begin(), vinv.end());
        processgetdata(pfrom);
    }


    else if (strcommand == "getblocks")
    {
        cblocklocator locator;
        uint256 hashstop;
        vrecv >> locator >> hashstop;

        lock(cs_main);

        // find the last block the caller has in the main chain
        cblockindex* pindex = findforkinglobalindex(chainactive, locator);

        // send the rest of the chain
        if (pindex)
            pindex = chainactive.next(pindex);
        int nlimit = 500;
        logprint("net", "getblocks %d to %s limit %d from peer=%d\n", (pindex ? pindex->nheight : -1), hashstop.isnull() ? "end" : hashstop.tostring(), nlimit, pfrom->id);
        for (; pindex; pindex = chainactive.next(pindex))
        {
            if (pindex->getblockhash() == hashstop)
            {
                logprint("net", "  getblocks stopping at %d %s\n", pindex->nheight, pindex->getblockhash().tostring());
                break;
            }
            pfrom->pushinventory(cinv(msg_block, pindex->getblockhash()));
            if (--nlimit <= 0)
            {
                // when this block is requested, we'll send an inv that'll
                // trigger the peer to getblocks the next batch of inventory.
                logprint("net", "  getblocks stopping at limit %d %s\n", pindex->nheight, pindex->getblockhash().tostring());
                pfrom->hashcontinue = pindex->getblockhash();
                break;
            }
        }
    }


    else if (strcommand == "getheaders")
    {
        cblocklocator locator;
        uint256 hashstop;
        vrecv >> locator >> hashstop;

        lock(cs_main);

        if (isinitialblockdownload())
            return true;

        cblockindex* pindex = null;
        if (locator.isnull())
        {
            // if locator is null, return the hashstop block
            blockmap::iterator mi = mapblockindex.find(hashstop);
            if (mi == mapblockindex.end())
                return true;
            pindex = (*mi).second;
        }
        else
        {
            // find the last block the caller has in the main chain
            pindex = findforkinglobalindex(chainactive, locator);
            if (pindex)
                pindex = chainactive.next(pindex);
        }

        // we must use cblocks, as cblockheaders won't include the 0x00 ntx count at the end
        vector<cblock> vheaders;
        int nlimit = max_headers_results;
        logprint("net", "getheaders %d to %s from peer=%d\n", (pindex ? pindex->nheight : -1), hashstop.tostring(), pfrom->id);
        for (; pindex; pindex = chainactive.next(pindex))
        {
            vheaders.push_back(pindex->getblockheader());
            if (--nlimit <= 0 || pindex->getblockhash() == hashstop)
                break;
        }
        pfrom->pushmessage("headers", vheaders);
    }


    else if (strcommand == "tx")
    {
        vector<uint256> vworkqueue;
        vector<uint256> verasequeue;
        ctransaction tx;
        vrecv >> tx;

        cinv inv(msg_tx, tx.gethash());
        pfrom->addinventoryknown(inv);

        lock(cs_main);

        bool fmissinginputs = false;
        cvalidationstate state;

        mapalreadyaskedfor.erase(inv);

        if (accepttomemorypool(mempool, state, tx, true, &fmissinginputs))
        {
            mempool.check(pcoinstip);
            relaytransaction(tx);
            vworkqueue.push_back(inv.hash);

            logprint("mempool", "accepttomemorypool: peer=%d %s: accepted %s (poolsz %u)\n",
                pfrom->id, pfrom->cleansubver,
                tx.gethash().tostring(),
                mempool.maptx.size());

            // recursively process any orphan transactions that depended on this one
            set<nodeid> setmisbehaving;
            for (unsigned int i = 0; i < vworkqueue.size(); i++)
            {
                map<uint256, set<uint256> >::iterator itbyprev = maporphantransactionsbyprev.find(vworkqueue[i]);
                if (itbyprev == maporphantransactionsbyprev.end())
                    continue;
                for (set<uint256>::iterator mi = itbyprev->second.begin();
                     mi != itbyprev->second.end();
                     ++mi)
                {
                    const uint256& orphanhash = *mi;
                    const ctransaction& orphantx = maporphantransactions[orphanhash].tx;
                    nodeid frompeer = maporphantransactions[orphanhash].frompeer;
                    bool fmissinginputs2 = false;
                    // use a dummy cvalidationstate so someone can't setup nodes to counter-dos based on orphan
                    // resolution (that is, feeding people an invalid transaction based on legittxx in order to get
                    // anyone relaying legittxx banned)
                    cvalidationstate statedummy;


                    if (setmisbehaving.count(frompeer))
                        continue;
                    if (accepttomemorypool(mempool, statedummy, orphantx, true, &fmissinginputs2))
                    {
                        logprint("mempool", "   accepted orphan tx %s\n", orphanhash.tostring());
                        relaytransaction(orphantx);
                        vworkqueue.push_back(orphanhash);
                        verasequeue.push_back(orphanhash);
                    }
                    else if (!fmissinginputs2)
                    {
                        int ndos = 0;
                        if (statedummy.isinvalid(ndos) && ndos > 0)
                        {
                            // punish peer that gave us an invalid orphan tx
                            misbehaving(frompeer, ndos);
                            setmisbehaving.insert(frompeer);
                            logprint("mempool", "   invalid orphan tx %s\n", orphanhash.tostring());
                        }
                        // has inputs but not accepted to mempool
                        // probably non-standard or insufficient fee/priority
                        logprint("mempool", "   removed orphan tx %s\n", orphanhash.tostring());
                        verasequeue.push_back(orphanhash);
                    }
                    mempool.check(pcoinstip);
                }
            }

            boost_foreach(uint256 hash, verasequeue)
                eraseorphantx(hash);
        }
        else if (fmissinginputs)
        {
            addorphantx(tx, pfrom->getid());

            // dos prevention: do not allow maporphantransactions to grow unbounded
            unsigned int nmaxorphantx = (unsigned int)std::max((int64_t)0, getarg("-maxorphantx", default_max_orphan_transactions));
            unsigned int nevicted = limitorphantxsize(nmaxorphantx);
            if (nevicted > 0)
                logprint("mempool", "maporphan overflow, removed %u tx\n", nevicted);
        } else if (pfrom->fwhitelisted) {
            // always relay transactions received from whitelisted peers, even
            // if they are already in the mempool (allowing the node to function
            // as a gateway for nodes hidden behind it).
            relaytransaction(tx);
        }
        int ndos = 0;
        if (state.isinvalid(ndos))
        {
            logprint("mempool", "%s from peer=%d %s was not accepted into the memory pool: %s\n", tx.gethash().tostring(),
                pfrom->id, pfrom->cleansubver,
                state.getrejectreason());
            pfrom->pushmessage("reject", strcommand, state.getrejectcode(),
                               state.getrejectreason().substr(0, max_reject_message_length), inv.hash);
            if (ndos > 0)
                misbehaving(pfrom->getid(), ndos);
        }
    }


    else if (strcommand == "headers" && !fimporting && !freindex) // ignore headers received while importing
    {
        std::vector<cblockheader> headers;

        // bypass the normal cblock deserialization, as we don't want to risk deserializing 2000 full blocks.
        unsigned int ncount = readcompactsize(vrecv);
        if (ncount > max_headers_results) {
            misbehaving(pfrom->getid(), 20);
            return error("headers message size = %u", ncount);
        }
        headers.resize(ncount);
        for (unsigned int n = 0; n < ncount; n++) {
            vrecv >> headers[n];
            readcompactsize(vrecv); // ignore tx count; assume it is 0.
        }

        lock(cs_main);

        if (ncount == 0) {
            // nothing interesting. stop asking this peers for more headers.
            return true;
        }

        cblockindex *pindexlast = null;
        boost_foreach(const cblockheader& header, headers) {
            cvalidationstate state;
            if (pindexlast != null && header.hashprevblock != pindexlast->getblockhash()) {
                misbehaving(pfrom->getid(), 20);
                return error("non-continuous headers sequence");
            }
            if (!acceptblockheader(header, state, &pindexlast)) {
                int ndos;
                if (state.isinvalid(ndos)) {
                    if (ndos > 0)
                        misbehaving(pfrom->getid(), ndos);
                    return error("invalid header received");
                }
            }
        }

        if (pindexlast)
            updateblockavailability(pfrom->getid(), pindexlast->getblockhash());

        if (ncount == max_headers_results && pindexlast) {
            // headers message had its maximum size; the peer may have more headers.
            // todo: optimize: if pindexlast is an ancestor of chainactive.tip or pindexbestheader, continue
            // from there instead.
            logprint("net", "more getheaders (%d) to end to peer=%d (startheight:%d)\n", pindexlast->nheight, pfrom->id, pfrom->nstartingheight);
            pfrom->pushmessage("getheaders", chainactive.getlocator(pindexlast), uint256());
        }

        checkblockindex();
    }

    else if (strcommand == "block" && !fimporting && !freindex) // ignore blocks received while importing
    {
        cblock block;
        vrecv >> block;

        cinv inv(msg_block, block.gethash());
        logprint("net", "received block %s peer=%d\n", inv.hash.tostring(), pfrom->id);

        pfrom->addinventoryknown(inv);

        cvalidationstate state;
        // process all blocks from whitelisted peers, even if not requested.
        processnewblock(state, pfrom, &block, pfrom->fwhitelisted, null);
        int ndos;
        if (state.isinvalid(ndos)) {
            pfrom->pushmessage("reject", strcommand, state.getrejectcode(),
                               state.getrejectreason().substr(0, max_reject_message_length), inv.hash);
            if (ndos > 0) {
                lock(cs_main);
                misbehaving(pfrom->getid(), ndos);
            }
        }

    }


    // this asymmetric behavior for inbound and outbound connections was introduced
    // to prevent a fingerprinting attack: an attacker can send specific fake addresses
    // to users' addrman and later request them by sending getaddr messages.
    // making nodes which are behind nat and can only make outgoing connections ignore
    // the getaddr message mitigates the attack.
    else if ((strcommand == "getaddr") && (pfrom->finbound))
    {
        pfrom->vaddrtosend.clear();
        vector<caddress> vaddr = addrman.getaddr();
        boost_foreach(const caddress &addr, vaddr)
            pfrom->pushaddress(addr);
    }


    else if (strcommand == "mempool")
    {
        lock2(cs_main, pfrom->cs_filter);

        std::vector<uint256> vtxid;
        mempool.queryhashes(vtxid);
        vector<cinv> vinv;
        boost_foreach(uint256& hash, vtxid) {
            cinv inv(msg_tx, hash);
            ctransaction tx;
            bool finmempool = mempool.lookup(hash, tx);
            if (!finmempool) continue; // another thread removed since queryhashes, maybe...
            if ((pfrom->pfilter && pfrom->pfilter->isrelevantandupdate(tx)) ||
               (!pfrom->pfilter))
                vinv.push_back(inv);
            if (vinv.size() == max_inv_sz) {
                pfrom->pushmessage("inv", vinv);
                vinv.clear();
            }
        }
        if (vinv.size() > 0)
            pfrom->pushmessage("inv", vinv);
    }


    else if (strcommand == "ping")
    {
        if (pfrom->nversion > bip0031_version)
        {
            uint64_t nonce = 0;
            vrecv >> nonce;
            // echo the message back with the nonce. this allows for two useful features:
            //
            // 1) a remote node can quickly check if the connection is operational
            // 2) remote nodes can measure the latency of the network thread. if this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // the nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->pushmessage("pong", nonce);
        }
    }


    else if (strcommand == "pong")
    {
        int64_t pingusecend = ntimereceived;
        uint64_t nonce = 0;
        size_t navail = vrecv.in_avail();
        bool bpingfinished = false;
        std::string sproblem;

        if (navail >= sizeof(nonce)) {
            vrecv >> nonce;

            // only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->npingnoncesent != 0) {
                if (nonce == pfrom->npingnoncesent) {
                    // matching pong received, this ping is no longer outstanding
                    bpingfinished = true;
                    int64_t pingusectime = pingusecend - pfrom->npingusecstart;
                    if (pingusectime > 0) {
                        // successful ping time measurement, replace previous
                        pfrom->npingusectime = pingusectime;
                    } else {
                        // this should never happen
                        sproblem = "timing mishap";
                    }
                } else {
                    // nonce mismatches are normal when pings are overlapping
                    sproblem = "nonce mismatch";
                    if (nonce == 0) {
                        // this is most likely a bug in another implementation somewhere; cancel this ping
                        bpingfinished = true;
                        sproblem = "nonce zero";
                    }
                }
            } else {
                sproblem = "unsolicited pong without ping";
            }
        } else {
            // this is most likely a bug in another implementation somewhere; cancel this ping
            bpingfinished = true;
            sproblem = "short payload";
        }

        if (!(sproblem.empty())) {
            logprint("net", "pong peer=%d %s: %s, %x expected, %x received, %u bytes\n",
                pfrom->id,
                pfrom->cleansubver,
                sproblem,
                pfrom->npingnoncesent,
                nonce,
                navail);
        }
        if (bpingfinished) {
            pfrom->npingnoncesent = 0;
        }
    }


    else if (falerts && strcommand == "alert")
    {
        calert alert;
        vrecv >> alert;

        uint256 alerthash = alert.gethash();
        if (pfrom->setknown.count(alerthash) == 0)
        {
            if (alert.processalert(params().alertkey()))
            {
                // relay
                pfrom->setknown.insert(alerthash);
                {
                    lock(cs_vnodes);
                    boost_foreach(cnode* pnode, vnodes)
                        alert.relayto(pnode);
                }
            }
            else {
                // small dos penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // this isn't a misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                misbehaving(pfrom->getid(), 10);
            }
        }
    }


    else if (strcommand == "filterload")
    {
        cbloomfilter filter;
        vrecv >> filter;

        if (!filter.iswithinsizeconstraints())
            // there is no excuse for sending a too-large filter
            misbehaving(pfrom->getid(), 100);
        else
        {
            lock(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new cbloomfilter(filter);
            pfrom->pfilter->updateemptyfull();
        }
        pfrom->frelaytxes = true;
    }


    else if (strcommand == "filteradd")
    {
        vector<unsigned char> vdata;
        vrecv >> vdata;

        // nodes must never send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (vdata.size() > max_script_element_size)
        {
            misbehaving(pfrom->getid(), 100);
        } else {
            lock(pfrom->cs_filter);
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vdata);
            else
                misbehaving(pfrom->getid(), 100);
        }
    }


    else if (strcommand == "filterclear")
    {
        lock(pfrom->cs_filter);
        delete pfrom->pfilter;
        pfrom->pfilter = new cbloomfilter();
        pfrom->frelaytxes = true;
    }


    else if (strcommand == "reject")
    {
        if (fdebug) {
            try {
                string strmsg; unsigned char ccode; string strreason;
                vrecv >> limited_string(strmsg, cmessageheader::command_size) >> ccode >> limited_string(strreason, max_reject_message_length);

                ostringstream ss;
                ss << strmsg << " code " << itostr(ccode) << ": " << strreason;

                if (strmsg == "block" || strmsg == "tx")
                {
                    uint256 hash;
                    vrecv >> hash;
                    ss << ": hash " << hash.tostring();
                }
                logprint("net", "reject %s\n", sanitizestring(ss.str()));
            } catch (const std::ios_base::failure&) {
                // avoid feedback loops by preventing reject messages from triggering a new reject message.
                logprint("net", "unparseable reject message received\n");
            }
        }
    }

    else
    {
        // ignore unknown commands for extensibility
        logprint("net", "unknown command \"%s\" from peer=%d\n", sanitizestring(strcommand), pfrom->id);
    }



    return true;
}

// requires lock(cs_vrecvmsg)
bool processmessages(cnode* pfrom)
{
    //if (fdebug)
    //    logprintf("%s(%u messages)\n", __func__, pfrom->vrecvmsg.size());

    //
    // message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fok = true;

    if (!pfrom->vrecvgetdata.empty())
        processgetdata(pfrom);

    // this maintains the order of responses
    if (!pfrom->vrecvgetdata.empty()) return fok;

    std::deque<cnetmessage>::iterator it = pfrom->vrecvmsg.begin();
    while (!pfrom->fdisconnect && it != pfrom->vrecvmsg.end()) {
        // don't bother if send buffer is too full to respond anyway
        if (pfrom->nsendsize >= sendbuffersize())
            break;

        // get next message
        cnetmessage& msg = *it;

        //if (fdebug)
        //    logprintf("%s(message %u msgsz, %u bytes, complete:%s)\n", __func__,
        //            msg.hdr.nmessagesize, msg.vrecv.size(),
        //            msg.complete() ? "y" : "n");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // scan for message start
        if (memcmp(msg.hdr.pchmessagestart, params().messagestart(), message_start_size) != 0) {
            logprintf("processmessage: invalid messagestart %s peer=%d\n", sanitizestring(msg.hdr.getcommand()), pfrom->id);
            fok = false;
            break;
        }

        // read header
        cmessageheader& hdr = msg.hdr;
        if (!hdr.isvalid(params().messagestart()))
        {
            logprintf("processmessage: errors in header %s peer=%d\n", sanitizestring(hdr.getcommand()), pfrom->id);
            continue;
        }
        string strcommand = hdr.getcommand();

        // message size
        unsigned int nmessagesize = hdr.nmessagesize;

        // checksum
        cdatastream& vrecv = msg.vrecv;
        uint256 hash = hash(vrecv.begin(), vrecv.begin() + nmessagesize);
        unsigned int nchecksum = readle32((unsigned char*)&hash);
        if (nchecksum != hdr.nchecksum)
        {
            logprintf("%s(%s, %u bytes): checksum error nchecksum=%08x hdr.nchecksum=%08x\n", __func__,
               sanitizestring(strcommand), nmessagesize, nchecksum, hdr.nchecksum);
            continue;
        }

        // process message
        bool fret = false;
        try
        {
            fret = processmessage(pfrom, strcommand, vrecv, msg.ntime);
            boost::this_thread::interruption_point();
        }
        catch (const std::ios_base::failure& e)
        {
            pfrom->pushmessage("reject", strcommand, reject_malformed, string("error parsing message"));
            if (strstr(e.what(), "end of data"))
            {
                // allow exceptions from under-length message on vrecv
                logprintf("%s(%s, %u bytes): exception '%s' caught, normally caused by a message being shorter than its stated length\n", __func__, sanitizestring(strcommand), nmessagesize, e.what());
            }
            else if (strstr(e.what(), "size too large"))
            {
                // allow exceptions from over-long size
                logprintf("%s(%s, %u bytes): exception '%s' caught\n", __func__, sanitizestring(strcommand), nmessagesize, e.what());
            }
            else
            {
                printexceptioncontinue(&e, "processmessages()");
            }
        }
        catch (const boost::thread_interrupted&) {
            throw;
        }
        catch (const std::exception& e) {
            printexceptioncontinue(&e, "processmessages()");
        } catch (...) {
            printexceptioncontinue(null, "processmessages()");
        }

        if (!fret)
            logprintf("%s(%s, %u bytes) failed peer=%d\n", __func__, sanitizestring(strcommand), nmessagesize, pfrom->id);

        break;
    }

    // in case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fdisconnect)
        pfrom->vrecvmsg.erase(pfrom->vrecvmsg.begin(), it);

    return fok;
}


bool sendmessages(cnode* pto, bool fsendtrickle)
{
    const consensus::params& consensusparams = params().getconsensus();
    {
        // don't send anything until we get its version message
        if (pto->nversion == 0)
            return true;

        //
        // message: ping
        //
        bool pingsend = false;
        if (pto->fpingqueued) {
            // rpc ping request by user
            pingsend = true;
        }
        if (pto->npingnoncesent == 0 && pto->npingusecstart + ping_interval * 1000000 < gettimemicros()) {
            // ping automatically sent as a latency probe & keepalive.
            pingsend = true;
        }
        if (pingsend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                getrandbytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fpingqueued = false;
            pto->npingusecstart = gettimemicros();
            if (pto->nversion > bip0031_version) {
                pto->npingnoncesent = nonce;
                pto->pushmessage("ping", nonce);
            } else {
                // peer is too old to support ping command with nonce, pong will never arrive.
                pto->npingnoncesent = 0;
                pto->pushmessage("ping");
            }
        }

        try_lock(cs_main, lockmain); // acquire cs_main for isinitialblockdownload() and cnodestate()
        if (!lockmain)
            return true;

        // address refresh broadcast
        static int64_t nlastrebroadcast;
        if (!isinitialblockdownload() && (gettime() - nlastrebroadcast > 24 * 60 * 60))
        {
            lock(cs_vnodes);
            boost_foreach(cnode* pnode, vnodes)
            {
                // periodically clear addrknown to allow refresh broadcasts
                if (nlastrebroadcast)
                    pnode->addrknown.clear();

                // rebroadcast our address
                advertizelocal(pnode);
            }
            if (!vnodes.empty())
                nlastrebroadcast = gettime();
        }

        //
        // message: addr
        //
        if (fsendtrickle)
        {
            vector<caddress> vaddr;
            vaddr.reserve(pto->vaddrtosend.size());
            boost_foreach(const caddress& addr, pto->vaddrtosend)
            {
                if (!pto->addrknown.contains(addr.getkey()))
                {
                    pto->addrknown.insert(addr.getkey());
                    vaddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vaddr.size() >= 1000)
                    {
                        pto->pushmessage("addr", vaddr);
                        vaddr.clear();
                    }
                }
            }
            pto->vaddrtosend.clear();
            if (!vaddr.empty())
                pto->pushmessage("addr", vaddr);
        }

        cnodestate &state = *state(pto->getid());
        if (state.fshouldban) {
            if (pto->fwhitelisted)
                logprintf("warning: not punishing whitelisted peer %s!\n", pto->addr.tostring());
            else {
                pto->fdisconnect = true;
                if (pto->addr.islocal())
                    logprintf("warning: not banning local peer %s!\n", pto->addr.tostring());
                else
                {
                    cnode::ban(pto->addr);
                }
            }
            state.fshouldban = false;
        }

        boost_foreach(const cblockreject& reject, state.rejects)
            pto->pushmessage("reject", (string)"block", reject.chrejectcode, reject.strrejectreason, reject.hashblock);
        state.rejects.clear();

        // start block sync
        if (pindexbestheader == null)
            pindexbestheader = chainactive.tip();
        bool ffetch = state.fpreferreddownload || (npreferreddownload == 0 && !pto->fclient && !pto->foneshot); // download if this is a nice peer, or we have no nice peers and this one might do.
        if (!state.fsyncstarted && !pto->fclient && !fimporting && !freindex) {
            // only actively request headers from a single peer, unless we're close to today.
            if ((nsyncstarted == 0 && ffetch) || pindexbestheader->getblocktime() > getadjustedtime() - 24 * 60 * 60) {
                state.fsyncstarted = true;
                nsyncstarted++;
                cblockindex *pindexstart = pindexbestheader->pprev ? pindexbestheader->pprev : pindexbestheader;
                logprint("net", "initial getheaders (%d) to peer=%d (startheight:%d)\n", pindexstart->nheight, pto->id, pto->nstartingheight);
                pto->pushmessage("getheaders", chainactive.getlocator(pindexstart), uint256());
            }
        }

        // resend wallet transactions that haven't gotten in a block yet
        // except during reindex, importing and ibd, when old wallet
        // transactions become unconfirmed and spams other nodes.
        if (!freindex && !fimporting && !isinitialblockdownload())
        {
            getmainsignals().broadcast(ntimebestreceived);
        }

        //
        // message: inventory
        //
        vector<cinv> vinv;
        vector<cinv> vinvwait;
        {
            lock(pto->cs_inventory);
            vinv.reserve(pto->vinventorytosend.size());
            vinvwait.reserve(pto->vinventorytosend.size());
            boost_foreach(const cinv& inv, pto->vinventorytosend)
            {
                if (pto->setinventoryknown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == msg_tx && !fsendtrickle)
                {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashsalt;
                    if (hashsalt.isnull())
                        hashsalt = getrandhash();
                    uint256 hashrand = arithtouint256(uinttoarith256(inv.hash) ^ uinttoarith256(hashsalt));
                    hashrand = hash(begin(hashrand), end(hashrand));
                    bool ftricklewait = ((uinttoarith256(hashrand) & 3) != 0);

                    if (ftricklewait)
                    {
                        vinvwait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setinventoryknown.insert(inv).second)
                {
                    vinv.push_back(inv);
                    if (vinv.size() >= 1000)
                    {
                        pto->pushmessage("inv", vinv);
                        vinv.clear();
                    }
                }
            }
            pto->vinventorytosend = vinvwait;
        }
        if (!vinv.empty())
            pto->pushmessage("inv", vinv);

        // detect whether we're stalling
        int64_t nnow = gettimemicros();
        if (!pto->fdisconnect && state.nstallingsince && state.nstallingsince < nnow - 1000000 * block_stalling_timeout) {
            // stalling only triggers when the block download window cannot move. during normal steady state,
            // the download window should be much larger than the to-be-downloaded set of blocks, so disconnection
            // should only happen during initial block download.
            logprintf("peer=%d is stalling block download, disconnecting\n", pto->id);
            pto->fdisconnect = true;
        }
        // in case there is a block that has been in flight from this peer for (2 + 0.5 * n) times the block interval
        // (with n the number of validated blocks that were in flight at the time it was requested), disconnect due to
        // timeout. we compensate for in-flight blocks to prevent killing off peers due to our own downstream link
        // being saturated. we only count validated in-flight blocks so peers can't advertise non-existing block hashes
        // to unreasonably increase our timeout.
        // we also compare the block download timeout originally calculated against the time at which we'd disconnect
        // if we assumed the block were being requested now (ignoring blocks we've requested from this peer, since we're
        // only looking at this peer's oldest request).  this way a large queue in the past doesn't result in a
        // permanently large window for this block to be delivered (ie if the number of blocks in flight is decreasing
        // more quickly than once every 5 minutes, then we'll shorten the download window for this block).
        if (!pto->fdisconnect && state.vblocksinflight.size() > 0) {
            queuedblock &queuedblock = state.vblocksinflight.front();
            int64_t ntimeoutifrequestednow = getblocktimeout(nnow, nqueuedvalidatedheaders - state.nblocksinflightvalidheaders, consensusparams);
            if (queuedblock.ntimedisconnect > ntimeoutifrequestednow) {
                logprint("net", "reducing block download timeout for peer=%d block=%s, orig=%d new=%d\n", pto->id, queuedblock.hash.tostring(), queuedblock.ntimedisconnect, ntimeoutifrequestednow);
                queuedblock.ntimedisconnect = ntimeoutifrequestednow;
            }
            if (queuedblock.ntimedisconnect < nnow) {
                logprintf("timeout downloading block %s from peer=%d, disconnecting\n", queuedblock.hash.tostring(), pto->id);
                pto->fdisconnect = true;
            }
        }

        //
        // message: getdata (blocks)
        //
        vector<cinv> vgetdata;
        if (!pto->fdisconnect && !pto->fclient && (ffetch || !isinitialblockdownload()) && state.nblocksinflight < max_blocks_in_transit_per_peer) {
            vector<cblockindex*> vtodownload;
            nodeid staller = -1;
            findnextblockstodownload(pto->getid(), max_blocks_in_transit_per_peer - state.nblocksinflight, vtodownload, staller);
            boost_foreach(cblockindex *pindex, vtodownload) {
                vgetdata.push_back(cinv(msg_block, pindex->getblockhash()));
                markblockasinflight(pto->getid(), pindex->getblockhash(), consensusparams, pindex);
                logprint("net", "requesting block %s (%d) peer=%d\n", pindex->getblockhash().tostring(),
                    pindex->nheight, pto->id);
            }
            if (state.nblocksinflight == 0 && staller != -1) {
                if (state(staller)->nstallingsince == 0) {
                    state(staller)->nstallingsince = nnow;
                    logprint("net", "stall started peer=%d\n", staller);
                }
            }
        }

        //
        // message: getdata (non-blocks)
        //
        while (!pto->fdisconnect && !pto->mapaskfor.empty() && (*pto->mapaskfor.begin()).first <= nnow)
        {
            const cinv& inv = (*pto->mapaskfor.begin()).second;
            if (!alreadyhave(inv))
            {
                if (fdebug)
                    logprint("net", "requesting %s peer=%d\n", inv.tostring(), pto->id);
                vgetdata.push_back(inv);
                if (vgetdata.size() >= 1000)
                {
                    pto->pushmessage("getdata", vgetdata);
                    vgetdata.clear();
                }
            }
            pto->mapaskfor.erase(pto->mapaskfor.begin());
        }
        if (!vgetdata.empty())
            pto->pushmessage("getdata", vgetdata);

    }
    return true;
}

 std::string cblockfileinfo::tostring() const {
     return strprintf("cblockfileinfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nblocks, nsize, nheightfirst, nheightlast, datetimestrformat("%y-%m-%d", ntimefirst), datetimestrformat("%y-%m-%d", ntimelast));
 }



class cmaincleanup
{
public:
    cmaincleanup() {}
    ~cmaincleanup() {
        // block headers
        blockmap::iterator it1 = mapblockindex.begin();
        for (; it1 != mapblockindex.end(); it1++)
            delete (*it1).second;
        mapblockindex.clear();

        // orphan transactions
        maporphantransactions.clear();
        maporphantransactionsbyprev.clear();
    }
} instance_of_cmaincleanup;
