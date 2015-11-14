// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_script_script_error_h
#define moorecoin_script_script_error_h

typedef enum scripterror_t
{
    script_err_ok = 0,
    script_err_unknown_error,
    script_err_eval_false,
    script_err_op_return,

    /* max sizes */
    script_err_script_size,
    script_err_push_size,
    script_err_op_count,
    script_err_stack_size,
    script_err_sig_count,
    script_err_pubkey_count,

    /* failed verify operations */
    script_err_verify,
    script_err_equalverify,
    script_err_checkmultisigverify,
    script_err_checksigverify,
    script_err_numequalverify,

    /* logical/format/canonical errors */
    script_err_bad_opcode,
    script_err_disabled_opcode,
    script_err_invalid_stack_operation,
    script_err_invalid_altstack_operation,
    script_err_unbalanced_conditional,

    /* bip62 */
    script_err_sig_hashtype,
    script_err_sig_der,
    script_err_minimaldata,
    script_err_sig_pushonly,
    script_err_sig_high_s,
    script_err_sig_nulldummy,
    script_err_pubkeytype,
    script_err_cleanstack,

    /* softfork safeness */
    script_err_discourage_upgradable_nops,

    script_err_error_count
} scripterror;

#define script_err_last script_err_error_count

const char* scripterrorstring(const scripterror error);

#endif // moorecoin_script_script_error_h
