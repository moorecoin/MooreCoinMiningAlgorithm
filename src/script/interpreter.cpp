// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "interpreter.h"

#include "primitives/transaction.h"
#include "crypto/ripemd160.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"
#include "eccryptoverify.h"
#include "pubkey.h"
#include "script/script.h"
#include "uint256.h"

using namespace std;

typedef vector<unsigned char> valtype;

namespace {

inline bool set_success(scripterror* ret)
{
    if (ret)
        *ret = script_err_ok;
    return true;
}

inline bool set_error(scripterror* ret, const scripterror serror)
{
    if (ret)
        *ret = serror;
    return false;
}

} // anon namespace

bool casttobool(const valtype& vch)
{
    for (unsigned int i = 0; i < vch.size(); i++)
    {
        if (vch[i] != 0)
        {
            // can be negative zero
            if (i == vch.size()-1 && vch[i] == 0x80)
                return false;
            return true;
        }
    }
    return false;
}

/**
 * script is a stack machine (like forth) that evaluates a predicate
 * returning a bool indicating valid or not.  there are no loops.
 */
#define stacktop(i)  (stack.at(stack.size()+(i)))
#define altstacktop(i)  (altstack.at(altstack.size()+(i)))
static inline void popstack(vector<valtype>& stack)
{
    if (stack.empty())
        throw runtime_error("popstack(): stack empty");
    stack.pop_back();
}

bool static iscompressedoruncompressedpubkey(const valtype &vchpubkey) {
    if (vchpubkey.size() < 33) {
        //  non-canonical public key: too short
        return false;
    }
    if (vchpubkey[0] == 0x04) {
        if (vchpubkey.size() != 65) {
            //  non-canonical public key: invalid length for uncompressed key
            return false;
        }
    } else if (vchpubkey[0] == 0x02 || vchpubkey[0] == 0x03) {
        if (vchpubkey.size() != 33) {
            //  non-canonical public key: invalid length for compressed key
            return false;
        }
    } else {
          //  non-canonical public key: neither compressed nor uncompressed
          return false;
    }
    return true;
}

/**
 * a canonical signature exists of: <30> <total len> <02> <len r> <r> <02> <len s> <s> <hashtype>
 * where r and s are not negative (their first byte has its highest bit not set), and not
 * excessively padded (do not start with a 0 byte, unless an otherwise negative number follows,
 * in which case a single 0 byte is necessary and even required).
 * 
 * see https://moorecointalk.org/index.php?topic=8392.msg127623#msg127623
 *
 * this function is consensus-critical since bip66.
 */
bool static isvalidsignatureencoding(const std::vector<unsigned char> &sig) {
    // format: 0x30 [total-length] 0x02 [r-length] [r] 0x02 [s-length] [s] [sighash]
    // * total-length: 1-byte length descriptor of everything that follows,
    //   excluding the sighash byte.
    // * r-length: 1-byte length descriptor of the r value that follows.
    // * r: arbitrary-length big-endian encoded r value. it must use the shortest
    //   possible encoding for a positive integers (which means no null bytes at
    //   the start, except a single one when the next byte has its highest bit set).
    // * s-length: 1-byte length descriptor of the s value that follows.
    // * s: arbitrary-length big-endian encoded s value. the same rules apply.
    // * sighash: 1-byte value indicating what data is hashed (not part of the der
    //   signature)

    // minimum and maximum size constraints.
    if (sig.size() < 9) return false;
    if (sig.size() > 73) return false;

    // a signature is of type 0x30 (compound).
    if (sig[0] != 0x30) return false;

    // make sure the length covers the entire signature.
    if (sig[1] != sig.size() - 3) return false;

    // extract the length of the r element.
    unsigned int lenr = sig[3];

    // make sure the length of the s element is still inside the signature.
    if (5 + lenr >= sig.size()) return false;

    // extract the length of the s element.
    unsigned int lens = sig[5 + lenr];

    // verify that the length of the signature matches the sum of the length
    // of the elements.
    if ((size_t)(lenr + lens + 7) != sig.size()) return false;
 
    // check whether the r element is an integer.
    if (sig[2] != 0x02) return false;

    // zero-length integers are not allowed for r.
    if (lenr == 0) return false;

    // negative numbers are not allowed for r.
    if (sig[4] & 0x80) return false;

    // null bytes at the start of r are not allowed, unless r would
    // otherwise be interpreted as a negative number.
    if (lenr > 1 && (sig[4] == 0x00) && !(sig[5] & 0x80)) return false;

    // check whether the s element is an integer.
    if (sig[lenr + 4] != 0x02) return false;

    // zero-length integers are not allowed for s.
    if (lens == 0) return false;

    // negative numbers are not allowed for s.
    if (sig[lenr + 6] & 0x80) return false;

    // null bytes at the start of s are not allowed, unless s would otherwise be
    // interpreted as a negative number.
    if (lens > 1 && (sig[lenr + 6] == 0x00) && !(sig[lenr + 7] & 0x80)) return false;

    return true;
}

bool static islowdersignature(const valtype &vchsig, scripterror* serror) {
    if (!isvalidsignatureencoding(vchsig)) {
        return set_error(serror, script_err_sig_der);
    }
    unsigned int nlenr = vchsig[3];
    unsigned int nlens = vchsig[5+nlenr];
    const unsigned char *s = &vchsig[6+nlenr];
    // if the s value is above the order of the curve divided by two, its
    // complement modulo the order could have been used instead, which is
    // one byte shorter when encoded correctly.
    if (!eccrypto::checksignatureelement(s, nlens, true))
        return set_error(serror, script_err_sig_high_s);

    return true;
}

bool static isdefinedhashtypesignature(const valtype &vchsig) {
    if (vchsig.size() == 0) {
        return false;
    }
    unsigned char nhashtype = vchsig[vchsig.size() - 1] & (~(sighash_anyonecanpay));
    if (nhashtype < sighash_all || nhashtype > sighash_single)
        return false;

    return true;
}

bool static checksignatureencoding(const valtype &vchsig, unsigned int flags, scripterror* serror) {
    // empty signature. not strictly der encoded, but allowed to provide a
    // compact way to provide an invalid signature for use with check(multi)sig
    if (vchsig.size() == 0) {
        return true;
    }
    if ((flags & (script_verify_dersig | script_verify_low_s | script_verify_strictenc)) != 0 && !isvalidsignatureencoding(vchsig)) {
        return set_error(serror, script_err_sig_der);
    } else if ((flags & script_verify_low_s) != 0 && !islowdersignature(vchsig, serror)) {
        // serror is set
        return false;
    } else if ((flags & script_verify_strictenc) != 0 && !isdefinedhashtypesignature(vchsig)) {
        return set_error(serror, script_err_sig_hashtype);
    }
    return true;
}

bool static checkpubkeyencoding(const valtype &vchsig, unsigned int flags, scripterror* serror) {
    if ((flags & script_verify_strictenc) != 0 && !iscompressedoruncompressedpubkey(vchsig)) {
        return set_error(serror, script_err_pubkeytype);
    }
    return true;
}

bool static checkminimalpush(const valtype& data, opcodetype opcode) {
    if (data.size() == 0) {
        // could have used op_0.
        return opcode == op_0;
    } else if (data.size() == 1 && data[0] >= 1 && data[0] <= 16) {
        // could have used op_1 .. op_16.
        return opcode == op_1 + (data[0] - 1);
    } else if (data.size() == 1 && data[0] == 0x81) {
        // could have used op_1negate.
        return opcode == op_1negate;
    } else if (data.size() <= 75) {
        // could have used a direct push (opcode indicating number of bytes pushed + those bytes).
        return opcode == data.size();
    } else if (data.size() <= 255) {
        // could have used op_pushdata.
        return opcode == op_pushdata1;
    } else if (data.size() <= 65535) {
        // could have used op_pushdata2.
        return opcode == op_pushdata2;
    }
    return true;
}

bool evalscript(vector<vector<unsigned char> >& stack, const cscript& script, unsigned int flags, const basesignaturechecker& checker, scripterror* serror)
{
    static const cscriptnum bnzero(0);
    static const cscriptnum bnone(1);
    static const cscriptnum bnfalse(0);
    static const cscriptnum bntrue(1);
    static const valtype vchfalse(0);
    static const valtype vchzero(0);
    static const valtype vchtrue(1, 1);

    cscript::const_iterator pc = script.begin();
    cscript::const_iterator pend = script.end();
    cscript::const_iterator pbegincodehash = script.begin();
    opcodetype opcode;
    valtype vchpushvalue;
    vector<bool> vfexec;
    vector<valtype> altstack;
    set_error(serror, script_err_unknown_error);
    if (script.size() > 10000)
        return set_error(serror, script_err_script_size);
    int nopcount = 0;
    bool frequireminimal = (flags & script_verify_minimaldata) != 0;

    try
    {
        while (pc < pend)
        {
            bool fexec = !count(vfexec.begin(), vfexec.end(), false);

            //
            // read instruction
            //
            if (!script.getop(pc, opcode, vchpushvalue))
                return set_error(serror, script_err_bad_opcode);
            if (vchpushvalue.size() > max_script_element_size)
                return set_error(serror, script_err_push_size);

            // note how op_reserved does not count towards the opcode limit.
            if (opcode > op_16 && ++nopcount > 201)
                return set_error(serror, script_err_op_count);

            if (opcode == op_cat ||
                opcode == op_substr ||
                opcode == op_left ||
                opcode == op_right ||
                opcode == op_invert ||
                opcode == op_and ||
                opcode == op_or ||
                opcode == op_xor ||
                opcode == op_2mul ||
                opcode == op_2div ||
                opcode == op_mul ||
                opcode == op_div ||
                opcode == op_mod ||
                opcode == op_lshift ||
                opcode == op_rshift)
                return set_error(serror, script_err_disabled_opcode); // disabled opcodes.

            if (fexec && 0 <= opcode && opcode <= op_pushdata4) {
                if (frequireminimal && !checkminimalpush(vchpushvalue, opcode)) {
                    return set_error(serror, script_err_minimaldata);
                }
                stack.push_back(vchpushvalue);
            } else if (fexec || (op_if <= opcode && opcode <= op_endif))
            switch (opcode)
            {
                //
                // push value
                //
                case op_1negate:
                case op_1:
                case op_2:
                case op_3:
                case op_4:
                case op_5:
                case op_6:
                case op_7:
                case op_8:
                case op_9:
                case op_10:
                case op_11:
                case op_12:
                case op_13:
                case op_14:
                case op_15:
                case op_16:
                {
                    // ( -- value)
                    cscriptnum bn((int)opcode - (int)(op_1 - 1));
                    stack.push_back(bn.getvch());
                    // the result of these opcodes should always be the minimal way to push the data
                    // they push, so no need for a checkminimalpush here.
                }
                break;


                //
                // control
                //
                case op_nop:
                break;

                case op_nop1: case op_nop2: case op_nop3: case op_nop4: case op_nop5:
                case op_nop6: case op_nop7: case op_nop8: case op_nop9: case op_nop10:
                {
                    if (flags & script_verify_discourage_upgradable_nops)
                        return set_error(serror, script_err_discourage_upgradable_nops);
                }
                break;

                case op_if:
                case op_notif:
                {
                    // <expression> if [statements] [else [statements]] endif
                    bool fvalue = false;
                    if (fexec)
                    {
                        if (stack.size() < 1)
                            return set_error(serror, script_err_unbalanced_conditional);
                        valtype& vch = stacktop(-1);
                        fvalue = casttobool(vch);
                        if (opcode == op_notif)
                            fvalue = !fvalue;
                        popstack(stack);
                    }
                    vfexec.push_back(fvalue);
                }
                break;

                case op_else:
                {
                    if (vfexec.empty())
                        return set_error(serror, script_err_unbalanced_conditional);
                    vfexec.back() = !vfexec.back();
                }
                break;

                case op_endif:
                {
                    if (vfexec.empty())
                        return set_error(serror, script_err_unbalanced_conditional);
                    vfexec.pop_back();
                }
                break;

                case op_verify:
                {
                    // (true -- ) or
                    // (false -- false) and return
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    bool fvalue = casttobool(stacktop(-1));
                    if (fvalue)
                        popstack(stack);
                    else
                        return set_error(serror, script_err_verify);
                }
                break;

                case op_return:
                {
                    return set_error(serror, script_err_op_return);
                }
                break;


                //
                // stack ops
                //
                case op_toaltstack:
                {
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    altstack.push_back(stacktop(-1));
                    popstack(stack);
                }
                break;

                case op_fromaltstack:
                {
                    if (altstack.size() < 1)
                        return set_error(serror, script_err_invalid_altstack_operation);
                    stack.push_back(altstacktop(-1));
                    popstack(altstack);
                }
                break;

                case op_2drop:
                {
                    // (x1 x2 -- )
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    popstack(stack);
                    popstack(stack);
                }
                break;

                case op_2dup:
                {
                    // (x1 x2 -- x1 x2 x1 x2)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch1 = stacktop(-2);
                    valtype vch2 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case op_3dup:
                {
                    // (x1 x2 x3 -- x1 x2 x3 x1 x2 x3)
                    if (stack.size() < 3)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch1 = stacktop(-3);
                    valtype vch2 = stacktop(-2);
                    valtype vch3 = stacktop(-1);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                    stack.push_back(vch3);
                }
                break;

                case op_2over:
                {
                    // (x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2)
                    if (stack.size() < 4)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch1 = stacktop(-4);
                    valtype vch2 = stacktop(-3);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case op_2rot:
                {
                    // (x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2)
                    if (stack.size() < 6)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch1 = stacktop(-6);
                    valtype vch2 = stacktop(-5);
                    stack.erase(stack.end()-6, stack.end()-4);
                    stack.push_back(vch1);
                    stack.push_back(vch2);
                }
                break;

                case op_2swap:
                {
                    // (x1 x2 x3 x4 -- x3 x4 x1 x2)
                    if (stack.size() < 4)
                        return set_error(serror, script_err_invalid_stack_operation);
                    swap(stacktop(-4), stacktop(-2));
                    swap(stacktop(-3), stacktop(-1));
                }
                break;

                case op_ifdup:
                {
                    // (x - 0 | x x)
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch = stacktop(-1);
                    if (casttobool(vch))
                        stack.push_back(vch);
                }
                break;

                case op_depth:
                {
                    // -- stacksize
                    cscriptnum bn(stack.size());
                    stack.push_back(bn.getvch());
                }
                break;

                case op_drop:
                {
                    // (x -- )
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    popstack(stack);
                }
                break;

                case op_dup:
                {
                    // (x -- x x)
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch = stacktop(-1);
                    stack.push_back(vch);
                }
                break;

                case op_nip:
                {
                    // (x1 x2 -- x2)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    stack.erase(stack.end() - 2);
                }
                break;

                case op_over:
                {
                    // (x1 x2 -- x1 x2 x1)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch = stacktop(-2);
                    stack.push_back(vch);
                }
                break;

                case op_pick:
                case op_roll:
                {
                    // (xn ... x2 x1 x0 n - xn ... x2 x1 x0 xn)
                    // (xn ... x2 x1 x0 n - ... x2 x1 x0 xn)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    int n = cscriptnum(stacktop(-1), frequireminimal).getint();
                    popstack(stack);
                    if (n < 0 || n >= (int)stack.size())
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch = stacktop(-n-1);
                    if (opcode == op_roll)
                        stack.erase(stack.end()-n-1);
                    stack.push_back(vch);
                }
                break;

                case op_rot:
                {
                    // (x1 x2 x3 -- x2 x3 x1)
                    //  x2 x1 x3  after first swap
                    //  x2 x3 x1  after second swap
                    if (stack.size() < 3)
                        return set_error(serror, script_err_invalid_stack_operation);
                    swap(stacktop(-3), stacktop(-2));
                    swap(stacktop(-2), stacktop(-1));
                }
                break;

                case op_swap:
                {
                    // (x1 x2 -- x2 x1)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    swap(stacktop(-2), stacktop(-1));
                }
                break;

                case op_tuck:
                {
                    // (x1 x2 -- x2 x1 x2)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype vch = stacktop(-1);
                    stack.insert(stack.end()-2, vch);
                }
                break;


                case op_size:
                {
                    // (in -- in size)
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    cscriptnum bn(stacktop(-1).size());
                    stack.push_back(bn.getvch());
                }
                break;


                //
                // bitwise logic
                //
                case op_equal:
                case op_equalverify:
                //case op_notequal: // use op_numnotequal
                {
                    // (x1 x2 - bool)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype& vch1 = stacktop(-2);
                    valtype& vch2 = stacktop(-1);
                    bool fequal = (vch1 == vch2);
                    // op_notequal is disabled because it would be too easy to say
                    // something like n != 1 and have some wiseguy pass in 1 with extra
                    // zero bytes after it (numerically, 0x01 == 0x0001 == 0x000001)
                    //if (opcode == op_notequal)
                    //    fequal = !fequal;
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fequal ? vchtrue : vchfalse);
                    if (opcode == op_equalverify)
                    {
                        if (fequal)
                            popstack(stack);
                        else
                            return set_error(serror, script_err_equalverify);
                    }
                }
                break;


                //
                // numeric
                //
                case op_1add:
                case op_1sub:
                case op_negate:
                case op_abs:
                case op_not:
                case op_0notequal:
                {
                    // (in -- out)
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    cscriptnum bn(stacktop(-1), frequireminimal);
                    switch (opcode)
                    {
                    case op_1add:       bn += bnone; break;
                    case op_1sub:       bn -= bnone; break;
                    case op_negate:     bn = -bn; break;
                    case op_abs:        if (bn < bnzero) bn = -bn; break;
                    case op_not:        bn = (bn == bnzero); break;
                    case op_0notequal:  bn = (bn != bnzero); break;
                    default:            assert(!"invalid opcode"); break;
                    }
                    popstack(stack);
                    stack.push_back(bn.getvch());
                }
                break;

                case op_add:
                case op_sub:
                case op_booland:
                case op_boolor:
                case op_numequal:
                case op_numequalverify:
                case op_numnotequal:
                case op_lessthan:
                case op_greaterthan:
                case op_lessthanorequal:
                case op_greaterthanorequal:
                case op_min:
                case op_max:
                {
                    // (x1 x2 -- out)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);
                    cscriptnum bn1(stacktop(-2), frequireminimal);
                    cscriptnum bn2(stacktop(-1), frequireminimal);
                    cscriptnum bn(0);
                    switch (opcode)
                    {
                    case op_add:
                        bn = bn1 + bn2;
                        break;

                    case op_sub:
                        bn = bn1 - bn2;
                        break;

                    case op_booland:             bn = (bn1 != bnzero && bn2 != bnzero); break;
                    case op_boolor:              bn = (bn1 != bnzero || bn2 != bnzero); break;
                    case op_numequal:            bn = (bn1 == bn2); break;
                    case op_numequalverify:      bn = (bn1 == bn2); break;
                    case op_numnotequal:         bn = (bn1 != bn2); break;
                    case op_lessthan:            bn = (bn1 < bn2); break;
                    case op_greaterthan:         bn = (bn1 > bn2); break;
                    case op_lessthanorequal:     bn = (bn1 <= bn2); break;
                    case op_greaterthanorequal:  bn = (bn1 >= bn2); break;
                    case op_min:                 bn = (bn1 < bn2 ? bn1 : bn2); break;
                    case op_max:                 bn = (bn1 > bn2 ? bn1 : bn2); break;
                    default:                     assert(!"invalid opcode"); break;
                    }
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(bn.getvch());

                    if (opcode == op_numequalverify)
                    {
                        if (casttobool(stacktop(-1)))
                            popstack(stack);
                        else
                            return set_error(serror, script_err_numequalverify);
                    }
                }
                break;

                case op_within:
                {
                    // (x min max -- out)
                    if (stack.size() < 3)
                        return set_error(serror, script_err_invalid_stack_operation);
                    cscriptnum bn1(stacktop(-3), frequireminimal);
                    cscriptnum bn2(stacktop(-2), frequireminimal);
                    cscriptnum bn3(stacktop(-1), frequireminimal);
                    bool fvalue = (bn2 <= bn1 && bn1 < bn3);
                    popstack(stack);
                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fvalue ? vchtrue : vchfalse);
                }
                break;


                //
                // crypto
                //
                case op_ripemd160:
                case op_sha1:
                case op_sha256:
                case op_hash160:
                case op_hash256:
                {
                    // (in -- hash)
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    valtype& vch = stacktop(-1);
                    valtype vchhash((opcode == op_ripemd160 || opcode == op_sha1 || opcode == op_hash160) ? 20 : 32);
                    if (opcode == op_ripemd160)
                        cripemd160().write(begin_ptr(vch), vch.size()).finalize(begin_ptr(vchhash));
                    else if (opcode == op_sha1)
                        csha1().write(begin_ptr(vch), vch.size()).finalize(begin_ptr(vchhash));
                    else if (opcode == op_sha256)
                        csha256().write(begin_ptr(vch), vch.size()).finalize(begin_ptr(vchhash));
                    else if (opcode == op_hash160)
                        chash160().write(begin_ptr(vch), vch.size()).finalize(begin_ptr(vchhash));
                    else if (opcode == op_hash256)
                        chash256().write(begin_ptr(vch), vch.size()).finalize(begin_ptr(vchhash));
                    popstack(stack);
                    stack.push_back(vchhash);
                }
                break;                                   

                case op_codeseparator:
                {
                    // hash starts after the code separator
                    pbegincodehash = pc;
                }
                break;

                case op_checksig:
                case op_checksigverify:
                {
                    // (sig pubkey -- bool)
                    if (stack.size() < 2)
                        return set_error(serror, script_err_invalid_stack_operation);

                    valtype& vchsig    = stacktop(-2);
                    valtype& vchpubkey = stacktop(-1);

                    // subset of script starting at the most recent codeseparator
                    cscript scriptcode(pbegincodehash, pend);

                    // drop the signature, since there's no way for a signature to sign itself
                    scriptcode.findanddelete(cscript(vchsig));

                    if (!checksignatureencoding(vchsig, flags, serror) || !checkpubkeyencoding(vchpubkey, flags, serror)) {
                        //serror is set
                        return false;
                    }
                    bool fsuccess = checker.checksig(vchsig, vchpubkey, scriptcode);

                    popstack(stack);
                    popstack(stack);
                    stack.push_back(fsuccess ? vchtrue : vchfalse);
                    if (opcode == op_checksigverify)
                    {
                        if (fsuccess)
                            popstack(stack);
                        else
                            return set_error(serror, script_err_checksigverify);
                    }
                }
                break;

                case op_checkmultisig:
                case op_checkmultisigverify:
                {
                    // ([sig ...] num_of_signatures [pubkey ...] num_of_pubkeys -- bool)

                    int i = 1;
                    if ((int)stack.size() < i)
                        return set_error(serror, script_err_invalid_stack_operation);

                    int nkeyscount = cscriptnum(stacktop(-i), frequireminimal).getint();
                    if (nkeyscount < 0 || nkeyscount > 20)
                        return set_error(serror, script_err_pubkey_count);
                    nopcount += nkeyscount;
                    if (nopcount > 201)
                        return set_error(serror, script_err_op_count);
                    int ikey = ++i;
                    i += nkeyscount;
                    if ((int)stack.size() < i)
                        return set_error(serror, script_err_invalid_stack_operation);

                    int nsigscount = cscriptnum(stacktop(-i), frequireminimal).getint();
                    if (nsigscount < 0 || nsigscount > nkeyscount)
                        return set_error(serror, script_err_sig_count);
                    int isig = ++i;
                    i += nsigscount;
                    if ((int)stack.size() < i)
                        return set_error(serror, script_err_invalid_stack_operation);

                    // subset of script starting at the most recent codeseparator
                    cscript scriptcode(pbegincodehash, pend);

                    // drop the signatures, since there's no way for a signature to sign itself
                    for (int k = 0; k < nsigscount; k++)
                    {
                        valtype& vchsig = stacktop(-isig-k);
                        scriptcode.findanddelete(cscript(vchsig));
                    }

                    bool fsuccess = true;
                    while (fsuccess && nsigscount > 0)
                    {
                        valtype& vchsig    = stacktop(-isig);
                        valtype& vchpubkey = stacktop(-ikey);

                        // note how this makes the exact order of pubkey/signature evaluation
                        // distinguishable by checkmultisig not if the strictenc flag is set.
                        // see the script_(in)valid tests for details.
                        if (!checksignatureencoding(vchsig, flags, serror) || !checkpubkeyencoding(vchpubkey, flags, serror)) {
                            // serror is set
                            return false;
                        }

                        // check signature
                        bool fok = checker.checksig(vchsig, vchpubkey, scriptcode);

                        if (fok) {
                            isig++;
                            nsigscount--;
                        }
                        ikey++;
                        nkeyscount--;

                        // if there are more signatures left than keys left,
                        // then too many signatures have failed. exit early,
                        // without checking any further signatures.
                        if (nsigscount > nkeyscount)
                            fsuccess = false;
                    }

                    // clean up stack of actual arguments
                    while (i-- > 1)
                        popstack(stack);

                    // a bug causes checkmultisig to consume one extra argument
                    // whose contents were not checked in any way.
                    //
                    // unfortunately this is a potential source of mutability,
                    // so optionally verify it is exactly equal to zero prior
                    // to removing it from the stack.
                    if (stack.size() < 1)
                        return set_error(serror, script_err_invalid_stack_operation);
                    if ((flags & script_verify_nulldummy) && stacktop(-1).size())
                        return set_error(serror, script_err_sig_nulldummy);
                    popstack(stack);

                    stack.push_back(fsuccess ? vchtrue : vchfalse);

                    if (opcode == op_checkmultisigverify)
                    {
                        if (fsuccess)
                            popstack(stack);
                        else
                            return set_error(serror, script_err_checkmultisigverify);
                    }
                }
                break;

                default:
                    return set_error(serror, script_err_bad_opcode);
            }

            // size limits
            if (stack.size() + altstack.size() > 1000)
                return set_error(serror, script_err_stack_size);
        }
    }
    catch (...)
    {
        return set_error(serror, script_err_unknown_error);
    }

    if (!vfexec.empty())
        return set_error(serror, script_err_unbalanced_conditional);

    return set_success(serror);
}

namespace {

/**
 * wrapper that serializes like ctransaction, but with the modifications
 *  required for the signature hash done in-place
 */
class ctransactionsignatureserializer {
private:
    const ctransaction &txto;  //! reference to the spending transaction (the one being serialized)
    const cscript &scriptcode; //! output script being consumed
    const unsigned int nin;    //! input index of txto being signed
    const bool fanyonecanpay;  //! whether the hashtype has the sighash_anyonecanpay flag set
    const bool fhashsingle;    //! whether the hashtype is sighash_single
    const bool fhashnone;      //! whether the hashtype is sighash_none

public:
    ctransactionsignatureserializer(const ctransaction &txtoin, const cscript &scriptcodein, unsigned int ninin, int nhashtypein) :
        txto(txtoin), scriptcode(scriptcodein), nin(ninin),
        fanyonecanpay(!!(nhashtypein & sighash_anyonecanpay)),
        fhashsingle((nhashtypein & 0x1f) == sighash_single),
        fhashnone((nhashtypein & 0x1f) == sighash_none) {}

    /** serialize the passed scriptcode, skipping op_codeseparators */
    template<typename s>
    void serializescriptcode(s &s, int ntype, int nversion) const {
        cscript::const_iterator it = scriptcode.begin();
        cscript::const_iterator itbegin = it;
        opcodetype opcode;
        unsigned int ncodeseparators = 0;
        while (scriptcode.getop(it, opcode)) {
            if (opcode == op_codeseparator)
                ncodeseparators++;
        }
        ::writecompactsize(s, scriptcode.size() - ncodeseparators);
        it = itbegin;
        while (scriptcode.getop(it, opcode)) {
            if (opcode == op_codeseparator) {
                s.write((char*)&itbegin[0], it-itbegin-1);
                itbegin = it;
            }
        }
        if (itbegin != scriptcode.end())
            s.write((char*)&itbegin[0], it-itbegin);
    }

    /** serialize an input of txto */
    template<typename s>
    void serializeinput(s &s, unsigned int ninput, int ntype, int nversion) const {
        // in case of sighash_anyonecanpay, only the input being signed is serialized
        if (fanyonecanpay)
            ninput = nin;
        // serialize the prevout
        ::serialize(s, txto.vin[ninput].prevout, ntype, nversion);
        // serialize the script
        if (ninput != nin)
            // blank out other inputs' signatures
            ::serialize(s, cscript(), ntype, nversion);
        else
            serializescriptcode(s, ntype, nversion);
        // serialize the nsequence
        if (ninput != nin && (fhashsingle || fhashnone))
            // let the others update at will
            ::serialize(s, (int)0, ntype, nversion);
        else
            ::serialize(s, txto.vin[ninput].nsequence, ntype, nversion);
    }

    /** serialize an output of txto */
    template<typename s>
    void serializeoutput(s &s, unsigned int noutput, int ntype, int nversion) const {
        if (fhashsingle && noutput != nin)
            // do not lock-in the txout payee at other indices as txin
            ::serialize(s, ctxout(), ntype, nversion);
        else
            ::serialize(s, txto.vout[noutput], ntype, nversion);
    }

    /** serialize txto */
    template<typename s>
    void serialize(s &s, int ntype, int nversion) const {
        // serialize nversion
        ::serialize(s, txto.nversion, ntype, nversion);
        // serialize vin
        unsigned int ninputs = fanyonecanpay ? 1 : txto.vin.size();
        ::writecompactsize(s, ninputs);
        for (unsigned int ninput = 0; ninput < ninputs; ninput++)
             serializeinput(s, ninput, ntype, nversion);
        // serialize vout
        unsigned int noutputs = fhashnone ? 0 : (fhashsingle ? nin+1 : txto.vout.size());
        ::writecompactsize(s, noutputs);
        for (unsigned int noutput = 0; noutput < noutputs; noutput++)
             serializeoutput(s, noutput, ntype, nversion);
        // serialize nlocktime
        ::serialize(s, txto.nlocktime, ntype, nversion);
    }
};

} // anon namespace

uint256 signaturehash(const cscript& scriptcode, const ctransaction& txto, unsigned int nin, int nhashtype)
{
    static const uint256 one(uint256s("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nin >= txto.vin.size()) {
        //  nin out of range
        return one;
    }

    // check for invalid use of sighash_single
    if ((nhashtype & 0x1f) == sighash_single) {
        if (nin >= txto.vout.size()) {
            //  nout out of range
            return one;
        }
    }

    // wrapper to serialize only the necessary parts of the transaction being signed
    ctransactionsignatureserializer txtmp(txto, scriptcode, nin, nhashtype);

    // serialize and hash
    chashwriter ss(ser_gethash, 0);
    ss << txtmp << nhashtype;
    return ss.gethash();
}

bool transactionsignaturechecker::verifysignature(const std::vector<unsigned char>& vchsig, const cpubkey& pubkey, const uint256& sighash) const
{
    return pubkey.verify(sighash, vchsig);
}

bool transactionsignaturechecker::checksig(const vector<unsigned char>& vchsigin, const vector<unsigned char>& vchpubkey, const cscript& scriptcode) const
{
    cpubkey pubkey(vchpubkey);
    if (!pubkey.isvalid())
        return false;

    // hash type is one byte tacked on to the end of the signature
    vector<unsigned char> vchsig(vchsigin);
    if (vchsig.empty())
        return false;
    int nhashtype = vchsig.back();
    vchsig.pop_back();

    uint256 sighash = signaturehash(scriptcode, *txto, nin, nhashtype);

    if (!verifysignature(vchsig, pubkey, sighash))
        return false;

    return true;
}

bool verifyscript(const cscript& scriptsig, const cscript& scriptpubkey, unsigned int flags, const basesignaturechecker& checker, scripterror* serror)
{
    set_error(serror, script_err_unknown_error);

    if ((flags & script_verify_sigpushonly) != 0 && !scriptsig.ispushonly()) {
        return set_error(serror, script_err_sig_pushonly);
    }

    vector<vector<unsigned char> > stack, stackcopy;
    if (!evalscript(stack, scriptsig, flags, checker, serror))
        // serror is set
        return false;
    if (flags & script_verify_p2sh)
        stackcopy = stack;
    if (!evalscript(stack, scriptpubkey, flags, checker, serror))
        // serror is set
        return false;
    if (stack.empty())
        return set_error(serror, script_err_eval_false);
    if (casttobool(stack.back()) == false)
        return set_error(serror, script_err_eval_false);

    // additional validation for spend-to-script-hash transactions:
    if ((flags & script_verify_p2sh) && scriptpubkey.ispaytoscripthash())
    {
        // scriptsig must be literals-only or validation fails
        if (!scriptsig.ispushonly())
            return set_error(serror, script_err_sig_pushonly);

        // restore stack.
        swap(stack, stackcopy);

        // stack cannot be empty here, because if it was the
        // p2sh  hash <> equal  scriptpubkey would be evaluated with
        // an empty stack and the evalscript above would return false.
        assert(!stack.empty());

        const valtype& pubkeyserialized = stack.back();
        cscript pubkey2(pubkeyserialized.begin(), pubkeyserialized.end());
        popstack(stack);

        if (!evalscript(stack, pubkey2, flags, checker, serror))
            // serror is set
            return false;
        if (stack.empty())
            return set_error(serror, script_err_eval_false);
        if (!casttobool(stack.back()))
            return set_error(serror, script_err_eval_false);
    }

    // the cleanstack check is only performed after potential p2sh evaluation,
    // as the non-p2sh evaluation of a p2sh script will obviously not result in
    // a clean stack (the p2sh inputs remain).
    if ((flags & script_verify_cleanstack) != 0) {
        // disallow cleanstack without p2sh, as otherwise a switch cleanstack->p2sh+cleanstack
        // would be possible, which is not a softfork (and p2sh should be one).
        assert((flags & script_verify_p2sh) != 0);
        if (stack.size() != 1) {
            return set_error(serror, script_err_cleanstack);
        }
    }

    return set_success(serror);
}
