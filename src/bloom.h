// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_bloom_h
#define moorecoin_bloom_h

#include "serialize.h"

#include <vector>

class coutpoint;
class ctransaction;
class uint256;

//! 20,000 items with fp rate < 0.1% or 10,000 items and <0.0001%
static const unsigned int max_bloom_filter_size = 36000; // bytes
static const unsigned int max_hash_funcs = 50;

/**
 * first two bits of nflags control how much isrelevantandupdate actually updates
 * the remaining bits are reserved
 */
enum bloomflags
{
    bloom_update_none = 0,
    bloom_update_all = 1,
    // only adds outpoints to the filter if the output is a pay-to-pubkey/pay-to-multisig script
    bloom_update_p2pubkey_only = 2,
    bloom_update_mask = 3,
};

/**
 * bloomfilter is a probabilistic filter which spv clients provide
 * so that we can filter the transactions we send them.
 * 
 * this allows for significantly more efficient transaction and block downloads.
 * 
 * because bloom filters are probabilistic, a spv node can increase the false-
 * positive rate, making us send it transactions which aren't actually its,
 * allowing clients to trade more bandwidth for more privacy by obfuscating which
 * keys are controlled by them.
 */
class cbloomfilter
{
private:
    std::vector<unsigned char> vdata;
    bool isfull;
    bool isempty;
    unsigned int nhashfuncs;
    unsigned int ntweak;
    unsigned char nflags;

    unsigned int hash(unsigned int nhashnum, const std::vector<unsigned char>& vdatatohash) const;

    // private constructor for crollingbloomfilter, no restrictions on size
    cbloomfilter(unsigned int nelements, double nfprate, unsigned int ntweak);
    friend class crollingbloomfilter;

public:
    /**
     * creates a new bloom filter which will provide the given fp rate when filled with the given number of elements
     * note that if the given parameters will result in a filter outside the bounds of the protocol limits,
     * the filter created will be as close to the given parameters as possible within the protocol limits.
     * this will apply if nfprate is very low or nelements is unreasonably high.
     * ntweak is a constant which is added to the seed value passed to the hash function
     * it should generally always be a random value (and is largely only exposed for unit testing)
     * nflags should be one of the bloom_update_* enums (not _mask)
     */
    cbloomfilter(unsigned int nelements, double nfprate, unsigned int ntweak, unsigned char nflagsin);
    cbloomfilter() : isfull(true), isempty(false), nhashfuncs(0), ntweak(0), nflags(0) {}

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(vdata);
        readwrite(nhashfuncs);
        readwrite(ntweak);
        readwrite(nflags);
    }

    void insert(const std::vector<unsigned char>& vkey);
    void insert(const coutpoint& outpoint);
    void insert(const uint256& hash);

    bool contains(const std::vector<unsigned char>& vkey) const;
    bool contains(const coutpoint& outpoint) const;
    bool contains(const uint256& hash) const;

    void clear();

    //! true if the size is <= max_bloom_filter_size and the number of hash functions is <= max_hash_funcs
    //! (catch a filter which was just deserialized which was too big)
    bool iswithinsizeconstraints() const;

    //! also adds any outputs which match the filter to the filter (to match their spending txes)
    bool isrelevantandupdate(const ctransaction& tx);

    //! checks for empty and full filters to avoid wasting cpu
    void updateemptyfull();
};

/**
 * rollingbloomfilter is a probabilistic "keep track of most recently inserted" set.
 * construct it with the number of items to keep track of, and a false-positive rate.
 *
 * contains(item) will always return true if item was one of the last n things
 * insert()'ed ... but may also return true for items that were not inserted.
 */
class crollingbloomfilter
{
public:
    crollingbloomfilter(unsigned int nelements, double nfprate, unsigned int ntweak);

    void insert(const std::vector<unsigned char>& vkey);
    bool contains(const std::vector<unsigned char>& vkey) const;

    void clear();

private:
    unsigned int nbloomsize;
    unsigned int ninsertions;
    cbloomfilter b1, b2;
};


#endif // moorecoin_bloom_h
