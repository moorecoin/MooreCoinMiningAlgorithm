// copyright (c) 2012 pieter wuille
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "addrman.h"

#include "hash.h"
#include "serialize.h"
#include "streams.h"

int caddrinfo::gettriedbucket(const uint256& nkey) const
{
    uint64_t hash1 = (chashwriter(ser_gethash, 0) << nkey << getkey()).gethash().getcheaphash();
    uint64_t hash2 = (chashwriter(ser_gethash, 0) << nkey << getgroup() << (hash1 % addrman_tried_buckets_per_group)).gethash().getcheaphash();
    return hash2 % addrman_tried_bucket_count;
}

int caddrinfo::getnewbucket(const uint256& nkey, const cnetaddr& src) const
{
    std::vector<unsigned char> vchsourcegroupkey = src.getgroup();
    uint64_t hash1 = (chashwriter(ser_gethash, 0) << nkey << getgroup() << vchsourcegroupkey).gethash().getcheaphash();
    uint64_t hash2 = (chashwriter(ser_gethash, 0) << nkey << vchsourcegroupkey << (hash1 % addrman_new_buckets_per_source_group)).gethash().getcheaphash();
    return hash2 % addrman_new_bucket_count;
}

int caddrinfo::getbucketposition(const uint256 &nkey, bool fnew, int nbucket) const
{
    uint64_t hash1 = (chashwriter(ser_gethash, 0) << nkey << (fnew ? 'n' : 'k') << nbucket << getkey()).gethash().getcheaphash();
    return hash1 % addrman_bucket_size;
}

bool caddrinfo::isterrible(int64_t nnow) const
{
    if (nlasttry && nlasttry >= nnow - 60) // never remove things tried in the last minute
        return false;

    if (ntime > nnow + 10 * 60) // came in a flying delorean
        return true;

    if (ntime == 0 || nnow - ntime > addrman_horizon_days * 24 * 60 * 60) // not seen in recent history
        return true;

    if (nlastsuccess == 0 && nattempts >= addrman_retries) // tried n times and never a success
        return true;

    if (nnow - nlastsuccess > addrman_min_fail_days * 24 * 60 * 60 && nattempts >= addrman_max_failures) // n successive failures in the last week
        return true;

    return false;
}

double caddrinfo::getchance(int64_t nnow) const
{
    double fchance = 1.0;

    int64_t nsincelastseen = nnow - ntime;
    int64_t nsincelasttry = nnow - nlasttry;

    if (nsincelastseen < 0)
        nsincelastseen = 0;
    if (nsincelasttry < 0)
        nsincelasttry = 0;

    // deprioritize very recent attempts away
    if (nsincelasttry < 60 * 10)
        fchance *= 0.01;

    // deprioritize 66% after each failed attempt, but at most 1/28th to avoid the search taking forever or overly penalizing outages.
    fchance *= pow(0.66, std::min(nattempts, 8));

    return fchance;
}

caddrinfo* caddrman::find(const cnetaddr& addr, int* pnid)
{
    std::map<cnetaddr, int>::iterator it = mapaddr.find(addr);
    if (it == mapaddr.end())
        return null;
    if (pnid)
        *pnid = (*it).second;
    std::map<int, caddrinfo>::iterator it2 = mapinfo.find((*it).second);
    if (it2 != mapinfo.end())
        return &(*it2).second;
    return null;
}

caddrinfo* caddrman::create(const caddress& addr, const cnetaddr& addrsource, int* pnid)
{
    int nid = nidcount++;
    mapinfo[nid] = caddrinfo(addr, addrsource);
    mapaddr[addr] = nid;
    mapinfo[nid].nrandompos = vrandom.size();
    vrandom.push_back(nid);
    if (pnid)
        *pnid = nid;
    return &mapinfo[nid];
}

void caddrman::swaprandom(unsigned int nrndpos1, unsigned int nrndpos2)
{
    if (nrndpos1 == nrndpos2)
        return;

    assert(nrndpos1 < vrandom.size() && nrndpos2 < vrandom.size());

    int nid1 = vrandom[nrndpos1];
    int nid2 = vrandom[nrndpos2];

    assert(mapinfo.count(nid1) == 1);
    assert(mapinfo.count(nid2) == 1);

    mapinfo[nid1].nrandompos = nrndpos2;
    mapinfo[nid2].nrandompos = nrndpos1;

    vrandom[nrndpos1] = nid2;
    vrandom[nrndpos2] = nid1;
}

void caddrman::delete(int nid)
{
    assert(mapinfo.count(nid) != 0);
    caddrinfo& info = mapinfo[nid];
    assert(!info.fintried);
    assert(info.nrefcount == 0);

    swaprandom(info.nrandompos, vrandom.size() - 1);
    vrandom.pop_back();
    mapaddr.erase(info);
    mapinfo.erase(nid);
    nnew--;
}

void caddrman::clearnew(int nubucket, int nubucketpos)
{
    // if there is an entry in the specified bucket, delete it.
    if (vvnew[nubucket][nubucketpos] != -1) {
        int niddelete = vvnew[nubucket][nubucketpos];
        caddrinfo& infodelete = mapinfo[niddelete];
        assert(infodelete.nrefcount > 0);
        infodelete.nrefcount--;
        vvnew[nubucket][nubucketpos] = -1;
        if (infodelete.nrefcount == 0) {
            delete(niddelete);
        }
    }
}

void caddrman::maketried(caddrinfo& info, int nid)
{
    // remove the entry from all new buckets
    for (int bucket = 0; bucket < addrman_new_bucket_count; bucket++) {
        int pos = info.getbucketposition(nkey, true, bucket);
        if (vvnew[bucket][pos] == nid) {
            vvnew[bucket][pos] = -1;
            info.nrefcount--;
        }
    }
    nnew--;

    assert(info.nrefcount == 0);

    // which tried bucket to move the entry to
    int nkbucket = info.gettriedbucket(nkey);
    int nkbucketpos = info.getbucketposition(nkey, false, nkbucket);

    // first make space to add it (the existing tried entry there is moved to new, deleting whatever is there).
    if (vvtried[nkbucket][nkbucketpos] != -1) {
        // find an item to evict
        int nidevict = vvtried[nkbucket][nkbucketpos];
        assert(mapinfo.count(nidevict) == 1);
        caddrinfo& infoold = mapinfo[nidevict];

        // remove the to-be-evicted item from the tried set.
        infoold.fintried = false;
        vvtried[nkbucket][nkbucketpos] = -1;
        ntried--;

        // find which new bucket it belongs to
        int nubucket = infoold.getnewbucket(nkey);
        int nubucketpos = infoold.getbucketposition(nkey, true, nubucket);
        clearnew(nubucket, nubucketpos);
        assert(vvnew[nubucket][nubucketpos] == -1);

        // enter it into the new set again.
        infoold.nrefcount = 1;
        vvnew[nubucket][nubucketpos] = nidevict;
        nnew++;
    }
    assert(vvtried[nkbucket][nkbucketpos] == -1);

    vvtried[nkbucket][nkbucketpos] = nid;
    ntried++;
    info.fintried = true;
}

void caddrman::good_(const cservice& addr, int64_t ntime)
{
    int nid;
    caddrinfo* pinfo = find(addr, &nid);

    // if not found, bail out
    if (!pinfo)
        return;

    caddrinfo& info = *pinfo;

    // check whether we are talking about the exact same cservice (including same port)
    if (info != addr)
        return;

    // update info
    info.nlastsuccess = ntime;
    info.nlasttry = ntime;
    info.nattempts = 0;
    // ntime is not updated here, to avoid leaking information about
    // currently-connected peers.

    // if it is already in the tried set, don't do anything else
    if (info.fintried)
        return;

    // find a bucket it is in now
    int nrnd = getrandint(addrman_new_bucket_count);
    int nubucket = -1;
    for (unsigned int n = 0; n < addrman_new_bucket_count; n++) {
        int nb = (n + nrnd) % addrman_new_bucket_count;
        int nbpos = info.getbucketposition(nkey, true, nb);
        if (vvnew[nb][nbpos] == nid) {
            nubucket = nb;
            break;
        }
    }

    // if no bucket is found, something bad happened;
    // todo: maybe re-add the node, but for now, just bail out
    if (nubucket == -1)
        return;

    logprint("addrman", "moving %s to tried\n", addr.tostring());

    // move nid to the tried tables
    maketried(info, nid);
}

bool caddrman::add_(const caddress& addr, const cnetaddr& source, int64_t ntimepenalty)
{
    if (!addr.isroutable())
        return false;

    bool fnew = false;
    int nid;
    caddrinfo* pinfo = find(addr, &nid);

    if (pinfo) {
        // periodically update ntime
        bool fcurrentlyonline = (getadjustedtime() - addr.ntime < 24 * 60 * 60);
        int64_t nupdateinterval = (fcurrentlyonline ? 60 * 60 : 24 * 60 * 60);
        if (addr.ntime && (!pinfo->ntime || pinfo->ntime < addr.ntime - nupdateinterval - ntimepenalty))
            pinfo->ntime = std::max((int64_t)0, addr.ntime - ntimepenalty);

        // add services
        pinfo->nservices |= addr.nservices;

        // do not update if no new information is present
        if (!addr.ntime || (pinfo->ntime && addr.ntime <= pinfo->ntime))
            return false;

        // do not update if the entry was already in the "tried" table
        if (pinfo->fintried)
            return false;

        // do not update if the max reference count is reached
        if (pinfo->nrefcount == addrman_new_buckets_per_address)
            return false;

        // stochastic test: previous nrefcount == n: 2^n times harder to increase it
        int nfactor = 1;
        for (int n = 0; n < pinfo->nrefcount; n++)
            nfactor *= 2;
        if (nfactor > 1 && (getrandint(nfactor) != 0))
            return false;
    } else {
        pinfo = create(addr, source, &nid);
        pinfo->ntime = std::max((int64_t)0, (int64_t)pinfo->ntime - ntimepenalty);
        nnew++;
        fnew = true;
    }

    int nubucket = pinfo->getnewbucket(nkey, source);
    int nubucketpos = pinfo->getbucketposition(nkey, true, nubucket);
    if (vvnew[nubucket][nubucketpos] != nid) {
        bool finsert = vvnew[nubucket][nubucketpos] == -1;
        if (!finsert) {
            caddrinfo& infoexisting = mapinfo[vvnew[nubucket][nubucketpos]];
            if (infoexisting.isterrible() || (infoexisting.nrefcount > 1 && pinfo->nrefcount == 0)) {
                // overwrite the existing new table entry.
                finsert = true;
            }
        }
        if (finsert) {
            clearnew(nubucket, nubucketpos);
            pinfo->nrefcount++;
            vvnew[nubucket][nubucketpos] = nid;
        } else {
            if (pinfo->nrefcount == 0) {
                delete(nid);
            }
        }
    }
    return fnew;
}

void caddrman::attempt_(const cservice& addr, int64_t ntime)
{
    caddrinfo* pinfo = find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    caddrinfo& info = *pinfo;

    // check whether we are talking about the exact same cservice (including same port)
    if (info != addr)
        return;

    // update info
    info.nlasttry = ntime;
    info.nattempts++;
}

caddrinfo caddrman::select_()
{
    if (size() == 0)
        return caddrinfo();

    // use a 50% chance for choosing between tried and new table entries.
    if (ntried > 0 && (nnew == 0 || getrandint(2) == 0)) {
        // use a tried node
        double fchancefactor = 1.0;
        while (1) {
            int nkbucket = getrandint(addrman_tried_bucket_count);
            int nkbucketpos = getrandint(addrman_bucket_size);
            if (vvtried[nkbucket][nkbucketpos] == -1)
                continue;
            int nid = vvtried[nkbucket][nkbucketpos];
            assert(mapinfo.count(nid) == 1);
            caddrinfo& info = mapinfo[nid];
            if (getrandint(1 << 30) < fchancefactor * info.getchance() * (1 << 30))
                return info;
            fchancefactor *= 1.2;
        }
    } else {
        // use a new node
        double fchancefactor = 1.0;
        while (1) {
            int nubucket = getrandint(addrman_new_bucket_count);
            int nubucketpos = getrandint(addrman_bucket_size);
            if (vvnew[nubucket][nubucketpos] == -1)
                continue;
            int nid = vvnew[nubucket][nubucketpos];
            assert(mapinfo.count(nid) == 1);
            caddrinfo& info = mapinfo[nid];
            if (getrandint(1 << 30) < fchancefactor * info.getchance() * (1 << 30))
                return info;
            fchancefactor *= 1.2;
        }
    }
}

#ifdef debug_addrman
int caddrman::check_()
{
    std::set<int> settried;
    std::map<int, int> mapnew;

    if (vrandom.size() != ntried + nnew)
        return -7;

    for (std::map<int, caddrinfo>::iterator it = mapinfo.begin(); it != mapinfo.end(); it++) {
        int n = (*it).first;
        caddrinfo& info = (*it).second;
        if (info.fintried) {
            if (!info.nlastsuccess)
                return -1;
            if (info.nrefcount)
                return -2;
            settried.insert(n);
        } else {
            if (info.nrefcount < 0 || info.nrefcount > addrman_new_buckets_per_address)
                return -3;
            if (!info.nrefcount)
                return -4;
            mapnew[n] = info.nrefcount;
        }
        if (mapaddr[info] != n)
            return -5;
        if (info.nrandompos < 0 || info.nrandompos >= vrandom.size() || vrandom[info.nrandompos] != n)
            return -14;
        if (info.nlasttry < 0)
            return -6;
        if (info.nlastsuccess < 0)
            return -8;
    }

    if (settried.size() != ntried)
        return -9;
    if (mapnew.size() != nnew)
        return -10;

    for (int n = 0; n < addrman_tried_bucket_count; n++) {
        for (int i = 0; i < addrman_bucket_size; i++) {
             if (vvtried[n][i] != -1) {
                 if (!settried.count(vvtried[n][i]))
                     return -11;
                 if (mapinfo[vvtried[n][i]].gettriedbucket(nkey) != n)
                     return -17;
                 if (mapinfo[vvtried[n][i]].getbucketposition(nkey, false, n) != i)
                     return -18;
                 settried.erase(vvtried[n][i]);
             }
        }
    }

    for (int n = 0; n < addrman_new_bucket_count; n++) {
        for (int i = 0; i < addrman_bucket_size; i++) {
            if (vvnew[n][i] != -1) {
                if (!mapnew.count(vvnew[n][i]))
                    return -12;
                if (mapinfo[vvnew[n][i]].getbucketposition(nkey, true, n) != i)
                    return -19;
                if (--mapnew[vvnew[n][i]] == 0)
                    mapnew.erase(vvnew[n][i]);
            }
        }
    }

    if (settried.size())
        return -13;
    if (mapnew.size())
        return -15;
    if (nkey.isnull())
        return -16;

    return 0;
}
#endif

void caddrman::getaddr_(std::vector<caddress>& vaddr)
{
    unsigned int nnodes = addrman_getaddr_max_pct * vrandom.size() / 100;
    if (nnodes > addrman_getaddr_max)
        nnodes = addrman_getaddr_max;

    // gather a list of random nodes, skipping those of low quality
    for (unsigned int n = 0; n < vrandom.size(); n++) {
        if (vaddr.size() >= nnodes)
            break;

        int nrndpos = getrandint(vrandom.size() - n) + n;
        swaprandom(n, nrndpos);
        assert(mapinfo.count(vrandom[n]) == 1);

        const caddrinfo& ai = mapinfo[vrandom[n]];
        if (!ai.isterrible())
            vaddr.push_back(ai);
    }
}

void caddrman::connected_(const cservice& addr, int64_t ntime)
{
    caddrinfo* pinfo = find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    caddrinfo& info = *pinfo;

    // check whether we are talking about the exact same cservice (including same port)
    if (info != addr)
        return;

    // update info
    int64_t nupdateinterval = 20 * 60;
    if (ntime - info.ntime > nupdateinterval)
        info.ntime = ntime;
}
