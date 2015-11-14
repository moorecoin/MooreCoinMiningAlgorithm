// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "script.h"

#include "tinyformat.h"
#include "utilstrencodings.h"

namespace {
inline std::string valuestring(const std::vector<unsigned char>& vch)
{
    if (vch.size() <= 4)
        return strprintf("%d", cscriptnum(vch, false).getint());
    else
        return hexstr(vch);
}
} // anon namespace

using namespace std;

const char* getopname(opcodetype opcode)
{
    switch (opcode)
    {
    // push value
    case op_0                      : return "0";
    case op_pushdata1              : return "op_pushdata1";
    case op_pushdata2              : return "op_pushdata2";
    case op_pushdata4              : return "op_pushdata4";
    case op_1negate                : return "-1";
    case op_reserved               : return "op_reserved";
    case op_1                      : return "1";
    case op_2                      : return "2";
    case op_3                      : return "3";
    case op_4                      : return "4";
    case op_5                      : return "5";
    case op_6                      : return "6";
    case op_7                      : return "7";
    case op_8                      : return "8";
    case op_9                      : return "9";
    case op_10                     : return "10";
    case op_11                     : return "11";
    case op_12                     : return "12";
    case op_13                     : return "13";
    case op_14                     : return "14";
    case op_15                     : return "15";
    case op_16                     : return "16";

    // control
    case op_nop                    : return "op_nop";
    case op_ver                    : return "op_ver";
    case op_if                     : return "op_if";
    case op_notif                  : return "op_notif";
    case op_verif                  : return "op_verif";
    case op_vernotif               : return "op_vernotif";
    case op_else                   : return "op_else";
    case op_endif                  : return "op_endif";
    case op_verify                 : return "op_verify";
    case op_return                 : return "op_return";

    // stack ops
    case op_toaltstack             : return "op_toaltstack";
    case op_fromaltstack           : return "op_fromaltstack";
    case op_2drop                  : return "op_2drop";
    case op_2dup                   : return "op_2dup";
    case op_3dup                   : return "op_3dup";
    case op_2over                  : return "op_2over";
    case op_2rot                   : return "op_2rot";
    case op_2swap                  : return "op_2swap";
    case op_ifdup                  : return "op_ifdup";
    case op_depth                  : return "op_depth";
    case op_drop                   : return "op_drop";
    case op_dup                    : return "op_dup";
    case op_nip                    : return "op_nip";
    case op_over                   : return "op_over";
    case op_pick                   : return "op_pick";
    case op_roll                   : return "op_roll";
    case op_rot                    : return "op_rot";
    case op_swap                   : return "op_swap";
    case op_tuck                   : return "op_tuck";

    // splice ops
    case op_cat                    : return "op_cat";
    case op_substr                 : return "op_substr";
    case op_left                   : return "op_left";
    case op_right                  : return "op_right";
    case op_size                   : return "op_size";

    // bit logic
    case op_invert                 : return "op_invert";
    case op_and                    : return "op_and";
    case op_or                     : return "op_or";
    case op_xor                    : return "op_xor";
    case op_equal                  : return "op_equal";
    case op_equalverify            : return "op_equalverify";
    case op_reserved1              : return "op_reserved1";
    case op_reserved2              : return "op_reserved2";

    // numeric
    case op_1add                   : return "op_1add";
    case op_1sub                   : return "op_1sub";
    case op_2mul                   : return "op_2mul";
    case op_2div                   : return "op_2div";
    case op_negate                 : return "op_negate";
    case op_abs                    : return "op_abs";
    case op_not                    : return "op_not";
    case op_0notequal              : return "op_0notequal";
    case op_add                    : return "op_add";
    case op_sub                    : return "op_sub";
    case op_mul                    : return "op_mul";
    case op_div                    : return "op_div";
    case op_mod                    : return "op_mod";
    case op_lshift                 : return "op_lshift";
    case op_rshift                 : return "op_rshift";
    case op_booland                : return "op_booland";
    case op_boolor                 : return "op_boolor";
    case op_numequal               : return "op_numequal";
    case op_numequalverify         : return "op_numequalverify";
    case op_numnotequal            : return "op_numnotequal";
    case op_lessthan               : return "op_lessthan";
    case op_greaterthan            : return "op_greaterthan";
    case op_lessthanorequal        : return "op_lessthanorequal";
    case op_greaterthanorequal     : return "op_greaterthanorequal";
    case op_min                    : return "op_min";
    case op_max                    : return "op_max";
    case op_within                 : return "op_within";

    // crypto
    case op_ripemd160              : return "op_ripemd160";
    case op_sha1                   : return "op_sha1";
    case op_sha256                 : return "op_sha256";
    case op_hash160                : return "op_hash160";
    case op_hash256                : return "op_hash256";
    case op_codeseparator          : return "op_codeseparator";
    case op_checksig               : return "op_checksig";
    case op_checksigverify         : return "op_checksigverify";
    case op_checkmultisig          : return "op_checkmultisig";
    case op_checkmultisigverify    : return "op_checkmultisigverify";

    // expanson
    case op_nop1                   : return "op_nop1";
    case op_nop2                   : return "op_nop2";
    case op_nop3                   : return "op_nop3";
    case op_nop4                   : return "op_nop4";
    case op_nop5                   : return "op_nop5";
    case op_nop6                   : return "op_nop6";
    case op_nop7                   : return "op_nop7";
    case op_nop8                   : return "op_nop8";
    case op_nop9                   : return "op_nop9";
    case op_nop10                  : return "op_nop10";

    case op_invalidopcode          : return "op_invalidopcode";

    // note:
    //  the template matching params op_smalldata/etc are defined in opcodetype enum
    //  as kind of implementation hack, they are *not* real opcodes.  if found in real
    //  script, just let the default: case deal with them.

    default:
        return "op_unknown";
    }
}

unsigned int cscript::getsigopcount(bool faccurate) const
{
    unsigned int n = 0;
    const_iterator pc = begin();
    opcodetype lastopcode = op_invalidopcode;
    while (pc < end())
    {
        opcodetype opcode;
        if (!getop(pc, opcode))
            break;
        if (opcode == op_checksig || opcode == op_checksigverify)
            n++;
        else if (opcode == op_checkmultisig || opcode == op_checkmultisigverify)
        {
            if (faccurate && lastopcode >= op_1 && lastopcode <= op_16)
                n += decodeop_n(lastopcode);
            else
                n += 20;
        }
        lastopcode = opcode;
    }
    return n;
}

unsigned int cscript::getsigopcount(const cscript& scriptsig) const
{
    if (!ispaytoscripthash())
        return getsigopcount(true);

    // this is a pay-to-script-hash scriptpubkey;
    // get the last item that the scriptsig
    // pushes onto the stack:
    const_iterator pc = scriptsig.begin();
    vector<unsigned char> data;
    while (pc < scriptsig.end())
    {
        opcodetype opcode;
        if (!scriptsig.getop(pc, opcode, data))
            return 0;
        if (opcode > op_16)
            return 0;
    }

    /// ... and return its opcount:
    cscript subscript(data.begin(), data.end());
    return subscript.getsigopcount(true);
}

bool cscript::ispaytoscripthash() const
{
    // extra-fast test for pay-to-script-hash cscripts:
    return (this->size() == 23 &&
            this->at(0) == op_hash160 &&
            this->at(1) == 0x14 &&
            this->at(22) == op_equal);
}

bool cscript::ispushonly() const
{
    const_iterator pc = begin();
    while (pc < end())
    {
        opcodetype opcode;
        if (!getop(pc, opcode))
            return false;
        // note that ispushonly() *does* consider op_reserved to be a
        // push-type opcode, however execution of op_reserved fails, so
        // it's not relevant to p2sh/bip62 as the scriptsig would fail prior to
        // the p2sh special validation code being executed.
        if (opcode > op_16)
            return false;
    }
    return true;
}

std::string cscript::tostring() const
{
    std::string str;
    opcodetype opcode;
    std::vector<unsigned char> vch;
    const_iterator pc = begin();
    while (pc < end())
    {
        if (!str.empty())
            str += " ";
        if (!getop(pc, opcode, vch))
        {
            str += "[error]";
            return str;
        }
        if (0 <= opcode && opcode <= op_pushdata4)
            str += valuestring(vch);
        else
            str += getopname(opcode);
    }
    return str;
}
