// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "keystore.h"

#include "key.h"
#include "util.h"

#include <boost/foreach.hpp>

bool ckeystore::getpubkey(const ckeyid &address, cpubkey &vchpubkeyout) const
{
    ckey key;
    if (!getkey(address, key))
        return false;
    vchpubkeyout = key.getpubkey();
    return true;
}

bool ckeystore::addkey(const ckey &key) {
    return addkeypubkey(key, key.getpubkey());
}

bool cbasickeystore::addkeypubkey(const ckey& key, const cpubkey &pubkey)
{
    lock(cs_keystore);
    mapkeys[pubkey.getid()] = key;
    return true;
}

bool cbasickeystore::addcscript(const cscript& redeemscript)
{
    if (redeemscript.size() > max_script_element_size)
        return error("cbasickeystore::addcscript(): redeemscripts > %i bytes are invalid", max_script_element_size);

    lock(cs_keystore);
    mapscripts[cscriptid(redeemscript)] = redeemscript;
    return true;
}

bool cbasickeystore::havecscript(const cscriptid& hash) const
{
    lock(cs_keystore);
    return mapscripts.count(hash) > 0;
}

bool cbasickeystore::getcscript(const cscriptid &hash, cscript& redeemscriptout) const
{
    lock(cs_keystore);
    scriptmap::const_iterator mi = mapscripts.find(hash);
    if (mi != mapscripts.end())
    {
        redeemscriptout = (*mi).second;
        return true;
    }
    return false;
}

bool cbasickeystore::addwatchonly(const cscript &dest)
{
    lock(cs_keystore);
    setwatchonly.insert(dest);
    return true;
}

bool cbasickeystore::removewatchonly(const cscript &dest)
{
    lock(cs_keystore);
    setwatchonly.erase(dest);
    return true;
}

bool cbasickeystore::havewatchonly(const cscript &dest) const
{
    lock(cs_keystore);
    return setwatchonly.count(dest) > 0;
}

bool cbasickeystore::havewatchonly() const
{
    lock(cs_keystore);
    return (!setwatchonly.empty());
}
