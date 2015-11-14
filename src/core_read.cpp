// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "core_io.h"

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "serialize.h"
#include "streams.h"
#include "univalue/univalue.h"
#include "util.h"
#include "utilstrencodings.h"
#include "version.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign/list_of.hpp>

using namespace std;

cscript parsescript(const std::string& s)
{
    cscript result;

    static map<string, opcodetype> mapopnames;

    if (mapopnames.empty())
    {
        for (int op = 0; op <= op_nop10; op++)
        {
            // allow op_reserved to get into mapopnames
            if (op < op_nop && op != op_reserved)
                continue;

            const char* name = getopname((opcodetype)op);
            if (strcmp(name, "op_unknown") == 0)
                continue;
            string strname(name);
            mapopnames[strname] = (opcodetype)op;
            // convenience: op_add and just add are both recognized:
            boost::algorithm::replace_first(strname, "op_", "");
            mapopnames[strname] = (opcodetype)op;
        }
    }

    vector<string> words;
    boost::algorithm::split(words, s, boost::algorithm::is_any_of(" \t\n"), boost::algorithm::token_compress_on);

    for (std::vector<std::string>::const_iterator w = words.begin(); w != words.end(); ++w)
    {
        if (w->empty())
        {
            // empty string, ignore. (boost::split given '' will return one word)
        }
        else if (all(*w, boost::algorithm::is_digit()) ||
            (boost::algorithm::starts_with(*w, "-") && all(string(w->begin()+1, w->end()), boost::algorithm::is_digit())))
        {
            // number
            int64_t n = atoi64(*w);
            result << n;
        }
        else if (boost::algorithm::starts_with(*w, "0x") && (w->begin()+2 != w->end()) && ishex(string(w->begin()+2, w->end())))
        {
            // raw hex data, inserted not pushed onto stack:
            std::vector<unsigned char> raw = parsehex(string(w->begin()+2, w->end()));
            result.insert(result.end(), raw.begin(), raw.end());
        }
        else if (w->size() >= 2 && boost::algorithm::starts_with(*w, "'") && boost::algorithm::ends_with(*w, "'"))
        {
            // single-quoted string, pushed as data. note: this is poor-man's
            // parsing, spaces/tabs/newlines in single-quoted strings won't work.
            std::vector<unsigned char> value(w->begin()+1, w->end()-1);
            result << value;
        }
        else if (mapopnames.count(*w))
        {
            // opcode, e.g. op_add or add:
            result << mapopnames[*w];
        }
        else
        {
            throw runtime_error("script parse error");
        }
    }

    return result;
}

bool decodehextx(ctransaction& tx, const std::string& strhextx)
{
    if (!ishex(strhextx))
        return false;

    vector<unsigned char> txdata(parsehex(strhextx));
    cdatastream ssdata(txdata, ser_network, protocol_version);
    try {
        ssdata >> tx;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}

bool decodehexblk(cblock& block, const std::string& strhexblk)
{
    if (!ishex(strhexblk))
        return false;

    std::vector<unsigned char> blockdata(parsehex(strhexblk));
    cdatastream ssblock(blockdata, ser_network, protocol_version);
    try {
        ssblock >> block;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}

uint256 parsehashuv(const univalue& v, const string& strname)
{
    string strhex;
    if (v.isstr())
        strhex = v.getvalstr();
    return parsehashstr(strhex, strname);  // note: parsehashstr("") throws a runtime_error
}

uint256 parsehashstr(const std::string& strhex, const std::string& strname)
{
    if (!ishex(strhex)) // note: ishex("") is false
        throw runtime_error(strname+" must be hexadecimal string (not '"+strhex+"')");

    uint256 result;
    result.sethex(strhex);
    return result;
}

vector<unsigned char> parsehexuv(const univalue& v, const string& strname)
{
    string strhex;
    if (v.isstr())
        strhex = v.getvalstr();
    if (!ishex(strhex))
        throw runtime_error(strname+" must be hexadecimal string (not '"+strhex+"')");
    return parsehex(strhex);
}
