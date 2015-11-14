// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcprotocol.h"

#include "clientversion.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "version.h"

#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/shared_ptr.hpp>

#include "univalue/univalue.h"

using namespace std;

//! number of bytes to allocate and read at most at once in post data
const size_t post_read_size = 256 * 1024;

/**
 * http protocol
 * 
 * this ain't apache.  we're just using http header for the length field
 * and to be compatible with other json-rpc implementations.
 */

string httppost(const string& strmsg, const map<string,string>& maprequestheaders)
{
    ostringstream s;
    s << "post / http/1.1\r\n"
      << "user-agent: moorecoin-json-rpc/" << formatfullversion() << "\r\n"
      << "host: 127.0.0.1\r\n"
      << "content-type: application/json\r\n"
      << "content-length: " << strmsg.size() << "\r\n"
      << "connection: close\r\n"
      << "accept: application/json\r\n";
    boost_foreach(const pairtype(string, string)& item, maprequestheaders)
        s << item.first << ": " << item.second << "\r\n";
    s << "\r\n" << strmsg;

    return s.str();
}

static string rfc1123time()
{
    return datetimestrformat("%a, %d %b %y %h:%m:%s +0000", gettime());
}

static const char *httpstatusdescription(int nstatus)
{
    switch (nstatus) {
        case http_ok: return "ok";
        case http_bad_request: return "bad request";
        case http_forbidden: return "forbidden";
        case http_not_found: return "not found";
        case http_internal_server_error: return "internal server error";
        default: return "";
    }
}

string httperror(int nstatus, bool keepalive, bool headersonly)
{
    if (nstatus == http_unauthorized)
        return strprintf("http/1.0 401 authorization required\r\n"
            "date: %s\r\n"
            "server: moorecoin-json-rpc/%s\r\n"
            "www-authenticate: basic realm=\"jsonrpc\"\r\n"
            "content-type: text/html\r\n"
            "content-length: 296\r\n"
            "\r\n"
            "<!doctype html public \"-//w3c//dtd html 4.01 transitional//en\"\r\n"
            "\"http://www.w3.org/tr/1999/rec-html401-19991224/loose.dtd\">\r\n"
            "<html>\r\n"
            "<head>\r\n"
            "<title>error</title>\r\n"
            "<meta http-equiv='content-type' content='text/html; charset=iso-8859-1'>\r\n"
            "</head>\r\n"
            "<body><h1>401 unauthorized.</h1></body>\r\n"
            "</html>\r\n", rfc1123time(), formatfullversion());

    return httpreply(nstatus, httpstatusdescription(nstatus), keepalive,
                     headersonly, "text/plain");
}

string httpreplyheader(int nstatus, bool keepalive, size_t contentlength, const char *contenttype)
{
    return strprintf(
            "http/1.1 %d %s\r\n"
            "date: %s\r\n"
            "connection: %s\r\n"
            "content-length: %u\r\n"
            "content-type: %s\r\n"
            "server: moorecoin-json-rpc/%s\r\n"
            "\r\n",
        nstatus,
        httpstatusdescription(nstatus),
        rfc1123time(),
        keepalive ? "keep-alive" : "close",
        contentlength,
        contenttype,
        formatfullversion());
}

string httpreply(int nstatus, const string& strmsg, bool keepalive,
                 bool headersonly, const char *contenttype)
{
    if (headersonly)
    {
        return httpreplyheader(nstatus, keepalive, 0, contenttype);
    } else {
        return httpreplyheader(nstatus, keepalive, strmsg.size(), contenttype) + strmsg;
    }
}

bool readhttprequestline(std::basic_istream<char>& stream, int &proto,
                         string& http_method, string& http_uri)
{
    string str;
    getline(stream, str);

    // http request line is space-delimited
    vector<string> vwords;
    boost::split(vwords, str, boost::is_any_of(" "));
    if (vwords.size() < 2)
        return false;

    // http methods permitted: get, post
    http_method = vwords[0];
    if (http_method != "get" && http_method != "post")
        return false;

    // http uri must be an absolute path, relative to current host
    http_uri = vwords[1];
    if (http_uri.size() == 0 || http_uri[0] != '/')
        return false;

    // parse proto, if present
    string strproto = "";
    if (vwords.size() > 2)
        strproto = vwords[2];

    proto = 0;
    const char *ver = strstr(strproto.c_str(), "http/1.");
    if (ver != null)
        proto = atoi(ver+7);

    return true;
}

int readhttpstatus(std::basic_istream<char>& stream, int &proto)
{
    string str;
    getline(stream, str);
    vector<string> vwords;
    boost::split(vwords, str, boost::is_any_of(" "));
    if (vwords.size() < 2)
        return http_internal_server_error;
    proto = 0;
    const char *ver = strstr(str.c_str(), "http/1.");
    if (ver != null)
        proto = atoi(ver+7);
    return atoi(vwords[1].c_str());
}

int readhttpheaders(std::basic_istream<char>& stream, map<string, string>& mapheadersret)
{
    int nlen = 0;
    while (true)
    {
        string str;
        std::getline(stream, str);
        if (str.empty() || str == "\r")
            break;
        string::size_type ncolon = str.find(":");
        if (ncolon != string::npos)
        {
            string strheader = str.substr(0, ncolon);
            boost::trim(strheader);
            boost::to_lower(strheader);
            string strvalue = str.substr(ncolon+1);
            boost::trim(strvalue);
            mapheadersret[strheader] = strvalue;
            if (strheader == "content-length")
                nlen = atoi(strvalue.c_str());
        }
    }
    return nlen;
}


int readhttpmessage(std::basic_istream<char>& stream, map<string,
                    string>& mapheadersret, string& strmessageret,
                    int nproto, size_t max_size)
{
    mapheadersret.clear();
    strmessageret = "";

    // read header
    int nlen = readhttpheaders(stream, mapheadersret);
    if (nlen < 0 || (size_t)nlen > max_size)
        return http_internal_server_error;

    // read message
    if (nlen > 0)
    {
        vector<char> vch;
        size_t ptr = 0;
        while (ptr < (size_t)nlen)
        {
            size_t bytes_to_read = std::min((size_t)nlen - ptr, post_read_size);
            vch.resize(ptr + bytes_to_read);
            stream.read(&vch[ptr], bytes_to_read);
            if (!stream) // connection lost while reading
                return http_internal_server_error;
            ptr += bytes_to_read;
        }
        strmessageret = string(vch.begin(), vch.end());
    }

    string sconhdr = mapheadersret["connection"];

    if ((sconhdr != "close") && (sconhdr != "keep-alive"))
    {
        if (nproto >= 1)
            mapheadersret["connection"] = "keep-alive";
        else
            mapheadersret["connection"] = "close";
    }

    return http_ok;
}

/**
 * json-rpc protocol.  moorecoin speaks version 1.0 for maximum compatibility,
 * but uses json-rpc 1.1/2.0 standards for parts of the 1.0 standard that were
 * unspecified (http errors and contents of 'error').
 * 
 * 1.0 spec: http://json-rpc.org/wiki/specification
 * 1.2 spec: http://jsonrpc.org/historical/json-rpc-over-http.html
 */

string jsonrpcrequest(const string& strmethod, const univalue& params, const univalue& id)
{
    univalue request(univalue::vobj);
    request.push_back(pair("method", strmethod));
    request.push_back(pair("params", params));
    request.push_back(pair("id", id));
    return request.write() + "\n";
}

univalue jsonrpcreplyobj(const univalue& result, const univalue& error, const univalue& id)
{
    univalue reply(univalue::vobj);
    if (!error.isnull())
        reply.push_back(pair("result", nullunivalue));
    else
        reply.push_back(pair("result", result));
    reply.push_back(pair("error", error));
    reply.push_back(pair("id", id));
    return reply;
}

string jsonrpcreply(const univalue& result, const univalue& error, const univalue& id)
{
    univalue reply = jsonrpcreplyobj(result, error, id);
    return reply.write() + "\n";
}

univalue jsonrpcerror(int code, const string& message)
{
    univalue error(univalue::vobj);
    error.push_back(pair("code", code));
    error.push_back(pair("message", message));
    return error;
}
