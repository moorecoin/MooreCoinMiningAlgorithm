// copyright (c) 2012 pieter wuille
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_addrman_h
#define moorecoin_addrman_h

#include "netbase.h"
#include "protocol.h"
#include "random.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"

#include <map>
#include <set>
#include <stdint.h>
#include <vector>

/**
 * extended statistics about a caddress
 */
class caddrinfo : public caddress
{
public:
    //! last try whatsoever by us (memory only)
    int64_t nlasttry;

private:
    //! where knowledge about this address first came from
    cnetaddr source;

    //! last successful connection by us
    int64_t nlastsuccess;

    //! connection attempts since last successful attempt
    int nattempts;

    //! reference count in new sets (memory only)
    int nrefcount;

    //! in tried set? (memory only)
    bool fintried;

    //! position in vrandom
    int nrandompos;

    friend class caddrman;

public:

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(*(caddress*)this);
        readwrite(source);
        readwrite(nlastsuccess);
        readwrite(nattempts);
    }

    void init()
    {
        nlastsuccess = 0;
        nlasttry = 0;
        nattempts = 0;
        nrefcount = 0;
        fintried = false;
        nrandompos = -1;
    }

    caddrinfo(const caddress &addrin, const cnetaddr &addrsource) : caddress(addrin), source(addrsource)
    {
        init();
    }

    caddrinfo() : caddress(), source()
    {
        init();
    }

    //! calculate in which "tried" bucket this entry belongs
    int gettriedbucket(const uint256 &nkey) const;

    //! calculate in which "new" bucket this entry belongs, given a certain source
    int getnewbucket(const uint256 &nkey, const cnetaddr& src) const;

    //! calculate in which "new" bucket this entry belongs, using its default source
    int getnewbucket(const uint256 &nkey) const
    {
        return getnewbucket(nkey, source);
    }

    //! calculate in which position of a bucket to store this entry.
    int getbucketposition(const uint256 &nkey, bool fnew, int nbucket) const;

    //! determine whether the statistics about this entry are bad enough so that it can just be deleted
    bool isterrible(int64_t nnow = getadjustedtime()) const;

    //! calculate the relative chance this entry should be given when selecting nodes to connect to
    double getchance(int64_t nnow = getadjustedtime()) const;

};

/** stochastic address manager
 *
 * design goals:
 *  * keep the address tables in-memory, and asynchronously dump the entire table to peers.dat.
 *  * make sure no (localized) attacker can fill the entire table with his nodes/addresses.
 *
 * to that end:
 *  * addresses are organized into buckets.
 *    * addresses that have not yet been tried go into 1024 "new" buckets.
 *      * based on the address range (/16 for ipv4) of the source of information, 64 buckets are selected at random.
 *      * the actual bucket is chosen from one of these, based on the range in which the address itself is located.
 *      * one single address can occur in up to 8 different buckets to increase selection chances for addresses that
 *        are seen frequently. the chance for increasing this multiplicity decreases exponentially.
 *      * when adding a new address to a full bucket, a randomly chosen entry (with a bias favoring less recently seen
 *        ones) is removed from it first.
 *    * addresses of nodes that are known to be accessible go into 256 "tried" buckets.
 *      * each address range selects at random 8 of these buckets.
 *      * the actual bucket is chosen from one of these, based on the full address.
 *      * when adding a new good address to a full bucket, a randomly chosen entry (with a bias favoring less recently
 *        tried ones) is evicted from it, back to the "new" buckets.
 *    * bucket selection is based on cryptographic hashing, using a randomly-generated 256-bit key, which should not
 *      be observable by adversaries.
 *    * several indexes are kept for high performance. defining debug_addrman will introduce frequent (and expensive)
 *      consistency checks for the entire data structure.
 */

//! total number of buckets for tried addresses
#define addrman_tried_bucket_count 256

//! total number of buckets for new addresses
#define addrman_new_bucket_count 1024

//! maximum allowed number of entries in buckets for new and tried addresses
#define addrman_bucket_size 64

//! over how many buckets entries with tried addresses from a single group (/16 for ipv4) are spread
#define addrman_tried_buckets_per_group 8

//! over how many buckets entries with new addresses originating from a single group are spread
#define addrman_new_buckets_per_source_group 64

//! in how many buckets for entries with new addresses a single address may occur
#define addrman_new_buckets_per_address 8

//! how old addresses can maximally be
#define addrman_horizon_days 30

//! after how many failed attempts we give up on a new node
#define addrman_retries 3

//! how many successive failures are allowed ...
#define addrman_max_failures 10

//! ... in at least this many days
#define addrman_min_fail_days 7

//! the maximum percentage of nodes to return in a getaddr call
#define addrman_getaddr_max_pct 23

//! the maximum number of nodes to return in a getaddr call
#define addrman_getaddr_max 2500

/** 
 * stochastical (ip) address manager 
 */
class caddrman
{
private:
    //! critical section to protect the inner data structures
    mutable ccriticalsection cs;

    //! secret key to randomize bucket select with
    uint256 nkey;

    //! last used nid
    int nidcount;

    //! table with information about all nids
    std::map<int, caddrinfo> mapinfo;

    //! find an nid based on its network address
    std::map<cnetaddr, int> mapaddr;

    //! randomly-ordered vector of all nids
    std::vector<int> vrandom;

    // number of "tried" entries
    int ntried;

    //! list of "tried" buckets
    int vvtried[addrman_tried_bucket_count][addrman_bucket_size];

    //! number of (unique) "new" entries
    int nnew;

    //! list of "new" buckets
    int vvnew[addrman_new_bucket_count][addrman_bucket_size];

protected:

    //! find an entry.
    caddrinfo* find(const cnetaddr& addr, int *pnid = null);

    //! find an entry, creating it if necessary.
    //! ntime and nservices of the found node are updated, if necessary.
    caddrinfo* create(const caddress &addr, const cnetaddr &addrsource, int *pnid = null);

    //! swap two elements in vrandom.
    void swaprandom(unsigned int nrandompos1, unsigned int nrandompos2);

    //! move an entry from the "new" table(s) to the "tried" table
    void maketried(caddrinfo& info, int nid);

    //! delete an entry. it must not be in tried, and have refcount 0.
    void delete(int nid);

    //! clear a position in a "new" table. this is the only place where entries are actually deleted.
    void clearnew(int nubucket, int nubucketpos);

    //! mark an entry "good", possibly moving it from "new" to "tried".
    void good_(const cservice &addr, int64_t ntime);

    //! add an entry to the "new" table.
    bool add_(const caddress &addr, const cnetaddr& source, int64_t ntimepenalty);

    //! mark an entry as attempted to connect.
    void attempt_(const cservice &addr, int64_t ntime);

    //! select an address to connect to.
    caddrinfo select_();

#ifdef debug_addrman
    //! perform consistency check. returns an error code or zero.
    int check_();
#endif

    //! select several addresses at once.
    void getaddr_(std::vector<caddress> &vaddr);

    //! mark an entry as currently-connected-to.
    void connected_(const cservice &addr, int64_t ntime);

public:
    /**
     * serialized format:
     * * version byte (currently 1)
     * * 0x20 + nkey (serialized as if it were a vector, for backward compatibility)
     * * nnew
     * * ntried
     * * number of "new" buckets xor 2**30
     * * all nnew addrinfos in vvnew
     * * all ntried addrinfos in vvtried
     * * for each bucket:
     *   * number of elements
     *   * for each element: index
     *
     * 2**30 is xorred with the number of buckets to make addrman deserializer v0 detect it
     * as incompatible. this is necessary because it did not check the version number on
     * deserialization.
     *
     * notice that vvtried, mapaddr and vvector are never encoded explicitly;
     * they are instead reconstructed from the other information.
     *
     * vvnew is serialized, but only used if addrman_unkown_bucket_count didn't change,
     * otherwise it is reconstructed as well.
     *
     * this format is more complex, but significantly smaller (at most 1.5 mib), and supports
     * changes to the addrman_ parameters without breaking the on-disk structure.
     *
     * we don't use add_serialize_methods since the serialization and deserialization code has
     * very little in common.
     */
    template<typename stream>
    void serialize(stream &s, int ntype, int nversiondummy) const
    {
        lock(cs);

        unsigned char nversion = 1;
        s << nversion;
        s << ((unsigned char)32);
        s << nkey;
        s << nnew;
        s << ntried;

        int nubuckets = addrman_new_bucket_count ^ (1 << 30);
        s << nubuckets;
        std::map<int, int> mapunkids;
        int nids = 0;
        for (std::map<int, caddrinfo>::const_iterator it = mapinfo.begin(); it != mapinfo.end(); it++) {
            mapunkids[(*it).first] = nids;
            const caddrinfo &info = (*it).second;
            if (info.nrefcount) {
                assert(nids != nnew); // this means nnew was wrong, oh ow
                s << info;
                nids++;
            }
        }
        nids = 0;
        for (std::map<int, caddrinfo>::const_iterator it = mapinfo.begin(); it != mapinfo.end(); it++) {
            const caddrinfo &info = (*it).second;
            if (info.fintried) {
                assert(nids != ntried); // this means ntried was wrong, oh ow
                s << info;
                nids++;
            }
        }
        for (int bucket = 0; bucket < addrman_new_bucket_count; bucket++) {
            int nsize = 0;
            for (int i = 0; i < addrman_bucket_size; i++) {
                if (vvnew[bucket][i] != -1)
                    nsize++;
            }
            s << nsize;
            for (int i = 0; i < addrman_bucket_size; i++) {
                if (vvnew[bucket][i] != -1) {
                    int nindex = mapunkids[vvnew[bucket][i]];
                    s << nindex;
                }
            }
        }
    }

    template<typename stream>
    void unserialize(stream& s, int ntype, int nversiondummy)
    {
        lock(cs);

        clear();

        unsigned char nversion;
        s >> nversion;
        unsigned char nkeysize;
        s >> nkeysize;
        if (nkeysize != 32) throw std::ios_base::failure("incorrect keysize in addrman deserialization");
        s >> nkey;
        s >> nnew;
        s >> ntried;
        int nubuckets = 0;
        s >> nubuckets;
        if (nversion != 0) {
            nubuckets ^= (1 << 30);
        }

        // deserialize entries from the new table.
        for (int n = 0; n < nnew; n++) {
            caddrinfo &info = mapinfo[n];
            s >> info;
            mapaddr[info] = n;
            info.nrandompos = vrandom.size();
            vrandom.push_back(n);
            if (nversion != 1 || nubuckets != addrman_new_bucket_count) {
                // in case the new table data cannot be used (nversion unknown, or bucket count wrong),
                // immediately try to give them a reference based on their primary source address.
                int nubucket = info.getnewbucket(nkey);
                int nubucketpos = info.getbucketposition(nkey, true, nubucket);
                if (vvnew[nubucket][nubucketpos] == -1) {
                    vvnew[nubucket][nubucketpos] = n;
                    info.nrefcount++;
                }
            }
        }
        nidcount = nnew;

        // deserialize entries from the tried table.
        int nlost = 0;
        for (int n = 0; n < ntried; n++) {
            caddrinfo info;
            s >> info;
            int nkbucket = info.gettriedbucket(nkey);
            int nkbucketpos = info.getbucketposition(nkey, false, nkbucket);
            if (vvtried[nkbucket][nkbucketpos] == -1) {
                info.nrandompos = vrandom.size();
                info.fintried = true;
                vrandom.push_back(nidcount);
                mapinfo[nidcount] = info;
                mapaddr[info] = nidcount;
                vvtried[nkbucket][nkbucketpos] = nidcount;
                nidcount++;
            } else {
                nlost++;
            }
        }
        ntried -= nlost;

        // deserialize positions in the new table (if possible).
        for (int bucket = 0; bucket < nubuckets; bucket++) {
            int nsize = 0;
            s >> nsize;
            for (int n = 0; n < nsize; n++) {
                int nindex = 0;
                s >> nindex;
                if (nindex >= 0 && nindex < nnew) {
                    caddrinfo &info = mapinfo[nindex];
                    int nubucketpos = info.getbucketposition(nkey, true, bucket);
                    if (nversion == 1 && nubuckets == addrman_new_bucket_count && vvnew[bucket][nubucketpos] == -1 && info.nrefcount < addrman_new_buckets_per_address) {
                        info.nrefcount++;
                        vvnew[bucket][nubucketpos] = nindex;
                    }
                }
            }
        }

        // prune new entries with refcount 0 (as a result of collisions).
        int nlostunk = 0;
        for (std::map<int, caddrinfo>::const_iterator it = mapinfo.begin(); it != mapinfo.end(); ) {
            if (it->second.fintried == false && it->second.nrefcount == 0) {
                std::map<int, caddrinfo>::const_iterator itcopy = it++;
                delete(itcopy->first);
                nlostunk++;
            } else {
                it++;
            }
        }
        if (nlost + nlostunk > 0) {
            logprint("addrman", "addrman lost %i new and %i tried addresses due to collisions\n", nlostunk, nlost);
        }

        check();
    }

    unsigned int getserializesize(int ntype, int nversion) const
    {
        return (csizecomputer(ntype, nversion) << *this).size();
    }

    void clear()
    {
        std::vector<int>().swap(vrandom);
        nkey = getrandhash();
        for (size_t bucket = 0; bucket < addrman_new_bucket_count; bucket++) {
            for (size_t entry = 0; entry < addrman_bucket_size; entry++) {
                vvnew[bucket][entry] = -1;
            }
        }
        for (size_t bucket = 0; bucket < addrman_tried_bucket_count; bucket++) {
            for (size_t entry = 0; entry < addrman_bucket_size; entry++) {
                vvtried[bucket][entry] = -1;
            }
        }

        nidcount = 0;
        ntried = 0;
        nnew = 0;
    }

    caddrman()
    {
        clear();
    }

    ~caddrman()
    {
        nkey.setnull();
    }

    //! return the number of (unique) addresses in all tables.
    size_t size() const
    {
        return vrandom.size();
    }

    //! consistency check
    void check()
    {
#ifdef debug_addrman
        {
            lock(cs);
            int err;
            if ((err=check_()))
                logprintf("addrman consistency check failed!!! err=%i\n", err);
        }
#endif
    }

    //! add a single address.
    bool add(const caddress &addr, const cnetaddr& source, int64_t ntimepenalty = 0)
    {
        bool fret = false;
        {
            lock(cs);
            check();
            fret |= add_(addr, source, ntimepenalty);
            check();
        }
        if (fret)
            logprint("addrman", "added %s from %s: %i tried, %i new\n", addr.tostringipport(), source.tostring(), ntried, nnew);
        return fret;
    }

    //! add multiple addresses.
    bool add(const std::vector<caddress> &vaddr, const cnetaddr& source, int64_t ntimepenalty = 0)
    {
        int nadd = 0;
        {
            lock(cs);
            check();
            for (std::vector<caddress>::const_iterator it = vaddr.begin(); it != vaddr.end(); it++)
                nadd += add_(*it, source, ntimepenalty) ? 1 : 0;
            check();
        }
        if (nadd)
            logprint("addrman", "added %i addresses from %s: %i tried, %i new\n", nadd, source.tostring(), ntried, nnew);
        return nadd > 0;
    }

    //! mark an entry as accessible.
    void good(const cservice &addr, int64_t ntime = getadjustedtime())
    {
        {
            lock(cs);
            check();
            good_(addr, ntime);
            check();
        }
    }

    //! mark an entry as connection attempted to.
    void attempt(const cservice &addr, int64_t ntime = getadjustedtime())
    {
        {
            lock(cs);
            check();
            attempt_(addr, ntime);
            check();
        }
    }

    /**
     * choose an address to connect to.
     */
    caddrinfo select()
    {
        caddrinfo addrret;
        {
            lock(cs);
            check();
            addrret = select_();
            check();
        }
        return addrret;
    }

    //! return a bunch of addresses, selected at random.
    std::vector<caddress> getaddr()
    {
        check();
        std::vector<caddress> vaddr;
        {
            lock(cs);
            getaddr_(vaddr);
        }
        check();
        return vaddr;
    }

    //! mark an entry as currently-connected-to.
    void connected(const cservice &addr, int64_t ntime = getadjustedtime())
    {
        {
            lock(cs);
            check();
            connected_(addr, ntime);
            check();
        }
    }
};

#endif // moorecoin_addrman_h
