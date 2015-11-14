// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "init.h"

#include "addrman.h"
#include "amount.h"
#include "checkpoints.h"
#include "compat/sanity.h"
#include "consensus/validation.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "rpcserver.h"
#include "script/standard.h"
#include "scheduler.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#ifdef enable_wallet
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <stdint.h>
#include <stdio.h>

#ifndef win32
#include <signal.h>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>

using namespace std;

#ifdef enable_wallet
cwallet* pwalletmain = null;
#endif
bool ffeeestimatesinitialized = false;

#ifdef win32
// win32 leveldb doesn't use filedescriptors, and the ones used for
// accessing block files don't count towards the fd_set size limit
// anyway.
#define min_core_filedescriptors 0
#else
#define min_core_filedescriptors 150
#endif

/** used to pass flags to the bind() function */
enum bindflags {
    bf_none         = 0,
    bf_explicit     = (1u << 0),
    bf_report_error = (1u << 1),
    bf_whitelist    = (1u << 2),
};

static const char* fee_estimates_filename="fee_estimates.dat";
cclientuiinterface uiinterface; // declared but not defined in ui_interface.h

//////////////////////////////////////////////////////////////////////////////
//
// shutdown
//

//
// thread management and startup/shutdown:
//
// the network-processing threads are all part of a thread group
// created by appinit() or the qt main() function.
//
// a clean exit happens when startshutdown() or the sigterm
// signal handler sets frequestshutdown, which triggers
// the detectshutdownthread(), which interrupts the main thread group.
// detectshutdownthread() then exits, which causes appinit() to
// continue (it .joins the shutdown thread).
// shutdown() is then
// called to clean up database connections, and stop other
// threads that should only be stopped after the main network-processing
// threads have exited.
//
// note that if running -daemon the parent process returns from appinit2
// before adding any threads to the threadgroup, so .join_all() returns
// immediately and the parent exits from main().
//
// shutdown for qt is very similar, only it uses a qtimer to detect
// frequestshutdown getting set, and then does the normal qt
// shutdown thing.
//

volatile bool frequestshutdown = false;

void startshutdown()
{
    frequestshutdown = true;
}
bool shutdownrequested()
{
    return frequestshutdown;
}

class ccoinsviewerrorcatcher : public ccoinsviewbacked
{
public:
    ccoinsviewerrorcatcher(ccoinsview* view) : ccoinsviewbacked(view) {}
    bool getcoins(const uint256 &txid, ccoins &coins) const {
        try {
            return ccoinsviewbacked::getcoins(txid, coins);
        } catch(const std::runtime_error& e) {
            uiinterface.threadsafemessagebox(_("error reading from database, shutting down."), "", cclientuiinterface::msg_error);
            logprintf("error reading from database: %s\n", e.what());
            // starting the shutdown sequence and returning false to the caller would be
            // interpreted as 'entry not found' (as opposed to unable to read data), and
            // could lead to invalid interpretation. just exit immediately, as we can't
            // continue anyway, and all writes should be atomic.
            abort();
        }
    }
    // writes do not need similar protection, as failure to write is handled by the caller.
};

static ccoinsviewdb *pcoinsdbview = null;
static ccoinsviewerrorcatcher *pcoinscatcher = null;

void shutdown()
{
    logprintf("%s: in progress...\n", __func__);
    static ccriticalsection cs_shutdown;
    try_lock(cs_shutdown, lockshutdown);
    if (!lockshutdown)
        return;

    /// note: shutdown() must be able to handle cases in which appinit2() failed part of the way,
    /// for example if the data directory was found to be locked.
    /// be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    renamethread("moorecoin-shutoff");
    mempool.addtransactionsupdated(1);
    stoprpcthreads();
#ifdef enable_wallet
    if (pwalletmain)
        pwalletmain->flush(false);
    generatemoorecoins(false, null, 0);
#endif
    stopnode();
    unregisternodesignals(getnodesignals());

    if (ffeeestimatesinitialized)
    {
        boost::filesystem::path est_path = getdatadir() / fee_estimates_filename;
        cautofile est_fileout(fopen(est_path.string().c_str(), "wb"), ser_disk, client_version);
        if (!est_fileout.isnull())
            mempool.writefeeestimates(est_fileout);
        else
            logprintf("%s: failed to write fee estimates to %s\n", __func__, est_path.string());
        ffeeestimatesinitialized = false;
    }

    {
        lock(cs_main);
        if (pcoinstip != null) {
            flushstatetodisk();
        }
        delete pcoinstip;
        pcoinstip = null;
        delete pcoinscatcher;
        pcoinscatcher = null;
        delete pcoinsdbview;
        pcoinsdbview = null;
        delete pblocktree;
        pblocktree = null;
    }
#ifdef enable_wallet
    if (pwalletmain)
        pwalletmain->flush(true);
#endif
#ifndef win32
    try {
        boost::filesystem::remove(getpidfile());
    } catch (const boost::filesystem::filesystem_error& e) {
        logprintf("%s: unable to remove pidfile: %s\n", __func__, e.what());
    }
#endif
    unregisterallvalidationinterfaces();
#ifdef enable_wallet
    delete pwalletmain;
    pwalletmain = null;
#endif
    ecc_stop();
    logprintf("%s: done\n", __func__);
}

/**
 * signal handlers are very limited in what they are allowed to do, so:
 */
void handlesigterm(int)
{
    frequestshutdown = true;
}

void handlesighup(int)
{
    freopendebuglog = true;
}

bool static initerror(const std::string &str)
{
    uiinterface.threadsafemessagebox(str, "", cclientuiinterface::msg_error);
    return false;
}

bool static initwarning(const std::string &str)
{
    uiinterface.threadsafemessagebox(str, "", cclientuiinterface::msg_warning);
    return true;
}

bool static bind(const cservice &addr, unsigned int flags) {
    if (!(flags & bf_explicit) && islimited(addr))
        return false;
    std::string strerror;
    if (!bindlistenport(addr, strerror, (flags & bf_whitelist) != 0)) {
        if (flags & bf_report_error)
            return initerror(strerror);
        return false;
    }
    return true;
}

void onrpcstopped()
{
    cvblockchange.notify_all();
    logprint("rpc", "rpc stopped.\n");
}

void onrpcprecommand(const crpccommand& cmd)
{
    // observe safe mode
    string strwarning = getwarnings("rpc");
    if (strwarning != "" && !getboolarg("-disablesafemode", false) &&
        !cmd.oksafemode)
        throw jsonrpcerror(rpc_forbidden_by_safe_mode, string("safe mode: ") + strwarning);
}

std::string helpmessage(helpmessagemode mode)
{
    const bool showdebug = getboolarg("-help-debug", false);

    // when adding new options to the categories, please keep and ensure alphabetical ordering.
    // do not translate _(...) -help-debug options, many technical terms, and only a very small audience, so is unnecessary stress to translators.
    string strusage = helpmessagegroup(_("options:"));
    strusage += helpmessageopt("-?", _("this help message"));
    strusage += helpmessageopt("-alerts", strprintf(_("receive and display p2p network alerts (default: %u)"), default_alerts));
    strusage += helpmessageopt("-alertnotify=<cmd>", _("execute command when a relevant alert is received or we see a really long fork (%s in cmd is replaced by message)"));
    strusage += helpmessageopt("-blocknotify=<cmd>", _("execute command when the best block changes (%s in cmd is replaced by block hash)"));
    strusage += helpmessageopt("-checkblocks=<n>", strprintf(_("how many blocks to check at startup (default: %u, 0 = all)"), 288));
    strusage += helpmessageopt("-checklevel=<n>", strprintf(_("how thorough the block verification of -checkblocks is (0-4, default: %u)"), 3));
    strusage += helpmessageopt("-conf=<file>", strprintf(_("specify configuration file (default: %s)"), "moorecoin.conf"));
    if (mode == hmm_moorecoind)
    {
#if !defined(win32)
        strusage += helpmessageopt("-daemon", _("run in the background as a daemon and accept commands"));
#endif
    }
    strusage += helpmessageopt("-datadir=<dir>", _("specify data directory"));
    strusage += helpmessageopt("-dbcache=<n>", strprintf(_("set database cache size in megabytes (%d to %d, default: %d)"), nmindbcache, nmaxdbcache, ndefaultdbcache));
    strusage += helpmessageopt("-loadblock=<file>", _("imports blocks from external blk000??.dat file") + " " + _("on startup"));
    strusage += helpmessageopt("-maxorphantx=<n>", strprintf(_("keep at most <n> unconnectable transactions in memory (default: %u)"), default_max_orphan_transactions));
    strusage += helpmessageopt("-par=<n>", strprintf(_("set the number of script verification threads (%u to %d, 0 = auto, <0 = leave that many cores free, default: %d)"),
        -(int)boost::thread::hardware_concurrency(), max_scriptcheck_threads, default_scriptcheck_threads));
#ifndef win32
    strusage += helpmessageopt("-pid=<file>", strprintf(_("specify pid file (default: %s)"), "moorecoind.pid"));
#endif
    strusage += helpmessageopt("-prune=<n>", strprintf(_("reduce storage requirements by pruning (deleting) old blocks. this mode disables wallet support and is incompatible with -txindex. "
            "warning: reverting this setting requires re-downloading the entire blockchain. "
            "(default: 0 = disable pruning blocks, >%u = target size in mib to use for block files)"), min_disk_space_for_block_files / 1024 / 1024));
    strusage += helpmessageopt("-reindex", _("rebuild block chain index from current blk000??.dat files on startup"));
#if !defined(win32)
    strusage += helpmessageopt("-sysperms", _("create new files with system default permissions, instead of umask 077 (only effective with disabled wallet functionality)"));
#endif
    strusage += helpmessageopt("-txindex", strprintf(_("maintain a full transaction index, used by the getrawtransaction rpc call (default: %u)"), 0));

    strusage += helpmessagegroup(_("connection options:"));
    strusage += helpmessageopt("-addnode=<ip>", _("add a node to connect to and attempt to keep the connection open"));
    strusage += helpmessageopt("-banscore=<n>", strprintf(_("threshold for disconnecting misbehaving peers (default: %u)"), 100));
    strusage += helpmessageopt("-bantime=<n>", strprintf(_("number of seconds to keep misbehaving peers from reconnecting (default: %u)"), 86400));
    strusage += helpmessageopt("-bind=<addr>", _("bind to given address and always listen on it. use [host]:port notation for ipv6"));
    strusage += helpmessageopt("-connect=<ip>", _("connect only to the specified node(s)"));
    strusage += helpmessageopt("-discover", _("discover own ip addresses (default: 1 when listening and no -externalip or -proxy)"));
    strusage += helpmessageopt("-dns", _("allow dns lookups for -addnode, -seednode and -connect") + " " + _("(default: 1)"));
    strusage += helpmessageopt("-dnsseed", _("query for peer addresses via dns lookup, if low on addresses (default: 1 unless -connect)"));
    strusage += helpmessageopt("-externalip=<ip>", _("specify your own public address"));
    strusage += helpmessageopt("-forcednsseed", strprintf(_("always query for peer addresses via dns lookup (default: %u)"), 0));
    strusage += helpmessageopt("-listen", _("accept connections from outside (default: 1 if no -proxy or -connect)"));
    strusage += helpmessageopt("-maxconnections=<n>", strprintf(_("maintain at most <n> connections to peers (default: %u)"), 125));
    strusage += helpmessageopt("-maxreceivebuffer=<n>", strprintf(_("maximum per-connection receive buffer, <n>*1000 bytes (default: %u)"), 5000));
    strusage += helpmessageopt("-maxsendbuffer=<n>", strprintf(_("maximum per-connection send buffer, <n>*1000 bytes (default: %u)"), 1000));
    strusage += helpmessageopt("-onion=<ip:port>", strprintf(_("use separate socks5 proxy to reach peers via tor hidden services (default: %s)"), "-proxy"));
    strusage += helpmessageopt("-onlynet=<net>", _("only connect to nodes in network <net> (ipv4, ipv6 or onion)"));
    strusage += helpmessageopt("-permitbaremultisig", strprintf(_("relay non-p2sh multisig (default: %u)"), 1));
    strusage += helpmessageopt("-port=<port>", strprintf(_("listen for connections on <port> (default: %u or testnet: %u)"), 8333, 18333));
    strusage += helpmessageopt("-proxy=<ip:port>", _("connect through socks5 proxy"));
    strusage += helpmessageopt("-proxyrandomize", strprintf(_("randomize credentials for every proxy connection. this enables tor stream isolation (default: %u)"), 1));
    strusage += helpmessageopt("-seednode=<ip>", _("connect to a node to retrieve peer addresses, and disconnect"));
    strusage += helpmessageopt("-timeout=<n>", strprintf(_("specify connection timeout in milliseconds (minimum: 1, default: %d)"), default_connect_timeout));
#ifdef use_upnp
#if use_upnp
    strusage += helpmessageopt("-upnp", _("use upnp to map the listening port (default: 1 when listening and no -proxy)"));
#else
    strusage += helpmessageopt("-upnp", strprintf(_("use upnp to map the listening port (default: %u)"), 0));
#endif
#endif
    strusage += helpmessageopt("-whitebind=<addr>", _("bind to given address and whitelist peers connecting to it. use [host]:port notation for ipv6"));
    strusage += helpmessageopt("-whitelist=<netmask>", _("whitelist peers connecting from the given netmask or ip address. can be specified multiple times.") +
        " " + _("whitelisted peers cannot be dos banned and their transactions are always relayed, even if they are already in the mempool, useful e.g. for a gateway"));

#ifdef enable_wallet
    strusage += helpmessagegroup(_("wallet options:"));
    strusage += helpmessageopt("-disablewallet", _("do not load the wallet and disable wallet rpc calls"));
    strusage += helpmessageopt("-keypool=<n>", strprintf(_("set key pool size to <n> (default: %u)"), 100));
    if (showdebug)
        strusage += helpmessageopt("-mintxfee=<amt>", strprintf("fees (in btc/kb) smaller than this are considered zero fee for transaction creation (default: %s)",
            formatmoney(cwallet::mintxfee.getfeeperk())));
    strusage += helpmessageopt("-paytxfee=<amt>", strprintf(_("fee (in btc/kb) to add to transactions you send (default: %s)"), formatmoney(paytxfee.getfeeperk())));
    strusage += helpmessageopt("-rescan", _("rescan the block chain for missing wallet transactions") + " " + _("on startup"));
    strusage += helpmessageopt("-salvagewallet", _("attempt to recover private keys from a corrupt wallet.dat") + " " + _("on startup"));
    strusage += helpmessageopt("-sendfreetransactions", strprintf(_("send transactions as zero-fee transactions if possible (default: %u)"), 0));
    strusage += helpmessageopt("-spendzeroconfchange", strprintf(_("spend unconfirmed change when sending transactions (default: %u)"), 1));
    strusage += helpmessageopt("-txconfirmtarget=<n>", strprintf(_("if paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), default_tx_confirm_target));
    strusage += helpmessageopt("-maxtxfee=<amt>", strprintf(_("maximum total fees to use in a single wallet transaction; setting this too low may abort large transactions (default: %s)"),
        formatmoney(maxtxfee)));
    strusage += helpmessageopt("-upgradewallet", _("upgrade wallet to latest format") + " " + _("on startup"));
    strusage += helpmessageopt("-wallet=<file>", _("specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), "wallet.dat"));
    strusage += helpmessageopt("-walletbroadcast", _("make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), true));
    strusage += helpmessageopt("-walletnotify=<cmd>", _("execute command when a wallet transaction changes (%s in cmd is replaced by txid)"));
    strusage += helpmessageopt("-zapwallettxes=<mode>", _("delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
        " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));
#endif

    strusage += helpmessagegroup(_("debugging/testing options:"));
    if (showdebug)
    {
        strusage += helpmessageopt("-checkpoints", strprintf("disable expensive verification for known chain history (default: %u)", 1));
        strusage += helpmessageopt("-dblogsize=<n>", strprintf("flush database activity from memory pool to disk log every <n> megabytes (default: %u)", 100));
        strusage += helpmessageopt("-disablesafemode", strprintf("disable safemode, override a real safe mode event (default: %u)", 0));
        strusage += helpmessageopt("-testsafemode", strprintf("force safe mode (default: %u)", 0));
        strusage += helpmessageopt("-dropmessagestest=<n>", "randomly drop 1 of every <n> network messages");
        strusage += helpmessageopt("-fuzzmessagestest=<n>", "randomly fuzz 1 of every <n> network messages");
        strusage += helpmessageopt("-flushwallet", strprintf("run a thread to flush wallet periodically (default: %u)", 1));
        strusage += helpmessageopt("-stopafterblockimport", strprintf("stop running after importing blocks from disk (default: %u)", 0));
    }
    string debugcategories = "addrman, alert, bench, coindb, db, lock, rand, rpc, selectcoins, mempool, net, proxy, prune"; // don't translate these and qt below
    if (mode == hmm_moorecoin_qt)
        debugcategories += ", qt";
    strusage += helpmessageopt("-debug=<category>", strprintf(_("output debugging information (default: %u, supplying <category> is optional)"), 0) + ". " +
        _("if <category> is not supplied or if <category> = 1, output all debugging information.") + _("<category> can be:") + " " + debugcategories + ".");
#ifdef enable_wallet
    strusage += helpmessageopt("-gen", strprintf(_("generate coins (default: %u)"), 0));
    strusage += helpmessageopt("-genproclimit=<n>", strprintf(_("set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), 1));
#endif
    strusage += helpmessageopt("-help-debug", _("show all debugging options (usage: --help -help-debug)"));
    strusage += helpmessageopt("-logips", strprintf(_("include ip addresses in debug output (default: %u)"), 0));
    strusage += helpmessageopt("-logtimestamps", strprintf(_("prepend debug output with timestamp (default: %u)"), 1));
    if (showdebug)
    {
        strusage += helpmessageopt("-limitfreerelay=<n>", strprintf("continuously rate-limit free transactions to <n>*1000 bytes per minute (default: %u)", 15));
        strusage += helpmessageopt("-relaypriority", strprintf("require high priority for relaying free or low-fee transactions (default: %u)", 1));
        strusage += helpmessageopt("-maxsigcachesize=<n>", strprintf("limit size of signature cache to <n> entries (default: %u)", 50000));
    }
    strusage += helpmessageopt("-minrelaytxfee=<amt>", strprintf(_("fees (in btc/kb) smaller than this are considered zero fee for relaying (default: %s)"), formatmoney(::minrelaytxfee.getfeeperk())));
    strusage += helpmessageopt("-printtoconsole", _("send trace/debug info to console instead of debug.log file"));
    if (showdebug)
    {
        strusage += helpmessageopt("-printpriority", strprintf("log transaction priority and fee per kb when mining blocks (default: %u)", 0));
        strusage += helpmessageopt("-privdb", strprintf("sets the db_private flag in the wallet db environment (default: %u)", 1));
        strusage += helpmessageopt("-regtest", "enter regression test mode, which uses a special chain in which blocks can be solved instantly. "
            "this is intended for regression testing tools and app development.");
    }
    strusage += helpmessageopt("-shrinkdebugfile", _("shrink debug.log file on client startup (default: 1 when no -debug)"));
    strusage += helpmessageopt("-testnet", _("use the test network"));

    strusage += helpmessagegroup(_("node relay options:"));
    strusage += helpmessageopt("-datacarrier", strprintf(_("relay and mine data carrier transactions (default: %u)"), 1));
    strusage += helpmessageopt("-datacarriersize", strprintf(_("maximum size of data in data carrier transactions we relay and mine (default: %u)"), max_op_return_relay));

    strusage += helpmessagegroup(_("block creation options:"));
    strusage += helpmessageopt("-blockminsize=<n>", strprintf(_("set minimum block size in bytes (default: %u)"), 0));
    strusage += helpmessageopt("-blockmaxsize=<n>", strprintf(_("set maximum block size in bytes (default: %d)"), default_block_max_size));
    strusage += helpmessageopt("-blockprioritysize=<n>", strprintf(_("set maximum size of high-priority/low-fee transactions in bytes (default: %d)"), default_block_priority_size));
    if (showdebug)
        strusage += helpmessageopt("-blockversion=<n>", strprintf("override block version to test forking scenarios (default: %d)", (int)cblock::current_version));

    strusage += helpmessagegroup(_("rpc server options:"));
    strusage += helpmessageopt("-server", _("accept command line and json-rpc commands"));
    strusage += helpmessageopt("-rest", strprintf(_("accept public rest requests (default: %u)"), 0));
    strusage += helpmessageopt("-rpcbind=<addr>", _("bind to given address to listen for json-rpc connections. use [host]:port notation for ipv6. this option can be specified multiple times (default: bind to all interfaces)"));
    strusage += helpmessageopt("-rpcuser=<user>", _("username for json-rpc connections"));
    strusage += helpmessageopt("-rpcpassword=<pw>", _("password for json-rpc connections"));
    strusage += helpmessageopt("-rpcport=<port>", strprintf(_("listen for json-rpc connections on <port> (default: %u or testnet: %u)"), 8332, 18332));
    strusage += helpmessageopt("-rpcallowip=<ip>", _("allow json-rpc connections from specified source. valid for <ip> are a single ip (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/cidr (e.g. 1.2.3.4/24). this option can be specified multiple times"));
    strusage += helpmessageopt("-rpcthreads=<n>", strprintf(_("set the number of threads to service rpc calls (default: %d)"), 4));
    strusage += helpmessageopt("-rpckeepalive", strprintf(_("rpc support for http persistent connections (default: %d)"), 1));

    strusage += helpmessagegroup(_("rpc ssl options: (see the moorecoin wiki for ssl setup instructions)"));
    strusage += helpmessageopt("-rpcssl", _("use openssl (https) for json-rpc connections"));
    strusage += helpmessageopt("-rpcsslcertificatechainfile=<file.cert>", strprintf(_("server certificate file (default: %s)"), "server.cert"));
    strusage += helpmessageopt("-rpcsslprivatekeyfile=<file.pem>", strprintf(_("server private key (default: %s)"), "server.pem"));
    strusage += helpmessageopt("-rpcsslciphers=<ciphers>", strprintf(_("acceptable ciphers (default: %s)"), "tlsv1.2+high:tlsv1+high:!sslv2:!anull:!enull:!3des:@strength"));

    if (mode == hmm_moorecoin_qt)
    {
        strusage += helpmessagegroup(_("ui options:"));
        if (showdebug) {
            strusage += helpmessageopt("-allowselfsignedrootcertificates", "allow self signed root certificates (default: 0)");
        }
        strusage += helpmessageopt("-choosedatadir", _("choose data directory on startup (default: 0)"));
        strusage += helpmessageopt("-lang=<lang>", _("set language, for example \"de_de\" (default: system locale)"));
        strusage += helpmessageopt("-min", _("start minimized"));
        strusage += helpmessageopt("-rootcertificates=<file>", _("set ssl root certificates for payment request (default: -system-)"));
        strusage += helpmessageopt("-splash", _("show splash screen on startup (default: 1)"));
    }

    return strusage;
}

std::string licenseinfo()
{
    return formatparagraph(strprintf(_("copyright (c) 2009-%i the moorecoin core developers"), copyright_year)) + "\n" +
           "\n" +
           formatparagraph(_("this is experimental software.")) + "\n" +
           "\n" +
           formatparagraph(_("distributed under the mit software license, see the accompanying file copying or <http://www.opensource.org/licenses/mit-license.php>.")) + "\n" +
           "\n" +
           formatparagraph(_("this product includes software developed by the openssl project for use in the openssl toolkit <https://www.openssl.org/> and cryptographic software written by eric young and upnp software written by thomas bernard.")) +
           "\n";
}

static void blocknotifycallback(const uint256& hashnewtip)
{
    std::string strcmd = getarg("-blocknotify", "");

    boost::replace_all(strcmd, "%s", hashnewtip.gethex());
    boost::thread t(runcommand, strcmd); // thread runs free
}

struct cimportingnow
{
    cimportingnow() {
        assert(fimporting == false);
        fimporting = true;
    }

    ~cimportingnow() {
        assert(fimporting == true);
        fimporting = false;
    }
};


// if we're using -prune with -reindex, then delete block files that will be ignored by the
// reindex.  since reindexing works by starting at block file 0 and looping until a blockfile
// is missing, do the same here to delete any later block files after a gap.  also delete all
// rev files since they'll be rewritten by the reindex anyway.  this ensures that vinfoblockfile
// is in sync with what's actually on disk by the time we start downloading, so that pruning
// works correctly.
void cleanupblockrevfiles()
{
    using namespace boost::filesystem;
    map<string, path> mapblockfiles;

    // glob all blk?????.dat and rev?????.dat files from the blocks directory.
    // remove the rev files immediately and insert the blk file paths into an
    // ordered map keyed by block file index.
    logprintf("removing unusable blk?????.dat and rev?????.dat files for -reindex with -prune\n");
    path blocksdir = getdatadir() / "blocks";
    for (directory_iterator it(blocksdir); it != directory_iterator(); it++) {
        if (is_regular_file(*it) &&
            it->path().filename().string().length() == 12 &&
            it->path().filename().string().substr(8,4) == ".dat")
        {
            if (it->path().filename().string().substr(0,3) == "blk")
                mapblockfiles[it->path().filename().string().substr(3,5)] = it->path();
            else if (it->path().filename().string().substr(0,3) == "rev")
                remove(it->path());
        }
    }

    // remove all block files that aren't part of a contiguous set starting at
    // zero by walking the ordered map (keys are block file indices) by
    // keeping a separate counter.  once we hit a gap (or if 0 doesn't exist)
    // start removing block files.
    int ncontigcounter = 0;
    boost_foreach(const pairtype(string, path)& item, mapblockfiles) {
        if (atoi(item.first) == ncontigcounter) {
            ncontigcounter++;
            continue;
        }
        remove(item.second);
    }
}

void threadimport(std::vector<boost::filesystem::path> vimportfiles)
{
    renamethread("moorecoin-loadblk");
    // -reindex
    if (freindex) {
        cimportingnow imp;
        int nfile = 0;
        while (true) {
            cdiskblockpos pos(nfile, 0);
            if (!boost::filesystem::exists(getblockposfilename(pos, "blk")))
                break; // no block files left to reindex
            file *file = openblockfile(pos, true);
            if (!file)
                break; // this error is logged in openblockfile
            logprintf("reindexing block file blk%05u.dat...\n", (unsigned int)nfile);
            loadexternalblockfile(file, &pos);
            nfile++;
        }
        pblocktree->writereindexing(false);
        freindex = false;
        logprintf("reindexing finished\n");
        // to avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
        initblockindex();
    }

    // hardcoded $datadir/bootstrap.dat
    boost::filesystem::path pathbootstrap = getdatadir() / "bootstrap.dat";
    if (boost::filesystem::exists(pathbootstrap)) {
        file *file = fopen(pathbootstrap.string().c_str(), "rb");
        if (file) {
            cimportingnow imp;
            boost::filesystem::path pathbootstrapold = getdatadir() / "bootstrap.dat.old";
            logprintf("importing bootstrap.dat...\n");
            loadexternalblockfile(file);
            renameover(pathbootstrap, pathbootstrapold);
        } else {
            logprintf("warning: could not open bootstrap file %s\n", pathbootstrap.string());
        }
    }

    // -loadblock=
    boost_foreach(const boost::filesystem::path& path, vimportfiles) {
        file *file = fopen(path.string().c_str(), "rb");
        if (file) {
            cimportingnow imp;
            logprintf("importing blocks file %s...\n", path.string());
            loadexternalblockfile(file);
        } else {
            logprintf("warning: could not open blocks file %s\n", path.string());
        }
    }

    if (getboolarg("-stopafterblockimport", false)) {
        logprintf("stopping after block import\n");
        startshutdown();
    }
}

/** sanity checks
 *  ensure that moorecoin is running in a usable environment with all
 *  necessary library support.
 */
bool initsanitycheck(void)
{
    if(!ecc_initsanitycheck()) {
        initerror("openssl appears to lack support for elliptic curve cryptography. for more "
                  "information, visit https://en.moorecoin.it/wiki/openssl_and_ec_libraries");
        return false;
    }
    if (!glibc_sanity_test() || !glibcxx_sanity_test())
        return false;

    return true;
}

/** initialize moorecoin.
 *  @pre parameters should be parsed and config file should be read.
 */
bool appinit2(boost::thread_group& threadgroup, cscheduler& scheduler)
{
    // ********************************************************* step 1: setup
#ifdef _msc_ver
    // turn off microsoft heap dump noise
    _crtsetreportmode(_crt_warn, _crtdbg_mode_file);
    _crtsetreportfile(_crt_warn, createfilea("nul", generic_write, 0, null, open_existing, 0, 0));
#endif
#if _msc_ver >= 1400
    // disable confusing "helpful" text message on abort, ctrl-c
    _set_abort_behavior(0, _write_abort_msg | _call_reportfault);
#endif
#ifdef win32
    // enable data execution prevention (dep)
    // minimum supported os versions: winxp sp3, winvista >= sp1, win server 2008
    // a failure is non-critical and needs no further attention!
#ifndef process_dep_enable
    // we define this here, because gccs winbase.h limits this to _win32_winnt >= 0x0601 (windows 7),
    // which is not correct. can be removed, when gccs winbase.h is fixed!
#define process_dep_enable 0x00000001
#endif
    typedef bool (winapi *psetprocdeppol)(dword);
    psetprocdeppol setprocdeppol = (psetprocdeppol)getprocaddress(getmodulehandlea("kernel32.dll"), "setprocessdeppolicy");
    if (setprocdeppol != null) setprocdeppol(process_dep_enable);

    // initialize windows sockets
    wsadata wsadata;
    int ret = wsastartup(makeword(2,2), &wsadata);
    if (ret != no_error || lobyte(wsadata.wversion ) != 2 || hibyte(wsadata.wversion) != 2)
    {
        return initerror(strprintf("error: winsock library failed to start (wsastartup returned error %d)", ret));
    }
#endif
#ifndef win32

    if (getboolarg("-sysperms", false)) {
#ifdef enable_wallet
        if (!getboolarg("-disablewallet", false))
            return initerror("error: -sysperms is not allowed in combination with enabled wallet functionality");
#endif
    } else {
        umask(077);
    }

    // clean shutdown on sigterm
    struct sigaction sa;
    sa.sa_handler = handlesigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sigterm, &sa, null);
    sigaction(sigint, &sa, null);

    // reopen debug.log on sighup
    struct sigaction sa_hup;
    sa_hup.sa_handler = handlesighup;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(sighup, &sa_hup, null);

#if defined (__svr4) && defined (__sun)
    // ignore sigpipe on solaris
    signal(sigpipe, sig_ign);
#endif
#endif

    // ********************************************************* step 2: parameter interactions
    const cchainparams& chainparams = params();

    // set this early so that parameter interactions go to console
    fprinttoconsole = getboolarg("-printtoconsole", false);
    flogtimestamps = getboolarg("-logtimestamps", true);
    flogips = getboolarg("-logips", false);

    // when specifying an explicit binding address, you want to listen on it
    // even when -connect or -proxy is specified
    if (mapargs.count("-bind")) {
        if (softsetboolarg("-listen", true))
            logprintf("%s: parameter interaction: -bind set -> setting -listen=1\n", __func__);
    }
    if (mapargs.count("-whitebind")) {
        if (softsetboolarg("-listen", true))
            logprintf("%s: parameter interaction: -whitebind set -> setting -listen=1\n", __func__);
    }

    if (mapargs.count("-connect") && mapmultiargs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via dns, or listen by default
        if (softsetboolarg("-dnsseed", false))
            logprintf("%s: parameter interaction: -connect set -> setting -dnsseed=0\n", __func__);
        if (softsetboolarg("-listen", false))
            logprintf("%s: parameter interaction: -connect set -> setting -listen=0\n", __func__);
    }

    if (mapargs.count("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (softsetboolarg("-listen", false))
            logprintf("%s: parameter interaction: -proxy set -> setting -listen=0\n", __func__);
        // to protect privacy, do not use upnp when a proxy is set. the user may still specify -listen=1
        // to listen locally, so don't rely on this happening through -listen below.
        if (softsetboolarg("-upnp", false))
            logprintf("%s: parameter interaction: -proxy set -> setting -upnp=0\n", __func__);
        // to protect privacy, do not discover addresses by default
        if (softsetboolarg("-discover", false))
            logprintf("%s: parameter interaction: -proxy set -> setting -discover=0\n", __func__);
    }

    if (!getboolarg("-listen", default_listen)) {
        // do not map ports or try to retrieve public ip when not listening (pointless)
        if (softsetboolarg("-upnp", false))
            logprintf("%s: parameter interaction: -listen=0 -> setting -upnp=0\n", __func__);
        if (softsetboolarg("-discover", false))
            logprintf("%s: parameter interaction: -listen=0 -> setting -discover=0\n", __func__);
    }

    if (mapargs.count("-externalip")) {
        // if an explicit public ip is specified, do not try to find others
        if (softsetboolarg("-discover", false))
            logprintf("%s: parameter interaction: -externalip set -> setting -discover=0\n", __func__);
    }

    if (getboolarg("-salvagewallet", false)) {
        // rewrite just private keys: rescan to find transactions
        if (softsetboolarg("-rescan", true))
            logprintf("%s: parameter interaction: -salvagewallet=1 -> setting -rescan=1\n", __func__);
    }

    // -zapwallettx implies a rescan
    if (getboolarg("-zapwallettxes", false)) {
        if (softsetboolarg("-rescan", true))
            logprintf("%s: parameter interaction: -zapwallettxes=<mode> -> setting -rescan=1\n", __func__);
    }

    // make sure enough file descriptors are available
    int nbind = std::max((int)mapargs.count("-bind") + (int)mapargs.count("-whitebind"), 1);
    nmaxconnections = getarg("-maxconnections", 125);
    nmaxconnections = std::max(std::min(nmaxconnections, (int)(fd_setsize - nbind - min_core_filedescriptors)), 0);
    int nfd = raisefiledescriptorlimit(nmaxconnections + min_core_filedescriptors);
    if (nfd < min_core_filedescriptors)
        return initerror(_("not enough file descriptors available."));
    if (nfd - min_core_filedescriptors < nmaxconnections)
        nmaxconnections = nfd - min_core_filedescriptors;

    // if using block pruning, then disable txindex
    if (getarg("-prune", 0)) {
        if (getboolarg("-txindex", false))
            return initerror(_("prune mode is incompatible with -txindex."));
#ifdef enable_wallet
        if (getboolarg("-rescan", false)) {
            return initerror(_("rescans are not possible in pruned mode. you will need to use -reindex which will download the whole blockchain again."));
        }
#endif
    }

    // ********************************************************* step 3: parameter-to-internal-flags

    fdebug = !mapmultiargs["-debug"].empty();
    // special-case: if -debug=0/-nodebug is set, turn off debugging messages
    const vector<string>& categories = mapmultiargs["-debug"];
    if (getboolarg("-nodebug", false) || find(categories.begin(), categories.end(), string("0")) != categories.end())
        fdebug = false;

    // check for -debugnet
    if (getboolarg("-debugnet", false))
        initwarning(_("warning: unsupported argument -debugnet ignored, use -debug=net."));
    // check for -socks - as this is a privacy risk to continue, exit here
    if (mapargs.count("-socks"))
        return initerror(_("error: unsupported argument -socks found. setting socks version isn't possible anymore, only socks5 proxies are supported."));
    // check for -tor - as this is a privacy risk to continue, exit here
    if (getboolarg("-tor", false))
        return initerror(_("error: unsupported argument -tor found, use -onion."));

    if (getboolarg("-benchmark", false))
        initwarning(_("warning: unsupported argument -benchmark ignored, use -debug=bench."));

    // checkmempool and checkblockindex default to true in regtest mode
    mempool.setsanitycheck(getboolarg("-checkmempool", chainparams.defaultconsistencychecks()));
    fcheckblockindex = getboolarg("-checkblockindex", chainparams.defaultconsistencychecks());
    fcheckpointsenabled = getboolarg("-checkpoints", true);

    // -par=0 means autodetect, but nscriptcheckthreads==0 means no concurrency
    nscriptcheckthreads = getarg("-par", default_scriptcheck_threads);
    if (nscriptcheckthreads <= 0)
        nscriptcheckthreads += boost::thread::hardware_concurrency();
    if (nscriptcheckthreads <= 1)
        nscriptcheckthreads = 0;
    else if (nscriptcheckthreads > max_scriptcheck_threads)
        nscriptcheckthreads = max_scriptcheck_threads;

    fserver = getboolarg("-server", false);

    // block pruning; get the amount of disk space (in mb) to allot for block & undo files
    int64_t nsignedprunetarget = getarg("-prune", 0) * 1024 * 1024;
    if (nsignedprunetarget < 0) {
        return initerror(_("prune cannot be configured with a negative value."));
    }
    nprunetarget = (uint64_t) nsignedprunetarget;
    if (nprunetarget) {
        if (nprunetarget < min_disk_space_for_block_files) {
            return initerror(strprintf(_("prune configured below the minimum of %d mb.  please use a higher number."), min_disk_space_for_block_files / 1024 / 1024));
        }
        logprintf("prune configured to target %umib on disk for block and undo files.\n", nprunetarget / 1024 / 1024);
        fprunemode = true;
    }

#ifdef enable_wallet
    bool fdisablewallet = getboolarg("-disablewallet", false);
#endif

    nconnecttimeout = getarg("-timeout", default_connect_timeout);
    if (nconnecttimeout <= 0)
        nconnecttimeout = default_connect_timeout;

    // fee-per-kilobyte amount considered the same as "free"
    // if you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. it should be set above the real
    // cost to you of processing a transaction.
    if (mapargs.count("-minrelaytxfee"))
    {
        camount n = 0;
        if (parsemoney(mapargs["-minrelaytxfee"], n) && n > 0)
            ::minrelaytxfee = cfeerate(n);
        else
            return initerror(strprintf(_("invalid amount for -minrelaytxfee=<amount>: '%s'"), mapargs["-minrelaytxfee"]));
    }

#ifdef enable_wallet
    if (mapargs.count("-mintxfee"))
    {
        camount n = 0;
        if (parsemoney(mapargs["-mintxfee"], n) && n > 0)
            cwallet::mintxfee = cfeerate(n);
        else
            return initerror(strprintf(_("invalid amount for -mintxfee=<amount>: '%s'"), mapargs["-mintxfee"]));
    }
    if (mapargs.count("-paytxfee"))
    {
        camount nfeeperk = 0;
        if (!parsemoney(mapargs["-paytxfee"], nfeeperk))
            return initerror(strprintf(_("invalid amount for -paytxfee=<amount>: '%s'"), mapargs["-paytxfee"]));
        if (nfeeperk > nhightransactionfeewarning)
            initwarning(_("warning: -paytxfee is set very high! this is the transaction fee you will pay if you send a transaction."));
        paytxfee = cfeerate(nfeeperk, 1000);
        if (paytxfee < ::minrelaytxfee)
        {
            return initerror(strprintf(_("invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                       mapargs["-paytxfee"], ::minrelaytxfee.tostring()));
        }
    }
    if (mapargs.count("-maxtxfee"))
    {
        camount nmaxfee = 0;
        if (!parsemoney(mapargs["-maxtxfee"], nmaxfee))
            return initerror(strprintf(_("invalid amount for -maxtxfee=<amount>: '%s'"), mapargs["-maptxfee"]));
        if (nmaxfee > nhightransactionmaxfeewarning)
            initwarning(_("warning: -maxtxfee is set very high! fees this large could be paid on a single transaction."));
        maxtxfee = nmaxfee;
        if (cfeerate(maxtxfee, 1000) < ::minrelaytxfee)
        {
            return initerror(strprintf(_("invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                                       mapargs["-maxtxfee"], ::minrelaytxfee.tostring()));
        }
    }
    ntxconfirmtarget = getarg("-txconfirmtarget", default_tx_confirm_target);
    bspendzeroconfchange = getboolarg("-spendzeroconfchange", true);
    fsendfreetransactions = getboolarg("-sendfreetransactions", false);

    std::string strwalletfile = getarg("-wallet", "wallet.dat");
#endif // enable_wallet

    fisbaremultisigstd = getboolarg("-permitbaremultisig", true);
    nmaxdatacarrierbytes = getarg("-datacarriersize", nmaxdatacarrierbytes);

    falerts = getboolarg("-alerts", default_alerts);

    // ********************************************************* step 4: application initialization: dir lock, daemonize, pidfile, debug log

    // initialize elliptic curve code
    ecc_start();

    // sanity check
    if (!initsanitycheck())
        return initerror(_("initialization sanity check failed. moorecoin core is shutting down."));

    std::string strdatadir = getdatadir().string();
#ifdef enable_wallet
    // wallet file must be a plain filename without a directory
    if (strwalletfile != boost::filesystem::basename(strwalletfile) + boost::filesystem::extension(strwalletfile))
        return initerror(strprintf(_("wallet %s resides outside data directory %s"), strwalletfile, strdatadir));
#endif
    // make sure only a single moorecoin process is using the data directory.
    boost::filesystem::path pathlockfile = getdatadir() / ".lock";
    file* file = fopen(pathlockfile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);

    try {
        static boost::interprocess::file_lock lock(pathlockfile.string().c_str());
        if (!lock.try_lock())
            return initerror(strprintf(_("cannot obtain a lock on data directory %s. moorecoin core is probably already running."), strdatadir));
    } catch(const boost::interprocess::interprocess_exception& e) {
        return initerror(strprintf(_("cannot obtain a lock on data directory %s. moorecoin core is probably already running.") + " %s.", strdatadir, e.what()));
    }

#ifndef win32
    createpidfile(getpidfile(), getpid());
#endif
    if (getboolarg("-shrinkdebugfile", !fdebug))
        shrinkdebugfile();
    logprintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    logprintf("moorecoin version %s (%s)\n", formatfullversion(), client_date);
    logprintf("using openssl version %s\n", ssleay_version(ssleay_version));
#ifdef enable_wallet
    logprintf("using berkeleydb version %s\n", dbenv::version(0, 0, 0));
#endif
    if (!flogtimestamps)
        logprintf("startup time: %s\n", datetimestrformat("%y-%m-%d %h:%m:%s", gettime()));
    logprintf("default data directory %s\n", getdefaultdatadir().string());
    logprintf("using data directory %s\n", strdatadir);
    logprintf("using config file %s\n", getconfigfile().string());
    logprintf("using at most %i connections (%i file descriptors available)\n", nmaxconnections, nfd);
    std::ostringstream strerrors;

    logprintf("using %u threads for script verification\n", nscriptcheckthreads);
    if (nscriptcheckthreads) {
        for (int i=0; i<nscriptcheckthreads-1; i++)
            threadgroup.create_thread(&threadscriptcheck);
    }

    // start the lightweight task scheduler thread
    cscheduler::function serviceloop = boost::bind(&cscheduler::servicequeue, &scheduler);
    threadgroup.create_thread(boost::bind(&tracethread<cscheduler::function>, "scheduler", serviceloop));

    /* start the rpc server already.  it will be started in "warmup" mode
     * and not really process calls already (but it will signify connections
     * that the server is there and will be ready later).  warmup mode will
     * be disabled when initialisation is finished.
     */
    if (fserver)
    {
        uiinterface.initmessage.connect(setrpcwarmupstatus);
        rpcserver::onstopped(&onrpcstopped);
        rpcserver::onprecommand(&onrpcprecommand);
        startrpcthreads();
    }

    int64_t nstart;

    // ********************************************************* step 5: verify wallet database integrity
#ifdef enable_wallet
    if (!fdisablewallet) {
        logprintf("using wallet %s\n", strwalletfile);
        uiinterface.initmessage(_("verifying wallet..."));

        std::string warningstring;
        std::string errorstring;

        if (!cwallet::verify(strwalletfile, warningstring, errorstring))
            return false;

        if (!warningstring.empty())
            initwarning(warningstring);
        if (!errorstring.empty())
            return initerror(warningstring);

    } // (!fdisablewallet)
#endif // enable_wallet
    // ********************************************************* step 6: network initialization

    registernodesignals(getnodesignals());

    if (mapargs.count("-onlynet")) {
        std::set<enum network> nets;
        boost_foreach(const std::string& snet, mapmultiargs["-onlynet"]) {
            enum network net = parsenetwork(snet);
            if (net == net_unroutable)
                return initerror(strprintf(_("unknown network specified in -onlynet: '%s'"), snet));
            nets.insert(net);
        }
        for (int n = 0; n < net_max; n++) {
            enum network net = (enum network)n;
            if (!nets.count(net))
                setlimited(net);
        }
    }

    if (mapargs.count("-whitelist")) {
        boost_foreach(const std::string& net, mapmultiargs["-whitelist"]) {
            csubnet subnet(net);
            if (!subnet.isvalid())
                return initerror(strprintf(_("invalid netmask specified in -whitelist: '%s'"), net));
            cnode::addwhitelistedrange(subnet);
        }
    }

    bool proxyrandomize = getboolarg("-proxyrandomize", true);
    // -proxy sets a proxy for all outgoing network traffic
    // -noproxy (or -proxy=0) as well as the empty string can be used to not set a proxy, this is the default
    std::string proxyarg = getarg("-proxy", "");
    if (proxyarg != "" && proxyarg != "0") {
        proxytype addrproxy = proxytype(cservice(proxyarg, 9050), proxyrandomize);
        if (!addrproxy.isvalid())
            return initerror(strprintf(_("invalid -proxy address: '%s'"), proxyarg));

        setproxy(net_ipv4, addrproxy);
        setproxy(net_ipv6, addrproxy);
        setproxy(net_tor, addrproxy);
        setnameproxy(addrproxy);
        setreachable(net_tor); // by default, -proxy sets onion as reachable, unless -noonion later
    }

    // -onion can be used to set only a proxy for .onion, or override normal proxy for .onion addresses
    // -noonion (or -onion=0) disables connecting to .onion entirely
    // an empty string is used to not override the onion proxy (in which case it defaults to -proxy set above, or none)
    std::string onionarg = getarg("-onion", "");
    if (onionarg != "") {
        if (onionarg == "0") { // handle -noonion/-onion=0
            setreachable(net_tor, false); // set onions as unreachable
        } else {
            proxytype addronion = proxytype(cservice(onionarg, 9050), proxyrandomize);
            if (!addronion.isvalid())
                return initerror(strprintf(_("invalid -onion address: '%s'"), onionarg));
            setproxy(net_tor, addronion);
            setreachable(net_tor);
        }
    }

    // see step 2: parameter interactions for more information about these
    flisten = getboolarg("-listen", default_listen);
    fdiscover = getboolarg("-discover", true);
    fnamelookup = getboolarg("-dns", true);

    bool fbound = false;
    if (flisten) {
        if (mapargs.count("-bind") || mapargs.count("-whitebind")) {
            boost_foreach(const std::string& strbind, mapmultiargs["-bind"]) {
                cservice addrbind;
                if (!lookup(strbind.c_str(), addrbind, getlistenport(), false))
                    return initerror(strprintf(_("cannot resolve -bind address: '%s'"), strbind));
                fbound |= bind(addrbind, (bf_explicit | bf_report_error));
            }
            boost_foreach(const std::string& strbind, mapmultiargs["-whitebind"]) {
                cservice addrbind;
                if (!lookup(strbind.c_str(), addrbind, 0, false))
                    return initerror(strprintf(_("cannot resolve -whitebind address: '%s'"), strbind));
                if (addrbind.getport() == 0)
                    return initerror(strprintf(_("need to specify a port with -whitebind: '%s'"), strbind));
                fbound |= bind(addrbind, (bf_explicit | bf_report_error | bf_whitelist));
            }
        }
        else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = inaddr_any;
            fbound |= bind(cservice(in6addr_any, getlistenport()), bf_none);
            fbound |= bind(cservice(inaddr_any, getlistenport()), !fbound ? bf_report_error : bf_none);
        }
        if (!fbound)
            return initerror(_("failed to listen on any port. use -listen=0 if you want this."));
    }

    if (mapargs.count("-externalip")) {
        boost_foreach(const std::string& straddr, mapmultiargs["-externalip"]) {
            cservice addrlocal(straddr, getlistenport(), fnamelookup);
            if (!addrlocal.isvalid())
                return initerror(strprintf(_("cannot resolve -externalip address: '%s'"), straddr));
            addlocal(cservice(straddr, getlistenport(), fnamelookup), local_manual);
        }
    }

    boost_foreach(const std::string& strdest, mapmultiargs["-seednode"])
        addoneshot(strdest);

    // ********************************************************* step 7: load block chain

    freindex = getboolarg("-reindex", false);

    // upgrading to 0.8; hard-link the old blknnnn.dat files into /blocks/
    boost::filesystem::path blocksdir = getdatadir() / "blocks";
    if (!boost::filesystem::exists(blocksdir))
    {
        boost::filesystem::create_directories(blocksdir);
        bool linked = false;
        for (unsigned int i = 1; i < 10000; i++) {
            boost::filesystem::path source = getdatadir() / strprintf("blk%04u.dat", i);
            if (!boost::filesystem::exists(source)) break;
            boost::filesystem::path dest = blocksdir / strprintf("blk%05u.dat", i-1);
            try {
                boost::filesystem::create_hard_link(source, dest);
                logprintf("hardlinked %s -> %s\n", source.string(), dest.string());
                linked = true;
            } catch (const boost::filesystem::filesystem_error& e) {
                // note: hardlink creation failing is not a disaster, it just means
                // blocks will get re-downloaded from peers.
                logprintf("error hardlinking blk%04u.dat: %s\n", i, e.what());
                break;
            }
        }
        if (linked)
        {
            freindex = true;
        }
    }

    // cache size calculations
    int64_t ntotalcache = (getarg("-dbcache", ndefaultdbcache) << 20);
    ntotalcache = std::max(ntotalcache, nmindbcache << 20); // total cache cannot be less than nmindbcache
    ntotalcache = std::min(ntotalcache, nmaxdbcache << 20); // total cache cannot be greated than nmaxdbcache
    int64_t nblocktreedbcache = ntotalcache / 8;
    if (nblocktreedbcache > (1 << 21) && !getboolarg("-txindex", false))
        nblocktreedbcache = (1 << 21); // block tree db cache shouldn't be larger than 2 mib
    ntotalcache -= nblocktreedbcache;
    int64_t ncoindbcache = std::min(ntotalcache / 2, (ntotalcache / 4) + (1 << 23)); // use 25%-50% of the remainder for disk cache
    ntotalcache -= ncoindbcache;
    ncoincacheusage = ntotalcache; // the rest goes to in-memory cache
    logprintf("cache configuration:\n");
    logprintf("* using %.1fmib for block index database\n", nblocktreedbcache * (1.0 / 1024 / 1024));
    logprintf("* using %.1fmib for chain state database\n", ncoindbcache * (1.0 / 1024 / 1024));
    logprintf("* using %.1fmib for in-memory utxo set\n", ncoincacheusage * (1.0 / 1024 / 1024));

    bool floaded = false;
    while (!floaded) {
        bool freset = freindex;
        std::string strloaderror;

        uiinterface.initmessage(_("loading block index..."));

        nstart = gettimemillis();
        do {
            try {
                unloadblockindex();
                delete pcoinstip;
                delete pcoinsdbview;
                delete pcoinscatcher;
                delete pblocktree;

                pblocktree = new cblocktreedb(nblocktreedbcache, false, freindex);
                pcoinsdbview = new ccoinsviewdb(ncoindbcache, false, freindex);
                pcoinscatcher = new ccoinsviewerrorcatcher(pcoinsdbview);
                pcoinstip = new ccoinsviewcache(pcoinscatcher);

                if (freindex) {
                    pblocktree->writereindexing(true);
                    //if we're reindexing in prune mode, wipe away unusable block files and all undo data files
                    if (fprunemode)
                        cleanupblockrevfiles();
                }

                if (!loadblockindex()) {
                    strloaderror = _("error loading block database");
                    break;
                }

                // if the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way around).
                if (!mapblockindex.empty() && mapblockindex.count(chainparams.getconsensus().hashgenesisblock) == 0)
                    return initerror(_("incorrect or no genesis block found. wrong datadir for network?"));

                // initialize the block index (no-op if non-empty database was already loaded)
                if (!initblockindex()) {
                    strloaderror = _("error initializing block database");
                    break;
                }

                // check for changed -txindex state
                if (ftxindex != getboolarg("-txindex", false)) {
                    strloaderror = _("you need to rebuild the database using -reindex to change -txindex");
                    break;
                }

                // check for changed -prune state.  what we are concerned about is a user who has pruned blocks
                // in the past, but is now trying to run unpruned.
                if (fhavepruned && !fprunemode) {
                    strloaderror = _("you need to rebuild the database using -reindex to go back to unpruned mode.  this will redownload the entire blockchain");
                    break;
                }

                uiinterface.initmessage(_("verifying blocks..."));
                if (fhavepruned && getarg("-checkblocks", 288) > min_blocks_to_keep) {
                    logprintf("prune: pruned datadir may not have more than %d blocks; -checkblocks=%d may fail\n",
                        min_blocks_to_keep, getarg("-checkblocks", 288));
                }
                if (!cverifydb().verifydb(pcoinsdbview, getarg("-checklevel", 3),
                              getarg("-checkblocks", 288))) {
                    strloaderror = _("corrupted block database detected");
                    break;
                }
            } catch (const std::exception& e) {
                if (fdebug) logprintf("%s\n", e.what());
                strloaderror = _("error opening block database");
                break;
            }

            floaded = true;
        } while(false);

        if (!floaded) {
            // first suggest a reindex
            if (!freset) {
                bool fret = uiinterface.threadsafemessagebox(
                    strloaderror + ".\n\n" + _("do you want to rebuild the block database now?"),
                    "", cclientuiinterface::msg_error | cclientuiinterface::btn_abort);
                if (fret) {
                    freindex = true;
                    frequestshutdown = false;
                } else {
                    logprintf("aborted block database rebuild. exiting.\n");
                    return false;
                }
            } else {
                return initerror(strloaderror);
            }
        }
    }

    // as loadblockindex can take several minutes, it's possible the user
    // requested to kill the gui during the last operation. if so, exit.
    // as the program has not fully started yet, shutdown() is possibly overkill.
    if (frequestshutdown)
    {
        logprintf("shutdown requested. exiting.\n");
        return false;
    }
    logprintf(" block index %15dms\n", gettimemillis() - nstart);

    boost::filesystem::path est_path = getdatadir() / fee_estimates_filename;
    cautofile est_filein(fopen(est_path.string().c_str(), "rb"), ser_disk, client_version);
    // allowed to fail as this file is missing on first startup.
    if (!est_filein.isnull())
        mempool.readfeeestimates(est_filein);
    ffeeestimatesinitialized = true;

    // if prune mode, unset node_network and prune block files
    if (fprunemode) {
        logprintf("unsetting node_network on prune mode\n");
        nlocalservices &= ~node_network;
        if (!freindex) {
            pruneandflush();
        }
    }

    // ********************************************************* step 8: load wallet
#ifdef enable_wallet
    if (fdisablewallet) {
        pwalletmain = null;
        logprintf("wallet disabled!\n");
    } else {

        // needed to restore wallet transaction meta data after -zapwallettxes
        std::vector<cwallettx> vwtx;

        if (getboolarg("-zapwallettxes", false)) {
            uiinterface.initmessage(_("zapping all transactions from wallet..."));

            pwalletmain = new cwallet(strwalletfile);
            dberrors nzapwalletret = pwalletmain->zapwallettx(vwtx);
            if (nzapwalletret != db_load_ok) {
                uiinterface.initmessage(_("error loading wallet.dat: wallet corrupted"));
                return false;
            }

            delete pwalletmain;
            pwalletmain = null;
        }

        uiinterface.initmessage(_("loading wallet..."));

        nstart = gettimemillis();
        bool ffirstrun = true;
        pwalletmain = new cwallet(strwalletfile);
        dberrors nloadwalletret = pwalletmain->loadwallet(ffirstrun);
        if (nloadwalletret != db_load_ok)
        {
            if (nloadwalletret == db_corrupt)
                strerrors << _("error loading wallet.dat: wallet corrupted") << "\n";
            else if (nloadwalletret == db_noncritical_error)
            {
                string msg(_("warning: error reading wallet.dat! all keys read correctly, but transaction data"
                             " or address book entries might be missing or incorrect."));
                initwarning(msg);
            }
            else if (nloadwalletret == db_too_new)
                strerrors << _("error loading wallet.dat: wallet requires newer version of moorecoin core") << "\n";
            else if (nloadwalletret == db_need_rewrite)
            {
                strerrors << _("wallet needed to be rewritten: restart moorecoin core to complete") << "\n";
                logprintf("%s", strerrors.str());
                return initerror(strerrors.str());
            }
            else
                strerrors << _("error loading wallet.dat") << "\n";
        }

        if (getboolarg("-upgradewallet", ffirstrun))
        {
            int nmaxversion = getarg("-upgradewallet", 0);
            if (nmaxversion == 0) // the -upgradewallet without argument case
            {
                logprintf("performing wallet upgrade to %i\n", feature_latest);
                nmaxversion = client_version;
                pwalletmain->setminversion(feature_latest); // permanently upgrade the wallet immediately
            }
            else
                logprintf("allowing wallet upgrade up to %i\n", nmaxversion);
            if (nmaxversion < pwalletmain->getversion())
                strerrors << _("cannot downgrade wallet") << "\n";
            pwalletmain->setmaxversion(nmaxversion);
        }

        if (ffirstrun)
        {
            // create new keyuser and set as default key
            randaddseedperfmon();

            cpubkey newdefaultkey;
            if (pwalletmain->getkeyfrompool(newdefaultkey)) {
                pwalletmain->setdefaultkey(newdefaultkey);
                if (!pwalletmain->setaddressbook(pwalletmain->vchdefaultkey.getid(), "", "receive"))
                    strerrors << _("cannot write default address") << "\n";
            }

            pwalletmain->setbestchain(chainactive.getlocator());
        }

        logprintf("%s", strerrors.str());
        logprintf(" wallet      %15dms\n", gettimemillis() - nstart);

        registervalidationinterface(pwalletmain);

        cblockindex *pindexrescan = chainactive.tip();
        if (getboolarg("-rescan", false))
            pindexrescan = chainactive.genesis();
        else
        {
            cwalletdb walletdb(strwalletfile);
            cblocklocator locator;
            if (walletdb.readbestblock(locator))
                pindexrescan = findforkinglobalindex(chainactive, locator);
            else
                pindexrescan = chainactive.genesis();
        }
        if (chainactive.tip() && chainactive.tip() != pindexrescan)
        {
            //we can't rescan beyond non-pruned blocks, stop and throw an error
            //this might happen if a user uses a old wallet within a pruned node
            // or if he ran -disablewallet for a longer time, then decided to re-enable
            if (fprunemode)
            {
                cblockindex *block = chainactive.tip();
                while (block && block->pprev && (block->pprev->nstatus & block_have_data) && block->pprev->ntx > 0 && pindexrescan != block)
                    block = block->pprev;

                if (pindexrescan != block)
                    return initerror(_("prune: last wallet synchronisation goes beyond pruned data. you need to -reindex (download the whole blockchain again in case of pruned node)"));
            }

            uiinterface.initmessage(_("rescanning..."));
            logprintf("rescanning last %i blocks (from block %i)...\n", chainactive.height() - pindexrescan->nheight, pindexrescan->nheight);
            nstart = gettimemillis();
            pwalletmain->scanforwallettransactions(pindexrescan, true);
            logprintf(" rescan      %15dms\n", gettimemillis() - nstart);
            pwalletmain->setbestchain(chainactive.getlocator());
            nwalletdbupdated++;

            // restore wallet transaction metadata after -zapwallettxes=1
            if (getboolarg("-zapwallettxes", false) && getarg("-zapwallettxes", "1") != "2")
            {
                cwalletdb walletdb(strwalletfile);

                boost_foreach(const cwallettx& wtxold, vwtx)
                {
                    uint256 hash = wtxold.gethash();
                    std::map<uint256, cwallettx>::iterator mi = pwalletmain->mapwallet.find(hash);
                    if (mi != pwalletmain->mapwallet.end())
                    {
                        const cwallettx* copyfrom = &wtxold;
                        cwallettx* copyto = &mi->second;
                        copyto->mapvalue = copyfrom->mapvalue;
                        copyto->vorderform = copyfrom->vorderform;
                        copyto->ntimereceived = copyfrom->ntimereceived;
                        copyto->ntimesmart = copyfrom->ntimesmart;
                        copyto->ffromme = copyfrom->ffromme;
                        copyto->strfromaccount = copyfrom->strfromaccount;
                        copyto->norderpos = copyfrom->norderpos;
                        copyto->writetodisk(&walletdb);
                    }
                }
            }
        }
        pwalletmain->setbroadcasttransactions(getboolarg("-walletbroadcast", true));
    } // (!fdisablewallet)
#else // enable_wallet
    logprintf("no wallet support compiled in!\n");
#endif // !enable_wallet
    // ********************************************************* step 9: import blocks

    if (mapargs.count("-blocknotify"))
        uiinterface.notifyblocktip.connect(blocknotifycallback);

    uiinterface.initmessage(_("activating best chain..."));
    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    cvalidationstate state;
    if (!activatebestchain(state))
        strerrors << "failed to connect best block";

    std::vector<boost::filesystem::path> vimportfiles;
    if (mapargs.count("-loadblock"))
    {
        boost_foreach(const std::string& strfile, mapmultiargs["-loadblock"])
            vimportfiles.push_back(strfile);
    }
    threadgroup.create_thread(boost::bind(&threadimport, vimportfiles));
    if (chainactive.tip() == null) {
        logprintf("waiting for genesis block to be imported...\n");
        while (!frequestshutdown && chainactive.tip() == null)
            millisleep(10);
    }

    // ********************************************************* step 10: start node

    if (!checkdiskspace())
        return false;

    if (!strerrors.str().empty())
        return initerror(strerrors.str());

    randaddseedperfmon();

    //// debug print
    logprintf("mapblockindex.size() = %u\n",   mapblockindex.size());
    logprintf("nbestheight = %d\n",                   chainactive.height());
#ifdef enable_wallet
    logprintf("setkeypool.size() = %u\n",      pwalletmain ? pwalletmain->setkeypool.size() : 0);
    logprintf("mapwallet.size() = %u\n",       pwalletmain ? pwalletmain->mapwallet.size() : 0);
    logprintf("mapaddressbook.size() = %u\n",  pwalletmain ? pwalletmain->mapaddressbook.size() : 0);
#endif

    startnode(threadgroup, scheduler);

    // monitor the chain, and alert if we get blocks much quicker or slower than expected
    int64_t npowtargetspacing = params().getconsensus().npowtargetspacing;
    cscheduler::function f = boost::bind(&partitioncheck, &isinitialblockdownload,
                                         boost::ref(cs_main), boost::cref(pindexbestheader), npowtargetspacing);
    scheduler.scheduleevery(f, npowtargetspacing);

#ifdef enable_wallet
    // generate coins in the background
    if (pwalletmain)
        generatemoorecoins(getboolarg("-gen", false), pwalletmain, getarg("-genproclimit", 1));
#endif

    // ********************************************************* step 11: finished

    setrpcwarmupfinished();
    uiinterface.initmessage(_("done loading"));

#ifdef enable_wallet
    if (pwalletmain) {
        // add wallet transactions that aren't already in a block to maptransactions
        pwalletmain->reacceptwallettransactions();

        // run a thread to flush wallet periodically
        threadgroup.create_thread(boost::bind(&threadflushwalletdb, boost::ref(pwalletmain->strwalletfile)));
    }
#endif

    return !frequestshutdown;
}
