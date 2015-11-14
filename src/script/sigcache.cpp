// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "sigcache.h"

#include "pubkey.h"
#include "random.h"
#include "uint256.h"
#include "util.h"

#include <boost/thread.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace {

/**
 * valid signature cache, to avoid doing expensive ecdsa signature checking
 * twice for every transaction (once when accepted into memory pool, and
 * again when accepted into the block chain)
 */
class csignaturecache
{
private:
     //! sigdata_type is (signature hash, signature, public key):
    typedef boost::tuple<uint256, std::vector<unsigned char>, cpubkey> sigdata_type;
    std::set< sigdata_type> setvalid;
    boost::shared_mutex cs_sigcache;

public:
    bool
    get(const uint256 &hash, const std::vector<unsigned char>& vchsig, const cpubkey& pubkey)
    {
        boost::shared_lock<boost::shared_mutex> lock(cs_sigcache);

        sigdata_type k(hash, vchsig, pubkey);
        std::set<sigdata_type>::iterator mi = setvalid.find(k);
        if (mi != setvalid.end())
            return true;
        return false;
    }

    void set(const uint256 &hash, const std::vector<unsigned char>& vchsig, const cpubkey& pubkey)
    {
        // dos prevention: limit cache size to less than 10mb
        // (~200 bytes per cache entry times 50,000 entries)
        // since there are a maximum of 20,000 signature operations per block
        // 50,000 is a reasonable default.
        int64_t nmaxcachesize = getarg("-maxsigcachesize", 50000);
        if (nmaxcachesize <= 0) return;

        boost::unique_lock<boost::shared_mutex> lock(cs_sigcache);

        while (static_cast<int64_t>(setvalid.size()) > nmaxcachesize)
        {
            // evict a random entry. random because that helps
            // foil would-be dos attackers who might try to pre-generate
            // and re-use a set of valid signatures just-slightly-greater
            // than our cache size.
            uint256 randomhash = getrandhash();
            std::vector<unsigned char> unused;
            std::set<sigdata_type>::iterator it =
                setvalid.lower_bound(sigdata_type(randomhash, unused, unused));
            if (it == setvalid.end())
                it = setvalid.begin();
            setvalid.erase(*it);
        }

        sigdata_type k(hash, vchsig, pubkey);
        setvalid.insert(k);
    }
};

}

bool cachingtransactionsignaturechecker::verifysignature(const std::vector<unsigned char>& vchsig, const cpubkey& pubkey, const uint256& sighash) const
{
    static csignaturecache signaturecache;

    if (signaturecache.get(sighash, vchsig, pubkey))
        return true;

    if (!transactionsignaturechecker::verifysignature(vchsig, pubkey, sighash))
        return false;

    if (store)
        signaturecache.set(sighash, vchsig, pubkey);
    return true;
}
