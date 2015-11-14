// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifdef have_config_h
#include "config/moorecoin-config.h"
#endif

#include "netbase.h"

#include "hash.h"
#include "sync.h"
#include "uint256.h"
#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#ifdef have_getaddrinfo_a
#include <netdb.h>
#endif

#ifndef win32
#if have_inet_pton
#include <arpa/inet.h>
#endif
#include <fcntl.h>
#endif

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/thread.hpp>

#if !defined(have_msg_nosignal) && !defined(msg_nosignal)
#define msg_nosignal 0
#endif

// settings
static proxytype proxyinfo[net_max];
static proxytype nameproxy;
static ccriticalsection cs_proxyinfos;
int nconnecttimeout = default_connect_timeout;
bool fnamelookup = false;

static const unsigned char pchipv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };

// need ample time for negotiation for very slow proxies such as tor (milliseconds)
static const int socks5_recv_timeout = 20 * 1000;

enum network parsenetwork(std::string net) {
    boost::to_lower(net);
    if (net == "ipv4") return net_ipv4;
    if (net == "ipv6") return net_ipv6;
    if (net == "tor" || net == "onion")  return net_tor;
    return net_unroutable;
}

std::string getnetworkname(enum network net) {
    switch(net)
    {
    case net_ipv4: return "ipv4";
    case net_ipv6: return "ipv6";
    case net_tor: return "onion";
    default: return "";
    }
}

void splithostport(std::string in, int &portout, std::string &hostout) {
    size_t colon = in.find_last_of(':');
    // if a : is found, and it either follows a [...], or no other : is in the string, treat it as port separator
    bool fhavecolon = colon != in.npos;
    bool fbracketed = fhavecolon && (in[0]=='[' && in[colon-1]==']'); // if there is a colon, and in[0]=='[', colon is not 0, so in[colon-1] is safe
    bool fmulticolon = fhavecolon && (in.find_last_of(':',colon-1) != in.npos);
    if (fhavecolon && (colon==0 || fbracketed || !fmulticolon)) {
        int32_t n;
        if (parseint32(in.substr(colon + 1), &n) && n > 0 && n < 0x10000) {
            in = in.substr(0, colon);
            portout = n;
        }
    }
    if (in.size()>0 && in[0] == '[' && in[in.size()-1] == ']')
        hostout = in.substr(1, in.size()-2);
    else
        hostout = in;
}

bool static lookupintern(const char *pszname, std::vector<cnetaddr>& vip, unsigned int nmaxsolutions, bool fallowlookup)
{
    vip.clear();

    {
        cnetaddr addr;
        if (addr.setspecial(std::string(pszname))) {
            vip.push_back(addr);
            return true;
        }
    }

#ifdef have_getaddrinfo_a
    struct in_addr ipv4_addr;
#ifdef have_inet_pton
    if (inet_pton(af_inet, pszname, &ipv4_addr) > 0) {
        vip.push_back(cnetaddr(ipv4_addr));
        return true;
    }

    struct in6_addr ipv6_addr;
    if (inet_pton(af_inet6, pszname, &ipv6_addr) > 0) {
        vip.push_back(cnetaddr(ipv6_addr));
        return true;
    }
#else
    ipv4_addr.s_addr = inet_addr(pszname);
    if (ipv4_addr.s_addr != inaddr_none) {
        vip.push_back(cnetaddr(ipv4_addr));
        return true;
    }
#endif
#endif

    struct addrinfo aihint;
    memset(&aihint, 0, sizeof(struct addrinfo));
    aihint.ai_socktype = sock_stream;
    aihint.ai_protocol = ipproto_tcp;
    aihint.ai_family = af_unspec;
#ifdef win32
    aihint.ai_flags = fallowlookup ? 0 : ai_numerichost;
#else
    aihint.ai_flags = fallowlookup ? ai_addrconfig : ai_numerichost;
#endif

    struct addrinfo *aires = null;
#ifdef have_getaddrinfo_a
    struct gaicb gcb, *query = &gcb;
    memset(query, 0, sizeof(struct gaicb));
    gcb.ar_name = pszname;
    gcb.ar_request = &aihint;
    int nerr = getaddrinfo_a(gai_nowait, &query, 1, null);
    if (nerr)
        return false;

    do {
        // should set the timeout limit to a resonable value to avoid
        // generating unnecessary checking call during the polling loop,
        // while it can still response to stop request quick enough.
        // 2 seconds looks fine in our situation.
        struct timespec ts = { 2, 0 };
        gai_suspend(&query, 1, &ts);
        boost::this_thread::interruption_point();

        nerr = gai_error(query);
        if (0 == nerr)
            aires = query->ar_result;
    } while (nerr == eai_inprogress);
#else
    int nerr = getaddrinfo(pszname, null, &aihint, &aires);
#endif
    if (nerr)
        return false;

    struct addrinfo *aitrav = aires;
    while (aitrav != null && (nmaxsolutions == 0 || vip.size() < nmaxsolutions))
    {
        if (aitrav->ai_family == af_inet)
        {
            assert(aitrav->ai_addrlen >= sizeof(sockaddr_in));
            vip.push_back(cnetaddr(((struct sockaddr_in*)(aitrav->ai_addr))->sin_addr));
        }

        if (aitrav->ai_family == af_inet6)
        {
            assert(aitrav->ai_addrlen >= sizeof(sockaddr_in6));
            vip.push_back(cnetaddr(((struct sockaddr_in6*)(aitrav->ai_addr))->sin6_addr));
        }

        aitrav = aitrav->ai_next;
    }

    freeaddrinfo(aires);

    return (vip.size() > 0);
}

bool lookuphost(const char *pszname, std::vector<cnetaddr>& vip, unsigned int nmaxsolutions, bool fallowlookup)
{
    std::string strhost(pszname);
    if (strhost.empty())
        return false;
    if (boost::algorithm::starts_with(strhost, "[") && boost::algorithm::ends_with(strhost, "]"))
    {
        strhost = strhost.substr(1, strhost.size() - 2);
    }

    return lookupintern(strhost.c_str(), vip, nmaxsolutions, fallowlookup);
}

bool lookup(const char *pszname, std::vector<cservice>& vaddr, int portdefault, bool fallowlookup, unsigned int nmaxsolutions)
{
    if (pszname[0] == 0)
        return false;
    int port = portdefault;
    std::string hostname = "";
    splithostport(std::string(pszname), port, hostname);

    std::vector<cnetaddr> vip;
    bool fret = lookupintern(hostname.c_str(), vip, nmaxsolutions, fallowlookup);
    if (!fret)
        return false;
    vaddr.resize(vip.size());
    for (unsigned int i = 0; i < vip.size(); i++)
        vaddr[i] = cservice(vip[i], port);
    return true;
}

bool lookup(const char *pszname, cservice& addr, int portdefault, bool fallowlookup)
{
    std::vector<cservice> vservice;
    bool fret = lookup(pszname, vservice, portdefault, fallowlookup, 1);
    if (!fret)
        return false;
    addr = vservice[0];
    return true;
}

bool lookupnumeric(const char *pszname, cservice& addr, int portdefault)
{
    return lookup(pszname, addr, portdefault, false);
}

/**
 * convert milliseconds to a struct timeval for select.
 */
struct timeval static millistotimeval(int64_t ntimeout)
{
    struct timeval timeout;
    timeout.tv_sec  = ntimeout / 1000;
    timeout.tv_usec = (ntimeout % 1000) * 1000;
    return timeout;
}

/**
 * read bytes from socket. this will either read the full number of bytes requested
 * or return false on error or timeout.
 * this function can be interrupted by boost thread interrupt.
 *
 * @param data buffer to receive into
 * @param len  length of data to receive
 * @param timeout  timeout in milliseconds for receive operation
 *
 * @note this function requires that hsocket is in non-blocking mode.
 */
bool static interruptiblerecv(char* data, size_t len, int timeout, socket& hsocket)
{
    int64_t curtime = gettimemillis();
    int64_t endtime = curtime + timeout;
    // maximum time to wait in one select call. it will take up until this time (in millis)
    // to break off in case of an interruption.
    const int64_t maxwait = 1000;
    while (len > 0 && curtime < endtime) {
        ssize_t ret = recv(hsocket, data, len, 0); // optimistically try the recv first
        if (ret > 0) {
            len -= ret;
            data += ret;
        } else if (ret == 0) { // unexpected disconnection
            return false;
        } else { // other error or blocking
            int nerr = wsagetlasterror();
            if (nerr == wsaeinprogress || nerr == wsaewouldblock || nerr == wsaeinval) {
                struct timeval tval = millistotimeval(std::min(endtime - curtime, maxwait));
                fd_set fdset;
                fd_zero(&fdset);
                fd_set(hsocket, &fdset);
                int nret = select(hsocket + 1, &fdset, null, null, &tval);
                if (nret == socket_error) {
                    return false;
                }
            } else {
                return false;
            }
        }
        boost::this_thread::interruption_point();
        curtime = gettimemillis();
    }
    return len == 0;
}

struct proxycredentials
{
    std::string username;
    std::string password;
};

/** connect using socks5 (as described in rfc1928) */
static bool socks5(const std::string& strdest, int port, const proxycredentials *auth, socket& hsocket)
{
    logprintf("socks5 connecting %s\n", strdest);
    if (strdest.size() > 255) {
        closesocket(hsocket);
        return error("hostname too long");
    }
    // accepted authentication methods
    std::vector<uint8_t> vsocks5init;
    vsocks5init.push_back(0x05);
    if (auth) {
        vsocks5init.push_back(0x02); // # methods
        vsocks5init.push_back(0x00); // x'00' no authentication required
        vsocks5init.push_back(0x02); // x'02' username/password (rfc1929)
    } else {
        vsocks5init.push_back(0x01); // # methods
        vsocks5init.push_back(0x00); // x'00' no authentication required
    }
    ssize_t ret = send(hsocket, (const char*)begin_ptr(vsocks5init), vsocks5init.size(), msg_nosignal);
    if (ret != (ssize_t)vsocks5init.size()) {
        closesocket(hsocket);
        return error("error sending to proxy");
    }
    char pchret1[2];
    if (!interruptiblerecv(pchret1, 2, socks5_recv_timeout, hsocket)) {
        closesocket(hsocket);
        return error("error reading proxy response");
    }
    if (pchret1[0] != 0x05) {
        closesocket(hsocket);
        return error("proxy failed to initialize");
    }
    if (pchret1[1] == 0x02 && auth) {
        // perform username/password authentication (as described in rfc1929)
        std::vector<uint8_t> vauth;
        vauth.push_back(0x01);
        if (auth->username.size() > 255 || auth->password.size() > 255)
            return error("proxy username or password too long");
        vauth.push_back(auth->username.size());
        vauth.insert(vauth.end(), auth->username.begin(), auth->username.end());
        vauth.push_back(auth->password.size());
        vauth.insert(vauth.end(), auth->password.begin(), auth->password.end());
        ret = send(hsocket, (const char*)begin_ptr(vauth), vauth.size(), msg_nosignal);
        if (ret != (ssize_t)vauth.size()) {
            closesocket(hsocket);
            return error("error sending authentication to proxy");
        }
        logprint("proxy", "socks5 sending proxy authentication %s:%s\n", auth->username, auth->password);
        char pchreta[2];
        if (!interruptiblerecv(pchreta, 2, socks5_recv_timeout, hsocket)) {
            closesocket(hsocket);
            return error("error reading proxy authentication response");
        }
        if (pchreta[0] != 0x01 || pchreta[1] != 0x00) {
            closesocket(hsocket);
            return error("proxy authentication unsuccesful");
        }
    } else if (pchret1[1] == 0x00) {
        // perform no authentication
    } else {
        closesocket(hsocket);
        return error("proxy requested wrong authentication method %02x", pchret1[1]);
    }
    std::vector<uint8_t> vsocks5;
    vsocks5.push_back(0x05); // ver protocol version
    vsocks5.push_back(0x01); // cmd connect
    vsocks5.push_back(0x00); // rsv reserved
    vsocks5.push_back(0x03); // atyp domainname
    vsocks5.push_back(strdest.size()); // length<=255 is checked at beginning of function
    vsocks5.insert(vsocks5.end(), strdest.begin(), strdest.end());
    vsocks5.push_back((port >> 8) & 0xff);
    vsocks5.push_back((port >> 0) & 0xff);
    ret = send(hsocket, (const char*)begin_ptr(vsocks5), vsocks5.size(), msg_nosignal);
    if (ret != (ssize_t)vsocks5.size()) {
        closesocket(hsocket);
        return error("error sending to proxy");
    }
    char pchret2[4];
    if (!interruptiblerecv(pchret2, 4, socks5_recv_timeout, hsocket)) {
        closesocket(hsocket);
        return error("error reading proxy response");
    }
    if (pchret2[0] != 0x05) {
        closesocket(hsocket);
        return error("proxy failed to accept request");
    }
    if (pchret2[1] != 0x00) {
        closesocket(hsocket);
        switch (pchret2[1])
        {
            case 0x01: return error("proxy error: general failure");
            case 0x02: return error("proxy error: connection not allowed");
            case 0x03: return error("proxy error: network unreachable");
            case 0x04: return error("proxy error: host unreachable");
            case 0x05: return error("proxy error: connection refused");
            case 0x06: return error("proxy error: ttl expired");
            case 0x07: return error("proxy error: protocol error");
            case 0x08: return error("proxy error: address type not supported");
            default:   return error("proxy error: unknown");
        }
    }
    if (pchret2[2] != 0x00) {
        closesocket(hsocket);
        return error("error: malformed proxy response");
    }
    char pchret3[256];
    switch (pchret2[3])
    {
        case 0x01: ret = interruptiblerecv(pchret3, 4, socks5_recv_timeout, hsocket); break;
        case 0x04: ret = interruptiblerecv(pchret3, 16, socks5_recv_timeout, hsocket); break;
        case 0x03:
        {
            ret = interruptiblerecv(pchret3, 1, socks5_recv_timeout, hsocket);
            if (!ret) {
                closesocket(hsocket);
                return error("error reading from proxy");
            }
            int nrecv = pchret3[0];
            ret = interruptiblerecv(pchret3, nrecv, socks5_recv_timeout, hsocket);
            break;
        }
        default: closesocket(hsocket); return error("error: malformed proxy response");
    }
    if (!ret) {
        closesocket(hsocket);
        return error("error reading from proxy");
    }
    if (!interruptiblerecv(pchret3, 2, socks5_recv_timeout, hsocket)) {
        closesocket(hsocket);
        return error("error reading from proxy");
    }
    logprintf("socks5 connected %s\n", strdest);
    return true;
}

bool static connectsocketdirectly(const cservice &addrconnect, socket& hsocketret, int ntimeout)
{
    hsocketret = invalid_socket;

    struct sockaddr_storage sockaddr;
    socklen_t len = sizeof(sockaddr);
    if (!addrconnect.getsockaddr((struct sockaddr*)&sockaddr, &len)) {
        logprintf("cannot connect to %s: unsupported network\n", addrconnect.tostring());
        return false;
    }

    socket hsocket = socket(((struct sockaddr*)&sockaddr)->sa_family, sock_stream, ipproto_tcp);
    if (hsocket == invalid_socket)
        return false;

#ifdef so_nosigpipe
    int set = 1;
    // different way of disabling sigpipe on bsd
    setsockopt(hsocket, sol_socket, so_nosigpipe, (void*)&set, sizeof(int));
#endif

    // set to non-blocking
    if (!setsocketnonblocking(hsocket, true))
        return error("connectsocketdirectly: setting socket to non-blocking failed, error %s\n", networkerrorstring(wsagetlasterror()));

    if (connect(hsocket, (struct sockaddr*)&sockaddr, len) == socket_error)
    {
        int nerr = wsagetlasterror();
        // wsaeinval is here because some legacy version of winsock uses it
        if (nerr == wsaeinprogress || nerr == wsaewouldblock || nerr == wsaeinval)
        {
            struct timeval timeout = millistotimeval(ntimeout);
            fd_set fdset;
            fd_zero(&fdset);
            fd_set(hsocket, &fdset);
            int nret = select(hsocket + 1, null, &fdset, null, &timeout);
            if (nret == 0)
            {
                logprint("net", "connection to %s timeout\n", addrconnect.tostring());
                closesocket(hsocket);
                return false;
            }
            if (nret == socket_error)
            {
                logprintf("select() for %s failed: %s\n", addrconnect.tostring(), networkerrorstring(wsagetlasterror()));
                closesocket(hsocket);
                return false;
            }
            socklen_t nretsize = sizeof(nret);
#ifdef win32
            if (getsockopt(hsocket, sol_socket, so_error, (char*)(&nret), &nretsize) == socket_error)
#else
            if (getsockopt(hsocket, sol_socket, so_error, &nret, &nretsize) == socket_error)
#endif
            {
                logprintf("getsockopt() for %s failed: %s\n", addrconnect.tostring(), networkerrorstring(wsagetlasterror()));
                closesocket(hsocket);
                return false;
            }
            if (nret != 0)
            {
                logprintf("connect() to %s failed after select(): %s\n", addrconnect.tostring(), networkerrorstring(nret));
                closesocket(hsocket);
                return false;
            }
        }
#ifdef win32
        else if (wsagetlasterror() != wsaeisconn)
#else
        else
#endif
        {
            logprintf("connect() to %s failed: %s\n", addrconnect.tostring(), networkerrorstring(wsagetlasterror()));
            closesocket(hsocket);
            return false;
        }
    }

    hsocketret = hsocket;
    return true;
}

bool setproxy(enum network net, const proxytype &addrproxy) {
    assert(net >= 0 && net < net_max);
    if (!addrproxy.isvalid())
        return false;
    lock(cs_proxyinfos);
    proxyinfo[net] = addrproxy;
    return true;
}

bool getproxy(enum network net, proxytype &proxyinfoout) {
    assert(net >= 0 && net < net_max);
    lock(cs_proxyinfos);
    if (!proxyinfo[net].isvalid())
        return false;
    proxyinfoout = proxyinfo[net];
    return true;
}

bool setnameproxy(const proxytype &addrproxy) {
    if (!addrproxy.isvalid())
        return false;
    lock(cs_proxyinfos);
    nameproxy = addrproxy;
    return true;
}

bool getnameproxy(proxytype &nameproxyout) {
    lock(cs_proxyinfos);
    if(!nameproxy.isvalid())
        return false;
    nameproxyout = nameproxy;
    return true;
}

bool havenameproxy() {
    lock(cs_proxyinfos);
    return nameproxy.isvalid();
}

bool isproxy(const cnetaddr &addr) {
    lock(cs_proxyinfos);
    for (int i = 0; i < net_max; i++) {
        if (addr == (cnetaddr)proxyinfo[i].proxy)
            return true;
    }
    return false;
}

static bool connectthroughproxy(const proxytype &proxy, const std::string& strdest, int port, socket& hsocketret, int ntimeout, bool *outproxyconnectionfailed)
{
    socket hsocket = invalid_socket;
    // first connect to proxy server
    if (!connectsocketdirectly(proxy.proxy, hsocket, ntimeout)) {
        if (outproxyconnectionfailed)
            *outproxyconnectionfailed = true;
        return false;
    }
    // do socks negotiation
    if (proxy.randomize_credentials) {
        proxycredentials random_auth;
        random_auth.username = strprintf("%i", insecure_rand());
        random_auth.password = strprintf("%i", insecure_rand());
        if (!socks5(strdest, (unsigned short)port, &random_auth, hsocket))
            return false;
    } else {
        if (!socks5(strdest, (unsigned short)port, 0, hsocket))
            return false;
    }

    hsocketret = hsocket;
    return true;
}

bool connectsocket(const cservice &addrdest, socket& hsocketret, int ntimeout, bool *outproxyconnectionfailed)
{
    proxytype proxy;
    if (outproxyconnectionfailed)
        *outproxyconnectionfailed = false;

    if (getproxy(addrdest.getnetwork(), proxy))
        return connectthroughproxy(proxy, addrdest.tostringip(), addrdest.getport(), hsocketret, ntimeout, outproxyconnectionfailed);
    else // no proxy needed (none set for target network)
        return connectsocketdirectly(addrdest, hsocketret, ntimeout);
}

bool connectsocketbyname(cservice &addr, socket& hsocketret, const char *pszdest, int portdefault, int ntimeout, bool *outproxyconnectionfailed)
{
    std::string strdest;
    int port = portdefault;

    if (outproxyconnectionfailed)
        *outproxyconnectionfailed = false;

    splithostport(std::string(pszdest), port, strdest);

    proxytype nameproxy;
    getnameproxy(nameproxy);

    cservice addrresolved(cnetaddr(strdest, fnamelookup && !havenameproxy()), port);
    if (addrresolved.isvalid()) {
        addr = addrresolved;
        return connectsocket(addr, hsocketret, ntimeout);
    }

    addr = cservice("0.0.0.0:0");

    if (!havenameproxy())
        return false;
    return connectthroughproxy(nameproxy, strdest, port, hsocketret, ntimeout, outproxyconnectionfailed);
}

void cnetaddr::init()
{
    memset(ip, 0, sizeof(ip));
}

void cnetaddr::setip(const cnetaddr& ipin)
{
    memcpy(ip, ipin.ip, sizeof(ip));
}

void cnetaddr::setraw(network network, const uint8_t *ip_in)
{
    switch(network)
    {
        case net_ipv4:
            memcpy(ip, pchipv4, 12);
            memcpy(ip+12, ip_in, 4);
            break;
        case net_ipv6:
            memcpy(ip, ip_in, 16);
            break;
        default:
            assert(!"invalid network");
    }
}

static const unsigned char pchonioncat[] = {0xfd,0x87,0xd8,0x7e,0xeb,0x43};

bool cnetaddr::setspecial(const std::string &strname)
{
    if (strname.size()>6 && strname.substr(strname.size() - 6, 6) == ".onion") {
        std::vector<unsigned char> vchaddr = decodebase32(strname.substr(0, strname.size() - 6).c_str());
        if (vchaddr.size() != 16-sizeof(pchonioncat))
            return false;
        memcpy(ip, pchonioncat, sizeof(pchonioncat));
        for (unsigned int i=0; i<16-sizeof(pchonioncat); i++)
            ip[i + sizeof(pchonioncat)] = vchaddr[i];
        return true;
    }
    return false;
}

cnetaddr::cnetaddr()
{
    init();
}

cnetaddr::cnetaddr(const struct in_addr& ipv4addr)
{
    setraw(net_ipv4, (const uint8_t*)&ipv4addr);
}

cnetaddr::cnetaddr(const struct in6_addr& ipv6addr)
{
    setraw(net_ipv6, (const uint8_t*)&ipv6addr);
}

cnetaddr::cnetaddr(const char *pszip, bool fallowlookup)
{
    init();
    std::vector<cnetaddr> vip;
    if (lookuphost(pszip, vip, 1, fallowlookup))
        *this = vip[0];
}

cnetaddr::cnetaddr(const std::string &strip, bool fallowlookup)
{
    init();
    std::vector<cnetaddr> vip;
    if (lookuphost(strip.c_str(), vip, 1, fallowlookup))
        *this = vip[0];
}

unsigned int cnetaddr::getbyte(int n) const
{
    return ip[15-n];
}

bool cnetaddr::isipv4() const
{
    return (memcmp(ip, pchipv4, sizeof(pchipv4)) == 0);
}

bool cnetaddr::isipv6() const
{
    return (!isipv4() && !istor());
}

bool cnetaddr::isrfc1918() const
{
    return isipv4() && (
        getbyte(3) == 10 ||
        (getbyte(3) == 192 && getbyte(2) == 168) ||
        (getbyte(3) == 172 && (getbyte(2) >= 16 && getbyte(2) <= 31)));
}

bool cnetaddr::isrfc2544() const
{
    return isipv4() && getbyte(3) == 198 && (getbyte(2) == 18 || getbyte(2) == 19);
}

bool cnetaddr::isrfc3927() const
{
    return isipv4() && (getbyte(3) == 169 && getbyte(2) == 254);
}

bool cnetaddr::isrfc6598() const
{
    return isipv4() && getbyte(3) == 100 && getbyte(2) >= 64 && getbyte(2) <= 127;
}

bool cnetaddr::isrfc5737() const
{
    return isipv4() && ((getbyte(3) == 192 && getbyte(2) == 0 && getbyte(1) == 2) ||
        (getbyte(3) == 198 && getbyte(2) == 51 && getbyte(1) == 100) ||
        (getbyte(3) == 203 && getbyte(2) == 0 && getbyte(1) == 113));
}

bool cnetaddr::isrfc3849() const
{
    return getbyte(15) == 0x20 && getbyte(14) == 0x01 && getbyte(13) == 0x0d && getbyte(12) == 0xb8;
}

bool cnetaddr::isrfc3964() const
{
    return (getbyte(15) == 0x20 && getbyte(14) == 0x02);
}

bool cnetaddr::isrfc6052() const
{
    static const unsigned char pchrfc6052[] = {0,0x64,0xff,0x9b,0,0,0,0,0,0,0,0};
    return (memcmp(ip, pchrfc6052, sizeof(pchrfc6052)) == 0);
}

bool cnetaddr::isrfc4380() const
{
    return (getbyte(15) == 0x20 && getbyte(14) == 0x01 && getbyte(13) == 0 && getbyte(12) == 0);
}

bool cnetaddr::isrfc4862() const
{
    static const unsigned char pchrfc4862[] = {0xfe,0x80,0,0,0,0,0,0};
    return (memcmp(ip, pchrfc4862, sizeof(pchrfc4862)) == 0);
}

bool cnetaddr::isrfc4193() const
{
    return ((getbyte(15) & 0xfe) == 0xfc);
}

bool cnetaddr::isrfc6145() const
{
    static const unsigned char pchrfc6145[] = {0,0,0,0,0,0,0,0,0xff,0xff,0,0};
    return (memcmp(ip, pchrfc6145, sizeof(pchrfc6145)) == 0);
}

bool cnetaddr::isrfc4843() const
{
    return (getbyte(15) == 0x20 && getbyte(14) == 0x01 && getbyte(13) == 0x00 && (getbyte(12) & 0xf0) == 0x10);
}

bool cnetaddr::istor() const
{
    return (memcmp(ip, pchonioncat, sizeof(pchonioncat)) == 0);
}

bool cnetaddr::islocal() const
{
    // ipv4 loopback
   if (isipv4() && (getbyte(3) == 127 || getbyte(3) == 0))
       return true;

   // ipv6 loopback (::1/128)
   static const unsigned char pchlocal[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
   if (memcmp(ip, pchlocal, 16) == 0)
       return true;

   return false;
}

bool cnetaddr::ismulticast() const
{
    return    (isipv4() && (getbyte(3) & 0xf0) == 0xe0)
           || (getbyte(15) == 0xff);
}

bool cnetaddr::isvalid() const
{
    // cleanup 3-byte shifted addresses caused by garbage in size field
    // of addr messages from versions before 0.2.9 checksum.
    // two consecutive addr messages look like this:
    // header20 vectorlen3 addr26 addr26 addr26 header20 vectorlen3 addr26 addr26 addr26...
    // so if the first length field is garbled, it reads the second batch
    // of addr misaligned by 3 bytes.
    if (memcmp(ip, pchipv4+3, sizeof(pchipv4)-3) == 0)
        return false;

    // unspecified ipv6 address (::/128)
    unsigned char ipnone[16] = {};
    if (memcmp(ip, ipnone, 16) == 0)
        return false;

    // documentation ipv6 address
    if (isrfc3849())
        return false;

    if (isipv4())
    {
        // inaddr_none
        uint32_t ipnone = inaddr_none;
        if (memcmp(ip+12, &ipnone, 4) == 0)
            return false;

        // 0
        ipnone = 0;
        if (memcmp(ip+12, &ipnone, 4) == 0)
            return false;
    }

    return true;
}

bool cnetaddr::isroutable() const
{
    return isvalid() && !(isrfc1918() || isrfc2544() || isrfc3927() || isrfc4862() || isrfc6598() || isrfc5737() || (isrfc4193() && !istor()) || isrfc4843() || islocal());
}

enum network cnetaddr::getnetwork() const
{
    if (!isroutable())
        return net_unroutable;

    if (isipv4())
        return net_ipv4;

    if (istor())
        return net_tor;

    return net_ipv6;
}

std::string cnetaddr::tostringip() const
{
    if (istor())
        return encodebase32(&ip[6], 10) + ".onion";
    cservice serv(*this, 0);
    struct sockaddr_storage sockaddr;
    socklen_t socklen = sizeof(sockaddr);
    if (serv.getsockaddr((struct sockaddr*)&sockaddr, &socklen)) {
        char name[1025] = "";
        if (!getnameinfo((const struct sockaddr*)&sockaddr, socklen, name, sizeof(name), null, 0, ni_numerichost))
            return std::string(name);
    }
    if (isipv4())
        return strprintf("%u.%u.%u.%u", getbyte(3), getbyte(2), getbyte(1), getbyte(0));
    else
        return strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                         getbyte(15) << 8 | getbyte(14), getbyte(13) << 8 | getbyte(12),
                         getbyte(11) << 8 | getbyte(10), getbyte(9) << 8 | getbyte(8),
                         getbyte(7) << 8 | getbyte(6), getbyte(5) << 8 | getbyte(4),
                         getbyte(3) << 8 | getbyte(2), getbyte(1) << 8 | getbyte(0));
}

std::string cnetaddr::tostring() const
{
    return tostringip();
}

bool operator==(const cnetaddr& a, const cnetaddr& b)
{
    return (memcmp(a.ip, b.ip, 16) == 0);
}

bool operator!=(const cnetaddr& a, const cnetaddr& b)
{
    return (memcmp(a.ip, b.ip, 16) != 0);
}

bool operator<(const cnetaddr& a, const cnetaddr& b)
{
    return (memcmp(a.ip, b.ip, 16) < 0);
}

bool cnetaddr::getinaddr(struct in_addr* pipv4addr) const
{
    if (!isipv4())
        return false;
    memcpy(pipv4addr, ip+12, 4);
    return true;
}

bool cnetaddr::getin6addr(struct in6_addr* pipv6addr) const
{
    memcpy(pipv6addr, ip, 16);
    return true;
}

// get canonical identifier of an address' group
// no two connections will be attempted to addresses with the same group
std::vector<unsigned char> cnetaddr::getgroup() const
{
    std::vector<unsigned char> vchret;
    int nclass = net_ipv6;
    int nstartbyte = 0;
    int nbits = 16;

    // all local addresses belong to the same group
    if (islocal())
    {
        nclass = 255;
        nbits = 0;
    }

    // all unroutable addresses belong to the same group
    if (!isroutable())
    {
        nclass = net_unroutable;
        nbits = 0;
    }
    // for ipv4 addresses, '1' + the 16 higher-order bits of the ip
    // includes mapped ipv4, siit translated ipv4, and the well-known prefix
    else if (isipv4() || isrfc6145() || isrfc6052())
    {
        nclass = net_ipv4;
        nstartbyte = 12;
    }
    // for 6to4 tunnelled addresses, use the encapsulated ipv4 address
    else if (isrfc3964())
    {
        nclass = net_ipv4;
        nstartbyte = 2;
    }
    // for teredo-tunnelled ipv6 addresses, use the encapsulated ipv4 address
    else if (isrfc4380())
    {
        vchret.push_back(net_ipv4);
        vchret.push_back(getbyte(3) ^ 0xff);
        vchret.push_back(getbyte(2) ^ 0xff);
        return vchret;
    }
    else if (istor())
    {
        nclass = net_tor;
        nstartbyte = 6;
        nbits = 4;
    }
    // for he.net, use /36 groups
    else if (getbyte(15) == 0x20 && getbyte(14) == 0x01 && getbyte(13) == 0x04 && getbyte(12) == 0x70)
        nbits = 36;
    // for the rest of the ipv6 network, use /32 groups
    else
        nbits = 32;

    vchret.push_back(nclass);
    while (nbits >= 8)
    {
        vchret.push_back(getbyte(15 - nstartbyte));
        nstartbyte++;
        nbits -= 8;
    }
    if (nbits > 0)
        vchret.push_back(getbyte(15 - nstartbyte) | ((1 << nbits) - 1));

    return vchret;
}

uint64_t cnetaddr::gethash() const
{
    uint256 hash = hash(&ip[0], &ip[16]);
    uint64_t nret;
    memcpy(&nret, &hash, sizeof(nret));
    return nret;
}

// private extensions to enum network, only returned by getextnetwork,
// and only used in getreachabilityfrom
static const int net_unknown = net_max + 0;
static const int net_teredo  = net_max + 1;
int static getextnetwork(const cnetaddr *addr)
{
    if (addr == null)
        return net_unknown;
    if (addr->isrfc4380())
        return net_teredo;
    return addr->getnetwork();
}

/** calculates a metric for how reachable (*this) is from a given partner */
int cnetaddr::getreachabilityfrom(const cnetaddr *paddrpartner) const
{
    enum reachability {
        reach_unreachable,
        reach_default,
        reach_teredo,
        reach_ipv6_weak,
        reach_ipv4,
        reach_ipv6_strong,
        reach_private
    };

    if (!isroutable())
        return reach_unreachable;

    int ournet = getextnetwork(this);
    int theirnet = getextnetwork(paddrpartner);
    bool ftunnel = isrfc3964() || isrfc6052() || isrfc6145();

    switch(theirnet) {
    case net_ipv4:
        switch(ournet) {
        default:       return reach_default;
        case net_ipv4: return reach_ipv4;
        }
    case net_ipv6:
        switch(ournet) {
        default:         return reach_default;
        case net_teredo: return reach_teredo;
        case net_ipv4:   return reach_ipv4;
        case net_ipv6:   return ftunnel ? reach_ipv6_weak : reach_ipv6_strong; // only prefer giving our ipv6 address if it's not tunnelled
        }
    case net_tor:
        switch(ournet) {
        default:         return reach_default;
        case net_ipv4:   return reach_ipv4; // tor users can connect to ipv4 as well
        case net_tor:    return reach_private;
        }
    case net_teredo:
        switch(ournet) {
        default:          return reach_default;
        case net_teredo:  return reach_teredo;
        case net_ipv6:    return reach_ipv6_weak;
        case net_ipv4:    return reach_ipv4;
        }
    case net_unknown:
    case net_unroutable:
    default:
        switch(ournet) {
        default:          return reach_default;
        case net_teredo:  return reach_teredo;
        case net_ipv6:    return reach_ipv6_weak;
        case net_ipv4:    return reach_ipv4;
        case net_tor:     return reach_private; // either from tor, or don't care about our address
        }
    }
}

void cservice::init()
{
    port = 0;
}

cservice::cservice()
{
    init();
}

cservice::cservice(const cnetaddr& cip, unsigned short portin) : cnetaddr(cip), port(portin)
{
}

cservice::cservice(const struct in_addr& ipv4addr, unsigned short portin) : cnetaddr(ipv4addr), port(portin)
{
}

cservice::cservice(const struct in6_addr& ipv6addr, unsigned short portin) : cnetaddr(ipv6addr), port(portin)
{
}

cservice::cservice(const struct sockaddr_in& addr) : cnetaddr(addr.sin_addr), port(ntohs(addr.sin_port))
{
    assert(addr.sin_family == af_inet);
}

cservice::cservice(const struct sockaddr_in6 &addr) : cnetaddr(addr.sin6_addr), port(ntohs(addr.sin6_port))
{
   assert(addr.sin6_family == af_inet6);
}

bool cservice::setsockaddr(const struct sockaddr *paddr)
{
    switch (paddr->sa_family) {
    case af_inet:
        *this = cservice(*(const struct sockaddr_in*)paddr);
        return true;
    case af_inet6:
        *this = cservice(*(const struct sockaddr_in6*)paddr);
        return true;
    default:
        return false;
    }
}

cservice::cservice(const char *pszipport, bool fallowlookup)
{
    init();
    cservice ip;
    if (lookup(pszipport, ip, 0, fallowlookup))
        *this = ip;
}

cservice::cservice(const char *pszipport, int portdefault, bool fallowlookup)
{
    init();
    cservice ip;
    if (lookup(pszipport, ip, portdefault, fallowlookup))
        *this = ip;
}

cservice::cservice(const std::string &stripport, bool fallowlookup)
{
    init();
    cservice ip;
    if (lookup(stripport.c_str(), ip, 0, fallowlookup))
        *this = ip;
}

cservice::cservice(const std::string &stripport, int portdefault, bool fallowlookup)
{
    init();
    cservice ip;
    if (lookup(stripport.c_str(), ip, portdefault, fallowlookup))
        *this = ip;
}

unsigned short cservice::getport() const
{
    return port;
}

bool operator==(const cservice& a, const cservice& b)
{
    return (cnetaddr)a == (cnetaddr)b && a.port == b.port;
}

bool operator!=(const cservice& a, const cservice& b)
{
    return (cnetaddr)a != (cnetaddr)b || a.port != b.port;
}

bool operator<(const cservice& a, const cservice& b)
{
    return (cnetaddr)a < (cnetaddr)b || ((cnetaddr)a == (cnetaddr)b && a.port < b.port);
}

bool cservice::getsockaddr(struct sockaddr* paddr, socklen_t *addrlen) const
{
    if (isipv4()) {
        if (*addrlen < (socklen_t)sizeof(struct sockaddr_in))
            return false;
        *addrlen = sizeof(struct sockaddr_in);
        struct sockaddr_in *paddrin = (struct sockaddr_in*)paddr;
        memset(paddrin, 0, *addrlen);
        if (!getinaddr(&paddrin->sin_addr))
            return false;
        paddrin->sin_family = af_inet;
        paddrin->sin_port = htons(port);
        return true;
    }
    if (isipv6()) {
        if (*addrlen < (socklen_t)sizeof(struct sockaddr_in6))
            return false;
        *addrlen = sizeof(struct sockaddr_in6);
        struct sockaddr_in6 *paddrin6 = (struct sockaddr_in6*)paddr;
        memset(paddrin6, 0, *addrlen);
        if (!getin6addr(&paddrin6->sin6_addr))
            return false;
        paddrin6->sin6_family = af_inet6;
        paddrin6->sin6_port = htons(port);
        return true;
    }
    return false;
}

std::vector<unsigned char> cservice::getkey() const
{
     std::vector<unsigned char> vkey;
     vkey.resize(18);
     memcpy(&vkey[0], ip, 16);
     vkey[16] = port / 0x100;
     vkey[17] = port & 0x0ff;
     return vkey;
}

std::string cservice::tostringport() const
{
    return strprintf("%u", port);
}

std::string cservice::tostringipport() const
{
    if (isipv4() || istor()) {
        return tostringip() + ":" + tostringport();
    } else {
        return "[" + tostringip() + "]:" + tostringport();
    }
}

std::string cservice::tostring() const
{
    return tostringipport();
}

void cservice::setport(unsigned short portin)
{
    port = portin;
}

csubnet::csubnet():
    valid(false)
{
    memset(netmask, 0, sizeof(netmask));
}

csubnet::csubnet(const std::string &strsubnet, bool fallowlookup)
{
    size_t slash = strsubnet.find_last_of('/');
    std::vector<cnetaddr> vip;

    valid = true;
    // default to /32 (ipv4) or /128 (ipv6), i.e. match single address
    memset(netmask, 255, sizeof(netmask));

    std::string straddress = strsubnet.substr(0, slash);
    if (lookuphost(straddress.c_str(), vip, 1, fallowlookup))
    {
        network = vip[0];
        if (slash != strsubnet.npos)
        {
            std::string strnetmask = strsubnet.substr(slash + 1);
            int32_t n;
            // ipv4 addresses start at offset 12, and first 12 bytes must match, so just offset n
            const int astartofs = network.isipv4() ? 12 : 0;
            if (parseint32(strnetmask, &n)) // if valid number, assume /24 symtex
            {
                if(n >= 0 && n <= (128 - astartofs*8)) // only valid if in range of bits of address
                {
                    n += astartofs*8;
                    // clear bits [n..127]
                    for (; n < 128; ++n)
                        netmask[n>>3] &= ~(1<<(7-(n&7)));
                }
                else
                {
                    valid = false;
                }
            }
            else // if not a valid number, try full netmask syntax
            {
                if (lookuphost(strnetmask.c_str(), vip, 1, false)) // never allow lookup for netmask
                {
                    // copy only the *last* four bytes in case of ipv4, the rest of the mask should stay 1's as
                    // we don't want pchipv4 to be part of the mask.
                    for(int x=astartofs; x<16; ++x)
                        netmask[x] = vip[0].ip[x];
                }
                else
                {
                    valid = false;
                }
            }
        }
    }
    else
    {
        valid = false;
    }

    // normalize network according to netmask
    for(int x=0; x<16; ++x)
        network.ip[x] &= netmask[x];
}

bool csubnet::match(const cnetaddr &addr) const
{
    if (!valid || !addr.isvalid())
        return false;
    for(int x=0; x<16; ++x)
        if ((addr.ip[x] & netmask[x]) != network.ip[x])
            return false;
    return true;
}

std::string csubnet::tostring() const
{
    std::string strnetmask;
    if (network.isipv4())
        strnetmask = strprintf("%u.%u.%u.%u", netmask[12], netmask[13], netmask[14], netmask[15]);
    else
        strnetmask = strprintf("%x:%x:%x:%x:%x:%x:%x:%x",
                         netmask[0] << 8 | netmask[1], netmask[2] << 8 | netmask[3],
                         netmask[4] << 8 | netmask[5], netmask[6] << 8 | netmask[7],
                         netmask[8] << 8 | netmask[9], netmask[10] << 8 | netmask[11],
                         netmask[12] << 8 | netmask[13], netmask[14] << 8 | netmask[15]);
    return network.tostring() + "/" + strnetmask;
}

bool csubnet::isvalid() const
{
    return valid;
}

bool operator==(const csubnet& a, const csubnet& b)
{
    return a.valid == b.valid && a.network == b.network && !memcmp(a.netmask, b.netmask, 16);
}

bool operator!=(const csubnet& a, const csubnet& b)
{
    return !(a==b);
}

bool operator<(const csubnet& a, const csubnet& b)
{
    return (a.network < b.network || (a.network == b.network && memcmp(a.netmask, b.netmask, 16) < 0));
}

#ifdef win32
std::string networkerrorstring(int err)
{
    char buf[256];
    buf[0] = 0;
    if(formatmessagea(format_message_from_system | format_message_ignore_inserts | format_message_max_width_mask,
            null, err, makelangid(lang_neutral, sublang_default),
            buf, sizeof(buf), null))
    {
        return strprintf("%s (%d)", buf, err);
    }
    else
    {
        return strprintf("unknown error (%d)", err);
    }
}
#else
std::string networkerrorstring(int err)
{
    char buf[256];
    const char *s = buf;
    buf[0] = 0;
    /* too bad there are two incompatible implementations of the
     * thread-safe strerror. */
#ifdef strerror_r_char_p /* gnu variant can return a pointer outside the passed buffer */
    s = strerror_r(err, buf, sizeof(buf));
#else /* posix variant always returns message in buffer */
    if (strerror_r(err, buf, sizeof(buf)))
        buf[0] = 0;
#endif
    return strprintf("%s (%d)", s, err);
}
#endif

bool closesocket(socket& hsocket)
{
    if (hsocket == invalid_socket)
        return false;
#ifdef win32
    int ret = closesocket(hsocket);
#else
    int ret = close(hsocket);
#endif
    hsocket = invalid_socket;
    return ret != socket_error;
}

bool setsocketnonblocking(socket& hsocket, bool fnonblocking)
{
    if (fnonblocking) {
#ifdef win32
        u_long none = 1;
        if (ioctlsocket(hsocket, fionbio, &none) == socket_error) {
#else
        int fflags = fcntl(hsocket, f_getfl, 0);
        if (fcntl(hsocket, f_setfl, fflags | o_nonblock) == socket_error) {
#endif
            closesocket(hsocket);
            return false;
        }
    } else {
#ifdef win32
        u_long nzero = 0;
        if (ioctlsocket(hsocket, fionbio, &nzero) == socket_error) {
#else
        int fflags = fcntl(hsocket, f_getfl, 0);
        if (fcntl(hsocket, f_setfl, fflags & ~o_nonblock) == socket_error) {
#endif
            closesocket(hsocket);
            return false;
        }
    }

    return true;
}
