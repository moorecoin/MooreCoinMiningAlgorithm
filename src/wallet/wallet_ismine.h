// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_wallet_wallet_ismine_h
#define moorecoin_wallet_wallet_ismine_h

#include "key.h"
#include "script/standard.h"

class ckeystore;
class cscript;

/** ismine() return codes */
enum isminetype
{
    ismine_no = 0,
    ismine_watch_only = 1,
    ismine_spendable = 2,
    ismine_all = ismine_watch_only | ismine_spendable
};
/** used for bitflags of isminetype */
typedef uint8_t isminefilter;

isminetype ismine(const ckeystore& keystore, const cscript& scriptpubkey);
isminetype ismine(const ckeystore& keystore, const ctxdestination& dest);

#endif // moorecoin_wallet_wallet_ismine_h
