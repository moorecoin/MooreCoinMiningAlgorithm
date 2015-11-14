// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "script_error.h"

const char* scripterrorstring(const scripterror serror)
{
    switch (serror)
    {
        case script_err_ok:
            return "no error";
        case script_err_eval_false:
            return "script evaluated without error but finished with a false/empty top stack element";
        case script_err_verify:
            return "script failed an op_verify operation";
        case script_err_equalverify:
            return "script failed an op_equalverify operation";
        case script_err_checkmultisigverify:
            return "script failed an op_checkmultisigverify operation";
        case script_err_checksigverify:
            return "script failed an op_checksigverify operation";
        case script_err_numequalverify:
            return "script failed an op_numequalverify operation";
        case script_err_script_size:
            return "script is too big";
        case script_err_push_size:
            return "push value size limit exceeded";
        case script_err_op_count:
            return "operation limit exceeded";
        case script_err_stack_size:
            return "stack size limit exceeded";
        case script_err_sig_count:
            return "signature count negative or greater than pubkey count";
        case script_err_pubkey_count:
            return "pubkey count negative or limit exceeded";
        case script_err_bad_opcode:
            return "opcode missing or not understood";
        case script_err_disabled_opcode:
            return "attempted to use a disabled opcode";
        case script_err_invalid_stack_operation:
            return "operation not valid with the current stack size";
        case script_err_invalid_altstack_operation:
            return "operation not valid with the current altstack size";
        case script_err_op_return:
            return "op_return was encountered";
        case script_err_unbalanced_conditional:
            return "invalid op_if construction";
        case script_err_sig_hashtype:
            return "signature hash type missing or not understood";
        case script_err_sig_der:
            return "non-canonical der signature";
        case script_err_minimaldata:
            return "data push larger than necessary";
        case script_err_sig_pushonly:
            return "only non-push operators allowed in signatures";
        case script_err_sig_high_s:
            return "non-canonical signature: s value is unnecessarily high";
        case script_err_sig_nulldummy:
            return "dummy checkmultisig argument must be zero";
        case script_err_discourage_upgradable_nops:
            return "nopx reserved for soft-fork upgrades";
        case script_err_pubkeytype:
            return "public key is neither compressed or uncompressed";
        case script_err_unknown_error:
        case script_err_error_count:
        default: break;
    }
    return "unknown error";
}
