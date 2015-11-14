// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_uint256_h
#define moorecoin_uint256_h

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

/** template base class for fixed-sized opaque blobs. */
template<unsigned int bits>
class base_blob
{
protected:
    enum { width=bits/8 };
    uint8_t data[width];
public:
    base_blob()
    {
        memset(data, 0, sizeof(data));
    }

    explicit base_blob(const std::vector<unsigned char>& vch);

    bool isnull() const
    {
        for (int i = 0; i < width; i++)
            if (data[i] != 0)
                return false;
        return true;
    }

    void setnull()
    {
        memset(data, 0, sizeof(data));
    }

    friend inline bool operator==(const base_blob& a, const base_blob& b) { return memcmp(a.data, b.data, sizeof(a.data)) == 0; }
    friend inline bool operator!=(const base_blob& a, const base_blob& b) { return memcmp(a.data, b.data, sizeof(a.data)) != 0; }
    friend inline bool operator<(const base_blob& a, const base_blob& b) { return memcmp(a.data, b.data, sizeof(a.data)) < 0; }

    std::string gethex() const;
    void sethex(const char* psz);
    void sethex(const std::string& str);
    std::string tostring() const;

    unsigned char* begin()
    {
        return &data[0];
    }

    unsigned char* end()
    {
        return &data[width];
    }

    const unsigned char* begin() const
    {
        return &data[0];
    }

    const unsigned char* end() const
    {
        return &data[width];
    }

    unsigned int size() const
    {
        return sizeof(data);
    }

    unsigned int getserializesize(int ntype, int nversion) const
    {
        return sizeof(data);
    }

    template<typename stream>
    void serialize(stream& s, int ntype, int nversion) const
    {
        s.write((char*)data, sizeof(data));
    }

    template<typename stream>
    void unserialize(stream& s, int ntype, int nversion)
    {
        s.read((char*)data, sizeof(data));
    }
};

/** 160-bit opaque blob.
 * @note this type is called uint160 for historical reasons only. it is an opaque
 * blob of 160 bits and has no integer operations.
 */
class uint160 : public base_blob<160> {
public:
    uint160() {}
    uint160(const base_blob<160>& b) : base_blob<160>(b) {}
    explicit uint160(const std::vector<unsigned char>& vch) : base_blob<160>(vch) {}
};

/** 256-bit opaque blob.
 * @note this type is called uint256 for historical reasons only. it is an
 * opaque blob of 256 bits and has no integer operations. use arith_uint256 if
 * those are required.
 */
class uint256 : public base_blob<256> {
public:
    uint256() {}
    uint256(const base_blob<256>& b) : base_blob<256>(b) {}
    explicit uint256(const std::vector<unsigned char>& vch) : base_blob<256>(vch) {}

    /** a cheap hash function that just returns 64 bits from the result, it can be
     * used when the contents are considered uniformly random. it is not appropriate
     * when the value can easily be influenced from outside as e.g. a network adversary could
     * provide values to trigger worst-case behavior.
     * @note the result of this function is not stable between little and big endian.
     */
    uint64_t getcheaphash() const
    {
        uint64_t result;
        memcpy((void*)&result, (void*)data, 8);
        return result;
    }

    /** a more secure, salted hash function.
     * @note this hash is not stable between little and big endian.
     */
    uint64_t gethash(const uint256& salt) const;
};

/* uint256 from const char *.
 * this is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline uint256 uint256s(const char *str)
{
    uint256 rv;
    rv.sethex(str);
    return rv;
}
/* uint256 from std::string.
 * this is a separate function because the constructor uint256(const std::string &str) can result
 * in dangerously catching uint256(0) via std::string(const char*).
 */
inline uint256 uint256s(const std::string& str)
{
    uint256 rv;
    rv.sethex(str);
    return rv;
}

#endif // moorecoin_uint256_h
