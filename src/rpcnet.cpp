// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"

#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "protocol.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"
#include "version.h"

#include <boost/foreach.hpp>

#include "univalue/univalue.h"

using namespace std;

univalue getconnectioncount(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "\nreturns the number of connections to other nodes.\n"
            "\nbresult:\n"
            "n          (numeric) the connection count\n"
            "\nexamples:\n"
            + helpexamplecli("getconnectioncount", "")
            + helpexamplerpc("getconnectioncount", "")
        );

    lock2(cs_main, cs_vnodes);

    return (int)vnodes.size();
}

univalue ping(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "ping\n"
            "\nrequests that a ping be sent to all other nodes, to measure ping time.\n"
            "results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            "ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n"
            "\nexamples:\n"
            + helpexamplecli("ping", "")
            + helpexamplerpc("ping", "")
        );

    // request that each node send a ping during next message processing pass
    lock2(cs_main, cs_vnodes);

    boost_foreach(cnode* pnode, vnodes) {
        pnode->fpingqueued = true;
    }

    return nullunivalue;
}

static void copynodestats(std::vector<cnodestats>& vstats)
{
    vstats.clear();

    lock(cs_vnodes);
    vstats.reserve(vnodes.size());
    boost_foreach(cnode* pnode, vnodes) {
        cnodestats stats;
        pnode->copystats(stats);
        vstats.push_back(stats);
    }
}

univalue getpeerinfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getpeerinfo\n"
            "\nreturns data about each connected network node as a json array of objects.\n"
            "\nbresult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                   (numeric) peer index\n"
            "    \"addr\":\"host:port\",      (string) the ip address and port of the peer\n"
            "    \"addrlocal\":\"ip:port\",   (string) local address\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\",   (string) the services offered\n"
            "    \"lastsend\": ttt,           (numeric) the time in seconds since epoch (jan 1 1970 gmt) of the last send\n"
            "    \"lastrecv\": ttt,           (numeric) the time in seconds since epoch (jan 1 1970 gmt) of the last receive\n"
            "    \"bytessent\": n,            (numeric) the total bytes sent\n"
            "    \"bytesrecv\": n,            (numeric) the total bytes received\n"
            "    \"conntime\": ttt,           (numeric) the connection time in seconds since epoch (jan 1 1970 gmt)\n"
            "    \"timeoffset\": ttt,         (numeric) the time offset in seconds\n"
            "    \"pingtime\": n,             (numeric) ping time\n"
            "    \"pingwait\": n,             (numeric) ping wait\n"
            "    \"version\": v,              (numeric) the peer version, such as 7001\n"
            "    \"subver\": \"/satoshi:0.8.5/\",  (string) the string version\n"
            "    \"inbound\": true|false,     (boolean) inbound (true) or outbound (false)\n"
            "    \"startingheight\": n,       (numeric) the starting height (block) of the peer\n"
            "    \"banscore\": n,             (numeric) the ban score\n"
            "    \"synced_headers\": n,       (numeric) the last header we have in common with this peer\n"
            "    \"synced_blocks\": n,        (numeric) the last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                        (numeric) the heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nexamples:\n"
            + helpexamplecli("getpeerinfo", "")
            + helpexamplerpc("getpeerinfo", "")
        );

    lock(cs_main);

    vector<cnodestats> vstats;
    copynodestats(vstats);

    univalue ret(univalue::varr);

    boost_foreach(const cnodestats& stats, vstats) {
        univalue obj(univalue::vobj);
        cnodestatestats statestats;
        bool fstatestats = getnodestatestats(stats.nodeid, statestats);
        obj.push_back(pair("id", stats.nodeid));
        obj.push_back(pair("addr", stats.addrname));
        if (!(stats.addrlocal.empty()))
            obj.push_back(pair("addrlocal", stats.addrlocal));
        obj.push_back(pair("services", strprintf("%016x", stats.nservices)));
        obj.push_back(pair("lastsend", stats.nlastsend));
        obj.push_back(pair("lastrecv", stats.nlastrecv));
        obj.push_back(pair("bytessent", stats.nsendbytes));
        obj.push_back(pair("bytesrecv", stats.nrecvbytes));
        obj.push_back(pair("conntime", stats.ntimeconnected));
        obj.push_back(pair("timeoffset", stats.ntimeoffset));
        obj.push_back(pair("pingtime", stats.dpingtime));
        if (stats.dpingwait > 0.0)
            obj.push_back(pair("pingwait", stats.dpingwait));
        obj.push_back(pair("version", stats.nversion));
        // use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the json output by putting special characters in
        // their ver message.
        obj.push_back(pair("subver", stats.cleansubver));
        obj.push_back(pair("inbound", stats.finbound));
        obj.push_back(pair("startingheight", stats.nstartingheight));
        if (fstatestats) {
            obj.push_back(pair("banscore", statestats.nmisbehavior));
            obj.push_back(pair("synced_headers", statestats.nsyncheight));
            obj.push_back(pair("synced_blocks", statestats.ncommonheight));
            univalue heights(univalue::varr);
            boost_foreach(int height, statestats.vheightinflight) {
                heights.push_back(height);
            }
            obj.push_back(pair("inflight", heights));
        }
        obj.push_back(pair("whitelisted", stats.fwhitelisted));

        ret.push_back(obj);
    }

    return ret;
}

univalue addnode(const univalue& params, bool fhelp)
{
    string strcommand;
    if (params.size() == 2)
        strcommand = params[1].get_str();
    if (fhelp || params.size() != 2 ||
        (strcommand != "onetry" && strcommand != "add" && strcommand != "remove"))
        throw runtime_error(
            "addnode \"node\" \"add|remove|onetry\"\n"
            "\nattempts add or remove a node from the addnode list.\n"
            "or try a connection to a node once.\n"
            "\narguments:\n"
            "1. \"node\"     (string, required) the node (see getpeerinfo for nodes)\n"
            "2. \"command\"  (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once\n"
            "\nexamples:\n"
            + helpexamplecli("addnode", "\"192.168.0.6:8333\" \"onetry\"")
            + helpexamplerpc("addnode", "\"192.168.0.6:8333\", \"onetry\"")
        );

    string strnode = params[0].get_str();

    if (strcommand == "onetry")
    {
        caddress addr;
        opennetworkconnection(addr, null, strnode.c_str());
        return nullunivalue;
    }

    lock(cs_vaddednodes);
    vector<string>::iterator it = vaddednodes.begin();
    for(; it != vaddednodes.end(); it++)
        if (strnode == *it)
            break;

    if (strcommand == "add")
    {
        if (it != vaddednodes.end())
            throw jsonrpcerror(rpc_client_node_already_added, "error: node already added");
        vaddednodes.push_back(strnode);
    }
    else if(strcommand == "remove")
    {
        if (it == vaddednodes.end())
            throw jsonrpcerror(rpc_client_node_not_added, "error: node has not been added.");
        vaddednodes.erase(it);
    }

    return nullunivalue;
}

univalue disconnectnode(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 1)
        throw runtime_error(
            "disconnectnode \"node\" \n"
            "\nimmediately disconnects from the specified node.\n"
            "\narguments:\n"
            "1. \"node\"     (string, required) the node (see getpeerinfo for nodes)\n"
            "\nexamples:\n"
            + helpexamplecli("disconnectnode", "\"192.168.0.6:8333\"")
            + helpexamplerpc("disconnectnode", "\"192.168.0.6:8333\"")
        );

    cnode* pnode = findnode(params[0].get_str());
    if (pnode == null)
        throw jsonrpcerror(rpc_client_node_not_connected, "node not found in connected nodes");

    pnode->fdisconnect = true;

    return nullunivalue;
}

univalue getaddednodeinfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getaddednodeinfo dns ( \"node\" )\n"
            "\nreturns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"
            "if dns is false, only a list of added nodes will be provided,\n"
            "otherwise connected information will also be available.\n"
            "\narguments:\n"
            "1. dns        (boolean, required) if false, only a list of added nodes will be provided, otherwise connected information will also be available.\n"
            "2. \"node\"   (string, optional) if provided, return information about this specific node, otherwise all nodes are returned.\n"
            "\nresult:\n"
            "[\n"
            "  {\n"
            "    \"addednode\" : \"192.168.0.201\",   (string) the node ip address\n"
            "    \"connected\" : true|false,          (boolean) if connected\n"
            "    \"addresses\" : [\n"
            "       {\n"
            "         \"address\" : \"192.168.0.201:8333\",  (string) the moorecoin server host and port\n"
            "         \"connected\" : \"outbound\"           (string) connection, inbound or outbound\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nexamples:\n"
            + helpexamplecli("getaddednodeinfo", "true")
            + helpexamplecli("getaddednodeinfo", "true \"192.168.0.201\"")
            + helpexamplerpc("getaddednodeinfo", "true, \"192.168.0.201\"")
        );

    bool fdns = params[0].get_bool();

    list<string> laddednodes(0);
    if (params.size() == 1)
    {
        lock(cs_vaddednodes);
        boost_foreach(const std::string& straddnode, vaddednodes)
            laddednodes.push_back(straddnode);
    }
    else
    {
        string strnode = params[1].get_str();
        lock(cs_vaddednodes);
        boost_foreach(const std::string& straddnode, vaddednodes) {
            if (straddnode == strnode)
            {
                laddednodes.push_back(straddnode);
                break;
            }
        }
        if (laddednodes.size() == 0)
            throw jsonrpcerror(rpc_client_node_not_added, "error: node has not been added.");
    }

    univalue ret(univalue::varr);
    if (!fdns)
    {
        boost_foreach (const std::string& straddnode, laddednodes) {
            univalue obj(univalue::vobj);
            obj.push_back(pair("addednode", straddnode));
            ret.push_back(obj);
        }
        return ret;
    }

    list<pair<string, vector<cservice> > > laddedaddreses(0);
    boost_foreach(const std::string& straddnode, laddednodes) {
        vector<cservice> vservnode(0);
        if(lookup(straddnode.c_str(), vservnode, params().getdefaultport(), fnamelookup, 0))
            laddedaddreses.push_back(make_pair(straddnode, vservnode));
        else
        {
            univalue obj(univalue::vobj);
            obj.push_back(pair("addednode", straddnode));
            obj.push_back(pair("connected", false));
            univalue addresses(univalue::varr);
            obj.push_back(pair("addresses", addresses));
        }
    }

    lock(cs_vnodes);
    for (list<pair<string, vector<cservice> > >::iterator it = laddedaddreses.begin(); it != laddedaddreses.end(); it++)
    {
        univalue obj(univalue::vobj);
        obj.push_back(pair("addednode", it->first));

        univalue addresses(univalue::varr);
        bool fconnected = false;
        boost_foreach(const cservice& addrnode, it->second) {
            bool ffound = false;
            univalue node(univalue::vobj);
            node.push_back(pair("address", addrnode.tostring()));
            boost_foreach(cnode* pnode, vnodes) {
                if (pnode->addr == addrnode)
                {
                    ffound = true;
                    fconnected = true;
                    node.push_back(pair("connected", pnode->finbound ? "inbound" : "outbound"));
                    break;
                }
            }
            if (!ffound)
                node.push_back(pair("connected", "false"));
            addresses.push_back(node);
        }
        obj.push_back(pair("connected", fconnected));
        obj.push_back(pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}

univalue getnettotals(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() > 0)
        throw runtime_error(
            "getnettotals\n"
            "\nreturns information about network traffic, including bytes in, bytes out,\n"
            "and current time.\n"
            "\nresult:\n"
            "{\n"
            "  \"totalbytesrecv\": n,   (numeric) total bytes received\n"
            "  \"totalbytessent\": n,   (numeric) total bytes sent\n"
            "  \"timemillis\": t        (numeric) total cpu time\n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getnettotals", "")
            + helpexamplerpc("getnettotals", "")
       );

    univalue obj(univalue::vobj);
    obj.push_back(pair("totalbytesrecv", cnode::gettotalbytesrecv()));
    obj.push_back(pair("totalbytessent", cnode::gettotalbytessent()));
    obj.push_back(pair("timemillis", gettimemillis()));
    return obj;
}

static univalue getnetworksinfo()
{
    univalue networks(univalue::varr);
    for(int n=0; n<net_max; ++n)
    {
        enum network network = static_cast<enum network>(n);
        if(network == net_unroutable)
            continue;
        proxytype proxy;
        univalue obj(univalue::vobj);
        getproxy(network, proxy);
        obj.push_back(pair("name", getnetworkname(network)));
        obj.push_back(pair("limited", islimited(network)));
        obj.push_back(pair("reachable", isreachable(network)));
        obj.push_back(pair("proxy", proxy.isvalid() ? proxy.proxy.tostringipport() : string()));
        obj.push_back(pair("proxy_randomize_credentials", proxy.randomize_credentials));
        networks.push_back(obj);
    }
    return networks;
}

univalue getnetworkinfo(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
            "getnetworkinfo\n"
            "returns an object containing various state info regarding p2p networking.\n"
            "\nresult:\n"
            "{\n"
            "  \"version\": xxxxx,                      (numeric) the server version\n"
            "  \"subversion\": \"/satoshi:x.x.x/\",     (string) the server subversion string\n"
            "  \"protocolversion\": xxxxx,              (numeric) the protocol version\n"
            "  \"localservices\": \"xxxxxxxxxxxxxxxx\", (string) the services we offer to the network\n"
            "  \"timeoffset\": xxxxx,                   (numeric) the time offset\n"
            "  \"connections\": xxxxx,                  (numeric) the number of connections\n"
            "  \"networks\": [                          (array) information per network\n"
            "  {\n"
            "    \"name\": \"xxx\",                     (string) network (ipv4, ipv6 or onion)\n"
            "    \"limited\": true|false,               (boolean) is the network limited using -onlynet?\n"
            "    \"reachable\": true|false,             (boolean) is the network reachable?\n"
            "    \"proxy\": \"host:port\"               (string) the proxy that is used for this network, or empty if none\n"
            "  }\n"
            "  ,...\n"
            "  ],\n"
            "  \"relayfee\": x.xxxxxxxx,                (numeric) minimum relay fee for non-free transactions in btc/kb\n"
            "  \"localaddresses\": [                    (array) list of local addresses\n"
            "  {\n"
            "    \"address\": \"xxxx\",                 (string) network address\n"
            "    \"port\": xxx,                         (numeric) network port\n"
            "    \"score\": xxx                         (numeric) relative score\n"
            "  }\n"
            "  ,...\n"
            "  ]\n"
            "  \"warnings\": \"...\"                    (string) any network warnings (such as alert messages) \n"
            "}\n"
            "\nexamples:\n"
            + helpexamplecli("getnetworkinfo", "")
            + helpexamplerpc("getnetworkinfo", "")
        );

    lock(cs_main);

    univalue obj(univalue::vobj);
    obj.push_back(pair("version",       client_version));
    obj.push_back(pair("subversion",
        formatsubversion(client_name, client_version, std::vector<string>())));
    obj.push_back(pair("protocolversion",protocol_version));
    obj.push_back(pair("localservices",       strprintf("%016x", nlocalservices)));
    obj.push_back(pair("timeoffset",    gettimeoffset()));
    obj.push_back(pair("connections",   (int)vnodes.size()));
    obj.push_back(pair("networks",      getnetworksinfo()));
    obj.push_back(pair("relayfee",      valuefromamount(::minrelaytxfee.getfeeperk())));
    univalue localaddresses(univalue::varr);
    {
        lock(cs_maplocalhost);
        boost_foreach(const pairtype(cnetaddr, localserviceinfo) &item, maplocalhost)
        {
            univalue rec(univalue::vobj);
            rec.push_back(pair("address", item.first.tostring()));
            rec.push_back(pair("port", item.second.nport));
            rec.push_back(pair("score", item.second.nscore));
            localaddresses.push_back(rec);
        }
    }
    obj.push_back(pair("localaddresses", localaddresses));
    obj.push_back(pair("warnings",       getwarnings("statusbar")));
    return obj;
}

univalue setban(const univalue& params, bool fhelp)
{
    string strcommand;
    if (params.size() >= 2)
        strcommand = params[1].get_str();
    if (fhelp || params.size() < 2 ||
        (strcommand != "add" && strcommand != "remove"))
        throw runtime_error(
                            "setban \"ip(/netmask)\" \"add|remove\" (bantime) (absolute)\n"
                            "\nattempts add or remove a ip/subnet from the banned list.\n"
                            "\narguments:\n"
                            "1. \"ip(/netmask)\" (string, required) the ip/subnet (see getpeerinfo for nodes ip) with a optional netmask (default is /32 = single ip)\n"
                            "2. \"command\"      (string, required) 'add' to add a ip/subnet to the list, 'remove' to remove a ip/subnet from the list\n"
                            "3. \"bantime\"      (numeric, optional) time in seconds how long (or until when if [absolute] is set) the ip is banned (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)\n"
                            "4. \"absolute\"     (boolean, optional) if set, the bantime must be a absolute timestamp in seconds since epoch (jan 1 1970 gmt)\n"
                            "\nexamples:\n"
                            + helpexamplecli("setban", "\"192.168.0.6\" \"add\" 86400")
                            + helpexamplecli("setban", "\"192.168.0.0/24\" \"add\"")
                            + helpexamplerpc("setban", "\"192.168.0.6\", \"add\" 86400")
                            );

    csubnet subnet;
    cnetaddr netaddr;
    bool issubnet = false;

    if (params[0].get_str().find("/") != string::npos)
        issubnet = true;

    if (!issubnet)
        netaddr = cnetaddr(params[0].get_str());
    else
        subnet = csubnet(params[0].get_str());

    if (! (issubnet ? subnet.isvalid() : netaddr.isvalid()) )
        throw jsonrpcerror(rpc_client_node_already_added, "error: invalid ip/subnet");

    if (strcommand == "add")
    {
        if (issubnet ? cnode::isbanned(subnet) : cnode::isbanned(netaddr))
            throw jsonrpcerror(rpc_client_node_already_added, "error: ip/subnet already banned");

        int64_t bantime = 0; //use standard bantime if not specified
        if (params.size() >= 3 && !params[2].isnull())
            bantime = params[2].get_int64();

        bool absolute = false;
        if (params.size() == 4 && params[3].istrue())
            absolute = true;

        issubnet ? cnode::ban(subnet, bantime, absolute) : cnode::ban(netaddr, bantime, absolute);

        //disconnect possible nodes
        while(cnode *bannednode = (issubnet ? findnode(subnet) : findnode(netaddr)))
            bannednode->fdisconnect = true;
    }
    else if(strcommand == "remove")
    {
        if (!( issubnet ? cnode::unban(subnet) : cnode::unban(netaddr) ))
            throw jsonrpcerror(rpc_misc_error, "error: unban failed");
    }

    return nullunivalue;
}

univalue listbanned(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
                            "listbanned\n"
                            "\nlist all banned ips/subnets.\n"
                            "\nexamples:\n"
                            + helpexamplecli("listbanned", "")
                            + helpexamplerpc("listbanned", "")
                            );

    std::map<csubnet, int64_t> banmap;
    cnode::getbanned(banmap);

    univalue bannedaddresses(univalue::varr);
    for (std::map<csubnet, int64_t>::iterator it = banmap.begin(); it != banmap.end(); it++)
    {
        univalue rec(univalue::vobj);
        rec.push_back(pair("address", (*it).first.tostring()));
        rec.push_back(pair("banned_untill", (*it).second));
        bannedaddresses.push_back(rec);
    }

    return bannedaddresses;
}

univalue clearbanned(const univalue& params, bool fhelp)
{
    if (fhelp || params.size() != 0)
        throw runtime_error(
                            "clearbanned\n"
                            "\nclear all banned ips.\n"
                            "\nexamples:\n"
                            + helpexamplecli("clearbanned", "")
                            + helpexamplerpc("clearbanned", "")
                            );

    cnode::clearbanned();

    return nullunivalue;
}
