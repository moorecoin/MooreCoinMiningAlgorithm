// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_validationinterface_h
#define moorecoin_validationinterface_h

#include <boost/signals2/signal.hpp>

class cblock;
struct cblocklocator;
class ctransaction;
class cvalidationinterface;
class cvalidationstate;
class uint256;

// these functions dispatch to one or all registered wallets

/** register a wallet to receive updates from core */
void registervalidationinterface(cvalidationinterface* pwalletin);
/** unregister a wallet from core */
void unregistervalidationinterface(cvalidationinterface* pwalletin);
/** unregister all wallets from core */
void unregisterallvalidationinterfaces();
/** push an updated transaction to all registered wallets */
void syncwithwallets(const ctransaction& tx, const cblock* pblock = null);

class cvalidationinterface {
protected:
    virtual void synctransaction(const ctransaction &tx, const cblock *pblock) {}
    virtual void setbestchain(const cblocklocator &locator) {}
    virtual void updatedtransaction(const uint256 &hash) {}
    virtual void inventory(const uint256 &hash) {}
    virtual void resendwallettransactions(int64_t nbestblocktime) {}
    virtual void blockchecked(const cblock&, const cvalidationstate&) {}
    friend void ::registervalidationinterface(cvalidationinterface*);
    friend void ::unregistervalidationinterface(cvalidationinterface*);
    friend void ::unregisterallvalidationinterfaces();
};

struct cmainsignals {
    /** notifies listeners of updated transaction data (transaction, and optionally the block it is found in. */
    boost::signals2::signal<void (const ctransaction &, const cblock *)> synctransaction;
    /** notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<void (const uint256 &)> updatedtransaction;
    /** notifies listeners of a new active block chain. */
    boost::signals2::signal<void (const cblocklocator &)> setbestchain;
    /** notifies listeners about an inventory item being seen on the network. */
    boost::signals2::signal<void (const uint256 &)> inventory;
    /** tells listeners to broadcast their data. */
    boost::signals2::signal<void (int64_t nbestblocktime)> broadcast;
    /** notifies listeners of a block validation result */
    boost::signals2::signal<void (const cblock&, const cvalidationstate&)> blockchecked;
};

cmainsignals& getmainsignals();

#endif // moorecoin_validationinterface_h
