// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "core_io.h"

#include "base58.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/standard.h"
#include "serialize.h"
#include "streams.h"
#include "univalue/univalue.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"

#include <boost/foreach.hpp>

using namespace std;

string formatscript(const cscript& script)
{
    string ret;
    cscript::const_iterator it = script.begin();
    opcodetype op;
    while (it != script.end()) {
        cscript::const_iterator it2 = it;
        vector<unsigned char> vch;
        if (script.getop2(it, op, &vch)) {
            if (op == op_0) {
                ret += "0 ";
                continue;
            } else if ((op >= op_1 && op <= op_16) || op == op_1negate) {
                ret += strprintf("%i ", op - op_1negate - 1);
                continue;
            } else if (op >= op_nop && op <= op_checkmultisigverify) {
                string str(getopname(op));
                if (str.substr(0, 3) == string("op_")) {
                    ret += str.substr(3, string::npos) + " ";
                    continue;
                }
            }
            if (vch.size() > 0) {
                ret += strprintf("0x%x 0x%x ", hexstr(it2, it - vch.size()), hexstr(it - vch.size(), it));
            } else {
                ret += strprintf("0x%x", hexstr(it2, it));
            }
            continue;
        }
        ret += strprintf("0x%x ", hexstr(it2, script.end()));
        break;
    }
    return ret.substr(0, ret.size() - 1);
}

string encodehextx(const ctransaction& tx)
{
    cdatastream sstx(ser_network, protocol_version);
    sstx << tx;
    return hexstr(sstx.begin(), sstx.end());
}

void scriptpubkeytouniv(const cscript& scriptpubkey,
                        univalue& out, bool fincludehex)
{
    txnouttype type;
    vector<ctxdestination> addresses;
    int nrequired;

    out.pushkv("asm", scriptpubkey.tostring());
    if (fincludehex)
        out.pushkv("hex", hexstr(scriptpubkey.begin(), scriptpubkey.end()));

    if (!extractdestinations(scriptpubkey, type, addresses, nrequired)) {
        out.pushkv("type", gettxnoutputtype(type));
        return;
    }

    out.pushkv("reqsigs", nrequired);
    out.pushkv("type", gettxnoutputtype(type));

    univalue a(univalue::varr);
    boost_foreach(const ctxdestination& addr, addresses)
        a.push_back(cmoorecoinaddress(addr).tostring());
    out.pushkv("addresses", a);
}

void txtouniv(const ctransaction& tx, const uint256& hashblock, univalue& entry)
{
    entry.pushkv("txid", tx.gethash().gethex());
    entry.pushkv("version", tx.nversion);
    entry.pushkv("locktime", (int64_t)tx.nlocktime);

    univalue vin(univalue::varr);
    boost_foreach(const ctxin& txin, tx.vin) {
        univalue in(univalue::vobj);
        if (tx.iscoinbase())
            in.pushkv("coinbase", hexstr(txin.scriptsig.begin(), txin.scriptsig.end()));
        else {
            in.pushkv("txid", txin.prevout.hash.gethex());
            in.pushkv("vout", (int64_t)txin.prevout.n);
            univalue o(univalue::vobj);
            o.pushkv("asm", txin.scriptsig.tostring());
            o.pushkv("hex", hexstr(txin.scriptsig.begin(), txin.scriptsig.end()));
            in.pushkv("scriptsig", o);
        }
        in.pushkv("sequence", (int64_t)txin.nsequence);
        vin.push_back(in);
    }
    entry.pushkv("vin", vin);

    univalue vout(univalue::varr);
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const ctxout& txout = tx.vout[i];

        univalue out(univalue::vobj);

        univalue outvalue(univalue::vnum, formatmoney(txout.nvalue));
        out.pushkv("value", outvalue);
        out.pushkv("n", (int64_t)i);

        univalue o(univalue::vobj);
        scriptpubkeytouniv(txout.scriptpubkey, o, true);
        out.pushkv("scriptpubkey", o);
        vout.push_back(out);
    }
    entry.pushkv("vout", vout);

    if (!hashblock.isnull())
        entry.pushkv("blockhash", hashblock.gethex());

    entry.pushkv("hex", encodehextx(tx)); // the hex-encoded transaction. used the name "hex" to be consistent with the verbose output of "getrawtransaction".
}
