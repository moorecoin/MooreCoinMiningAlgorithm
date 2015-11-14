// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "script/standard.h"

#include "pubkey.h"
#include "script/script.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned nmaxdatacarrierbytes = max_op_return_relay;

cscriptid::cscriptid(const cscript& in) : uint160(hash160(in.begin(), in.end())) {}

const char* gettxnoutputtype(txnouttype t)
{
    switch (t)
    {
    case tx_nonstandard: return "nonstandard";
    case tx_pubkey: return "pubkey";
    case tx_pubkeyhash: return "pubkeyhash";
    case tx_scripthash: return "scripthash";
    case tx_multisig: return "multisig";
    case tx_null_data: return "nulldata";
    }
    return null;
}

/**
 * return public keys or hashes from scriptpubkey, for 'standard' transaction types.
 */
bool solver(const cscript& scriptpubkey, txnouttype& typeret, vector<vector<unsigned char> >& vsolutionsret)
{
    // templates
    static multimap<txnouttype, cscript> mtemplates;
    if (mtemplates.empty())
    {
        // standard tx, sender provides pubkey, receiver adds signature
        mtemplates.insert(make_pair(tx_pubkey, cscript() << op_pubkey << op_checksig));

        // moorecoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mtemplates.insert(make_pair(tx_pubkeyhash, cscript() << op_dup << op_hash160 << op_pubkeyhash << op_equalverify << op_checksig));

        // sender provides n pubkeys, receivers provides m signatures
        mtemplates.insert(make_pair(tx_multisig, cscript() << op_smallinteger << op_pubkeys << op_smallinteger << op_checkmultisig));

        // empty, provably prunable, data-carrying output
        if (getboolarg("-datacarrier", true))
            mtemplates.insert(make_pair(tx_null_data, cscript() << op_return << op_smalldata));
        mtemplates.insert(make_pair(tx_null_data, cscript() << op_return));
    }

    // shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always op_hash160 20 [20 byte hash] op_equal
    if (scriptpubkey.ispaytoscripthash())
    {
        typeret = tx_scripthash;
        vector<unsigned char> hashbytes(scriptpubkey.begin()+2, scriptpubkey.begin()+22);
        vsolutionsret.push_back(hashbytes);
        return true;
    }

    // scan templates
    const cscript& script1 = scriptpubkey;
    boost_foreach(const pairtype(txnouttype, cscript)& tplate, mtemplates)
    {
        const cscript& script2 = tplate.second;
        vsolutionsret.clear();

        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // compare
        cscript::const_iterator pc1 = script1.begin();
        cscript::const_iterator pc2 = script2.begin();
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // found a match
                typeret = tplate.first;
                if (typeret == tx_multisig)
                {
                    // additional checks for tx_multisig:
                    unsigned char m = vsolutionsret.front()[0];
                    unsigned char n = vsolutionsret.back()[0];
                    if (m < 1 || n < 1 || m > n || vsolutionsret.size()-2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.getop(pc1, opcode1, vch1))
                break;
            if (!script2.getop(pc2, opcode2, vch2))
                break;

            // template matching opcodes:
            if (opcode2 == op_pubkeys)
            {
                while (vch1.size() >= 33 && vch1.size() <= 65)
                {
                    vsolutionsret.push_back(vch1);
                    if (!script1.getop(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.getop(pc2, opcode2, vch2))
                    break;
                // normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == op_pubkey)
            {
                if (vch1.size() < 33 || vch1.size() > 65)
                    break;
                vsolutionsret.push_back(vch1);
            }
            else if (opcode2 == op_pubkeyhash)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vsolutionsret.push_back(vch1);
            }
            else if (opcode2 == op_smallinteger)
            {   // single-byte small integer pushed onto vsolutions
                if (opcode1 == op_0 ||
                    (opcode1 >= op_1 && opcode1 <= op_16))
                {
                    char n = (char)cscript::decodeop_n(opcode1);
                    vsolutionsret.push_back(valtype(1, n));
                }
                else
                    break;
            }
            else if (opcode2 == op_smalldata)
            {
                // small pushdata, <= nmaxdatacarrierbytes
                if (vch1.size() > nmaxdatacarrierbytes)
                    break;
            }
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                // others must match exactly
                break;
            }
        }
    }

    vsolutionsret.clear();
    typeret = tx_nonstandard;
    return false;
}

int scriptsigargsexpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vsolutions)
{
    switch (t)
    {
    case tx_nonstandard:
    case tx_null_data:
        return -1;
    case tx_pubkey:
        return 1;
    case tx_pubkeyhash:
        return 2;
    case tx_multisig:
        if (vsolutions.size() < 1 || vsolutions[0].size() < 1)
            return -1;
        return vsolutions[0][0] + 1;
    case tx_scripthash:
        return 1; // doesn't include args needed by the script
    }
    return -1;
}

bool isstandard(const cscript& scriptpubkey, txnouttype& whichtype)
{
    vector<valtype> vsolutions;
    if (!solver(scriptpubkey, whichtype, vsolutions))
        return false;

    if (whichtype == tx_multisig)
    {
        unsigned char m = vsolutions.front()[0];
        unsigned char n = vsolutions.back()[0];
        // support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    }

    return whichtype != tx_nonstandard;
}

bool extractdestination(const cscript& scriptpubkey, ctxdestination& addressret)
{
    vector<valtype> vsolutions;
    txnouttype whichtype;
    if (!solver(scriptpubkey, whichtype, vsolutions))
        return false;

    if (whichtype == tx_pubkey)
    {
        cpubkey pubkey(vsolutions[0]);
        if (!pubkey.isvalid())
            return false;

        addressret = pubkey.getid();
        return true;
    }
    else if (whichtype == tx_pubkeyhash)
    {
        addressret = ckeyid(uint160(vsolutions[0]));
        return true;
    }
    else if (whichtype == tx_scripthash)
    {
        addressret = cscriptid(uint160(vsolutions[0]));
        return true;
    }
    // multisig txns have more than one address...
    return false;
}

bool extractdestinations(const cscript& scriptpubkey, txnouttype& typeret, vector<ctxdestination>& addressret, int& nrequiredret)
{
    addressret.clear();
    typeret = tx_nonstandard;
    vector<valtype> vsolutions;
    if (!solver(scriptpubkey, typeret, vsolutions))
        return false;
    if (typeret == tx_null_data){
        // this is data, not addresses
        return false;
    }

    if (typeret == tx_multisig)
    {
        nrequiredret = vsolutions.front()[0];
        for (unsigned int i = 1; i < vsolutions.size()-1; i++)
        {
            cpubkey pubkey(vsolutions[i]);
            if (!pubkey.isvalid())
                continue;

            ctxdestination address = pubkey.getid();
            addressret.push_back(address);
        }

        if (addressret.empty())
            return false;
    }
    else
    {
        nrequiredret = 1;
        ctxdestination address;
        if (!extractdestination(scriptpubkey, address))
           return false;
        addressret.push_back(address);
    }

    return true;
}

namespace
{
class cscriptvisitor : public boost::static_visitor<bool>
{
private:
    cscript *script;
public:
    cscriptvisitor(cscript *scriptin) { script = scriptin; }

    bool operator()(const cnodestination &dest) const {
        script->clear();
        return false;
    }

    bool operator()(const ckeyid &keyid) const {
        script->clear();
        *script << op_dup << op_hash160 << tobytevector(keyid) << op_equalverify << op_checksig;
        return true;
    }

    bool operator()(const cscriptid &scriptid) const {
        script->clear();
        *script << op_hash160 << tobytevector(scriptid) << op_equal;
        return true;
    }
};
}

cscript getscriptfordestination(const ctxdestination& dest)
{
    cscript script;

    boost::apply_visitor(cscriptvisitor(&script), dest);
    return script;
}

cscript getscriptformultisig(int nrequired, const std::vector<cpubkey>& keys)
{
    cscript script;

    script << cscript::encodeop_n(nrequired);
    boost_foreach(const cpubkey& key, keys)
        script << tobytevector(key);
    script << cscript::encodeop_n(keys.size()) << op_checkmultisig;
    return script;
}
