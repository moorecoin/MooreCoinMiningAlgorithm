// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "bloom.h"

#include "primitives/transaction.h"
#include "hash.h"
#include "script/script.h"
#include "script/standard.h"
#include "streams.h"

#include <math.h>
#include <stdlib.h>

#include <boost/foreach.hpp>

#define ln2squared 0.4804530139182014246671025263266649717305529515945455
#define ln2 0.6931471805599453094172321214581765680755001343602552

using namespace std;

cbloomfilter::cbloomfilter(unsigned int nelements, double nfprate, unsigned int ntweakin, unsigned char nflagsin) :
    /**
     * the ideal size for a bloom filter with a given number of elements and false positive rate is:
     * - nelements * log(fp rate) / ln(2)^2
     * we ignore filter parameters which will create a bloom filter larger than the protocol limits
     */
    vdata(min((unsigned int)(-1  / ln2squared * nelements * log(nfprate)), max_bloom_filter_size * 8) / 8),
    /**
     * the ideal number of hash functions is filter size * ln(2) / number of elements
     * again, we ignore filter parameters which will create a bloom filter with more hash functions than the protocol limits
     * see https://en.wikipedia.org/wiki/bloom_filter for an explanation of these formulas
     */
    isfull(false),
    isempty(false),
    nhashfuncs(min((unsigned int)(vdata.size() * 8 / nelements * ln2), max_hash_funcs)),
    ntweak(ntweakin),
    nflags(nflagsin)
{
}

// private constructor used by crollingbloomfilter
cbloomfilter::cbloomfilter(unsigned int nelements, double nfprate, unsigned int ntweakin) :
    vdata((unsigned int)(-1  / ln2squared * nelements * log(nfprate)) / 8),
    isfull(false),
    isempty(true),
    nhashfuncs((unsigned int)(vdata.size() * 8 / nelements * ln2)),
    ntweak(ntweakin),
    nflags(bloom_update_none)
{
}

inline unsigned int cbloomfilter::hash(unsigned int nhashnum, const std::vector<unsigned char>& vdatatohash) const
{
    // 0xfba4c795 chosen as it guarantees a reasonable bit difference between nhashnum values.
    return murmurhash3(nhashnum * 0xfba4c795 + ntweak, vdatatohash) % (vdata.size() * 8);
}

void cbloomfilter::insert(const vector<unsigned char>& vkey)
{
    if (isfull)
        return;
    for (unsigned int i = 0; i < nhashfuncs; i++)
    {
        unsigned int nindex = hash(i, vkey);
        // sets bit nindex of vdata
        vdata[nindex >> 3] |= (1 << (7 & nindex));
    }
    isempty = false;
}

void cbloomfilter::insert(const coutpoint& outpoint)
{
    cdatastream stream(ser_network, protocol_version);
    stream << outpoint;
    vector<unsigned char> data(stream.begin(), stream.end());
    insert(data);
}

void cbloomfilter::insert(const uint256& hash)
{
    vector<unsigned char> data(hash.begin(), hash.end());
    insert(data);
}

bool cbloomfilter::contains(const vector<unsigned char>& vkey) const
{
    if (isfull)
        return true;
    if (isempty)
        return false;
    for (unsigned int i = 0; i < nhashfuncs; i++)
    {
        unsigned int nindex = hash(i, vkey);
        // checks bit nindex of vdata
        if (!(vdata[nindex >> 3] & (1 << (7 & nindex))))
            return false;
    }
    return true;
}

bool cbloomfilter::contains(const coutpoint& outpoint) const
{
    cdatastream stream(ser_network, protocol_version);
    stream << outpoint;
    vector<unsigned char> data(stream.begin(), stream.end());
    return contains(data);
}

bool cbloomfilter::contains(const uint256& hash) const
{
    vector<unsigned char> data(hash.begin(), hash.end());
    return contains(data);
}

void cbloomfilter::clear()
{
    vdata.assign(vdata.size(),0);
    isfull = false;
    isempty = true;
}

bool cbloomfilter::iswithinsizeconstraints() const
{
    return vdata.size() <= max_bloom_filter_size && nhashfuncs <= max_hash_funcs;
}

bool cbloomfilter::isrelevantandupdate(const ctransaction& tx)
{
    bool ffound = false;
    // match if the filter contains the hash of tx
    //  for finding tx when they appear in a block
    if (isfull)
        return true;
    if (isempty)
        return false;
    const uint256& hash = tx.gethash();
    if (contains(hash))
        ffound = true;

    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const ctxout& txout = tx.vout[i];
        // match if the filter contains any arbitrary script data element in any scriptpubkey in tx
        // if this matches, also add the specific output that was matched.
        // this means clients don't have to update the filter themselves when a new relevant tx 
        // is discovered in order to find spending transactions, which avoids round-tripping and race conditions.
        cscript::const_iterator pc = txout.scriptpubkey.begin();
        vector<unsigned char> data;
        while (pc < txout.scriptpubkey.end())
        {
            opcodetype opcode;
            if (!txout.scriptpubkey.getop(pc, opcode, data))
                break;
            if (data.size() != 0 && contains(data))
            {
                ffound = true;
                if ((nflags & bloom_update_mask) == bloom_update_all)
                    insert(coutpoint(hash, i));
                else if ((nflags & bloom_update_mask) == bloom_update_p2pubkey_only)
                {
                    txnouttype type;
                    vector<vector<unsigned char> > vsolutions;
                    if (solver(txout.scriptpubkey, type, vsolutions) &&
                            (type == tx_pubkey || type == tx_multisig))
                        insert(coutpoint(hash, i));
                }
                break;
            }
        }
    }

    if (ffound)
        return true;

    boost_foreach(const ctxin& txin, tx.vin)
    {
        // match if the filter contains an outpoint tx spends
        if (contains(txin.prevout))
            return true;

        // match if the filter contains any arbitrary script data element in any scriptsig in tx
        cscript::const_iterator pc = txin.scriptsig.begin();
        vector<unsigned char> data;
        while (pc < txin.scriptsig.end())
        {
            opcodetype opcode;
            if (!txin.scriptsig.getop(pc, opcode, data))
                break;
            if (data.size() != 0 && contains(data))
                return true;
        }
    }

    return false;
}

void cbloomfilter::updateemptyfull()
{
    bool full = true;
    bool empty = true;
    for (unsigned int i = 0; i < vdata.size(); i++)
    {
        full &= vdata[i] == 0xff;
        empty &= vdata[i] == 0;
    }
    isfull = full;
    isempty = empty;
}

crollingbloomfilter::crollingbloomfilter(unsigned int nelements, double fprate, unsigned int ntweak) :
    b1(nelements * 2, fprate, ntweak), b2(nelements * 2, fprate, ntweak)
{
    // implemented using two bloom filters of 2 * nelements each.
    // we fill them up, and clear them, staggered, every nelements
    // inserted, so at least one always contains the last nelements
    // inserted.
    nbloomsize = nelements * 2;
    ninsertions = 0;
}

void crollingbloomfilter::insert(const std::vector<unsigned char>& vkey)
{
    if (ninsertions == 0) {
        b1.clear();
    } else if (ninsertions == nbloomsize / 2) {
        b2.clear();
    }
    b1.insert(vkey);
    b2.insert(vkey);
    if (++ninsertions == nbloomsize) {
        ninsertions = 0;
    }
}

bool crollingbloomfilter::contains(const std::vector<unsigned char>& vkey) const
{
    if (ninsertions < nbloomsize / 2) {
        return b2.contains(vkey);
    }
    return b1.contains(vkey);
}

void crollingbloomfilter::clear()
{
    b1.clear();
    b2.clear();
    ninsertions = 0;
}
