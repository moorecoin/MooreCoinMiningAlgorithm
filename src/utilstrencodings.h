// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

/**
 * utilities for converting data from/to strings.
 */
#ifndef moorecoin_utilstrencodings_h
#define moorecoin_utilstrencodings_h

#include <stdint.h>
#include <string>
#include <vector>

#define begin(a)            ((char*)&(a))
#define end(a)              ((char*)&((&(a))[1]))
#define ubegin(a)           ((unsigned char*)&(a))
#define uend(a)             ((unsigned char*)&((&(a))[1]))
#define arraylen(array)     (sizeof(array)/sizeof((array)[0]))

/** this is needed because the foreach macro can't get over the comma in pair<t1, t2> */
#define pairtype(t1, t2)    std::pair<t1, t2>

std::string sanitizestring(const std::string& str);
std::vector<unsigned char> parsehex(const char* psz);
std::vector<unsigned char> parsehex(const std::string& str);
signed char hexdigit(char c);
bool ishex(const std::string& str);
std::vector<unsigned char> decodebase64(const char* p, bool* pfinvalid = null);
std::string decodebase64(const std::string& str);
std::string encodebase64(const unsigned char* pch, size_t len);
std::string encodebase64(const std::string& str);
std::vector<unsigned char> decodebase32(const char* p, bool* pfinvalid = null);
std::string decodebase32(const std::string& str);
std::string encodebase32(const unsigned char* pch, size_t len);
std::string encodebase32(const std::string& str);

std::string i64tostr(int64_t n);
std::string itostr(int n);
int64_t atoi64(const char* psz);
int64_t atoi64(const std::string& str);
int atoi(const std::string& str);

/**
 * convert string to signed 32-bit integer with strict parse error feedback.
 * @returns true if the entire string could be parsed as valid integer,
 *   false if not the entire string could be parsed or when overflow or underflow occurred.
 */
bool parseint32(const std::string& str, int32_t *out);

/**
 * convert string to signed 64-bit integer with strict parse error feedback.
 * @returns true if the entire string could be parsed as valid integer,
 *   false if not the entire string could be parsed or when overflow or underflow occurred.
 */
bool parseint64(const std::string& str, int64_t *out);

/**
 * convert string to double with strict parse error feedback.
 * @returns true if the entire string could be parsed as valid double,
 *   false if not the entire string could be parsed or when overflow or underflow occurred.
 */
bool parsedouble(const std::string& str, double *out);

template<typename t>
std::string hexstr(const t itbegin, const t itend, bool fspaces=false)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(t it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fspaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return rv;
}

template<typename t>
inline std::string hexstr(const t& vch, bool fspaces=false)
{
    return hexstr(vch.begin(), vch.end(), fspaces);
}

/**
 * format a paragraph of text to a fixed width, adding spaces for
 * indentation to any added line.
 */
std::string formatparagraph(const std::string& in, size_t width = 79, size_t indent = 0);

/**
 * timing-attack-resistant comparison.
 * takes time proportional to length
 * of first argument.
 */
template <typename t>
bool timingresistantequal(const t& a, const t& b)
{
    if (b.size() == 0) return a.size() == 0;
    size_t accumulator = a.size() ^ b.size();
    for (size_t i = 0; i < a.size(); i++)
        accumulator |= a[i] ^ b[i%b.size()];
    return accumulator == 0;
}

#endif // moorecoin_utilstrencodings_h
