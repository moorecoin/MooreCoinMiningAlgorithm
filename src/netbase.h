// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_netbase_h
#define moorecoin_netbase_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "compat.h"
#include "serialize.h"

#include <stdint.h>
#include <string>
#include <vector>

extern int nconnecttimeout;
extern bool fnamelookup;

/** -timeout default */
static const int default_connect_timeout = 5000;

#ifdef win32
// in msvc, this is defined as a macro, undefine it to prevent a compile and link error
#undef setport
#endif

enum network
{
    net_unroutable = 0,
    net_ipv4,
    net_ipv6,
    net_tor,

    net_max,
};

/** ip address (ipv6, or ipv4 using mapped ipv6 range (::ffff:0:0/96)) */
class cnetaddr
{
    protected:
        unsigned char ip[16]; // in network byte order

    public:
        cnetaddr();
        cnetaddr(const struct in_addr& ipv4addr);
        explicit cnetaddr(const char *pszip, bool fallowlookup = false);
        explicit cnetaddr(const std::string &strip, bool fallowlookup = false);
        void init();
        void setip(const cnetaddr& ip);

        /**
         * set raw ipv4 or ipv6 address (in network byte order)
         * @note only net_ipv4 and net_ipv6 are allowed for network.
         */
        void setraw(network network, const uint8_t *data);

        bool setspecial(const std::string &strname); // for tor addresses
        bool isipv4() const;    // ipv4 mapped address (::ffff:0:0/96, 0.0.0.0/0)
        bool isipv6() const;    // ipv6 address (not mapped ipv4, not tor)
        bool isrfc1918() const; // ipv4 private networks (10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12)
        bool isrfc2544() const; // ipv4 inter-network communcations (192.18.0.0/15)
        bool isrfc6598() const; // ipv4 isp-level nat (100.64.0.0/10)
        bool isrfc5737() const; // ipv4 documentation addresses (192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24)
        bool isrfc3849() const; // ipv6 documentation address (2001:0db8::/32)
        bool isrfc3927() const; // ipv4 autoconfig (169.254.0.0/16)
        bool isrfc3964() const; // ipv6 6to4 tunnelling (2002::/16)
        bool isrfc4193() const; // ipv6 unique local (fc00::/7)
        bool isrfc4380() const; // ipv6 teredo tunnelling (2001::/32)
        bool isrfc4843() const; // ipv6 orchid (2001:10::/28)
        bool isrfc4862() const; // ipv6 autoconfig (fe80::/64)
        bool isrfc6052() const; // ipv6 well-known prefix (64:ff9b::/96)
        bool isrfc6145() const; // ipv6 ipv4-translated address (::ffff:0:0:0/96)
        bool istor() const;
        bool islocal() const;
        bool isroutable() const;
        bool isvalid() const;
        bool ismulticast() const;
        enum network getnetwork() const;
        std::string tostring() const;
        std::string tostringip() const;
        unsigned int getbyte(int n) const;
        uint64_t gethash() const;
        bool getinaddr(struct in_addr* pipv4addr) const;
        std::vector<unsigned char> getgroup() const;
        int getreachabilityfrom(const cnetaddr *paddrpartner = null) const;

        cnetaddr(const struct in6_addr& pipv6addr);
        bool getin6addr(struct in6_addr* pipv6addr) const;

        friend bool operator==(const cnetaddr& a, const cnetaddr& b);
        friend bool operator!=(const cnetaddr& a, const cnetaddr& b);
        friend bool operator<(const cnetaddr& a, const cnetaddr& b);

        add_serialize_methods;

        template <typename stream, typename operation>
        inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
            readwrite(flatdata(ip));
        }

        friend class csubnet;
};

class csubnet
{
    protected:
        /// network (base) address
        cnetaddr network;
        /// netmask, in network byte order
        uint8_t netmask[16];
        /// is this value valid? (only used to signal parse errors)
        bool valid;

    public:
        csubnet();
        explicit csubnet(const std::string &strsubnet, bool fallowlookup = false);

        bool match(const cnetaddr &addr) const;

        std::string tostring() const;
        bool isvalid() const;

        friend bool operator==(const csubnet& a, const csubnet& b);
        friend bool operator!=(const csubnet& a, const csubnet& b);
        friend bool operator<(const csubnet& a, const csubnet& b);
};

/** a combination of a network address (cnetaddr) and a (tcp) port */
class cservice : public cnetaddr
{
    protected:
        unsigned short port; // host order

    public:
        cservice();
        cservice(const cnetaddr& ip, unsigned short port);
        cservice(const struct in_addr& ipv4addr, unsigned short port);
        cservice(const struct sockaddr_in& addr);
        explicit cservice(const char *pszipport, int portdefault, bool fallowlookup = false);
        explicit cservice(const char *pszipport, bool fallowlookup = false);
        explicit cservice(const std::string& stripport, int portdefault, bool fallowlookup = false);
        explicit cservice(const std::string& stripport, bool fallowlookup = false);
        void init();
        void setport(unsigned short portin);
        unsigned short getport() const;
        bool getsockaddr(struct sockaddr* paddr, socklen_t *addrlen) const;
        bool setsockaddr(const struct sockaddr* paddr);
        friend bool operator==(const cservice& a, const cservice& b);
        friend bool operator!=(const cservice& a, const cservice& b);
        friend bool operator<(const cservice& a, const cservice& b);
        std::vector<unsigned char> getkey() const;
        std::string tostring() const;
        std::string tostringport() const;
        std::string tostringipport() const;

        cservice(const struct in6_addr& ipv6addr, unsigned short port);
        cservice(const struct sockaddr_in6& addr);

        add_serialize_methods;

        template <typename stream, typename operation>
        inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
            readwrite(flatdata(ip));
            unsigned short portn = htons(port);
            readwrite(flatdata(portn));
            if (ser_action.forread())
                 port = ntohs(portn);
        }
};

class proxytype
{
public:
    proxytype(): randomize_credentials(false) {}
    proxytype(const cservice &proxy, bool randomize_credentials=false): proxy(proxy), randomize_credentials(randomize_credentials) {}

    bool isvalid() const { return proxy.isvalid(); }

    cservice proxy;
    bool randomize_credentials;
};

enum network parsenetwork(std::string net);
std::string getnetworkname(enum network net);
void splithostport(std::string in, int &portout, std::string &hostout);
bool setproxy(enum network net, const proxytype &addrproxy);
bool getproxy(enum network net, proxytype &proxyinfoout);
bool isproxy(const cnetaddr &addr);
bool setnameproxy(const proxytype &addrproxy);
bool havenameproxy();
bool lookuphost(const char *pszname, std::vector<cnetaddr>& vip, unsigned int nmaxsolutions = 0, bool fallowlookup = true);
bool lookup(const char *pszname, cservice& addr, int portdefault = 0, bool fallowlookup = true);
bool lookup(const char *pszname, std::vector<cservice>& vaddr, int portdefault = 0, bool fallowlookup = true, unsigned int nmaxsolutions = 0);
bool lookupnumeric(const char *pszname, cservice& addr, int portdefault = 0);
bool connectsocket(const cservice &addr, socket& hsocketret, int ntimeout, bool *outproxyconnectionfailed = 0);
bool connectsocketbyname(cservice &addr, socket& hsocketret, const char *pszdest, int portdefault, int ntimeout, bool *outproxyconnectionfailed = 0);
/** return readable error string for a network error code */
std::string networkerrorstring(int err);
/** close socket and set hsocket to invalid_socket */
bool closesocket(socket& hsocket);
/** disable or enable blocking-mode for a socket */
bool setsocketnonblocking(socket& hsocket, bool fnonblocking);

#endif // moorecoin_netbase_h
