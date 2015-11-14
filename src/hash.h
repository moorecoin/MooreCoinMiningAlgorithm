// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_hash_h
#define moorecoin_hash_h

#include "crypto/ripemd160.h"
#include "crypto/sha256.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"

#include <vector>

typedef uint256 chaincode;

/** a hasher class for moorecoin's 256-bit hash (double sha-256). */
class chash256 {
private:
    csha256 sha;
public:
    static const size_t output_size = csha256::output_size;

    void finalize(unsigned char hash[output_size]) {
        unsigned char buf[sha.output_size];
        sha.finalize(buf);
        sha.reset().write(buf, sha.output_size).finalize(hash);
    }

    chash256& write(const unsigned char *data, size_t len) {
        sha.write(data, len);
        return *this;
    }

    chash256& reset() {
        sha.reset();
        return *this;
    }
};

/** a hasher class for moorecoin's 160-bit hash (sha-256 + ripemd-160). */
class chash160 {
private:
    csha256 sha;
public:
    static const size_t output_size = cripemd160::output_size;

    void finalize(unsigned char hash[output_size]) {
        unsigned char buf[sha.output_size];
        sha.finalize(buf);
        cripemd160().write(buf, sha.output_size).finalize(hash);
    }

    chash160& write(const unsigned char *data, size_t len) {
        sha.write(data, len);
        return *this;
    }

    chash160& reset() {
        sha.reset();
        return *this;
    }
};

/** compute the 256-bit hash of an object. */
template<typename t1>
inline uint256 hash(const t1 pbegin, const t1 pend)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    chash256().write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .finalize((unsigned char*)&result);
    return result;
}

/** compute the 256-bit hash of the concatenation of two objects. */
template<typename t1, typename t2>
inline uint256 hash(const t1 p1begin, const t1 p1end,
                    const t2 p2begin, const t2 p2end) {
    static const unsigned char pblank[1] = {};
    uint256 result;
    chash256().write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
              .write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
              .finalize((unsigned char*)&result);
    return result;
}

/** compute the 256-bit hash of the concatenation of three objects. */
template<typename t1, typename t2, typename t3>
inline uint256 hash(const t1 p1begin, const t1 p1end,
                    const t2 p2begin, const t2 p2end,
                    const t3 p3begin, const t3 p3end) {
    static const unsigned char pblank[1] = {};
    uint256 result;
    chash256().write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
              .write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
              .write(p3begin == p3end ? pblank : (const unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0]))
              .finalize((unsigned char*)&result);
    return result;
}

/** compute the 160-bit hash an object. */
template<typename t1>
inline uint160 hash160(const t1 pbegin, const t1 pend)
{
    static unsigned char pblank[1] = {};
    uint160 result;
    chash160().write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .finalize((unsigned char*)&result);
    return result;
}

/** compute the 160-bit hash of a vector. */
inline uint160 hash160(const std::vector<unsigned char>& vch)
{
    return hash160(vch.begin(), vch.end());
}

/** a writer stream (for serialization) that computes a 256-bit hash. */
class chashwriter
{
private:
    chash256 ctx;

public:
    int ntype;
    int nversion;

    chashwriter(int ntypein, int nversionin) : ntype(ntypein), nversion(nversionin) {}

    chashwriter& write(const char *pch, size_t size) {
        ctx.write((const unsigned char*)pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 gethash() {
        uint256 result;
        ctx.finalize((unsigned char*)&result);
        return result;
    }

    template<typename t>
    chashwriter& operator<<(const t& obj) {
        // serialize to this stream
        ::serialize(*this, obj, ntype, nversion);
        return (*this);
    }
};

/** compute the 256-bit hash of an object's serialization. */
template<typename t>
uint256 serializehash(const t& obj, int ntype=ser_gethash, int nversion=protocol_version)
{
    chashwriter ss(ntype, nversion);
    ss << obj;
    return ss.gethash();
}

unsigned int murmurhash3(unsigned int nhashseed, const std::vector<unsigned char>& vdatatohash);

void bip32hash(const chaincode &chaincode, unsigned int nchild, unsigned char header, const unsigned char data[32], unsigned char output[64]);

#endif // moorecoin_hash_h
