// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "net.h"

#include "addrman.h"
#include "chainparams.h"
#include "clientversion.h"
#include "primitives/transaction.h"
#include "scheduler.h"
#include "ui_interface.h"
#include "crypto/common.h"

#ifdef win32
#include <string.h>
#else
#include <fcntl.h>
#endif

#ifdef use_upnp
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/miniwget.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

// dump addresses to peers.dat every 15 minutes (900s)
#define dump_addresses_interval 900

#if !defined(have_msg_nosignal) && !defined(msg_nosignal)
#define msg_nosignal 0
#endif

// fix for ancient mingw versions, that don't have defined these in ws2tcpip.h.
// todo: can be removed when our pull-tester is upgraded to a modern mingw version.
#ifdef win32
#ifndef protection_level_unrestricted
#define protection_level_unrestricted 10
#endif
#ifndef ipv6_protection_level
#define ipv6_protection_level 23
#endif
#endif

using namespace std;

namespace {
    const int max_outbound_connections = 8;

    struct listensocket {
        socket socket;
        bool whitelisted;

        listensocket(socket socket, bool whitelisted) : socket(socket), whitelisted(whitelisted) {}
    };
}

//
// global state variables
//
bool fdiscover = true;
bool flisten = true;
uint64_t nlocalservices = node_network;
ccriticalsection cs_maplocalhost;
map<cnetaddr, localserviceinfo> maplocalhost;
static bool vfreachable[net_max] = {};
static bool vflimited[net_max] = {};
static cnode* pnodelocalhost = null;
uint64_t nlocalhostnonce = 0;
static std::vector<listensocket> vhlistensocket;
caddrman addrman;
int nmaxconnections = 125;
bool faddressesinitialized = false;

vector<cnode*> vnodes;
ccriticalsection cs_vnodes;
map<cinv, cdatastream> maprelay;
deque<pair<int64_t, cinv> > vrelayexpiration;
ccriticalsection cs_maprelay;
limitedmap<cinv, int64_t> mapalreadyaskedfor(max_inv_sz);

static deque<string> voneshots;
ccriticalsection cs_voneshots;

set<cnetaddr> setservaddnodeaddresses;
ccriticalsection cs_setservaddnodeaddresses;

vector<std::string> vaddednodes;
ccriticalsection cs_vaddednodes;

nodeid nlastnodeid = 0;
ccriticalsection cs_nlastnodeid;

static csemaphore *semoutbound = null;
boost::condition_variable messagehandlercondition;

// signals for message handling
static cnodesignals g_signals;
cnodesignals& getnodesignals() { return g_signals; }

void addoneshot(const std::string& strdest)
{
    lock(cs_voneshots);
    voneshots.push_back(strdest);
}

unsigned short getlistenport()
{
    return (unsigned short)(getarg("-port", params().getdefaultport()));
}

// find 'best' local address for a particular peer
bool getlocal(cservice& addr, const cnetaddr *paddrpeer)
{
    if (!flisten)
        return false;

    int nbestscore = -1;
    int nbestreachability = -1;
    {
        lock(cs_maplocalhost);
        for (map<cnetaddr, localserviceinfo>::iterator it = maplocalhost.begin(); it != maplocalhost.end(); it++)
        {
            int nscore = (*it).second.nscore;
            int nreachability = (*it).first.getreachabilityfrom(paddrpeer);
            if (nreachability > nbestreachability || (nreachability == nbestreachability && nscore > nbestscore))
            {
                addr = cservice((*it).first, (*it).second.nport);
                nbestreachability = nreachability;
                nbestscore = nscore;
            }
        }
    }
    return nbestscore >= 0;
}

//! convert the pnseeds6 array into usable address objects.
static std::vector<caddress> convertseed6(const std::vector<seedspec6> &vseedsin)
{
    // it'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t noneweek = 7*24*60*60;
    std::vector<caddress> vseedsout;
    vseedsout.reserve(vseedsin.size());
    for (std::vector<seedspec6>::const_iterator i(vseedsin.begin()); i != vseedsin.end(); ++i)
    {
        struct in6_addr ip;
        memcpy(&ip, i->addr, sizeof(ip));
        caddress addr(cservice(ip, i->port));
        addr.ntime = gettime() - getrand(noneweek) - noneweek;
        vseedsout.push_back(addr);
    }
    return vseedsout;
}

// get best local address for a particular peer as a caddress
// otherwise, return the unroutable 0.0.0.0 but filled in with
// the normal parameters, since the ip may be changed to a useful
// one by discovery.
caddress getlocaladdress(const cnetaddr *paddrpeer)
{
    caddress ret(cservice("0.0.0.0",getlistenport()),0);
    cservice addr;
    if (getlocal(addr, paddrpeer))
    {
        ret = caddress(addr);
    }
    ret.nservices = nlocalservices;
    ret.ntime = getadjustedtime();
    return ret;
}

int getnscore(const cservice& addr)
{
    lock(cs_maplocalhost);
    if (maplocalhost.count(addr) == local_none)
        return 0;
    return maplocalhost[addr].nscore;
}

// is our peer's addrlocal potentially useful as an external ip source?
bool ispeeraddrlocalgood(cnode *pnode)
{
    return fdiscover && pnode->addr.isroutable() && pnode->addrlocal.isroutable() &&
           !islimited(pnode->addrlocal.getnetwork());
}

// pushes our own address to a peer
void advertizelocal(cnode *pnode)
{
    if (flisten && pnode->fsuccessfullyconnected)
    {
        caddress addrlocal = getlocaladdress(&pnode->addr);
        // if discovery is enabled, sometimes give our peer the address it
        // tells us that it sees us as in case it has a better idea of our
        // address than we do.
        if (ispeeraddrlocalgood(pnode) && (!addrlocal.isroutable() ||
             getrand((getnscore(addrlocal) > local_manual) ? 8:2) == 0))
        {
            addrlocal.setip(pnode->addrlocal);
        }
        if (addrlocal.isroutable())
        {
            pnode->pushaddress(addrlocal);
        }
    }
}

void setreachable(enum network net, bool fflag)
{
    lock(cs_maplocalhost);
    vfreachable[net] = fflag;
    if (net == net_ipv6 && fflag)
        vfreachable[net_ipv4] = true;
}

// learn a new local address
bool addlocal(const cservice& addr, int nscore)
{
    if (!addr.isroutable())
        return false;

    if (!fdiscover && nscore < local_manual)
        return false;

    if (islimited(addr))
        return false;

    logprintf("addlocal(%s,%i)\n", addr.tostring(), nscore);

    {
        lock(cs_maplocalhost);
        bool falready = maplocalhost.count(addr) > 0;
        localserviceinfo &info = maplocalhost[addr];
        if (!falready || nscore >= info.nscore) {
            info.nscore = nscore + (falready ? 1 : 0);
            info.nport = addr.getport();
        }
        setreachable(addr.getnetwork());
    }

    return true;
}

bool addlocal(const cnetaddr &addr, int nscore)
{
    return addlocal(cservice(addr, getlistenport()), nscore);
}

/** make a particular network entirely off-limits (no automatic connects to it) */
void setlimited(enum network net, bool flimited)
{
    if (net == net_unroutable)
        return;
    lock(cs_maplocalhost);
    vflimited[net] = flimited;
}

bool islimited(enum network net)
{
    lock(cs_maplocalhost);
    return vflimited[net];
}

bool islimited(const cnetaddr &addr)
{
    return islimited(addr.getnetwork());
}

/** vote for a local address */
bool seenlocal(const cservice& addr)
{
    {
        lock(cs_maplocalhost);
        if (maplocalhost.count(addr) == 0)
            return false;
        maplocalhost[addr].nscore++;
    }
    return true;
}


/** check whether a given address is potentially local */
bool islocal(const cservice& addr)
{
    lock(cs_maplocalhost);
    return maplocalhost.count(addr) > 0;
}

/** check whether a given network is one we can probably connect to */
bool isreachable(enum network net)
{
    lock(cs_maplocalhost);
    return vfreachable[net] && !vflimited[net];
}

/** check whether a given address is in a network we can probably connect to */
bool isreachable(const cnetaddr& addr)
{
    enum network net = addr.getnetwork();
    return isreachable(net);
}

void addresscurrentlyconnected(const cservice& addr)
{
    addrman.connected(addr);
}


uint64_t cnode::ntotalbytesrecv = 0;
uint64_t cnode::ntotalbytessent = 0;
ccriticalsection cnode::cs_totalbytesrecv;
ccriticalsection cnode::cs_totalbytessent;

cnode* findnode(const cnetaddr& ip)
{
    lock(cs_vnodes);
    boost_foreach(cnode* pnode, vnodes)
        if ((cnetaddr)pnode->addr == ip)
            return (pnode);
    return null;
}

cnode* findnode(const csubnet& subnet)
{
    lock(cs_vnodes);
    boost_foreach(cnode* pnode, vnodes)
    if (subnet.match((cnetaddr)pnode->addr))
        return (pnode);
    return null;
}

cnode* findnode(const std::string& addrname)
{
    lock(cs_vnodes);
    boost_foreach(cnode* pnode, vnodes)
        if (pnode->addrname == addrname)
            return (pnode);
    return null;
}

cnode* findnode(const cservice& addr)
{
    lock(cs_vnodes);
    boost_foreach(cnode* pnode, vnodes)
        if ((cservice)pnode->addr == addr)
            return (pnode);
    return null;
}

cnode* connectnode(caddress addrconnect, const char *pszdest)
{
    if (pszdest == null) {
        if (islocal(addrconnect))
            return null;

        // look for an existing connection
        cnode* pnode = findnode((cservice)addrconnect);
        if (pnode)
        {
            pnode->addref();
            return pnode;
        }
    }

    /// debug print
    logprint("net", "trying connection %s lastseen=%.1fhrs\n",
        pszdest ? pszdest : addrconnect.tostring(),
        pszdest ? 0.0 : (double)(getadjustedtime() - addrconnect.ntime)/3600.0);

    // connect
    socket hsocket;
    bool proxyconnectionfailed = false;
    if (pszdest ? connectsocketbyname(addrconnect, hsocket, pszdest, params().getdefaultport(), nconnecttimeout, &proxyconnectionfailed) :
                  connectsocket(addrconnect, hsocket, nconnecttimeout, &proxyconnectionfailed))
    {
        addrman.attempt(addrconnect);

        // add node
        cnode* pnode = new cnode(hsocket, addrconnect, pszdest ? pszdest : "", false);
        pnode->addref();

        {
            lock(cs_vnodes);
            vnodes.push_back(pnode);
        }

        pnode->ntimeconnected = gettime();

        return pnode;
    } else if (!proxyconnectionfailed) {
        // if connecting to the node failed, and failure is not caused by a problem connecting to
        // the proxy, mark this as an attempt.
        addrman.attempt(addrconnect);
    }

    return null;
}

void cnode::closesocketdisconnect()
{
    fdisconnect = true;
    if (hsocket != invalid_socket)
    {
        logprint("net", "disconnecting peer=%d\n", id);
        closesocket(hsocket);
    }

    // in case this fails, we'll empty the recv buffer when the cnode is deleted
    try_lock(cs_vrecvmsg, lockrecv);
    if (lockrecv)
        vrecvmsg.clear();
}

void cnode::pushversion()
{
    int nbestheight = g_signals.getheight().get_value_or(0);

    int64_t ntime = (finbound ? getadjustedtime() : gettime());
    caddress addryou = (addr.isroutable() && !isproxy(addr) ? addr : caddress(cservice("0.0.0.0",0)));
    caddress addrme = getlocaladdress(&addr);
    getrandbytes((unsigned char*)&nlocalhostnonce, sizeof(nlocalhostnonce));
    if (flogips)
        logprint("net", "send version message: version %d, blocks=%d, us=%s, them=%s, peer=%d\n", protocol_version, nbestheight, addrme.tostring(), addryou.tostring(), id);
    else
        logprint("net", "send version message: version %d, blocks=%d, us=%s, peer=%d\n", protocol_version, nbestheight, addrme.tostring(), id);
    pushmessage("version", protocol_version, nlocalservices, ntime, addryou, addrme,
                nlocalhostnonce, formatsubversion(client_name, client_version, std::vector<string>()), nbestheight, true);
}





std::map<csubnet, int64_t> cnode::setbanned;
ccriticalsection cnode::cs_setbanned;

void cnode::clearbanned()
{
    lock(cs_setbanned);
    setbanned.clear();
}

bool cnode::isbanned(cnetaddr ip)
{
    bool fresult = false;
    {
        lock(cs_setbanned);
        for (std::map<csubnet, int64_t>::iterator it = setbanned.begin(); it != setbanned.end(); it++)
        {
            csubnet subnet = (*it).first;
            int64_t t = (*it).second;

            if(subnet.match(ip) && gettime() < t)
                fresult = true;
        }
    }
    return fresult;
}

bool cnode::isbanned(csubnet subnet)
{
    bool fresult = false;
    {
        lock(cs_setbanned);
        std::map<csubnet, int64_t>::iterator i = setbanned.find(subnet);
        if (i != setbanned.end())
        {
            int64_t t = (*i).second;
            if (gettime() < t)
                fresult = true;
        }
    }
    return fresult;
}

void cnode::ban(const cnetaddr& addr, int64_t bantimeoffset, bool sinceunixepoch) {
    csubnet subnet(addr.tostring()+(addr.isipv4() ? "/32" : "/128"));
    ban(subnet, bantimeoffset, sinceunixepoch);
}

void cnode::ban(const csubnet& subnet, int64_t bantimeoffset, bool sinceunixepoch) {
    int64_t bantime = gettime()+getarg("-bantime", 60*60*24);  // default 24-hour ban
    if (bantimeoffset > 0)
        bantime = (sinceunixepoch ? 0 : gettime() )+bantimeoffset;

    lock(cs_setbanned);
    if (setbanned[subnet] < bantime)
        setbanned[subnet] = bantime;
}

bool cnode::unban(const cnetaddr &addr) {
    csubnet subnet(addr.tostring()+(addr.isipv4() ? "/32" : "/128"));
    return unban(subnet);
}

bool cnode::unban(const csubnet &subnet) {
    lock(cs_setbanned);
    if (setbanned.erase(subnet))
        return true;
    return false;
}

void cnode::getbanned(std::map<csubnet, int64_t> &banmap)
{
    lock(cs_setbanned);
    banmap = setbanned; //create a thread safe copy
}


std::vector<csubnet> cnode::vwhitelistedrange;
ccriticalsection cnode::cs_vwhitelistedrange;

bool cnode::iswhitelistedrange(const cnetaddr &addr) {
    lock(cs_vwhitelistedrange);
    boost_foreach(const csubnet& subnet, vwhitelistedrange) {
        if (subnet.match(addr))
            return true;
    }
    return false;
}

void cnode::addwhitelistedrange(const csubnet &subnet) {
    lock(cs_vwhitelistedrange);
    vwhitelistedrange.push_back(subnet);
}

#undef x
#define x(name) stats.name = name
void cnode::copystats(cnodestats &stats)
{
    stats.nodeid = this->getid();
    x(nservices);
    x(nlastsend);
    x(nlastrecv);
    x(ntimeconnected);
    x(ntimeoffset);
    x(addrname);
    x(nversion);
    x(cleansubver);
    x(finbound);
    x(nstartingheight);
    x(nsendbytes);
    x(nrecvbytes);
    x(fwhitelisted);

    // it is common for nodes with good ping times to suddenly become lagged,
    // due to a new block arriving or other large transfer.
    // merely reporting pingtime might fool the caller into thinking the node was still responsive,
    // since pingtime does not update until the ping is complete, which might take a while.
    // so, if a ping is taking an unusually long time in flight,
    // the caller can immediately detect that this is happening.
    int64_t npingusecwait = 0;
    if ((0 != npingnoncesent) && (0 != npingusecstart)) {
        npingusecwait = gettimemicros() - npingusecstart;
    }

    // raw ping time is in microseconds, but show it to user as whole seconds (moorecoin users should be well used to small numbers with many decimal places by now :)
    stats.dpingtime = (((double)npingusectime) / 1e6);
    stats.dpingwait = (((double)npingusecwait) / 1e6);

    // leave string empty if addrlocal invalid (not filled in yet)
    stats.addrlocal = addrlocal.isvalid() ? addrlocal.tostring() : "";
}
#undef x

// requires lock(cs_vrecvmsg)
bool cnode::receivemsgbytes(const char *pch, unsigned int nbytes)
{
    while (nbytes > 0) {

        // get current incomplete message, or create a new one
        if (vrecvmsg.empty() ||
            vrecvmsg.back().complete())
            vrecvmsg.push_back(cnetmessage(params().messagestart(), ser_network, nrecvversion));

        cnetmessage& msg = vrecvmsg.back();

        // absorb network data
        int handled;
        if (!msg.in_data)
            handled = msg.readheader(pch, nbytes);
        else
            handled = msg.readdata(pch, nbytes);

        if (handled < 0)
                return false;

        if (msg.in_data && msg.hdr.nmessagesize > max_protocol_message_length) {
            logprint("net", "oversized message from peer=%i, disconnecting", getid());
            return false;
        }

        pch += handled;
        nbytes -= handled;

        if (msg.complete()) {
            msg.ntime = gettimemicros();
            messagehandlercondition.notify_one();
        }
    }

    return true;
}

int cnetmessage::readheader(const char *pch, unsigned int nbytes)
{
    // copy data to temporary parsing buffer
    unsigned int nremaining = 24 - nhdrpos;
    unsigned int ncopy = std::min(nremaining, nbytes);

    memcpy(&hdrbuf[nhdrpos], pch, ncopy);
    nhdrpos += ncopy;

    // if header incomplete, exit
    if (nhdrpos < 24)
        return ncopy;

    // deserialize to cmessageheader
    try {
        hdrbuf >> hdr;
    }
    catch (const std::exception&) {
        return -1;
    }

    // reject messages larger than max_size
    if (hdr.nmessagesize > max_size)
            return -1;

    // switch state to reading message data
    in_data = true;

    return ncopy;
}

int cnetmessage::readdata(const char *pch, unsigned int nbytes)
{
    unsigned int nremaining = hdr.nmessagesize - ndatapos;
    unsigned int ncopy = std::min(nremaining, nbytes);

    if (vrecv.size() < ndatapos + ncopy) {
        // allocate up to 256 kib ahead, but never more than the total message size.
        vrecv.resize(std::min(hdr.nmessagesize, ndatapos + ncopy + 256 * 1024));
    }

    memcpy(&vrecv[ndatapos], pch, ncopy);
    ndatapos += ncopy;

    return ncopy;
}









// requires lock(cs_vsend)
void socketsenddata(cnode *pnode)
{
    std::deque<cserializedata>::iterator it = pnode->vsendmsg.begin();

    while (it != pnode->vsendmsg.end()) {
        const cserializedata &data = *it;
        assert(data.size() > pnode->nsendoffset);
        int nbytes = send(pnode->hsocket, &data[pnode->nsendoffset], data.size() - pnode->nsendoffset, msg_nosignal | msg_dontwait);
        if (nbytes > 0) {
            pnode->nlastsend = gettime();
            pnode->nsendbytes += nbytes;
            pnode->nsendoffset += nbytes;
            pnode->recordbytessent(nbytes);
            if (pnode->nsendoffset == data.size()) {
                pnode->nsendoffset = 0;
                pnode->nsendsize -= data.size();
                it++;
            } else {
                // could not send full message; stop sending more
                break;
            }
        } else {
            if (nbytes < 0) {
                // error
                int nerr = wsagetlasterror();
                if (nerr != wsaewouldblock && nerr != wsaemsgsize && nerr != wsaeintr && nerr != wsaeinprogress)
                {
                    logprintf("socket send error %s\n", networkerrorstring(nerr));
                    pnode->closesocketdisconnect();
                }
            }
            // couldn't send anything at all
            break;
        }
    }

    if (it == pnode->vsendmsg.end()) {
        assert(pnode->nsendoffset == 0);
        assert(pnode->nsendsize == 0);
    }
    pnode->vsendmsg.erase(pnode->vsendmsg.begin(), it);
}

static list<cnode*> vnodesdisconnected;

void threadsockethandler()
{
    unsigned int nprevnodecount = 0;
    while (true)
    {
        //
        // disconnect nodes
        //
        {
            lock(cs_vnodes);
            // disconnect unused nodes
            vector<cnode*> vnodescopy = vnodes;
            boost_foreach(cnode* pnode, vnodescopy)
            {
                if (pnode->fdisconnect ||
                    (pnode->getrefcount() <= 0 && pnode->vrecvmsg.empty() && pnode->nsendsize == 0 && pnode->sssend.empty()))
                {
                    // remove from vnodes
                    vnodes.erase(remove(vnodes.begin(), vnodes.end(), pnode), vnodes.end());

                    // release outbound grant (if any)
                    pnode->grantoutbound.release();

                    // close socket and cleanup
                    pnode->closesocketdisconnect();

                    // hold in disconnected pool until all refs are released
                    if (pnode->fnetworknode || pnode->finbound)
                        pnode->release();
                    vnodesdisconnected.push_back(pnode);
                }
            }
        }
        {
            // delete disconnected nodes
            list<cnode*> vnodesdisconnectedcopy = vnodesdisconnected;
            boost_foreach(cnode* pnode, vnodesdisconnectedcopy)
            {
                // wait until threads are done using it
                if (pnode->getrefcount() <= 0)
                {
                    bool fdelete = false;
                    {
                        try_lock(pnode->cs_vsend, locksend);
                        if (locksend)
                        {
                            try_lock(pnode->cs_vrecvmsg, lockrecv);
                            if (lockrecv)
                            {
                                try_lock(pnode->cs_inventory, lockinv);
                                if (lockinv)
                                    fdelete = true;
                            }
                        }
                    }
                    if (fdelete)
                    {
                        vnodesdisconnected.remove(pnode);
                        delete pnode;
                    }
                }
            }
        }
        if(vnodes.size() != nprevnodecount) {
            nprevnodecount = vnodes.size();
            uiinterface.notifynumconnectionschanged(nprevnodecount);
        }

        //
        // find which sockets have data to receive
        //
        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000; // frequency to poll pnode->vsend

        fd_set fdsetrecv;
        fd_set fdsetsend;
        fd_set fdseterror;
        fd_zero(&fdsetrecv);
        fd_zero(&fdsetsend);
        fd_zero(&fdseterror);
        socket hsocketmax = 0;
        bool have_fds = false;

        boost_foreach(const listensocket& hlistensocket, vhlistensocket) {
            fd_set(hlistensocket.socket, &fdsetrecv);
            hsocketmax = max(hsocketmax, hlistensocket.socket);
            have_fds = true;
        }

        {
            lock(cs_vnodes);
            boost_foreach(cnode* pnode, vnodes)
            {
                if (pnode->hsocket == invalid_socket)
                    continue;
                fd_set(pnode->hsocket, &fdseterror);
                hsocketmax = max(hsocketmax, pnode->hsocket);
                have_fds = true;

                // implement the following logic:
                // * if there is data to send, select() for sending data. as this only
                //   happens when optimistic write failed, we choose to first drain the
                //   write buffer in this case before receiving more. this avoids
                //   needlessly queueing received data, if the remote peer is not themselves
                //   receiving data. this means properly utilizing tcp flow control signalling.
                // * otherwise, if there is no (complete) message in the receive buffer,
                //   or there is space left in the buffer, select() for receiving data.
                // * (if neither of the above applies, there is certainly one message
                //   in the receiver buffer ready to be processed).
                // together, that means that at least one of the following is always possible,
                // so we don't deadlock:
                // * we send some data.
                // * we wait for data to be received (and disconnect after timeout).
                // * we process a message in the buffer (message handler thread).
                {
                    try_lock(pnode->cs_vsend, locksend);
                    if (locksend && !pnode->vsendmsg.empty()) {
                        fd_set(pnode->hsocket, &fdsetsend);
                        continue;
                    }
                }
                {
                    try_lock(pnode->cs_vrecvmsg, lockrecv);
                    if (lockrecv && (
                        pnode->vrecvmsg.empty() || !pnode->vrecvmsg.front().complete() ||
                        pnode->gettotalrecvsize() <= receivefloodsize()))
                        fd_set(pnode->hsocket, &fdsetrecv);
                }
            }
        }

        int nselect = select(have_fds ? hsocketmax + 1 : 0,
                             &fdsetrecv, &fdsetsend, &fdseterror, &timeout);
        boost::this_thread::interruption_point();

        if (nselect == socket_error)
        {
            if (have_fds)
            {
                int nerr = wsagetlasterror();
                logprintf("socket select error %s\n", networkerrorstring(nerr));
                for (unsigned int i = 0; i <= hsocketmax; i++)
                    fd_set(i, &fdsetrecv);
            }
            fd_zero(&fdsetsend);
            fd_zero(&fdseterror);
            millisleep(timeout.tv_usec/1000);
        }

        //
        // accept new connections
        //
        boost_foreach(const listensocket& hlistensocket, vhlistensocket)
        {
            if (hlistensocket.socket != invalid_socket && fd_isset(hlistensocket.socket, &fdsetrecv))
            {
                struct sockaddr_storage sockaddr;
                socklen_t len = sizeof(sockaddr);
                socket hsocket = accept(hlistensocket.socket, (struct sockaddr*)&sockaddr, &len);
                caddress addr;
                int ninbound = 0;

                if (hsocket != invalid_socket)
                    if (!addr.setsockaddr((const struct sockaddr*)&sockaddr))
                        logprintf("warning: unknown socket family\n");

                bool whitelisted = hlistensocket.whitelisted || cnode::iswhitelistedrange(addr);
                {
                    lock(cs_vnodes);
                    boost_foreach(cnode* pnode, vnodes)
                        if (pnode->finbound)
                            ninbound++;
                }

                if (hsocket == invalid_socket)
                {
                    int nerr = wsagetlasterror();
                    if (nerr != wsaewouldblock)
                        logprintf("socket error accept failed: %s\n", networkerrorstring(nerr));
                }
                else if (ninbound >= nmaxconnections - max_outbound_connections)
                {
                    closesocket(hsocket);
                }
                else if (cnode::isbanned(addr) && !whitelisted)
                {
                    logprintf("connection from %s dropped (banned)\n", addr.tostring());
                    closesocket(hsocket);
                }
                else
                {
                    cnode* pnode = new cnode(hsocket, addr, "", true);
                    pnode->addref();
                    pnode->fwhitelisted = whitelisted;

                    {
                        lock(cs_vnodes);
                        vnodes.push_back(pnode);
                    }
                }
            }
        }

        //
        // service each socket
        //
        vector<cnode*> vnodescopy;
        {
            lock(cs_vnodes);
            vnodescopy = vnodes;
            boost_foreach(cnode* pnode, vnodescopy)
                pnode->addref();
        }
        boost_foreach(cnode* pnode, vnodescopy)
        {
            boost::this_thread::interruption_point();

            //
            // receive
            //
            if (pnode->hsocket == invalid_socket)
                continue;
            if (fd_isset(pnode->hsocket, &fdsetrecv) || fd_isset(pnode->hsocket, &fdseterror))
            {
                try_lock(pnode->cs_vrecvmsg, lockrecv);
                if (lockrecv)
                {
                    {
                        // typical socket buffer is 8k-64k
                        char pchbuf[0x10000];
                        int nbytes = recv(pnode->hsocket, pchbuf, sizeof(pchbuf), msg_dontwait);
                        if (nbytes > 0)
                        {
                            if (!pnode->receivemsgbytes(pchbuf, nbytes))
                                pnode->closesocketdisconnect();
                            pnode->nlastrecv = gettime();
                            pnode->nrecvbytes += nbytes;
                            pnode->recordbytesrecv(nbytes);
                        }
                        else if (nbytes == 0)
                        {
                            // socket closed gracefully
                            if (!pnode->fdisconnect)
                                logprint("net", "socket closed\n");
                            pnode->closesocketdisconnect();
                        }
                        else if (nbytes < 0)
                        {
                            // error
                            int nerr = wsagetlasterror();
                            if (nerr != wsaewouldblock && nerr != wsaemsgsize && nerr != wsaeintr && nerr != wsaeinprogress)
                            {
                                if (!pnode->fdisconnect)
                                    logprintf("socket recv error %s\n", networkerrorstring(nerr));
                                pnode->closesocketdisconnect();
                            }
                        }
                    }
                }
            }

            //
            // send
            //
            if (pnode->hsocket == invalid_socket)
                continue;
            if (fd_isset(pnode->hsocket, &fdsetsend))
            {
                try_lock(pnode->cs_vsend, locksend);
                if (locksend)
                    socketsenddata(pnode);
            }

            //
            // inactivity checking
            //
            int64_t ntime = gettime();
            if (ntime - pnode->ntimeconnected > 60)
            {
                if (pnode->nlastrecv == 0 || pnode->nlastsend == 0)
                {
                    logprint("net", "socket no message in first 60 seconds, %d %d from %d\n", pnode->nlastrecv != 0, pnode->nlastsend != 0, pnode->id);
                    pnode->fdisconnect = true;
                }
                else if (ntime - pnode->nlastsend > timeout_interval)
                {
                    logprintf("socket sending timeout: %is\n", ntime - pnode->nlastsend);
                    pnode->fdisconnect = true;
                }
                else if (ntime - pnode->nlastrecv > (pnode->nversion > bip0031_version ? timeout_interval : 90*60))
                {
                    logprintf("socket receive timeout: %is\n", ntime - pnode->nlastrecv);
                    pnode->fdisconnect = true;
                }
                else if (pnode->npingnoncesent && pnode->npingusecstart + timeout_interval * 1000000 < gettimemicros())
                {
                    logprintf("ping timeout: %fs\n", 0.000001 * (gettimemicros() - pnode->npingusecstart));
                    pnode->fdisconnect = true;
                }
            }
        }
        {
            lock(cs_vnodes);
            boost_foreach(cnode* pnode, vnodescopy)
                pnode->release();
        }
    }
}









#ifdef use_upnp
void threadmapport()
{
    std::string port = strprintf("%u", getlistenport());
    const char * multicastif = 0;
    const char * minissdpdpath = 0;
    struct upnpdev * devlist = 0;
    char lanaddr[64];

#ifndef upnpdiscover_success
    /* miniupnpc 1.5 */
    devlist = upnpdiscover(2000, multicastif, minissdpdpath, 0);
#else
    /* miniupnpc 1.6 */
    int error = 0;
    devlist = upnpdiscover(2000, multicastif, minissdpdpath, 0, 0, &error);
#endif

    struct upnpurls urls;
    struct igddatas data;
    int r;

    r = upnp_getvalidigd(devlist, &urls, &data, lanaddr, sizeof(lanaddr));
    if (r == 1)
    {
        if (fdiscover) {
            char externalipaddress[40];
            r = upnp_getexternalipaddress(urls.controlurl, data.first.servicetype, externalipaddress);
            if(r != upnpcommand_success)
                logprintf("upnp: getexternalipaddress() returned %d\n", r);
            else
            {
                if(externalipaddress[0])
                {
                    logprintf("upnp: externalipaddress = %s\n", externalipaddress);
                    addlocal(cnetaddr(externalipaddress), local_upnp);
                }
                else
                    logprintf("upnp: getexternalipaddress failed.\n");
            }
        }

        string strdesc = "moorecoin " + formatfullversion();

        try {
            while (true) {
#ifndef upnpdiscover_success
                /* miniupnpc 1.5 */
                r = upnp_addportmapping(urls.controlurl, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strdesc.c_str(), "tcp", 0);
#else
                /* miniupnpc 1.6 */
                r = upnp_addportmapping(urls.controlurl, data.first.servicetype,
                                    port.c_str(), port.c_str(), lanaddr, strdesc.c_str(), "tcp", 0, "0");
#endif

                if(r!=upnpcommand_success)
                    logprintf("addportmapping(%s, %s, %s) failed with code %d (%s)\n",
                        port, port, lanaddr, r, strupnperror(r));
                else
                    logprintf("upnp port mapping successful.\n");;

                millisleep(20*60*1000); // refresh every 20 minutes
            }
        }
        catch (const boost::thread_interrupted&)
        {
            r = upnp_deleteportmapping(urls.controlurl, data.first.servicetype, port.c_str(), "tcp", 0);
            logprintf("upnp_deleteportmapping() returned: %d\n", r);
            freeupnpdevlist(devlist); devlist = 0;
            freeupnpurls(&urls);
            throw;
        }
    } else {
        logprintf("no valid upnp igds found\n");
        freeupnpdevlist(devlist); devlist = 0;
        if (r != 0)
            freeupnpurls(&urls);
    }
}

void mapport(bool fuseupnp)
{
    static boost::thread* upnp_thread = null;

    if (fuseupnp)
    {
        if (upnp_thread) {
            upnp_thread->interrupt();
            upnp_thread->join();
            delete upnp_thread;
        }
        upnp_thread = new boost::thread(boost::bind(&tracethread<void (*)()>, "upnp", &threadmapport));
    }
    else if (upnp_thread) {
        upnp_thread->interrupt();
        upnp_thread->join();
        delete upnp_thread;
        upnp_thread = null;
    }
}

#else
void mapport(bool)
{
    // intentionally left blank.
}
#endif






void threaddnsaddressseed()
{
    // goal: only query dns seeds if address need is acute
    if ((addrman.size() > 0) &&
        (!getboolarg("-forcednsseed", false))) {
        millisleep(11 * 1000);

        lock(cs_vnodes);
        if (vnodes.size() >= 2) {
            logprintf("p2p peers available. skipped dns seeding.\n");
            return;
        }
    }

    const vector<cdnsseeddata> &vseeds = params().dnsseeds();
    int found = 0;

    logprintf("loading addresses from dns seeds (could take a while)\n");

    boost_foreach(const cdnsseeddata &seed, vseeds) {
        if (havenameproxy()) {
            addoneshot(seed.host);
        } else {
            vector<cnetaddr> vips;
            vector<caddress> vadd;
            if (lookuphost(seed.host.c_str(), vips))
            {
                boost_foreach(const cnetaddr& ip, vips)
                {
                    int noneday = 24*3600;
                    caddress addr = caddress(cservice(ip, params().getdefaultport()));
                    addr.ntime = gettime() - 3*noneday - getrand(4*noneday); // use a random age between 3 and 7 days old
                    vadd.push_back(addr);
                    found++;
                }
            }
            addrman.add(vadd, cnetaddr(seed.name, true));
        }
    }

    logprintf("%d addresses found from dns seeds\n", found);
}












void dumpaddresses()
{
    int64_t nstart = gettimemillis();

    caddrdb adb;
    adb.write(addrman);

    logprint("net", "flushed %d addresses to peers.dat  %dms\n",
           addrman.size(), gettimemillis() - nstart);
}

void static processoneshot()
{
    string strdest;
    {
        lock(cs_voneshots);
        if (voneshots.empty())
            return;
        strdest = voneshots.front();
        voneshots.pop_front();
    }
    caddress addr;
    csemaphoregrant grant(*semoutbound, true);
    if (grant) {
        if (!opennetworkconnection(addr, &grant, strdest.c_str(), true))
            addoneshot(strdest);
    }
}

void threadopenconnections()
{
    // connect to specific addresses
    if (mapargs.count("-connect") && mapmultiargs["-connect"].size() > 0)
    {
        for (int64_t nloop = 0;; nloop++)
        {
            processoneshot();
            boost_foreach(const std::string& straddr, mapmultiargs["-connect"])
            {
                caddress addr;
                opennetworkconnection(addr, null, straddr.c_str());
                for (int i = 0; i < 10 && i < nloop; i++)
                {
                    millisleep(500);
                }
            }
            millisleep(500);
        }
    }

    // initiate network connections
    int64_t nstart = gettime();
    while (true)
    {
        processoneshot();

        millisleep(500);

        csemaphoregrant grant(*semoutbound);
        boost::this_thread::interruption_point();

        // add seed nodes if dns seeds are all down (an infrastructure attack?).
        if (addrman.size() == 0 && (gettime() - nstart > 60)) {
            static bool done = false;
            if (!done) {
                logprintf("adding fixed seed nodes as dns doesn't seem to be available.\n");
                addrman.add(convertseed6(params().fixedseeds()), cnetaddr("127.0.0.1"));
                done = true;
            }
        }

        //
        // choose an address to connect to based on most recently seen
        //
        caddress addrconnect;

        // only connect out to one peer per network group (/16 for ipv4).
        // do this here so we don't have to critsect vnodes inside mapaddresses critsect.
        int noutbound = 0;
        set<vector<unsigned char> > setconnected;
        {
            lock(cs_vnodes);
            boost_foreach(cnode* pnode, vnodes) {
                if (!pnode->finbound) {
                    setconnected.insert(pnode->addr.getgroup());
                    noutbound++;
                }
            }
        }

        int64_t nanow = getadjustedtime();

        int ntries = 0;
        while (true)
        {
            caddrinfo addr = addrman.select();

            // if we selected an invalid address, restart
            if (!addr.isvalid() || setconnected.count(addr.getgroup()) || islocal(addr))
                break;

            // if we didn't find an appropriate destination after trying 100 addresses fetched from addrman,
            // stop this loop, and let the outer loop run again (which sleeps, adds seed nodes, recalculates
            // already-connected network ranges, ...) before trying new addrman addresses.
            ntries++;
            if (ntries > 100)
                break;

            if (islimited(addr))
                continue;

            // only consider very recently tried nodes after 30 failed attempts
            if (nanow - addr.nlasttry < 600 && ntries < 30)
                continue;

            // do not allow non-default ports, unless after 50 invalid addresses selected already
            if (addr.getport() != params().getdefaultport() && ntries < 50)
                continue;

            addrconnect = addr;
            break;
        }

        if (addrconnect.isvalid())
            opennetworkconnection(addrconnect, &grant);
    }
}

void threadopenaddedconnections()
{
    {
        lock(cs_vaddednodes);
        vaddednodes = mapmultiargs["-addnode"];
    }

    if (havenameproxy()) {
        while(true) {
            list<string> laddresses(0);
            {
                lock(cs_vaddednodes);
                boost_foreach(const std::string& straddnode, vaddednodes)
                    laddresses.push_back(straddnode);
            }
            boost_foreach(const std::string& straddnode, laddresses) {
                caddress addr;
                csemaphoregrant grant(*semoutbound);
                opennetworkconnection(addr, &grant, straddnode.c_str());
                millisleep(500);
            }
            millisleep(120000); // retry every 2 minutes
        }
    }

    for (unsigned int i = 0; true; i++)
    {
        list<string> laddresses(0);
        {
            lock(cs_vaddednodes);
            boost_foreach(const std::string& straddnode, vaddednodes)
                laddresses.push_back(straddnode);
        }

        list<vector<cservice> > lservaddressestoadd(0);
        boost_foreach(const std::string& straddnode, laddresses) {
            vector<cservice> vservnode(0);
            if(lookup(straddnode.c_str(), vservnode, params().getdefaultport(), fnamelookup, 0))
            {
                lservaddressestoadd.push_back(vservnode);
                {
                    lock(cs_setservaddnodeaddresses);
                    boost_foreach(const cservice& serv, vservnode)
                        setservaddnodeaddresses.insert(serv);
                }
            }
        }
        // attempt to connect to each ip for each addnode entry until at least one is successful per addnode entry
        // (keeping in mind that addnode entries can have many ips if fnamelookup)
        {
            lock(cs_vnodes);
            boost_foreach(cnode* pnode, vnodes)
                for (list<vector<cservice> >::iterator it = lservaddressestoadd.begin(); it != lservaddressestoadd.end(); it++)
                    boost_foreach(const cservice& addrnode, *(it))
                        if (pnode->addr == addrnode)
                        {
                            it = lservaddressestoadd.erase(it);
                            it--;
                            break;
                        }
        }
        boost_foreach(vector<cservice>& vserv, lservaddressestoadd)
        {
            csemaphoregrant grant(*semoutbound);
            opennetworkconnection(caddress(vserv[i % vserv.size()]), &grant);
            millisleep(500);
        }
        millisleep(120000); // retry every 2 minutes
    }
}

// if successful, this moves the passed grant to the constructed node
bool opennetworkconnection(const caddress& addrconnect, csemaphoregrant *grantoutbound, const char *pszdest, bool foneshot)
{
    //
    // initiate outbound network connection
    //
    boost::this_thread::interruption_point();
    if (!pszdest) {
        if (islocal(addrconnect) ||
            findnode((cnetaddr)addrconnect) || cnode::isbanned(addrconnect) ||
            findnode(addrconnect.tostringipport()))
            return false;
    } else if (findnode(std::string(pszdest)))
        return false;

    cnode* pnode = connectnode(addrconnect, pszdest);
    boost::this_thread::interruption_point();

    if (!pnode)
        return false;
    if (grantoutbound)
        grantoutbound->moveto(pnode->grantoutbound);
    pnode->fnetworknode = true;
    if (foneshot)
        pnode->foneshot = true;

    return true;
}


void threadmessagehandler()
{
    boost::mutex condition_mutex;
    boost::unique_lock<boost::mutex> lock(condition_mutex);

    setthreadpriority(thread_priority_below_normal);
    while (true)
    {
        vector<cnode*> vnodescopy;
        {
            lock(cs_vnodes);
            vnodescopy = vnodes;
            boost_foreach(cnode* pnode, vnodescopy) {
                pnode->addref();
            }
        }

        // poll the connected nodes for messages
        cnode* pnodetrickle = null;
        if (!vnodescopy.empty())
            pnodetrickle = vnodescopy[getrand(vnodescopy.size())];

        bool fsleep = true;

        boost_foreach(cnode* pnode, vnodescopy)
        {
            if (pnode->fdisconnect)
                continue;

            // receive messages
            {
                try_lock(pnode->cs_vrecvmsg, lockrecv);
                if (lockrecv)
                {
                    if (!g_signals.processmessages(pnode))
                        pnode->closesocketdisconnect();

                    if (pnode->nsendsize < sendbuffersize())
                    {
                        if (!pnode->vrecvgetdata.empty() || (!pnode->vrecvmsg.empty() && pnode->vrecvmsg[0].complete()))
                        {
                            fsleep = false;
                        }
                    }
                }
            }
            boost::this_thread::interruption_point();

            // send messages
            {
                try_lock(pnode->cs_vsend, locksend);
                if (locksend)
                    g_signals.sendmessages(pnode, pnode == pnodetrickle || pnode->fwhitelisted);
            }
            boost::this_thread::interruption_point();
        }

        {
            lock(cs_vnodes);
            boost_foreach(cnode* pnode, vnodescopy)
                pnode->release();
        }

        if (fsleep)
            messagehandlercondition.timed_wait(lock, boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(100));
    }
}






bool bindlistenport(const cservice &addrbind, string& strerror, bool fwhitelisted)
{
    strerror = "";
    int none = 1;

    // create socket for listening for incoming connections
    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrbind.getsockaddr((struct sockaddr*)&sockaddr, &len))
    {
        strerror = strprintf("error: bind address family for %s not supported", addrbind.tostring());
        logprintf("%s\n", strerror);
        return false;
    }

    socket hlistensocket = socket(((struct sockaddr*)&sockaddr)->sa_family, sock_stream, ipproto_tcp);
    if (hlistensocket == invalid_socket)
    {
        strerror = strprintf("error: couldn't open socket for incoming connections (socket returned error %s)", networkerrorstring(wsagetlasterror()));
        logprintf("%s\n", strerror);
        return false;
    }

#ifndef win32
#ifdef so_nosigpipe
    // different way of disabling sigpipe on bsd
    setsockopt(hlistensocket, sol_socket, so_nosigpipe, (void*)&none, sizeof(int));
#endif
    // allow binding if the port is still in time_wait state after
    // the program was closed and restarted. not an issue on windows!
    setsockopt(hlistensocket, sol_socket, so_reuseaddr, (void*)&none, sizeof(int));
#endif

    // set to non-blocking, incoming connections will also inherit this
    if (!setsocketnonblocking(hlistensocket, true)) {
        strerror = strprintf("bindlistenport: setting listening socket to non-blocking failed, error %s\n", networkerrorstring(wsagetlasterror()));
        logprintf("%s\n", strerror);
        return false;
    }

    // some systems don't have ipv6_v6only but are always v6only; others do have the option
    // and enable it by default or not. try to enable it, if possible.
    if (addrbind.isipv6()) {
#ifdef ipv6_v6only
#ifdef win32
        setsockopt(hlistensocket, ipproto_ipv6, ipv6_v6only, (const char*)&none, sizeof(int));
#else
        setsockopt(hlistensocket, ipproto_ipv6, ipv6_v6only, (void*)&none, sizeof(int));
#endif
#endif
#ifdef win32
        int nprotlevel = protection_level_unrestricted;
        setsockopt(hlistensocket, ipproto_ipv6, ipv6_protection_level, (const char*)&nprotlevel, sizeof(int));
#endif
    }

    if (::bind(hlistensocket, (struct sockaddr*)&sockaddr, len) == socket_error)
    {
        int nerr = wsagetlasterror();
        if (nerr == wsaeaddrinuse)
            strerror = strprintf(_("unable to bind to %s on this computer. moorecoin core is probably already running."), addrbind.tostring());
        else
            strerror = strprintf(_("unable to bind to %s on this computer (bind returned error %s)"), addrbind.tostring(), networkerrorstring(nerr));
        logprintf("%s\n", strerror);
        closesocket(hlistensocket);
        return false;
    }
    logprintf("bound to %s\n", addrbind.tostring());

    // listen for incoming connections
    if (listen(hlistensocket, somaxconn) == socket_error)
    {
        strerror = strprintf(_("error: listening for incoming connections failed (listen returned error %s)"), networkerrorstring(wsagetlasterror()));
        logprintf("%s\n", strerror);
        closesocket(hlistensocket);
        return false;
    }

    vhlistensocket.push_back(listensocket(hlistensocket, fwhitelisted));

    if (addrbind.isroutable() && fdiscover && !fwhitelisted)
        addlocal(addrbind, local_bind);

    return true;
}

void static discover(boost::thread_group& threadgroup)
{
    if (!fdiscover)
        return;

#ifdef win32
    // get local host ip
    char pszhostname[256] = "";
    if (gethostname(pszhostname, sizeof(pszhostname)) != socket_error)
    {
        vector<cnetaddr> vaddr;
        if (lookuphost(pszhostname, vaddr))
        {
            boost_foreach (const cnetaddr &addr, vaddr)
            {
                if (addlocal(addr, local_if))
                    logprintf("%s: %s - %s\n", __func__, pszhostname, addr.tostring());
            }
        }
    }
#else
    // get local host ip
    struct ifaddrs* myaddrs;
    if (getifaddrs(&myaddrs) == 0)
    {
        for (struct ifaddrs* ifa = myaddrs; ifa != null; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == null) continue;
            if ((ifa->ifa_flags & iff_up) == 0) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            if (strcmp(ifa->ifa_name, "lo0") == 0) continue;
            if (ifa->ifa_addr->sa_family == af_inet)
            {
                struct sockaddr_in* s4 = (struct sockaddr_in*)(ifa->ifa_addr);
                cnetaddr addr(s4->sin_addr);
                if (addlocal(addr, local_if))
                    logprintf("%s: ipv4 %s: %s\n", __func__, ifa->ifa_name, addr.tostring());
            }
            else if (ifa->ifa_addr->sa_family == af_inet6)
            {
                struct sockaddr_in6* s6 = (struct sockaddr_in6*)(ifa->ifa_addr);
                cnetaddr addr(s6->sin6_addr);
                if (addlocal(addr, local_if))
                    logprintf("%s: ipv6 %s: %s\n", __func__, ifa->ifa_name, addr.tostring());
            }
        }
        freeifaddrs(myaddrs);
    }
#endif
}

void startnode(boost::thread_group& threadgroup, cscheduler& scheduler)
{
    uiinterface.initmessage(_("loading addresses..."));
    // load addresses for peers.dat
    int64_t nstart = gettimemillis();
    {
        caddrdb adb;
        if (!adb.read(addrman))
            logprintf("invalid or missing peers.dat; recreating\n");
    }
    logprintf("loaded %i addresses from peers.dat  %dms\n",
           addrman.size(), gettimemillis() - nstart);
    faddressesinitialized = true;

    if (semoutbound == null) {
        // initialize semaphore
        int nmaxoutbound = min(max_outbound_connections, nmaxconnections);
        semoutbound = new csemaphore(nmaxoutbound);
    }

    if (pnodelocalhost == null)
        pnodelocalhost = new cnode(invalid_socket, caddress(cservice("127.0.0.1", 0), nlocalservices));

    discover(threadgroup);

    //
    // start threads
    //

    if (!getboolarg("-dnsseed", true))
        logprintf("dns seeding disabled\n");
    else
        threadgroup.create_thread(boost::bind(&tracethread<void (*)()>, "dnsseed", &threaddnsaddressseed));

    // map ports with upnp
    mapport(getboolarg("-upnp", default_upnp));

    // send and receive from sockets, accept connections
    threadgroup.create_thread(boost::bind(&tracethread<void (*)()>, "net", &threadsockethandler));

    // initiate outbound connections from -addnode
    threadgroup.create_thread(boost::bind(&tracethread<void (*)()>, "addcon", &threadopenaddedconnections));

    // initiate outbound connections
    threadgroup.create_thread(boost::bind(&tracethread<void (*)()>, "opencon", &threadopenconnections));

    // process messages
    threadgroup.create_thread(boost::bind(&tracethread<void (*)()>, "msghand", &threadmessagehandler));

    // dump network addresses
    scheduler.scheduleevery(&dumpaddresses, dump_addresses_interval);
}

bool stopnode()
{
    logprintf("stopnode()\n");
    mapport(false);
    if (semoutbound)
        for (int i=0; i<max_outbound_connections; i++)
            semoutbound->post();

    if (faddressesinitialized)
    {
        dumpaddresses();
        faddressesinitialized = false;
    }

    return true;
}

class cnetcleanup
{
public:
    cnetcleanup() {}

    ~cnetcleanup()
    {
        // close sockets
        boost_foreach(cnode* pnode, vnodes)
            if (pnode->hsocket != invalid_socket)
                closesocket(pnode->hsocket);
        boost_foreach(listensocket& hlistensocket, vhlistensocket)
            if (hlistensocket.socket != invalid_socket)
                if (!closesocket(hlistensocket.socket))
                    logprintf("closesocket(hlistensocket) failed with error %s\n", networkerrorstring(wsagetlasterror()));

        // clean up some globals (to help leak detection)
        boost_foreach(cnode *pnode, vnodes)
            delete pnode;
        boost_foreach(cnode *pnode, vnodesdisconnected)
            delete pnode;
        vnodes.clear();
        vnodesdisconnected.clear();
        vhlistensocket.clear();
        delete semoutbound;
        semoutbound = null;
        delete pnodelocalhost;
        pnodelocalhost = null;

#ifdef win32
        // shutdown windows sockets
        wsacleanup();
#endif
    }
}
instance_of_cnetcleanup;







void relaytransaction(const ctransaction& tx)
{
    cdatastream ss(ser_network, protocol_version);
    ss.reserve(10000);
    ss << tx;
    relaytransaction(tx, ss);
}

void relaytransaction(const ctransaction& tx, const cdatastream& ss)
{
    cinv inv(msg_tx, tx.gethash());
    {
        lock(cs_maprelay);
        // expire old relay messages
        while (!vrelayexpiration.empty() && vrelayexpiration.front().first < gettime())
        {
            maprelay.erase(vrelayexpiration.front().second);
            vrelayexpiration.pop_front();
        }

        // save original serialized message so newer versions are preserved
        maprelay.insert(std::make_pair(inv, ss));
        vrelayexpiration.push_back(std::make_pair(gettime() + 15 * 60, inv));
    }
    lock(cs_vnodes);
    boost_foreach(cnode* pnode, vnodes)
    {
        if(!pnode->frelaytxes)
            continue;
        lock(pnode->cs_filter);
        if (pnode->pfilter)
        {
            if (pnode->pfilter->isrelevantandupdate(tx))
                pnode->pushinventory(inv);
        } else
            pnode->pushinventory(inv);
    }
}

void cnode::recordbytesrecv(uint64_t bytes)
{
    lock(cs_totalbytesrecv);
    ntotalbytesrecv += bytes;
}

void cnode::recordbytessent(uint64_t bytes)
{
    lock(cs_totalbytessent);
    ntotalbytessent += bytes;
}

uint64_t cnode::gettotalbytesrecv()
{
    lock(cs_totalbytesrecv);
    return ntotalbytesrecv;
}

uint64_t cnode::gettotalbytessent()
{
    lock(cs_totalbytessent);
    return ntotalbytessent;
}

void cnode::fuzz(int nchance)
{
    if (!fsuccessfullyconnected) return; // don't fuzz initial handshake
    if (getrand(nchance) != 0) return; // fuzz 1 of every nchance messages

    switch (getrand(3))
    {
    case 0:
        // xor a random byte with a random value:
        if (!sssend.empty()) {
            cdatastream::size_type pos = getrand(sssend.size());
            sssend[pos] ^= (unsigned char)(getrand(256));
        }
        break;
    case 1:
        // delete a random byte:
        if (!sssend.empty()) {
            cdatastream::size_type pos = getrand(sssend.size());
            sssend.erase(sssend.begin()+pos);
        }
        break;
    case 2:
        // insert a random byte at a random position
        {
            cdatastream::size_type pos = getrand(sssend.size());
            char ch = (char)getrand(256);
            sssend.insert(sssend.begin()+pos, ch);
        }
        break;
    }
    // chance of more than one change half the time:
    // (more changes exponentially less likely):
    fuzz(2);
}

//
// caddrdb
//

caddrdb::caddrdb()
{
    pathaddr = getdatadir() / "peers.dat";
}

bool caddrdb::write(const caddrman& addr)
{
    // generate random temporary filename
    unsigned short randv = 0;
    getrandbytes((unsigned char*)&randv, sizeof(randv));
    std::string tmpfn = strprintf("peers.dat.%04x", randv);

    // serialize addresses, checksum data up to that point, then append csum
    cdatastream sspeers(ser_disk, client_version);
    sspeers << flatdata(params().messagestart());
    sspeers << addr;
    uint256 hash = hash(sspeers.begin(), sspeers.end());
    sspeers << hash;

    // open temp output file, and associate with cautofile
    boost::filesystem::path pathtmp = getdatadir() / tmpfn;
    file *file = fopen(pathtmp.string().c_str(), "wb");
    cautofile fileout(file, ser_disk, client_version);
    if (fileout.isnull())
        return error("%s: failed to open file %s", __func__, pathtmp.string());

    // write and commit header, data
    try {
        fileout << sspeers;
    }
    catch (const std::exception& e) {
        return error("%s: serialize or i/o error - %s", __func__, e.what());
    }
    filecommit(fileout.get());
    fileout.fclose();

    // replace existing peers.dat, if any, with new peers.dat.xxxx
    if (!renameover(pathtmp, pathaddr))
        return error("%s: rename-into-place failed", __func__);

    return true;
}

bool caddrdb::read(caddrman& addr)
{
    // open input file, and associate with cautofile
    file *file = fopen(pathaddr.string().c_str(), "rb");
    cautofile filein(file, ser_disk, client_version);
    if (filein.isnull())
        return error("%s: failed to open file %s", __func__, pathaddr.string());

    // use file size to size memory buffer
    int filesize = boost::filesystem::file_size(pathaddr);
    int datasize = filesize - sizeof(uint256);
    // don't try to resize to a negative number if file is small
    if (datasize < 0)
        datasize = 0;
    vector<unsigned char> vchdata;
    vchdata.resize(datasize);
    uint256 hashin;

    // read data and checksum from file
    try {
        filein.read((char *)&vchdata[0], datasize);
        filein >> hashin;
    }
    catch (const std::exception& e) {
        return error("%s: deserialize or i/o error - %s", __func__, e.what());
    }
    filein.fclose();

    cdatastream sspeers(vchdata, ser_disk, client_version);

    // verify stored checksum matches input data
    uint256 hashtmp = hash(sspeers.begin(), sspeers.end());
    if (hashin != hashtmp)
        return error("%s: checksum mismatch, data corrupted", __func__);

    unsigned char pchmsgtmp[4];
    try {
        // de-serialize file header (network specific magic number) and ..
        sspeers >> flatdata(pchmsgtmp);

        // ... verify the network matches ours
        if (memcmp(pchmsgtmp, params().messagestart(), sizeof(pchmsgtmp)))
            return error("%s: invalid network magic number", __func__);

        // de-serialize address data into one caddrman object
        sspeers >> addr;
    }
    catch (const std::exception& e) {
        return error("%s: deserialize or i/o error - %s", __func__, e.what());
    }

    return true;
}

unsigned int receivefloodsize() { return 1000*getarg("-maxreceivebuffer", 5*1000); }
unsigned int sendbuffersize() { return 1000*getarg("-maxsendbuffer", 1*1000); }

cnode::cnode(socket hsocketin, const caddress& addrin, const std::string& addrnamein, bool finboundin) :
    sssend(ser_network, init_proto_version),
    addrknown(5000, 0.001, insecure_rand()),
    setinventoryknown(sendbuffersize() / 1000)
{
    nservices = 0;
    hsocket = hsocketin;
    nrecvversion = init_proto_version;
    nlastsend = 0;
    nlastrecv = 0;
    nsendbytes = 0;
    nrecvbytes = 0;
    ntimeconnected = gettime();
    ntimeoffset = 0;
    addr = addrin;
    addrname = addrnamein == "" ? addr.tostringipport() : addrnamein;
    nversion = 0;
    strsubver = "";
    fwhitelisted = false;
    foneshot = false;
    fclient = false; // set by version message
    finbound = finboundin;
    fnetworknode = false;
    fsuccessfullyconnected = false;
    fdisconnect = false;
    nrefcount = 0;
    nsendsize = 0;
    nsendoffset = 0;
    hashcontinue = uint256();
    nstartingheight = -1;
    fgetaddr = false;
    frelaytxes = false;
    pfilter = new cbloomfilter();
    npingnoncesent = 0;
    npingusecstart = 0;
    npingusectime = 0;
    fpingqueued = false;

    {
        lock(cs_nlastnodeid);
        id = nlastnodeid++;
    }

    if (flogips)
        logprint("net", "added connection to %s peer=%d\n", addrname, id);
    else
        logprint("net", "added connection peer=%d\n", id);

    // be shy and don't send version until we hear
    if (hsocket != invalid_socket && !finbound)
        pushversion();

    getnodesignals().initializenode(getid(), this);
}

cnode::~cnode()
{
    closesocket(hsocket);

    if (pfilter)
        delete pfilter;

    getnodesignals().finalizenode(getid());
}

void cnode::askfor(const cinv& inv)
{
    if (mapaskfor.size() > mapaskfor_max_sz)
        return;
    // we're using mapaskfor as a priority queue,
    // the key is the earliest time the request can be sent
    int64_t nrequesttime;
    limitedmap<cinv, int64_t>::const_iterator it = mapalreadyaskedfor.find(inv);
    if (it != mapalreadyaskedfor.end())
        nrequesttime = it->second;
    else
        nrequesttime = 0;
    logprint("net", "askfor %s  %d (%s) peer=%d\n", inv.tostring(), nrequesttime, datetimestrformat("%h:%m:%s", nrequesttime/1000000), id);

    // make sure not to reuse time indexes to keep things in the same order
    int64_t nnow = gettimemicros() - 1000000;
    static int64_t nlasttime;
    ++nlasttime;
    nnow = std::max(nnow, nlasttime);
    nlasttime = nnow;

    // each retry is 2 minutes after the last
    nrequesttime = std::max(nrequesttime + 2 * 60 * 1000000, nnow);
    if (it != mapalreadyaskedfor.end())
        mapalreadyaskedfor.update(it, nrequesttime);
    else
        mapalreadyaskedfor.insert(std::make_pair(inv, nrequesttime));
    mapaskfor.insert(std::make_pair(nrequesttime, inv));
}

void cnode::beginmessage(const char* pszcommand) exclusive_lock_function(cs_vsend)
{
    enter_critical_section(cs_vsend);
    assert(sssend.size() == 0);
    sssend << cmessageheader(params().messagestart(), pszcommand, 0);
    logprint("net", "sending: %s ", sanitizestring(pszcommand));
}

void cnode::abortmessage() unlock_function(cs_vsend)
{
    sssend.clear();

    leave_critical_section(cs_vsend);

    logprint("net", "(aborted)\n");
}

void cnode::endmessage() unlock_function(cs_vsend)
{
    // the -*messagestest options are intentionally not documented in the help message,
    // since they are only used during development to debug the networking code and are
    // not intended for end-users.
    if (mapargs.count("-dropmessagestest") && getrand(getarg("-dropmessagestest", 2)) == 0)
    {
        logprint("net", "dropmessages dropping send message\n");
        abortmessage();
        return;
    }
    if (mapargs.count("-fuzzmessagestest"))
        fuzz(getarg("-fuzzmessagestest", 10));

    if (sssend.size() == 0)
        return;

    // set the size
    unsigned int nsize = sssend.size() - cmessageheader::header_size;
    writele32((uint8_t*)&sssend[cmessageheader::message_size_offset], nsize);

    // set the checksum
    uint256 hash = hash(sssend.begin() + cmessageheader::header_size, sssend.end());
    unsigned int nchecksum = 0;
    memcpy(&nchecksum, &hash, sizeof(nchecksum));
    assert(sssend.size () >= cmessageheader::checksum_offset + sizeof(nchecksum));
    memcpy((char*)&sssend[cmessageheader::checksum_offset], &nchecksum, sizeof(nchecksum));

    logprint("net", "(%d bytes) peer=%d\n", nsize, id);

    std::deque<cserializedata>::iterator it = vsendmsg.insert(vsendmsg.end(), cserializedata());
    sssend.getandclear(*it);
    nsendsize += (*it).size();

    // if write queue empty, attempt "optimistic write"
    if (it == vsendmsg.begin())
        socketsenddata(this);

    leave_critical_section(cs_vsend);
}
