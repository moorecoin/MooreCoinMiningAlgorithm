// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "random.h"

#include "support/cleanse.h"
#ifdef win32
#include "compat.h" // for windows api
#endif
#include "serialize.h"        // for begin_ptr(vec)
#include "util.h"             // for logprint()
#include "utilstrencodings.h" // for gettime()

#include <limits>

#ifndef win32
#include <sys/time.h>
#endif

#include <openssl/err.h>
#include <openssl/rand.h>

static inline int64_t getperformancecounter()
{
    int64_t ncounter = 0;
#ifdef win32
    queryperformancecounter((large_integer*)&ncounter);
#else
    timeval t;
    gettimeofday(&t, null);
    ncounter = (int64_t)(t.tv_sec * 1000000 + t.tv_usec);
#endif
    return ncounter;
}

void randaddseed()
{
    // seed with cpu performance counter
    int64_t ncounter = getperformancecounter();
    rand_add(&ncounter, sizeof(ncounter), 1.5);
    memory_cleanse((void*)&ncounter, sizeof(ncounter));
}

void randaddseedperfmon()
{
    randaddseed();

#ifdef win32
    // don't need this on linux, openssl automatically uses /dev/urandom
    // seed with the entire set of perfmon data

    // this can take up to 2 seconds, so only do it every 10 minutes
    static int64_t nlastperfmon;
    if (gettime() < nlastperfmon + 10 * 60)
        return;
    nlastperfmon = gettime();

    std::vector<unsigned char> vdata(250000, 0);
    long ret = 0;
    unsigned long nsize = 0;
    const size_t nmaxsize = 10000000; // bail out at more than 10mb of performance data
    while (true) {
        nsize = vdata.size();
        ret = regqueryvalueexa(hkey_performance_data, "global", null, null, begin_ptr(vdata), &nsize);
        if (ret != error_more_data || vdata.size() >= nmaxsize)
            break;
        vdata.resize(std::max((vdata.size() * 3) / 2, nmaxsize)); // grow size of buffer exponentially
    }
    regclosekey(hkey_performance_data);
    if (ret == error_success) {
        rand_add(begin_ptr(vdata), nsize, nsize / 100.0);
        memory_cleanse(begin_ptr(vdata), nsize);
        logprint("rand", "%s: %lu bytes\n", __func__, nsize);
    } else {
        static bool warned = false; // warn only once
        if (!warned) {
            logprintf("%s: warning: regqueryvalueexa(hkey_performance_data) failed with code %i\n", __func__, ret);
            warned = true;
        }
    }
#endif
}

void getrandbytes(unsigned char* buf, int num)
{
    if (rand_bytes(buf, num) != 1) {
        logprintf("%s: openssl rand_bytes() failed with error: %s\n", __func__, err_error_string(err_get_error(), null));
        assert(false);
    }
}

uint64_t getrand(uint64_t nmax)
{
    if (nmax == 0)
        return 0;

    // the range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64_t nrange = (std::numeric_limits<uint64_t>::max() / nmax) * nmax;
    uint64_t nrand = 0;
    do {
        getrandbytes((unsigned char*)&nrand, sizeof(nrand));
    } while (nrand >= nrange);
    return (nrand % nmax);
}

int getrandint(int nmax)
{
    return getrand(nmax);
}

uint256 getrandhash()
{
    uint256 hash;
    getrandbytes((unsigned char*)&hash, sizeof(hash));
    return hash;
}

uint32_t insecure_rand_rz = 11;
uint32_t insecure_rand_rw = 11;
void seed_insecure_rand(bool fdeterministic)
{
    // the seed values have some unlikely fixed points which we avoid.
    if (fdeterministic) {
        insecure_rand_rz = insecure_rand_rw = 11;
    } else {
        uint32_t tmp;
        do {
            getrandbytes((unsigned char*)&tmp, 4);
        } while (tmp == 0 || tmp == 0x9068ffffu);
        insecure_rand_rz = tmp;
        do {
            getrandbytes((unsigned char*)&tmp, 4);
        } while (tmp == 0 || tmp == 0x464fffffu);
        insecure_rand_rw = tmp;
    }
}
