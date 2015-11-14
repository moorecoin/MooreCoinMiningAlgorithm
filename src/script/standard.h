// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_script_standard_h
#define moorecoin_script_standard_h

#include "script/interpreter.h"
#include "uint256.h"

#include <boost/variant.hpp>

#include <stdint.h>

class ckeyid;
class cscript;

/** a reference to a cscript: the hash160 of its serialization (see script.h) */
class cscriptid : public uint160
{
public:
    cscriptid() : uint160() {}
    cscriptid(const cscript& in);
    cscriptid(const uint160& in) : uint160(in) {}
};

static const unsigned int max_op_return_relay = 80;      //! bytes
extern unsigned nmaxdatacarrierbytes;

/**
 * mandatory script verification flags that all new blocks must comply with for
 * them to be valid. (but old blocks may not comply with) currently just p2sh,
 * but in the future other flags may be added, such as a soft-fork to enforce
 * strict der encoding.
 * 
 * failing one of these tests may trigger a dos ban - see checkinputs() for
 * details.
 */
static const unsigned int mandatory_script_verify_flags = script_verify_p2sh;

/**
 * standard script verification flags that standard transactions will comply
 * with. however scripts violating these flags may still be present in valid
 * blocks and we must accept those blocks.
 */
static const unsigned int standard_script_verify_flags = mandatory_script_verify_flags |
                                                         script_verify_dersig |
                                                         script_verify_strictenc |
                                                         script_verify_minimaldata |
                                                         script_verify_nulldummy |
                                                         script_verify_discourage_upgradable_nops |
                                                         script_verify_cleanstack;

/** for convenience, standard but not mandatory verify flags. */
static const unsigned int standard_not_mandatory_verify_flags = standard_script_verify_flags & ~mandatory_script_verify_flags;

enum txnouttype
{
    tx_nonstandard,
    // 'standard' transaction types:
    tx_pubkey,
    tx_pubkeyhash,
    tx_scripthash,
    tx_multisig,
    tx_null_data,
};

class cnodestination {
public:
    friend bool operator==(const cnodestination &a, const cnodestination &b) { return true; }
    friend bool operator<(const cnodestination &a, const cnodestination &b) { return true; }
};

/** 
 * a txout script template with a specific destination. it is either:
 *  * cnodestination: no destination set
 *  * ckeyid: tx_pubkeyhash destination
 *  * cscriptid: tx_scripthash destination
 *  a ctxdestination is the internal data type encoded in a cmoorecoinaddress
 */
typedef boost::variant<cnodestination, ckeyid, cscriptid> ctxdestination;

const char* gettxnoutputtype(txnouttype t);

bool solver(const cscript& scriptpubkey, txnouttype& typeret, std::vector<std::vector<unsigned char> >& vsolutionsret);
int scriptsigargsexpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vsolutions);
bool isstandard(const cscript& scriptpubkey, txnouttype& whichtype);
bool extractdestination(const cscript& scriptpubkey, ctxdestination& addressret);
bool extractdestinations(const cscript& scriptpubkey, txnouttype& typeret, std::vector<ctxdestination>& addressret, int& nrequiredret);

cscript getscriptfordestination(const ctxdestination& dest);
cscript getscriptformultisig(int nrequired, const std::vector<cpubkey>& keys);

#endif // moorecoin_script_standard_h
