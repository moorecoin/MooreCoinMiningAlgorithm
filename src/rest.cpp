// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "main.h"
#include "rpcserver.h"
#include "streams.h"
#include "sync.h"
#include "txmempool.h"
#include "utilstrencodings.h"
#include "version.h"

#include <boost/algorithm/string.hpp>
#include <boost/dynamic_bitset.hpp>

#include "univalue/univalue.h"

using namespace std;

static const int max_getutxos_outpoints = 15; //allow a max of 15 outpoints to be queried at once

enum retformat {
    rf_undef,
    rf_binary,
    rf_hex,
    rf_json,
};

static const struct {
    enum retformat rf;
    const char* name;
} rf_names[] = {
      {rf_undef, ""},
      {rf_binary, "bin"},
      {rf_hex, "hex"},
      {rf_json, "json"},
};

struct ccoin {
    uint32_t ntxver; // don't call this nversion, that name has a special meaning inside implement_serialize
    uint32_t nheight;
    ctxout out;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion)
    {
        readwrite(ntxver);
        readwrite(nheight);
        readwrite(out);
    }
};

class resterr
{
public:
    enum httpstatuscode status;
    string message;
};

extern void txtojson(const ctransaction& tx, const uint256 hashblock, univalue& entry);
extern univalue blocktojson(const cblock& block, const cblockindex* blockindex, bool txdetails = false);
extern void scriptpubkeytojson(const cscript& scriptpubkey, univalue& out, bool fincludehex);

static resterr resterr(enum httpstatuscode status, string message)
{
    resterr re;
    re.status = status;
    re.message = message;
    return re;
}

static enum retformat parsedataformat(vector<string>& params, const string strreq)
{
    boost::split(params, strreq, boost::is_any_of("."));
    if (params.size() > 1) {
        for (unsigned int i = 0; i < arraylen(rf_names); i++)
            if (params[1] == rf_names[i].name)
                return rf_names[i].rf;
    }

    return rf_names[0].rf;
}

static string availabledataformatsstring()
{
    string formats = "";
    for (unsigned int i = 0; i < arraylen(rf_names); i++)
        if (strlen(rf_names[i].name) > 0) {
            formats.append(".");
            formats.append(rf_names[i].name);
            formats.append(", ");
        }

    if (formats.length() > 0)
        return formats.substr(0, formats.length() - 2);

    return formats;
}

static bool parsehashstr(const string& strreq, uint256& v)
{
    if (!ishex(strreq) || (strreq.size() != 64))
        return false;

    v.sethex(strreq);
    return true;
}

static bool rest_headers(acceptedconnection* conn,
                         const std::string& struripart,
                         const std::string& strrequest,
                         const std::map<std::string, std::string>& mapheaders,
                         bool frun)
{
    vector<string> params;
    const retformat rf = parsedataformat(params, struripart);
    vector<string> path;
    boost::split(path, params[0], boost::is_any_of("/"));

    if (path.size() != 2)
        throw resterr(http_bad_request, "no header count specified. use /rest/headers/<count>/<hash>.<ext>.");

    long count = strtol(path[0].c_str(), null, 10);
    if (count < 1 || count > 2000)
        throw resterr(http_bad_request, "header count out of range: " + path[0]);

    string hashstr = path[1];
    uint256 hash;
    if (!parsehashstr(hashstr, hash))
        throw resterr(http_bad_request, "invalid hash: " + hashstr);

    std::vector<cblockheader> headers;
    headers.reserve(count);
    {
        lock(cs_main);
        blockmap::const_iterator it = mapblockindex.find(hash);
        const cblockindex *pindex = (it != mapblockindex.end()) ? it->second : null;
        while (pindex != null && chainactive.contains(pindex)) {
            headers.push_back(pindex->getblockheader());
            if (headers.size() == (unsigned long)count)
                break;
            pindex = chainactive.next(pindex);
        }
    }

    cdatastream ssheader(ser_network, protocol_version);
    boost_foreach(const cblockheader &header, headers) {
        ssheader << header;
    }

    switch (rf) {
    case rf_binary: {
        string binaryheader = ssheader.str();
        conn->stream() << httpreplyheader(http_ok, frun, binaryheader.size(), "application/octet-stream") << binaryheader << std::flush;
        return true;
    }

    case rf_hex: {
        string strhex = hexstr(ssheader.begin(), ssheader.end()) + "\n";
        conn->stream() << httpreply(http_ok, strhex, frun, false, "text/plain") << std::flush;
        return true;
    }

    default: {
        throw resterr(http_not_found, "output format not found (available: .bin, .hex)");
    }
    }

    // not reached
    return true; // continue to process further http reqs on this cxn
}

static bool rest_block(acceptedconnection* conn,
                       const std::string& struripart,
                       const std::string& strrequest,
                       const std::map<std::string, std::string>& mapheaders,
                       bool frun,
                       bool showtxdetails)
{
    vector<string> params;
    const retformat rf = parsedataformat(params, struripart);

    string hashstr = params[0];
    uint256 hash;
    if (!parsehashstr(hashstr, hash))
        throw resterr(http_bad_request, "invalid hash: " + hashstr);

    cblock block;
    cblockindex* pblockindex = null;
    {
        lock(cs_main);
        if (mapblockindex.count(hash) == 0)
            throw resterr(http_not_found, hashstr + " not found");

        pblockindex = mapblockindex[hash];
        if (fhavepruned && !(pblockindex->nstatus & block_have_data) && pblockindex->ntx > 0)
            throw resterr(http_not_found, hashstr + " not available (pruned data)");

        if (!readblockfromdisk(block, pblockindex))
            throw resterr(http_not_found, hashstr + " not found");
    }

    cdatastream ssblock(ser_network, protocol_version);
    ssblock << block;

    switch (rf) {
    case rf_binary: {
        string binaryblock = ssblock.str();
        conn->stream() << httpreplyheader(http_ok, frun, binaryblock.size(), "application/octet-stream") << binaryblock << std::flush;
        return true;
    }

    case rf_hex: {
        string strhex = hexstr(ssblock.begin(), ssblock.end()) + "\n";
        conn->stream() << httpreply(http_ok, strhex, frun, false, "text/plain") << std::flush;
        return true;
    }

    case rf_json: {
        univalue objblock = blocktojson(block, pblockindex, showtxdetails);
        string strjson = objblock.write() + "\n";
        conn->stream() << httpreply(http_ok, strjson, frun) << std::flush;
        return true;
    }

    default: {
        throw resterr(http_not_found, "output format not found (available: " + availabledataformatsstring() + ")");
    }
    }

    // not reached
    return true; // continue to process further http reqs on this cxn
}

static bool rest_block_extended(acceptedconnection* conn,
                       const std::string& struripart,
                       const std::string& strrequest,
                       const std::map<std::string, std::string>& mapheaders,
                       bool frun)
{
    return rest_block(conn, struripart, strrequest, mapheaders, frun, true);
}

static bool rest_block_notxdetails(acceptedconnection* conn,
                       const std::string& struripart,
                       const std::string& strrequest,
                       const std::map<std::string, std::string>& mapheaders,
                       bool frun)
{
    return rest_block(conn, struripart, strrequest, mapheaders, frun, false);
}

static bool rest_chaininfo(acceptedconnection* conn,
                           const std::string& struripart,
                           const std::string& strrequest,
                           const std::map<std::string, std::string>& mapheaders,
                           bool frun)
{
    vector<string> params;
    const retformat rf = parsedataformat(params, struripart);

    switch (rf) {
    case rf_json: {
        univalue rpcparams(univalue::varr);
        univalue chaininfoobject = getblockchaininfo(rpcparams, false);
        string strjson = chaininfoobject.write() + "\n";
        conn->stream() << httpreply(http_ok, strjson, frun) << std::flush;
        return true;
    }
    default: {
        throw resterr(http_not_found, "output format not found (available: json)");
    }
    }

    // not reached
    return true; // continue to process further http reqs on this cxn
}

static bool rest_tx(acceptedconnection* conn,
                    const std::string& struripart,
                    const std::string& strrequest,
                    const std::map<std::string, std::string>& mapheaders,
                    bool frun)
{
    vector<string> params;
    const retformat rf = parsedataformat(params, struripart);

    string hashstr = params[0];
    uint256 hash;
    if (!parsehashstr(hashstr, hash))
        throw resterr(http_bad_request, "invalid hash: " + hashstr);

    ctransaction tx;
    uint256 hashblock = uint256();
    if (!gettransaction(hash, tx, hashblock, true))
        throw resterr(http_not_found, hashstr + " not found");

    cdatastream sstx(ser_network, protocol_version);
    sstx << tx;

    switch (rf) {
    case rf_binary: {
        string binarytx = sstx.str();
        conn->stream() << httpreplyheader(http_ok, frun, binarytx.size(), "application/octet-stream") << binarytx << std::flush;
        return true;
    }

    case rf_hex: {
        string strhex = hexstr(sstx.begin(), sstx.end()) + "\n";
        conn->stream() << httpreply(http_ok, strhex, frun, false, "text/plain") << std::flush;
        return true;
    }

    case rf_json: {
        univalue objtx(univalue::vobj);
        txtojson(tx, hashblock, objtx);
        string strjson = objtx.write() + "\n";
        conn->stream() << httpreply(http_ok, strjson, frun) << std::flush;
        return true;
    }

    default: {
        throw resterr(http_not_found, "output format not found (available: " + availabledataformatsstring() + ")");
    }
    }

    // not reached
    return true; // continue to process further http reqs on this cxn
}

static bool rest_getutxos(acceptedconnection* conn,
                          const std::string& struripart,
                          const std::string& strrequest,
                          const std::map<std::string, std::string>& mapheaders,
                          bool frun)
{
    vector<string> params;
    enum retformat rf = parsedataformat(params, struripart);

    vector<string> uriparts;
    if (params.size() > 0 && params[0].length() > 1)
    {
        std::string struriparams = params[0].substr(1);
        boost::split(uriparts, struriparams, boost::is_any_of("/"));
    }

    // throw exception in case of a empty request
    if (strrequest.length() == 0 && uriparts.size() == 0)
        throw resterr(http_internal_server_error, "error: empty request");

    bool finputparsed = false;
    bool fcheckmempool = false;
    vector<coutpoint> voutpoints;

    // parse/deserialize input
    // input-format = output-format, rest/getutxos/bin requires binary input, gives binary output, ...

    if (uriparts.size() > 0)
    {

        //inputs is sent over uri scheme (/rest/getutxos/checkmempool/txid1-n/txid2-n/...)
        if (uriparts.size() > 0 && uriparts[0] == "checkmempool")
            fcheckmempool = true;

        for (size_t i = (fcheckmempool) ? 1 : 0; i < uriparts.size(); i++)
        {
            uint256 txid;
            int32_t noutput;
            std::string strtxid = uriparts[i].substr(0, uriparts[i].find("-"));
            std::string stroutput = uriparts[i].substr(uriparts[i].find("-")+1);

            if (!parseint32(stroutput, &noutput) || !ishex(strtxid))
                throw resterr(http_internal_server_error, "parse error");

            txid.sethex(strtxid);
            voutpoints.push_back(coutpoint(txid, (uint32_t)noutput));
        }

        if (voutpoints.size() > 0)
            finputparsed = true;
        else
            throw resterr(http_internal_server_error, "error: empty request");
    }

    string strrequestmutable = strrequest; //convert const string to string for allowing hex to bin converting

    switch (rf) {
    case rf_hex: {
        // convert hex to bin, continue then with bin part
        std::vector<unsigned char> strrequestv = parsehex(strrequest);
        strrequestmutable.assign(strrequestv.begin(), strrequestv.end());
    }

    case rf_binary: {
        try {
            //deserialize only if user sent a request
            if (strrequestmutable.size() > 0)
            {
                if (finputparsed) //don't allow sending input over uri and http raw data
                    throw resterr(http_internal_server_error, "combination of uri scheme inputs and raw post data is not allowed");

                cdatastream oss(ser_network, protocol_version);
                oss << strrequestmutable;
                oss >> fcheckmempool;
                oss >> voutpoints;
            }
        } catch (const std::ios_base::failure& e) {
            // abort in case of unreadable binary data
            throw resterr(http_internal_server_error, "parse error");
        }
        break;
    }

    case rf_json: {
        if (!finputparsed)
            throw resterr(http_internal_server_error, "error: empty request");
        break;
    }
    default: {
        throw resterr(http_not_found, "output format not found (available: " + availabledataformatsstring() + ")");
    }
    }

    // limit max outpoints
    if (voutpoints.size() > max_getutxos_outpoints)
        throw resterr(http_internal_server_error, strprintf("error: max outpoints exceeded (max: %d, tried: %d)", max_getutxos_outpoints, voutpoints.size()));

    // check spentness and form a bitmap (as well as a json capable human-readble string representation)
    vector<unsigned char> bitmap;
    vector<ccoin> outs;
    std::string bitmapstringrepresentation;
    boost::dynamic_bitset<unsigned char> hits(voutpoints.size());
    {
        lock2(cs_main, mempool.cs);

        ccoinsview viewdummy;
        ccoinsviewcache view(&viewdummy);

        ccoinsviewcache& viewchain = *pcoinstip;
        ccoinsviewmempool viewmempool(&viewchain, mempool);

        if (fcheckmempool)
            view.setbackend(viewmempool); // switch cache backend to db+mempool in case user likes to query mempool

        for (size_t i = 0; i < voutpoints.size(); i++) {
            ccoins coins;
            uint256 hash = voutpoints[i].hash;
            if (view.getcoins(hash, coins)) {
                mempool.prunespent(hash, coins);
                if (coins.isavailable(voutpoints[i].n)) {
                    hits[i] = true;
                    // safe to index into vout here because isavailable checked if it's off the end of the array, or if
                    // n is valid but points to an already spent output (isnull).
                    ccoin coin;
                    coin.ntxver = coins.nversion;
                    coin.nheight = coins.nheight;
                    coin.out = coins.vout.at(voutpoints[i].n);
                    assert(!coin.out.isnull());
                    outs.push_back(coin);
                }
            }

            bitmapstringrepresentation.append(hits[i] ? "1" : "0"); // form a binary string representation (human-readable for json output)
        }
    }
    boost::to_block_range(hits, std::back_inserter(bitmap));

    switch (rf) {
    case rf_binary: {
        // serialize data
        // use exact same output as mentioned in bip64
        cdatastream ssgetutxoresponse(ser_network, protocol_version);
        ssgetutxoresponse << chainactive.height() << chainactive.tip()->getblockhash() << bitmap << outs;
        string ssgetutxoresponsestring = ssgetutxoresponse.str();

        conn->stream() << httpreplyheader(http_ok, frun, ssgetutxoresponsestring.size(), "application/octet-stream") << ssgetutxoresponsestring << std::flush;
        return true;
    }

    case rf_hex: {
        cdatastream ssgetutxoresponse(ser_network, protocol_version);
        ssgetutxoresponse << chainactive.height() << chainactive.tip()->getblockhash() << bitmap << outs;
        string strhex = hexstr(ssgetutxoresponse.begin(), ssgetutxoresponse.end()) + "\n";

        conn->stream() << httpreply(http_ok, strhex, frun, false, "text/plain") << std::flush;
        return true;
    }

    case rf_json: {
        univalue objgetutxoresponse(univalue::vobj);

        // pack in some essentials
        // use more or less the same output as mentioned in bip64
        objgetutxoresponse.push_back(pair("chainheight", chainactive.height()));
        objgetutxoresponse.push_back(pair("chaintiphash", chainactive.tip()->getblockhash().gethex()));
        objgetutxoresponse.push_back(pair("bitmap", bitmapstringrepresentation));

        univalue utxos(univalue::varr);
        boost_foreach (const ccoin& coin, outs) {
            univalue utxo(univalue::vobj);
            utxo.push_back(pair("txvers", (int32_t)coin.ntxver));
            utxo.push_back(pair("height", (int32_t)coin.nheight));
            utxo.push_back(pair("value", valuefromamount(coin.out.nvalue)));

            // include the script in a json output
            univalue o(univalue::vobj);
            scriptpubkeytojson(coin.out.scriptpubkey, o, true);
            utxo.push_back(pair("scriptpubkey", o));
            utxos.push_back(utxo);
        }
        objgetutxoresponse.push_back(pair("utxos", utxos));

        // return json string
        string strjson = objgetutxoresponse.write() + "\n";
        conn->stream() << httpreply(http_ok, strjson, frun) << std::flush;
        return true;
    }
    default: {
        throw resterr(http_not_found, "output format not found (available: " + availabledataformatsstring() + ")");
    }
    }

    // not reached
    return true; // continue to process further http reqs on this cxn
}

static const struct {
    const char* prefix;
    bool (*handler)(acceptedconnection* conn,
                    const std::string& struripart,
                    const std::string& strrequest,
                    const std::map<std::string, std::string>& mapheaders,
                    bool frun);
} uri_prefixes[] = {
      {"/rest/tx/", rest_tx},
      {"/rest/block/notxdetails/", rest_block_notxdetails},
      {"/rest/block/", rest_block_extended},
      {"/rest/chaininfo", rest_chaininfo},
      {"/rest/headers/", rest_headers},
      {"/rest/getutxos", rest_getutxos},
};

bool httpreq_rest(acceptedconnection* conn,
                  const std::string& struri,
                  const string& strrequest,
                  const std::map<std::string, std::string>& mapheaders,
                  bool frun)
{
    try {
        std::string statusmessage;
        if (rpcisinwarmup(&statusmessage))
            throw resterr(http_service_unavailable, "service temporarily unavailable: " + statusmessage);

        for (unsigned int i = 0; i < arraylen(uri_prefixes); i++) {
            unsigned int plen = strlen(uri_prefixes[i].prefix);
            if (struri.substr(0, plen) == uri_prefixes[i].prefix) {
                string struripart = struri.substr(plen);
                return uri_prefixes[i].handler(conn, struripart, strrequest, mapheaders, frun);
            }
        }
    } catch (const resterr& re) {
        conn->stream() << httpreply(re.status, re.message + "\r\n", false, false, "text/plain") << std::flush;
        return false;
    }

    conn->stream() << httperror(http_not_found, false) << std::flush;
    return false;
}
