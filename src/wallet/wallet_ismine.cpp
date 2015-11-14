// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "wallet_ismine.h"

#include "key.h"
#include "keystore.h"
#include "script/script.h"
#include "script/standard.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned int havekeys(const vector<valtype>& pubkeys, const ckeystore& keystore)
{
    unsigned int nresult = 0;
    boost_foreach(const valtype& pubkey, pubkeys)
    {
        ckeyid keyid = cpubkey(pubkey).getid();
        if (keystore.havekey(keyid))
            ++nresult;
    }
    return nresult;
}

isminetype ismine(const ckeystore &keystore, const ctxdestination& dest)
{
    cscript script = getscriptfordestination(dest);
    return ismine(keystore, script);
}

isminetype ismine(const ckeystore &keystore, const cscript& scriptpubkey)
{
    vector<valtype> vsolutions;
    txnouttype whichtype;
    if (!solver(scriptpubkey, whichtype, vsolutions)) {
        if (keystore.havewatchonly(scriptpubkey))
            return ismine_watch_only;
        return ismine_no;
    }

    ckeyid keyid;
    switch (whichtype)
    {
    case tx_nonstandard:
    case tx_null_data:
        break;
    case tx_pubkey:
        keyid = cpubkey(vsolutions[0]).getid();
        if (keystore.havekey(keyid))
            return ismine_spendable;
        break;
    case tx_pubkeyhash:
        keyid = ckeyid(uint160(vsolutions[0]));
        if (keystore.havekey(keyid))
            return ismine_spendable;
        break;
    case tx_scripthash:
    {
        cscriptid scriptid = cscriptid(uint160(vsolutions[0]));
        cscript subscript;
        if (keystore.getcscript(scriptid, subscript)) {
            isminetype ret = ismine(keystore, subscript);
            if (ret == ismine_spendable)
                return ret;
        }
        break;
    }
    case tx_multisig:
    {
        // only consider transactions "mine" if we own all the
        // keys involved. multi-signature transactions that are
        // partially owned (somebody else has a key that can spend
        // them) enable spend-out-from-under-you attacks, especially
        // in shared-wallet situations.
        vector<valtype> keys(vsolutions.begin()+1, vsolutions.begin()+vsolutions.size()-1);
        if (havekeys(keys, keystore) == keys.size())
            return ismine_spendable;
        break;
    }
    }

    if (keystore.havewatchonly(scriptpubkey))
        return ismine_watch_only;
    return ismine_no;
}
