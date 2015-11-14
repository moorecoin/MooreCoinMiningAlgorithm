// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_serialize_h
#define moorecoin_serialize_h

#include "compat/endian.h"

#include <algorithm>
#include <assert.h>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <string.h>
#include <utility>
#include <vector>

class cscript;

static const unsigned int max_size = 0x02000000;

/**
 * used to bypass the rule against non-const reference to temporary
 * where it makes sense with wrappers such as cflatdata or ctxdb
 */
template<typename t>
inline t& ref(const t& val)
{
    return const_cast<t&>(val);
}

/**
 * used to acquire a non-const pointer "this" to generate bodies
 * of const serialization operations from a template
 */
template<typename t>
inline t* nconst_ptr(const t* val)
{
    return const_cast<t*>(val);
}

/** 
 * get begin pointer of vector (non-const version).
 * @note these functions avoid the undefined case of indexing into an empty
 * vector, as well as that of indexing after the end of the vector.
 */
template <class t, class tal>
inline t* begin_ptr(std::vector<t,tal>& v)
{
    return v.empty() ? null : &v[0];
}
/** get begin pointer of vector (const version) */
template <class t, class tal>
inline const t* begin_ptr(const std::vector<t,tal>& v)
{
    return v.empty() ? null : &v[0];
}
/** get end pointer of vector (non-const version) */
template <class t, class tal>
inline t* end_ptr(std::vector<t,tal>& v)
{
    return v.empty() ? null : (&v[0] + v.size());
}
/** get end pointer of vector (const version) */
template <class t, class tal>
inline const t* end_ptr(const std::vector<t,tal>& v)
{
    return v.empty() ? null : (&v[0] + v.size());
}

/*
 * lowest-level serialization and conversion.
 * @note sizes of these types are verified in the tests
 */
template<typename stream> inline void ser_writedata8(stream &s, uint8_t obj)
{
    s.write((char*)&obj, 1);
}
template<typename stream> inline void ser_writedata16(stream &s, uint16_t obj)
{
    obj = htole16(obj);
    s.write((char*)&obj, 2);
}
template<typename stream> inline void ser_writedata32(stream &s, uint32_t obj)
{
    obj = htole32(obj);
    s.write((char*)&obj, 4);
}
template<typename stream> inline void ser_writedata64(stream &s, uint64_t obj)
{
    obj = htole64(obj);
    s.write((char*)&obj, 8);
}
template<typename stream> inline uint8_t ser_readdata8(stream &s)
{
    uint8_t obj;
    s.read((char*)&obj, 1);
    return obj;
}
template<typename stream> inline uint16_t ser_readdata16(stream &s)
{
    uint16_t obj;
    s.read((char*)&obj, 2);
    return le16toh(obj);
}
template<typename stream> inline uint32_t ser_readdata32(stream &s)
{
    uint32_t obj;
    s.read((char*)&obj, 4);
    return le32toh(obj);
}
template<typename stream> inline uint64_t ser_readdata64(stream &s)
{
    uint64_t obj;
    s.read((char*)&obj, 8);
    return le64toh(obj);
}
inline uint64_t ser_double_to_uint64(double x)
{
    union { double x; uint64_t y; } tmp;
    tmp.x = x;
    return tmp.y;
}
inline uint32_t ser_float_to_uint32(float x)
{
    union { float x; uint32_t y; } tmp;
    tmp.x = x;
    return tmp.y;
}
inline double ser_uint64_to_double(uint64_t y)
{
    union { double x; uint64_t y; } tmp;
    tmp.y = y;
    return tmp.x;
}
inline float ser_uint32_to_float(uint32_t y)
{
    union { float x; uint32_t y; } tmp;
    tmp.y = y;
    return tmp.x;
}


/////////////////////////////////////////////////////////////////
//
// templates for serializing to anything that looks like a stream,
// i.e. anything that supports .read(char*, size_t) and .write(char*, size_t)
//

enum
{
    // primary actions
    ser_network         = (1 << 0),
    ser_disk            = (1 << 1),
    ser_gethash         = (1 << 2),
};

#define readwrite(obj)      (::serreadwrite(s, (obj), ntype, nversion, ser_action))

/** 
 * implement three methods for serializable objects. these are actually wrappers over
 * "serializationop" template, which implements the body of each class' serialization
 * code. adding "add_serialize_methods" in the body of the class causes these wrappers to be
 * added as members. 
 */
#define add_serialize_methods                                                          \
    size_t getserializesize(int ntype, int nversion) const {                         \
        csizecomputer s(ntype, nversion);                                            \
        nconst_ptr(this)->serializationop(s, cseractionserialize(), ntype, nversion);\
        return s.size();                                                             \
    }                                                                                \
    template<typename stream>                                                        \
    void serialize(stream& s, int ntype, int nversion) const {                       \
        nconst_ptr(this)->serializationop(s, cseractionserialize(), ntype, nversion);\
    }                                                                                \
    template<typename stream>                                                        \
    void unserialize(stream& s, int ntype, int nversion) {                           \
        serializationop(s, cseractionunserialize(), ntype, nversion);                \
    }

/*
 * basic types
 */
inline unsigned int getserializesize(char a,      int, int=0) { return 1; }
inline unsigned int getserializesize(int8_t a,    int, int=0) { return 1; }
inline unsigned int getserializesize(uint8_t a,   int, int=0) { return 1; }
inline unsigned int getserializesize(int16_t a,   int, int=0) { return 2; }
inline unsigned int getserializesize(uint16_t a,  int, int=0) { return 2; }
inline unsigned int getserializesize(int32_t a,   int, int=0) { return 4; }
inline unsigned int getserializesize(uint32_t a,  int, int=0) { return 4; }
inline unsigned int getserializesize(int64_t a,   int, int=0) { return 8; }
inline unsigned int getserializesize(uint64_t a,  int, int=0) { return 8; }
inline unsigned int getserializesize(float a,     int, int=0) { return 4; }
inline unsigned int getserializesize(double a,    int, int=0) { return 8; }

template<typename stream> inline void serialize(stream& s, char a,         int, int=0) { ser_writedata8(s, a); } // todo get rid of bare char
template<typename stream> inline void serialize(stream& s, int8_t a,       int, int=0) { ser_writedata8(s, a); }
template<typename stream> inline void serialize(stream& s, uint8_t a,      int, int=0) { ser_writedata8(s, a); }
template<typename stream> inline void serialize(stream& s, int16_t a,      int, int=0) { ser_writedata16(s, a); }
template<typename stream> inline void serialize(stream& s, uint16_t a,     int, int=0) { ser_writedata16(s, a); }
template<typename stream> inline void serialize(stream& s, int32_t a,      int, int=0) { ser_writedata32(s, a); }
template<typename stream> inline void serialize(stream& s, uint32_t a,     int, int=0) { ser_writedata32(s, a); }
template<typename stream> inline void serialize(stream& s, int64_t a,      int, int=0) { ser_writedata64(s, a); }
template<typename stream> inline void serialize(stream& s, uint64_t a,     int, int=0) { ser_writedata64(s, a); }
template<typename stream> inline void serialize(stream& s, float a,        int, int=0) { ser_writedata32(s, ser_float_to_uint32(a)); }
template<typename stream> inline void serialize(stream& s, double a,       int, int=0) { ser_writedata64(s, ser_double_to_uint64(a)); }

template<typename stream> inline void unserialize(stream& s, char& a,      int, int=0) { a = ser_readdata8(s); } // todo get rid of bare char
template<typename stream> inline void unserialize(stream& s, int8_t& a,    int, int=0) { a = ser_readdata8(s); }
template<typename stream> inline void unserialize(stream& s, uint8_t& a,   int, int=0) { a = ser_readdata8(s); }
template<typename stream> inline void unserialize(stream& s, int16_t& a,   int, int=0) { a = ser_readdata16(s); }
template<typename stream> inline void unserialize(stream& s, uint16_t& a,  int, int=0) { a = ser_readdata16(s); }
template<typename stream> inline void unserialize(stream& s, int32_t& a,   int, int=0) { a = ser_readdata32(s); }
template<typename stream> inline void unserialize(stream& s, uint32_t& a,  int, int=0) { a = ser_readdata32(s); }
template<typename stream> inline void unserialize(stream& s, int64_t& a,   int, int=0) { a = ser_readdata64(s); }
template<typename stream> inline void unserialize(stream& s, uint64_t& a,  int, int=0) { a = ser_readdata64(s); }
template<typename stream> inline void unserialize(stream& s, float& a,     int, int=0) { a = ser_uint32_to_float(ser_readdata32(s)); }
template<typename stream> inline void unserialize(stream& s, double& a,    int, int=0) { a = ser_uint64_to_double(ser_readdata64(s)); }

inline unsigned int getserializesize(bool a, int, int=0)                          { return sizeof(char); }
template<typename stream> inline void serialize(stream& s, bool a, int, int=0)    { char f=a; ser_writedata8(s, f); }
template<typename stream> inline void unserialize(stream& s, bool& a, int, int=0) { char f=ser_readdata8(s); a=f; }






/**
 * compact size
 * size <  253        -- 1 byte
 * size <= ushrt_max  -- 3 bytes  (253 + 2 bytes)
 * size <= uint_max   -- 5 bytes  (254 + 4 bytes)
 * size >  uint_max   -- 9 bytes  (255 + 8 bytes)
 */
inline unsigned int getsizeofcompactsize(uint64_t nsize)
{
    if (nsize < 253)             return sizeof(unsigned char);
    else if (nsize <= std::numeric_limits<unsigned short>::max()) return sizeof(unsigned char) + sizeof(unsigned short);
    else if (nsize <= std::numeric_limits<unsigned int>::max())  return sizeof(unsigned char) + sizeof(unsigned int);
    else                         return sizeof(unsigned char) + sizeof(uint64_t);
}

template<typename stream>
void writecompactsize(stream& os, uint64_t nsize)
{
    if (nsize < 253)
    {
        ser_writedata8(os, nsize);
    }
    else if (nsize <= std::numeric_limits<unsigned short>::max())
    {
        ser_writedata8(os, 253);
        ser_writedata16(os, nsize);
    }
    else if (nsize <= std::numeric_limits<unsigned int>::max())
    {
        ser_writedata8(os, 254);
        ser_writedata32(os, nsize);
    }
    else
    {
        ser_writedata8(os, 255);
        ser_writedata64(os, nsize);
    }
    return;
}

template<typename stream>
uint64_t readcompactsize(stream& is)
{
    uint8_t chsize = ser_readdata8(is);
    uint64_t nsizeret = 0;
    if (chsize < 253)
    {
        nsizeret = chsize;
    }
    else if (chsize == 253)
    {
        nsizeret = ser_readdata16(is);
        if (nsizeret < 253)
            throw std::ios_base::failure("non-canonical readcompactsize()");
    }
    else if (chsize == 254)
    {
        nsizeret = ser_readdata32(is);
        if (nsizeret < 0x10000u)
            throw std::ios_base::failure("non-canonical readcompactsize()");
    }
    else
    {
        nsizeret = ser_readdata64(is);
        if (nsizeret < 0x100000000ull)
            throw std::ios_base::failure("non-canonical readcompactsize()");
    }
    if (nsizeret > (uint64_t)max_size)
        throw std::ios_base::failure("readcompactsize(): size too large");
    return nsizeret;
}

/**
 * variable-length integers: bytes are a msb base-128 encoding of the number.
 * the high bit in each byte signifies whether another digit follows. to make
 * sure the encoding is one-to-one, one is subtracted from all but the last digit.
 * thus, the byte sequence a[] with length len, where all but the last byte
 * has bit 128 set, encodes the number:
 * 
 *  (a[len-1] & 0x7f) + sum(i=1..len-1, 128^i*((a[len-i-1] & 0x7f)+1))
 * 
 * properties:
 * * very small (0-127: 1 byte, 128-16511: 2 bytes, 16512-2113663: 3 bytes)
 * * every integer has exactly one encoding
 * * encoding does not depend on size of original integer type
 * * no redundancy: every (infinite) byte sequence corresponds to a list
 *   of encoded integers.
 * 
 * 0:         [0x00]  256:        [0x81 0x00]
 * 1:         [0x01]  16383:      [0xfe 0x7f]
 * 127:       [0x7f]  16384:      [0xff 0x00]
 * 128:  [0x80 0x00]  16511: [0x80 0xff 0x7f]
 * 255:  [0x80 0x7f]  65535: [0x82 0xfd 0x7f]
 * 2^32:           [0x8e 0xfe 0xfe 0xff 0x00]
 */

template<typename i>
inline unsigned int getsizeofvarint(i n)
{
    int nret = 0;
    while(true) {
        nret++;
        if (n <= 0x7f)
            break;
        n = (n >> 7) - 1;
    }
    return nret;
}

template<typename stream, typename i>
void writevarint(stream& os, i n)
{
    unsigned char tmp[(sizeof(n)*8+6)/7];
    int len=0;
    while(true) {
        tmp[len] = (n & 0x7f) | (len ? 0x80 : 0x00);
        if (n <= 0x7f)
            break;
        n = (n >> 7) - 1;
        len++;
    }
    do {
        ser_writedata8(os, tmp[len]);
    } while(len--);
}

template<typename stream, typename i>
i readvarint(stream& is)
{
    i n = 0;
    while(true) {
        unsigned char chdata = ser_readdata8(is);
        n = (n << 7) | (chdata & 0x7f);
        if (chdata & 0x80)
            n++;
        else
            return n;
    }
}

#define flatdata(obj) ref(cflatdata((char*)&(obj), (char*)&(obj) + sizeof(obj)))
#define varint(obj) ref(wrapvarint(ref(obj)))
#define limited_string(obj,n) ref(limitedstring< n >(ref(obj)))

/** 
 * wrapper for serializing arrays and pod.
 */
class cflatdata
{
protected:
    char* pbegin;
    char* pend;
public:
    cflatdata(void* pbeginin, void* pendin) : pbegin((char*)pbeginin), pend((char*)pendin) { }
    template <class t, class tal>
    explicit cflatdata(std::vector<t,tal> &v)
    {
        pbegin = (char*)begin_ptr(v);
        pend = (char*)end_ptr(v);
    }
    char* begin() { return pbegin; }
    const char* begin() const { return pbegin; }
    char* end() { return pend; }
    const char* end() const { return pend; }

    unsigned int getserializesize(int, int=0) const
    {
        return pend - pbegin;
    }

    template<typename stream>
    void serialize(stream& s, int, int=0) const
    {
        s.write(pbegin, pend - pbegin);
    }

    template<typename stream>
    void unserialize(stream& s, int, int=0)
    {
        s.read(pbegin, pend - pbegin);
    }
};

template<typename i>
class cvarint
{
protected:
    i &n;
public:
    cvarint(i& nin) : n(nin) { }

    unsigned int getserializesize(int, int) const {
        return getsizeofvarint<i>(n);
    }

    template<typename stream>
    void serialize(stream &s, int, int) const {
        writevarint<stream,i>(s, n);
    }

    template<typename stream>
    void unserialize(stream& s, int, int) {
        n = readvarint<stream,i>(s);
    }
};

template<size_t limit>
class limitedstring
{
protected:
    std::string& string;
public:
    limitedstring(std::string& string) : string(string) {}

    template<typename stream>
    void unserialize(stream& s, int, int=0)
    {
        size_t size = readcompactsize(s);
        if (size > limit) {
            throw std::ios_base::failure("string length limit exceeded");
        }
        string.resize(size);
        if (size != 0)
            s.read((char*)&string[0], size);
    }

    template<typename stream>
    void serialize(stream& s, int, int=0) const
    {
        writecompactsize(s, string.size());
        if (!string.empty())
            s.write((char*)&string[0], string.size());
    }

    unsigned int getserializesize(int, int=0) const
    {
        return getsizeofcompactsize(string.size()) + string.size();
    }
};

template<typename i>
cvarint<i> wrapvarint(i& n) { return cvarint<i>(n); }

/**
 * forward declarations
 */

/**
 *  string
 */
template<typename c> unsigned int getserializesize(const std::basic_string<c>& str, int, int=0);
template<typename stream, typename c> void serialize(stream& os, const std::basic_string<c>& str, int, int=0);
template<typename stream, typename c> void unserialize(stream& is, std::basic_string<c>& str, int, int=0);

/**
 * vector
 * vectors of unsigned char are a special case and are intended to be serialized as a single opaque blob.
 */
template<typename t, typename a> unsigned int getserializesize_impl(const std::vector<t, a>& v, int ntype, int nversion, const unsigned char&);
template<typename t, typename a, typename v> unsigned int getserializesize_impl(const std::vector<t, a>& v, int ntype, int nversion, const v&);
template<typename t, typename a> inline unsigned int getserializesize(const std::vector<t, a>& v, int ntype, int nversion);
template<typename stream, typename t, typename a> void serialize_impl(stream& os, const std::vector<t, a>& v, int ntype, int nversion, const unsigned char&);
template<typename stream, typename t, typename a, typename v> void serialize_impl(stream& os, const std::vector<t, a>& v, int ntype, int nversion, const v&);
template<typename stream, typename t, typename a> inline void serialize(stream& os, const std::vector<t, a>& v, int ntype, int nversion);
template<typename stream, typename t, typename a> void unserialize_impl(stream& is, std::vector<t, a>& v, int ntype, int nversion, const unsigned char&);
template<typename stream, typename t, typename a, typename v> void unserialize_impl(stream& is, std::vector<t, a>& v, int ntype, int nversion, const v&);
template<typename stream, typename t, typename a> inline void unserialize(stream& is, std::vector<t, a>& v, int ntype, int nversion);

/**
 * others derived from vector
 */
extern inline unsigned int getserializesize(const cscript& v, int ntype, int nversion);
template<typename stream> void serialize(stream& os, const cscript& v, int ntype, int nversion);
template<typename stream> void unserialize(stream& is, cscript& v, int ntype, int nversion);

/**
 * pair
 */
template<typename k, typename t> unsigned int getserializesize(const std::pair<k, t>& item, int ntype, int nversion);
template<typename stream, typename k, typename t> void serialize(stream& os, const std::pair<k, t>& item, int ntype, int nversion);
template<typename stream, typename k, typename t> void unserialize(stream& is, std::pair<k, t>& item, int ntype, int nversion);

/**
 * map
 */
template<typename k, typename t, typename pred, typename a> unsigned int getserializesize(const std::map<k, t, pred, a>& m, int ntype, int nversion);
template<typename stream, typename k, typename t, typename pred, typename a> void serialize(stream& os, const std::map<k, t, pred, a>& m, int ntype, int nversion);
template<typename stream, typename k, typename t, typename pred, typename a> void unserialize(stream& is, std::map<k, t, pred, a>& m, int ntype, int nversion);

/**
 * set
 */
template<typename k, typename pred, typename a> unsigned int getserializesize(const std::set<k, pred, a>& m, int ntype, int nversion);
template<typename stream, typename k, typename pred, typename a> void serialize(stream& os, const std::set<k, pred, a>& m, int ntype, int nversion);
template<typename stream, typename k, typename pred, typename a> void unserialize(stream& is, std::set<k, pred, a>& m, int ntype, int nversion);





/**
 * if none of the specialized versions above matched, default to calling member function.
 * "int ntype" is changed to "long ntype" to keep from getting an ambiguous overload error.
 * the compiler will only cast int to long if none of the other templates matched.
 * thanks to boost serialization for this idea.
 */
template<typename t>
inline unsigned int getserializesize(const t& a, long ntype, int nversion)
{
    return a.getserializesize((int)ntype, nversion);
}

template<typename stream, typename t>
inline void serialize(stream& os, const t& a, long ntype, int nversion)
{
    a.serialize(os, (int)ntype, nversion);
}

template<typename stream, typename t>
inline void unserialize(stream& is, t& a, long ntype, int nversion)
{
    a.unserialize(is, (int)ntype, nversion);
}





/**
 * string
 */
template<typename c>
unsigned int getserializesize(const std::basic_string<c>& str, int, int)
{
    return getsizeofcompactsize(str.size()) + str.size() * sizeof(str[0]);
}

template<typename stream, typename c>
void serialize(stream& os, const std::basic_string<c>& str, int, int)
{
    writecompactsize(os, str.size());
    if (!str.empty())
        os.write((char*)&str[0], str.size() * sizeof(str[0]));
}

template<typename stream, typename c>
void unserialize(stream& is, std::basic_string<c>& str, int, int)
{
    unsigned int nsize = readcompactsize(is);
    str.resize(nsize);
    if (nsize != 0)
        is.read((char*)&str[0], nsize * sizeof(str[0]));
}



/**
 * vector
 */
template<typename t, typename a>
unsigned int getserializesize_impl(const std::vector<t, a>& v, int ntype, int nversion, const unsigned char&)
{
    return (getsizeofcompactsize(v.size()) + v.size() * sizeof(t));
}

template<typename t, typename a, typename v>
unsigned int getserializesize_impl(const std::vector<t, a>& v, int ntype, int nversion, const v&)
{
    unsigned int nsize = getsizeofcompactsize(v.size());
    for (typename std::vector<t, a>::const_iterator vi = v.begin(); vi != v.end(); ++vi)
        nsize += getserializesize((*vi), ntype, nversion);
    return nsize;
}

template<typename t, typename a>
inline unsigned int getserializesize(const std::vector<t, a>& v, int ntype, int nversion)
{
    return getserializesize_impl(v, ntype, nversion, t());
}


template<typename stream, typename t, typename a>
void serialize_impl(stream& os, const std::vector<t, a>& v, int ntype, int nversion, const unsigned char&)
{
    writecompactsize(os, v.size());
    if (!v.empty())
        os.write((char*)&v[0], v.size() * sizeof(t));
}

template<typename stream, typename t, typename a, typename v>
void serialize_impl(stream& os, const std::vector<t, a>& v, int ntype, int nversion, const v&)
{
    writecompactsize(os, v.size());
    for (typename std::vector<t, a>::const_iterator vi = v.begin(); vi != v.end(); ++vi)
        ::serialize(os, (*vi), ntype, nversion);
}

template<typename stream, typename t, typename a>
inline void serialize(stream& os, const std::vector<t, a>& v, int ntype, int nversion)
{
    serialize_impl(os, v, ntype, nversion, t());
}


template<typename stream, typename t, typename a>
void unserialize_impl(stream& is, std::vector<t, a>& v, int ntype, int nversion, const unsigned char&)
{
    // limit size per read so bogus size value won't cause out of memory
    v.clear();
    unsigned int nsize = readcompactsize(is);
    unsigned int i = 0;
    while (i < nsize)
    {
        unsigned int blk = std::min(nsize - i, (unsigned int)(1 + 4999999 / sizeof(t)));
        v.resize(i + blk);
        is.read((char*)&v[i], blk * sizeof(t));
        i += blk;
    }
}

template<typename stream, typename t, typename a, typename v>
void unserialize_impl(stream& is, std::vector<t, a>& v, int ntype, int nversion, const v&)
{
    v.clear();
    unsigned int nsize = readcompactsize(is);
    unsigned int i = 0;
    unsigned int nmid = 0;
    while (nmid < nsize)
    {
        nmid += 5000000 / sizeof(t);
        if (nmid > nsize)
            nmid = nsize;
        v.resize(nmid);
        for (; i < nmid; i++)
            unserialize(is, v[i], ntype, nversion);
    }
}

template<typename stream, typename t, typename a>
inline void unserialize(stream& is, std::vector<t, a>& v, int ntype, int nversion)
{
    unserialize_impl(is, v, ntype, nversion, t());
}



/**
 * others derived from vector
 */
inline unsigned int getserializesize(const cscript& v, int ntype, int nversion)
{
    return getserializesize((const std::vector<unsigned char>&)v, ntype, nversion);
}

template<typename stream>
void serialize(stream& os, const cscript& v, int ntype, int nversion)
{
    serialize(os, (const std::vector<unsigned char>&)v, ntype, nversion);
}

template<typename stream>
void unserialize(stream& is, cscript& v, int ntype, int nversion)
{
    unserialize(is, (std::vector<unsigned char>&)v, ntype, nversion);
}



/**
 * pair
 */
template<typename k, typename t>
unsigned int getserializesize(const std::pair<k, t>& item, int ntype, int nversion)
{
    return getserializesize(item.first, ntype, nversion) + getserializesize(item.second, ntype, nversion);
}

template<typename stream, typename k, typename t>
void serialize(stream& os, const std::pair<k, t>& item, int ntype, int nversion)
{
    serialize(os, item.first, ntype, nversion);
    serialize(os, item.second, ntype, nversion);
}

template<typename stream, typename k, typename t>
void unserialize(stream& is, std::pair<k, t>& item, int ntype, int nversion)
{
    unserialize(is, item.first, ntype, nversion);
    unserialize(is, item.second, ntype, nversion);
}



/**
 * map
 */
template<typename k, typename t, typename pred, typename a>
unsigned int getserializesize(const std::map<k, t, pred, a>& m, int ntype, int nversion)
{
    unsigned int nsize = getsizeofcompactsize(m.size());
    for (typename std::map<k, t, pred, a>::const_iterator mi = m.begin(); mi != m.end(); ++mi)
        nsize += getserializesize((*mi), ntype, nversion);
    return nsize;
}

template<typename stream, typename k, typename t, typename pred, typename a>
void serialize(stream& os, const std::map<k, t, pred, a>& m, int ntype, int nversion)
{
    writecompactsize(os, m.size());
    for (typename std::map<k, t, pred, a>::const_iterator mi = m.begin(); mi != m.end(); ++mi)
        serialize(os, (*mi), ntype, nversion);
}

template<typename stream, typename k, typename t, typename pred, typename a>
void unserialize(stream& is, std::map<k, t, pred, a>& m, int ntype, int nversion)
{
    m.clear();
    unsigned int nsize = readcompactsize(is);
    typename std::map<k, t, pred, a>::iterator mi = m.begin();
    for (unsigned int i = 0; i < nsize; i++)
    {
        std::pair<k, t> item;
        unserialize(is, item, ntype, nversion);
        mi = m.insert(mi, item);
    }
}



/**
 * set
 */
template<typename k, typename pred, typename a>
unsigned int getserializesize(const std::set<k, pred, a>& m, int ntype, int nversion)
{
    unsigned int nsize = getsizeofcompactsize(m.size());
    for (typename std::set<k, pred, a>::const_iterator it = m.begin(); it != m.end(); ++it)
        nsize += getserializesize((*it), ntype, nversion);
    return nsize;
}

template<typename stream, typename k, typename pred, typename a>
void serialize(stream& os, const std::set<k, pred, a>& m, int ntype, int nversion)
{
    writecompactsize(os, m.size());
    for (typename std::set<k, pred, a>::const_iterator it = m.begin(); it != m.end(); ++it)
        serialize(os, (*it), ntype, nversion);
}

template<typename stream, typename k, typename pred, typename a>
void unserialize(stream& is, std::set<k, pred, a>& m, int ntype, int nversion)
{
    m.clear();
    unsigned int nsize = readcompactsize(is);
    typename std::set<k, pred, a>::iterator it = m.begin();
    for (unsigned int i = 0; i < nsize; i++)
    {
        k key;
        unserialize(is, key, ntype, nversion);
        it = m.insert(it, key);
    }
}



/**
 * support for add_serialize_methods and readwrite macro
 */
struct cseractionserialize
{
    bool forread() const { return false; }
};
struct cseractionunserialize
{
    bool forread() const { return true; }
};

template<typename stream, typename t>
inline void serreadwrite(stream& s, const t& obj, int ntype, int nversion, cseractionserialize ser_action)
{
    ::serialize(s, obj, ntype, nversion);
}

template<typename stream, typename t>
inline void serreadwrite(stream& s, t& obj, int ntype, int nversion, cseractionunserialize ser_action)
{
    ::unserialize(s, obj, ntype, nversion);
}









class csizecomputer
{
protected:
    size_t nsize;

public:
    int ntype;
    int nversion;

    csizecomputer(int ntypein, int nversionin) : nsize(0), ntype(ntypein), nversion(nversionin) {}

    csizecomputer& write(const char *psz, size_t nsize)
    {
        this->nsize += nsize;
        return *this;
    }

    template<typename t>
    csizecomputer& operator<<(const t& obj)
    {
        ::serialize(*this, obj, ntype, nversion);
        return (*this);
    }

    size_t size() const {
        return nsize;
    }
};

#endif // moorecoin_serialize_h
