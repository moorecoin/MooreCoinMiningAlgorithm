// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_chainparamsbase_h
#define moorecoin_chainparamsbase_h

#include <string>
#include <vector>

/**
 * cbasechainparams defines the base parameters (shared between moorecoin-cli and moorecoind)
 * of a given instance of the moorecoin system.
 */
class cbasechainparams
{
public:
    enum network {
        main,
        testnet,
        regtest,

        max_network_types
    };

    const std::string& datadir() const { return strdatadir; }
    int rpcport() const { return nrpcport; }

protected:
    cbasechainparams() {}

    int nrpcport;
    std::string strdatadir;
};

/**
 * return the currently selected parameters. this won't change after app
 * startup, except for unit tests.
 */
const cbasechainparams& baseparams();

/** sets the params returned by params() to those for the given network. */
void selectbaseparams(cbasechainparams::network network);

/**
 * looks for -regtest or -testnet and returns the appropriate network id.
 * returns max_network_types if an invalid combination is given.
 */
cbasechainparams::network networkidfromcommandline();

/**
 * calls networkidfromcommandline() and then calls selectparams as appropriate.
 * returns false if an invalid combination is given.
 */
bool selectbaseparamsfromcommandline();

/**
 * return true if selectbaseparamsfromcommandline() has been called to select
 * a network.
 */
bool arebaseparamsconfigured();

#endif // moorecoin_chainparamsbase_h
