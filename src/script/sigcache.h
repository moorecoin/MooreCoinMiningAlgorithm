// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_script_sigcache_h
#define moorecoin_script_sigcache_h

#include "script/interpreter.h"

#include <vector>

class cpubkey;

class cachingtransactionsignaturechecker : public transactionsignaturechecker
{
private:
    bool store;

public:
    cachingtransactionsignaturechecker(const ctransaction* txtoin, unsigned int ninin, bool storein=true) : transactionsignaturechecker(txtoin, ninin), store(storein) {}

    bool verifysignature(const std::vector<unsigned char>& vchsig, const cpubkey& vchpubkey, const uint256& sighash) const;
};

#endif // moorecoin_script_sigcache_h
