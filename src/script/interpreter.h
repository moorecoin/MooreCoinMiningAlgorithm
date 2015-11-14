// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_script_interpreter_h
#define moorecoin_script_interpreter_h

#include "script_error.h"
#include "primitives/transaction.h"

#include <vector>
#include <stdint.h>
#include <string>

class cpubkey;
class cscript;
class ctransaction;
class uint256;

/** signature hash types/flags */
enum
{
    sighash_all = 1,
    sighash_none = 2,
    sighash_single = 3,
    sighash_anyonecanpay = 0x80,
};

/** script verification flags */
enum
{
    script_verify_none      = 0,

    // evaluate p2sh subscripts (softfork safe, bip16).
    script_verify_p2sh      = (1u << 0),

    // passing a non-strict-der signature or one with undefined hashtype to a checksig operation causes script failure.
    // evaluating a pubkey that is not (0x04 + 64 bytes) or (0x02 or 0x03 + 32 bytes) by checksig causes script failure.
    // (softfork safe, but not used or intended as a consensus rule).
    script_verify_strictenc = (1u << 1),

    // passing a non-strict-der signature to a checksig operation causes script failure (softfork safe, bip62 rule 1)
    script_verify_dersig    = (1u << 2),

    // passing a non-strict-der signature or one with s > order/2 to a checksig operation causes script failure
    // (softfork safe, bip62 rule 5).
    script_verify_low_s     = (1u << 3),

    // verify dummy stack item consumed by checkmultisig is of zero-length (softfork safe, bip62 rule 7).
    script_verify_nulldummy = (1u << 4),

    // using a non-push operator in the scriptsig causes script failure (softfork safe, bip62 rule 2).
    script_verify_sigpushonly = (1u << 5),

    // require minimal encodings for all push operations (op_0... op_16, op_1negate where possible, direct
    // pushes up to 75 bytes, op_pushdata up to 255 bytes, op_pushdata2 for anything larger). evaluating
    // any other push causes the script to fail (bip62 rule 3).
    // in addition, whenever a stack element is interpreted as a number, it must be of minimal length (bip62 rule 4).
    // (softfork safe)
    script_verify_minimaldata = (1u << 6),

    // discourage use of nops reserved for upgrades (nop1-10)
    //
    // provided so that nodes can avoid accepting or mining transactions
    // containing executed nop's whose meaning may change after a soft-fork,
    // thus rendering the script invalid; with this flag set executing
    // discouraged nops fails the script. this verification flag will never be
    // a mandatory flag applied to scripts in a block. nops that are not
    // executed, e.g.  within an unexecuted if endif block, are *not* rejected.
    script_verify_discourage_upgradable_nops  = (1u << 7),

    // require that only a single stack element remains after evaluation. this changes the success criterion from
    // "at least one stack element must remain, and when interpreted as a boolean, it must be true" to
    // "exactly one stack element must remain, and when interpreted as a boolean, it must be true".
    // (softfork safe, bip62 rule 6)
    // note: cleanstack should never be used without p2sh.
    script_verify_cleanstack = (1u << 8),
};

uint256 signaturehash(const cscript &scriptcode, const ctransaction& txto, unsigned int nin, int nhashtype);

class basesignaturechecker
{
public:
    virtual bool checksig(const std::vector<unsigned char>& scriptsig, const std::vector<unsigned char>& vchpubkey, const cscript& scriptcode) const
    {
        return false;
    }

    virtual ~basesignaturechecker() {}
};

class transactionsignaturechecker : public basesignaturechecker
{
private:
    const ctransaction* txto;
    unsigned int nin;

protected:
    virtual bool verifysignature(const std::vector<unsigned char>& vchsig, const cpubkey& vchpubkey, const uint256& sighash) const;

public:
    transactionsignaturechecker(const ctransaction* txtoin, unsigned int ninin) : txto(txtoin), nin(ninin) {}
    bool checksig(const std::vector<unsigned char>& scriptsig, const std::vector<unsigned char>& vchpubkey, const cscript& scriptcode) const;
};

class mutabletransactionsignaturechecker : public transactionsignaturechecker
{
private:
    const ctransaction txto;

public:
    mutabletransactionsignaturechecker(const cmutabletransaction* txtoin, unsigned int ninin) : transactionsignaturechecker(&txto, ninin), txto(*txtoin) {}
};

bool evalscript(std::vector<std::vector<unsigned char> >& stack, const cscript& script, unsigned int flags, const basesignaturechecker& checker, scripterror* error = null);
bool verifyscript(const cscript& scriptsig, const cscript& scriptpubkey, unsigned int flags, const basesignaturechecker& checker, scripterror* error = null);

#endif // moorecoin_script_interpreter_h
