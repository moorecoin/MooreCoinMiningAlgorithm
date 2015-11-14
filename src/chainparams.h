// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_chainparams_h
#define moorecoin_chainparams_h

#include "chainparamsbase.h"
#include "checkpoints.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"

#include <vector>

struct cdnsseeddata {
    std::string name, host;
    cdnsseeddata(const std::string &strname, const std::string &strhost) : name(strname), host(strhost) {}
};

struct seedspec6 {
    uint8_t addr[16];
    uint16_t port;
};


/**
 * cchainparams defines various tweakable parameters of a given instance of the
 * moorecoin system. there are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. it has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class cchainparams
{
public:
    enum base58type {
        pubkey_address,
        script_address,
        secret_key,
        ext_public_key,
        ext_secret_key,

        max_base58_types
    };

    const consensus::params& getconsensus() const { return consensus; }
    const cmessageheader::messagestartchars& messagestart() const { return pchmessagestart; }
    const std::vector<unsigned char>& alertkey() const { return valertpubkey; }
    int getdefaultport() const { return ndefaultport; }

    /** used if generatemoorecoins is called with a negative number of threads */
    int defaultminerthreads() const { return nminerthreads; }
    const cblock& genesisblock() const { return genesis; }
    bool requirerpcpassword() const { return frequirerpcpassword; }
    /** make miner wait to have peers to avoid wasting work */
    bool miningrequirespeers() const { return fminingrequirespeers; }
    /** default value for -checkmempool and -checkblockindex argument */
    bool defaultconsistencychecks() const { return fdefaultconsistencychecks; }
    /** policy: filter transactions that do not match well-defined patterns */
    bool requirestandard() const { return frequirestandard; }
    int64_t pruneafterheight() const { return npruneafterheight; }
    /** make miner stop after a block is found. in rpc, don't return until ngenproclimit blocks are generated */
    bool mineblocksondemand() const { return fmineblocksondemand; }
    /** in the future use networkidstring() for rpc fields */
    bool testnettobedeprecatedfieldrpc() const { return ftestnettobedeprecatedfieldrpc; }
    /** return the bip70 network string (main, test or regtest) */
    std::string networkidstring() const { return strnetworkid; }
    const std::vector<cdnsseeddata>& dnsseeds() const { return vseeds; }
    const std::vector<unsigned char>& base58prefix(base58type type) const { return base58prefixes[type]; }
    const std::vector<seedspec6>& fixedseeds() const { return vfixedseeds; }
    const checkpoints::ccheckpointdata& checkpoints() const { return checkpointdata; }
protected:
    cchainparams() {}

    consensus::params consensus;
    cmessageheader::messagestartchars pchmessagestart;
    //! raw pub key bytes for the broadcast alert signing key.
    std::vector<unsigned char> valertpubkey;
    int ndefaultport;
    int nminerthreads;
    uint64_t npruneafterheight;
    std::vector<cdnsseeddata> vseeds;
    std::vector<unsigned char> base58prefixes[max_base58_types];
    std::string strnetworkid;
    cblock genesis;
    std::vector<seedspec6> vfixedseeds;
    bool frequirerpcpassword;
    bool fminingrequirespeers;
    bool fdefaultconsistencychecks;
    bool frequirestandard;
    bool fmineblocksondemand;
    bool ftestnettobedeprecatedfieldrpc;
    checkpoints::ccheckpointdata checkpointdata;
};

/**
 * return the currently selected parameters. this won't change after app
 * startup, except for unit tests.
 */
const cchainparams &params();

/** return parameters for the given network. */
cchainparams &params(cbasechainparams::network network);

/** sets the params returned by params() to those for the given network. */
void selectparams(cbasechainparams::network network);

/**
 * looks for -regtest or -testnet and then calls selectparams as appropriate.
 * returns false if an invalid combination is given.
 */
bool selectparamsfromcommandline();

#endif // moorecoin_chainparams_h
