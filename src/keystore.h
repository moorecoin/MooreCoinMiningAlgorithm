// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_keystore_h
#define moorecoin_keystore_h

#include "key.h"
#include "pubkey.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"

#include <boost/signals2/signal.hpp>
#include <boost/variant.hpp>

/** a virtual base class for key stores */
class ckeystore
{
protected:
    mutable ccriticalsection cs_keystore;

public:
    virtual ~ckeystore() {}

    //! add a key to the store.
    virtual bool addkeypubkey(const ckey &key, const cpubkey &pubkey) =0;
    virtual bool addkey(const ckey &key);

    //! check whether a key corresponding to a given address is present in the store.
    virtual bool havekey(const ckeyid &address) const =0;
    virtual bool getkey(const ckeyid &address, ckey& keyout) const =0;
    virtual void getkeys(std::set<ckeyid> &setaddress) const =0;
    virtual bool getpubkey(const ckeyid &address, cpubkey& vchpubkeyout) const;

    //! support for bip 0013 : see https://github.com/moorecoin/bips/blob/master/bip-0013.mediawiki
    virtual bool addcscript(const cscript& redeemscript) =0;
    virtual bool havecscript(const cscriptid &hash) const =0;
    virtual bool getcscript(const cscriptid &hash, cscript& redeemscriptout) const =0;

    //! support for watch-only addresses
    virtual bool addwatchonly(const cscript &dest) =0;
    virtual bool removewatchonly(const cscript &dest) =0;
    virtual bool havewatchonly(const cscript &dest) const =0;
    virtual bool havewatchonly() const =0;
};

typedef std::map<ckeyid, ckey> keymap;
typedef std::map<cscriptid, cscript > scriptmap;
typedef std::set<cscript> watchonlyset;

/** basic key store, that keeps keys in an address->secret map */
class cbasickeystore : public ckeystore
{
protected:
    keymap mapkeys;
    scriptmap mapscripts;
    watchonlyset setwatchonly;

public:
    bool addkeypubkey(const ckey& key, const cpubkey &pubkey);
    bool havekey(const ckeyid &address) const
    {
        bool result;
        {
            lock(cs_keystore);
            result = (mapkeys.count(address) > 0);
        }
        return result;
    }
    void getkeys(std::set<ckeyid> &setaddress) const
    {
        setaddress.clear();
        {
            lock(cs_keystore);
            keymap::const_iterator mi = mapkeys.begin();
            while (mi != mapkeys.end())
            {
                setaddress.insert((*mi).first);
                mi++;
            }
        }
    }
    bool getkey(const ckeyid &address, ckey &keyout) const
    {
        {
            lock(cs_keystore);
            keymap::const_iterator mi = mapkeys.find(address);
            if (mi != mapkeys.end())
            {
                keyout = mi->second;
                return true;
            }
        }
        return false;
    }
    virtual bool addcscript(const cscript& redeemscript);
    virtual bool havecscript(const cscriptid &hash) const;
    virtual bool getcscript(const cscriptid &hash, cscript& redeemscriptout) const;

    virtual bool addwatchonly(const cscript &dest);
    virtual bool removewatchonly(const cscript &dest);
    virtual bool havewatchonly(const cscript &dest) const;
    virtual bool havewatchonly() const;
};

typedef std::vector<unsigned char, secure_allocator<unsigned char> > ckeyingmaterial;
typedef std::map<ckeyid, std::pair<cpubkey, std::vector<unsigned char> > > cryptedkeymap;

#endif // moorecoin_keystore_h
