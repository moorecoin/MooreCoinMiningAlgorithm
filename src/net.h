// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_net_h
#define moorecoin_net_h

#include "bloom.h"
#include "compat.h"
#include "hash.h"
#include "limitedmap.h"
#include "mruset.h"
#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "streams.h"
#include "sync.h"
#include "uint256.h"
#include "utilstrencodings.h"

#include <deque>
#include <stdint.h>

#ifndef win32
#include <arpa/inet.h>
#endif

#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/signals2/signal.hpp>

class caddrman;
class cblockindex;
class cscheduler;
class cnode;

namespace boost {
    class thread_group;
} // namespace boost

/** time between pings automatically sent out for latency probing and keepalive (in seconds). */
static const int ping_interval = 2 * 60;
/** time after which to disconnect, after waiting for a ping response (or inactivity). */
static const int timeout_interval = 20 * 60;
/** the maximum number of entries in an 'inv' protocol message */
static const unsigned int max_inv_sz = 50000;
/** the maximum number of new addresses to accumulate before announcing. */
static const unsigned int max_addr_to_send = 1000;
/** maximum length of incoming protocol messages (no message over 2 mib is currently acceptable). */
static const unsigned int max_protocol_message_length = 2 * 1024 * 1024;
/** -listen default */
static const bool default_listen = true;
/** -upnp default */
#ifdef use_upnp
static const bool default_upnp = use_upnp;
#else
static const bool default_upnp = false;
#endif
/** the maximum number of entries in mapaskfor */
static const size_t mapaskfor_max_sz = max_inv_sz;

unsigned int receivefloodsize();
unsigned int sendbuffersize();

void addoneshot(const std::string& strdest);
void addresscurrentlyconnected(const cservice& addr);
cnode* findnode(const cnetaddr& ip);
cnode* findnode(const csubnet& subnet);
cnode* findnode(const std::string& addrname);
cnode* findnode(const cservice& ip);
cnode* connectnode(caddress addrconnect, const char *pszdest = null);
bool opennetworkconnection(const caddress& addrconnect, csemaphoregrant *grantoutbound = null, const char *strdest = null, bool foneshot = false);
void mapport(bool fuseupnp);
unsigned short getlistenport();
bool bindlistenport(const cservice &bindaddr, std::string& strerror, bool fwhitelisted = false);
void startnode(boost::thread_group& threadgroup, cscheduler& scheduler);
bool stopnode();
void socketsenddata(cnode *pnode);

typedef int nodeid;

struct combinerall
{
    typedef bool result_type;

    template<typename i>
    bool operator()(i first, i last) const
    {
        while (first != last) {
            if (!(*first)) return false;
            ++first;
        }
        return true;
    }
};

// signals for message handling
struct cnodesignals
{
    boost::signals2::signal<int ()> getheight;
    boost::signals2::signal<bool (cnode*), combinerall> processmessages;
    boost::signals2::signal<bool (cnode*, bool), combinerall> sendmessages;
    boost::signals2::signal<void (nodeid, const cnode*)> initializenode;
    boost::signals2::signal<void (nodeid)> finalizenode;
};


cnodesignals& getnodesignals();


enum
{
    local_none,   // unknown
    local_if,     // address a local interface listens on
    local_bind,   // address explicit bound to
    local_upnp,   // address reported by upnp
    local_manual, // address explicitly specified (-externalip=)

    local_max
};

bool ispeeraddrlocalgood(cnode *pnode);
void advertizelocal(cnode *pnode);
void setlimited(enum network net, bool flimited = true);
bool islimited(enum network net);
bool islimited(const cnetaddr& addr);
bool addlocal(const cservice& addr, int nscore = local_none);
bool addlocal(const cnetaddr& addr, int nscore = local_none);
bool seenlocal(const cservice& addr);
bool islocal(const cservice& addr);
bool getlocal(cservice &addr, const cnetaddr *paddrpeer = null);
bool isreachable(enum network net);
bool isreachable(const cnetaddr &addr);
void setreachable(enum network net, bool fflag = true);
caddress getlocaladdress(const cnetaddr *paddrpeer = null);


extern bool fdiscover;
extern bool flisten;
extern uint64_t nlocalservices;
extern uint64_t nlocalhostnonce;
extern caddrman addrman;
extern int nmaxconnections;

extern std::vector<cnode*> vnodes;
extern ccriticalsection cs_vnodes;
extern std::map<cinv, cdatastream> maprelay;
extern std::deque<std::pair<int64_t, cinv> > vrelayexpiration;
extern ccriticalsection cs_maprelay;
extern limitedmap<cinv, int64_t> mapalreadyaskedfor;

extern std::vector<std::string> vaddednodes;
extern ccriticalsection cs_vaddednodes;

extern nodeid nlastnodeid;
extern ccriticalsection cs_nlastnodeid;

struct localserviceinfo {
    int nscore;
    int nport;
};

extern ccriticalsection cs_maplocalhost;
extern std::map<cnetaddr, localserviceinfo> maplocalhost;

class cnodestats
{
public:
    nodeid nodeid;
    uint64_t nservices;
    int64_t nlastsend;
    int64_t nlastrecv;
    int64_t ntimeconnected;
    int64_t ntimeoffset;
    std::string addrname;
    int nversion;
    std::string cleansubver;
    bool finbound;
    int nstartingheight;
    uint64_t nsendbytes;
    uint64_t nrecvbytes;
    bool fwhitelisted;
    double dpingtime;
    double dpingwait;
    std::string addrlocal;
};




class cnetmessage {
public:
    bool in_data;                   // parsing header (false) or data (true)

    cdatastream hdrbuf;             // partially received header
    cmessageheader hdr;             // complete header
    unsigned int nhdrpos;

    cdatastream vrecv;              // received message data
    unsigned int ndatapos;

    int64_t ntime;                  // time (in microseconds) of message receipt.

    cnetmessage(const cmessageheader::messagestartchars& pchmessagestartin, int ntypein, int nversionin) : hdrbuf(ntypein, nversionin), hdr(pchmessagestartin), vrecv(ntypein, nversionin) {
        hdrbuf.resize(24);
        in_data = false;
        nhdrpos = 0;
        ndatapos = 0;
        ntime = 0;
    }

    bool complete() const
    {
        if (!in_data)
            return false;
        return (hdr.nmessagesize == ndatapos);
    }

    void setversion(int nversionin)
    {
        hdrbuf.setversion(nversionin);
        vrecv.setversion(nversionin);
    }

    int readheader(const char *pch, unsigned int nbytes);
    int readdata(const char *pch, unsigned int nbytes);
};





/** information about a peer */
class cnode
{
public:
    // socket
    uint64_t nservices;
    socket hsocket;
    cdatastream sssend;
    size_t nsendsize; // total size of all vsendmsg entries
    size_t nsendoffset; // offset inside the first vsendmsg already sent
    uint64_t nsendbytes;
    std::deque<cserializedata> vsendmsg;
    ccriticalsection cs_vsend;

    std::deque<cinv> vrecvgetdata;
    std::deque<cnetmessage> vrecvmsg;
    ccriticalsection cs_vrecvmsg;
    uint64_t nrecvbytes;
    int nrecvversion;

    int64_t nlastsend;
    int64_t nlastrecv;
    int64_t ntimeconnected;
    int64_t ntimeoffset;
    caddress addr;
    std::string addrname;
    cservice addrlocal;
    int nversion;
    // strsubver is whatever byte array we read from the wire. however, this field is intended
    // to be printed out, displayed to humans in various forms and so on. so we sanitize it and
    // store the sanitized version in cleansubver. the original should be used when dealing with
    // the network or wire types and the cleaned string used when displayed or logged.
    std::string strsubver, cleansubver;
    bool fwhitelisted; // this peer can bypass dos banning.
    bool foneshot;
    bool fclient;
    bool finbound;
    bool fnetworknode;
    bool fsuccessfullyconnected;
    bool fdisconnect;
    // we use frelaytxes for two purposes -
    // a) it allows us to not relay tx invs before receiving the peer's version message
    // b) the peer may tell us in its version message that we should not relay tx invs
    //    until it has initialized its bloom filter.
    bool frelaytxes;
    csemaphoregrant grantoutbound;
    ccriticalsection cs_filter;
    cbloomfilter* pfilter;
    int nrefcount;
    nodeid id;
protected:

    // denial-of-service detection/prevention
    // key is ip address, value is banned-until-time
    static std::map<csubnet, int64_t> setbanned;
    static ccriticalsection cs_setbanned;

    // whitelisted ranges. any node connecting from these is automatically
    // whitelisted (as well as those connecting to whitelisted binds).
    static std::vector<csubnet> vwhitelistedrange;
    static ccriticalsection cs_vwhitelistedrange;

    // basic fuzz-testing
    void fuzz(int nchance); // modifies sssend

public:
    uint256 hashcontinue;
    int nstartingheight;

    // flood relay
    std::vector<caddress> vaddrtosend;
    crollingbloomfilter addrknown;
    bool fgetaddr;
    std::set<uint256> setknown;

    // inventory based relay
    mruset<cinv> setinventoryknown;
    std::vector<cinv> vinventorytosend;
    ccriticalsection cs_inventory;
    std::multimap<int64_t, cinv> mapaskfor;

    // ping time measurement:
    // the pong reply we're expecting, or 0 if no pong expected.
    uint64_t npingnoncesent;
    // time (in usec) the last ping was sent, or 0 if no ping was ever sent.
    int64_t npingusecstart;
    // last measured round-trip time.
    int64_t npingusectime;
    // whether a ping is requested.
    bool fpingqueued;

    cnode(socket hsocketin, const caddress &addrin, const std::string &addrnamein = "", bool finboundin = false);
    ~cnode();

private:
    // network usage totals
    static ccriticalsection cs_totalbytesrecv;
    static ccriticalsection cs_totalbytessent;
    static uint64_t ntotalbytesrecv;
    static uint64_t ntotalbytessent;

    cnode(const cnode&);
    void operator=(const cnode&);

public:

    nodeid getid() const {
      return id;
    }

    int getrefcount()
    {
        assert(nrefcount >= 0);
        return nrefcount;
    }

    // requires lock(cs_vrecvmsg)
    unsigned int gettotalrecvsize()
    {
        unsigned int total = 0;
        boost_foreach(const cnetmessage &msg, vrecvmsg)
            total += msg.vrecv.size() + 24;
        return total;
    }

    // requires lock(cs_vrecvmsg)
    bool receivemsgbytes(const char *pch, unsigned int nbytes);

    // requires lock(cs_vrecvmsg)
    void setrecvversion(int nversionin)
    {
        nrecvversion = nversionin;
        boost_foreach(cnetmessage &msg, vrecvmsg)
            msg.setversion(nversionin);
    }

    cnode* addref()
    {
        nrefcount++;
        return this;
    }

    void release()
    {
        nrefcount--;
    }



    void addaddressknown(const caddress& addr)
    {
        addrknown.insert(addr.getkey());
    }

    void pushaddress(const caddress& addr)
    {
        // known checking here is only to save space from duplicates.
        // sendmessages will filter it again for knowns that were added
        // after addresses were pushed.
        if (addr.isvalid() && !addrknown.contains(addr.getkey())) {
            if (vaddrtosend.size() >= max_addr_to_send) {
                vaddrtosend[insecure_rand() % vaddrtosend.size()] = addr;
            } else {
                vaddrtosend.push_back(addr);
            }
        }
    }


    void addinventoryknown(const cinv& inv)
    {
        {
            lock(cs_inventory);
            setinventoryknown.insert(inv);
        }
    }

    void pushinventory(const cinv& inv)
    {
        {
            lock(cs_inventory);
            if (!setinventoryknown.count(inv))
                vinventorytosend.push_back(inv);
        }
    }

    void askfor(const cinv& inv);

    // todo: document the postcondition of this function.  is cs_vsend locked?
    void beginmessage(const char* pszcommand) exclusive_lock_function(cs_vsend);

    // todo: document the precondition of this function.  is cs_vsend locked?
    void abortmessage() unlock_function(cs_vsend);

    // todo: document the precondition of this function.  is cs_vsend locked?
    void endmessage() unlock_function(cs_vsend);

    void pushversion();


    void pushmessage(const char* pszcommand)
    {
        try
        {
            beginmessage(pszcommand);
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1>
    void pushmessage(const char* pszcommand, const t1& a1)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3, typename t4>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3, const t4& a4)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3 << a4;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3, typename t4, typename t5>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3, const t4& a4, const t5& a5)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3 << a4 << a5;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3, typename t4, typename t5, typename t6>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3, const t4& a4, const t5& a5, const t6& a6)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3 << a4 << a5 << a6;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3, typename t4, typename t5, typename t6, typename t7>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3, const t4& a4, const t5& a5, const t6& a6, const t7& a7)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3 << a4 << a5 << a6 << a7;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3, typename t4, typename t5, typename t6, typename t7, typename t8>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3, const t4& a4, const t5& a5, const t6& a6, const t7& a7, const t8& a8)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    template<typename t1, typename t2, typename t3, typename t4, typename t5, typename t6, typename t7, typename t8, typename t9>
    void pushmessage(const char* pszcommand, const t1& a1, const t2& a2, const t3& a3, const t4& a4, const t5& a5, const t6& a6, const t7& a7, const t8& a8, const t9& a9)
    {
        try
        {
            beginmessage(pszcommand);
            sssend << a1 << a2 << a3 << a4 << a5 << a6 << a7 << a8 << a9;
            endmessage();
        }
        catch (...)
        {
            abortmessage();
            throw;
        }
    }

    void closesocketdisconnect();

    // denial-of-service detection/prevention
    // the idea is to detect peers that are behaving
    // badly and disconnect/ban them, but do it in a
    // one-coding-mistake-won't-shatter-the-entire-network
    // way.
    // important:  there should be nothing i can give a
    // node that it will forward on that will make that
    // node's peers drop it. if there is, an attacker
    // can isolate a node and/or try to split the network.
    // dropping a node for sending stuff that is invalid
    // now but might be valid in a later version is also
    // dangerous, because it can cause a network split
    // between nodes running old code and nodes running
    // new code.
    static void clearbanned(); // needed for unit testing
    static bool isbanned(cnetaddr ip);
    static bool isbanned(csubnet subnet);
    static void ban(const cnetaddr &ip, int64_t bantimeoffset = 0, bool sinceunixepoch = false);
    static void ban(const csubnet &subnet, int64_t bantimeoffset = 0, bool sinceunixepoch = false);
    static bool unban(const cnetaddr &ip);
    static bool unban(const csubnet &ip);
    static void getbanned(std::map<csubnet, int64_t> &banmap);

    void copystats(cnodestats &stats);

    static bool iswhitelistedrange(const cnetaddr &ip);
    static void addwhitelistedrange(const csubnet &subnet);

    // network stats
    static void recordbytesrecv(uint64_t bytes);
    static void recordbytessent(uint64_t bytes);

    static uint64_t gettotalbytesrecv();
    static uint64_t gettotalbytessent();
};



class ctransaction;
void relaytransaction(const ctransaction& tx);
void relaytransaction(const ctransaction& tx, const cdatastream& ss);

/** access to the (ip) address database (peers.dat) */
class caddrdb
{
private:
    boost::filesystem::path pathaddr;
public:
    caddrdb();
    bool write(const caddrman& addr);
    bool read(caddrman& addr);
};

#endif // moorecoin_net_h
