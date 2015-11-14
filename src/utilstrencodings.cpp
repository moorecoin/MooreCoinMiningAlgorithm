// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "utilstrencodings.h"

#include "tinyformat.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <limits>

using namespace std;

string sanitizestring(const string& str)
{
    /**
     * safechars chosen to allow simple messages/urls/email addresses, but avoid anything
     * even possibly remotely dangerous like & or >
     */
    static string safechars("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234567890 .,;_/:?@()");
    string strresult;
    for (std::string::size_type i = 0; i < str.size(); i++)
    {
        if (safechars.find(str[i]) != std::string::npos)
            strresult.push_back(str[i]);
    }
    return strresult;
}

const signed char p_util_hexdigit[256] =
{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,
  -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, };

signed char hexdigit(char c)
{
    return p_util_hexdigit[(unsigned char)c];
}

bool ishex(const string& str)
{
    for(std::string::const_iterator it(str.begin()); it != str.end(); ++it)
    {
        if (hexdigit(*it) < 0)
            return false;
    }
    return (str.size() > 0) && (str.size()%2 == 0);
}

vector<unsigned char> parsehex(const char* psz)
{
    // convert hex dump to vector
    vector<unsigned char> vch;
    while (true)
    {
        while (isspace(*psz))
            psz++;
        signed char c = hexdigit(*psz++);
        if (c == (signed char)-1)
            break;
        unsigned char n = (c << 4);
        c = hexdigit(*psz++);
        if (c == (signed char)-1)
            break;
        n |= c;
        vch.push_back(n);
    }
    return vch;
}

vector<unsigned char> parsehex(const string& str)
{
    return parsehex(str.c_str());
}

string encodebase64(const unsigned char* pch, size_t len)
{
    static const char *pbase64 = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789+/";

    string strret="";
    strret.reserve((len+2)/3*4);

    int mode=0, left=0;
    const unsigned char *pchend = pch+len;

    while (pch<pchend)
    {
        int enc = *(pch++);
        switch (mode)
        {
            case 0: // we have no bits
                strret += pbase64[enc >> 2];
                left = (enc & 3) << 4;
                mode = 1;
                break;

            case 1: // we have two bits
                strret += pbase64[left | (enc >> 4)];
                left = (enc & 15) << 2;
                mode = 2;
                break;

            case 2: // we have four bits
                strret += pbase64[left | (enc >> 6)];
                strret += pbase64[enc & 63];
                mode = 0;
                break;
        }
    }

    if (mode)
    {
        strret += pbase64[left];
        strret += '=';
        if (mode == 1)
            strret += '=';
    }

    return strret;
}

string encodebase64(const string& str)
{
    return encodebase64((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> decodebase64(const char* p, bool* pfinvalid)
{
    static const int decode64_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    if (pfinvalid)
        *pfinvalid = false;

    vector<unsigned char> vchret;
    vchret.reserve(strlen(p)*3/4);

    int mode = 0;
    int left = 0;

    while (1)
    {
         int dec = decode64_table[(unsigned char)*p];
         if (dec == -1) break;
         p++;
         switch (mode)
         {
             case 0: // we have no bits and get 6
                 left = dec;
                 mode = 1;
                 break;

              case 1: // we have 6 bits and keep 4
                  vchret.push_back((left<<2) | (dec>>4));
                  left = dec & 15;
                  mode = 2;
                  break;

             case 2: // we have 4 bits and get 6, we keep 2
                 vchret.push_back((left<<4) | (dec>>2));
                 left = dec & 3;
                 mode = 3;
                 break;

             case 3: // we have 2 bits and get 6
                 vchret.push_back((left<<6) | dec);
                 mode = 0;
                 break;
         }
    }

    if (pfinvalid)
        switch (mode)
        {
            case 0: // 4n base64 characters processed: ok
                break;

            case 1: // 4n+1 base64 character processed: impossible
                *pfinvalid = true;
                break;

            case 2: // 4n+2 base64 characters processed: require '=='
                if (left || p[0] != '=' || p[1] != '=' || decode64_table[(unsigned char)p[2]] != -1)
                    *pfinvalid = true;
                break;

            case 3: // 4n+3 base64 characters processed: require '='
                if (left || p[0] != '=' || decode64_table[(unsigned char)p[1]] != -1)
                    *pfinvalid = true;
                break;
        }

    return vchret;
}

string decodebase64(const string& str)
{
    vector<unsigned char> vchret = decodebase64(str.c_str());
    return (vchret.size() == 0) ? string() : string((const char*)&vchret[0], vchret.size());
}

string encodebase32(const unsigned char* pch, size_t len)
{
    static const char *pbase32 = "abcdefghijklmnopqrstuvwxyz234567";

    string strret="";
    strret.reserve((len+4)/5*8);

    int mode=0, left=0;
    const unsigned char *pchend = pch+len;

    while (pch<pchend)
    {
        int enc = *(pch++);
        switch (mode)
        {
            case 0: // we have no bits
                strret += pbase32[enc >> 3];
                left = (enc & 7) << 2;
                mode = 1;
                break;

            case 1: // we have three bits
                strret += pbase32[left | (enc >> 6)];
                strret += pbase32[(enc >> 1) & 31];
                left = (enc & 1) << 4;
                mode = 2;
                break;

            case 2: // we have one bit
                strret += pbase32[left | (enc >> 4)];
                left = (enc & 15) << 1;
                mode = 3;
                break;

            case 3: // we have four bits
                strret += pbase32[left | (enc >> 7)];
                strret += pbase32[(enc >> 2) & 31];
                left = (enc & 3) << 3;
                mode = 4;
                break;

            case 4: // we have two bits
                strret += pbase32[left | (enc >> 5)];
                strret += pbase32[enc & 31];
                mode = 0;
        }
    }

    static const int npadding[5] = {0, 6, 4, 3, 1};
    if (mode)
    {
        strret += pbase32[left];
        for (int n=0; n<npadding[mode]; n++)
             strret += '=';
    }

    return strret;
}

string encodebase32(const string& str)
{
    return encodebase32((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> decodebase32(const char* p, bool* pfinvalid)
{
    static const int decode32_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1,  0,  1,  2,
         3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    if (pfinvalid)
        *pfinvalid = false;

    vector<unsigned char> vchret;
    vchret.reserve((strlen(p))*5/8);

    int mode = 0;
    int left = 0;

    while (1)
    {
         int dec = decode32_table[(unsigned char)*p];
         if (dec == -1) break;
         p++;
         switch (mode)
         {
             case 0: // we have no bits and get 5
                 left = dec;
                 mode = 1;
                 break;

              case 1: // we have 5 bits and keep 2
                  vchret.push_back((left<<3) | (dec>>2));
                  left = dec & 3;
                  mode = 2;
                  break;

             case 2: // we have 2 bits and keep 7
                 left = left << 5 | dec;
                 mode = 3;
                 break;

             case 3: // we have 7 bits and keep 4
                 vchret.push_back((left<<1) | (dec>>4));
                 left = dec & 15;
                 mode = 4;
                 break;

             case 4: // we have 4 bits, and keep 1
                 vchret.push_back((left<<4) | (dec>>1));
                 left = dec & 1;
                 mode = 5;
                 break;

             case 5: // we have 1 bit, and keep 6
                 left = left << 5 | dec;
                 mode = 6;
                 break;

             case 6: // we have 6 bits, and keep 3
                 vchret.push_back((left<<2) | (dec>>3));
                 left = dec & 7;
                 mode = 7;
                 break;

             case 7: // we have 3 bits, and keep 0
                 vchret.push_back((left<<5) | dec);
                 mode = 0;
                 break;
         }
    }

    if (pfinvalid)
        switch (mode)
        {
            case 0: // 8n base32 characters processed: ok
                break;

            case 1: // 8n+1 base32 characters processed: impossible
            case 3: //   +3
            case 6: //   +6
                *pfinvalid = true;
                break;

            case 2: // 8n+2 base32 characters processed: require '======'
                if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || p[3] != '=' || p[4] != '=' || p[5] != '=' || decode32_table[(unsigned char)p[6]] != -1)
                    *pfinvalid = true;
                break;

            case 4: // 8n+4 base32 characters processed: require '===='
                if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || p[3] != '=' || decode32_table[(unsigned char)p[4]] != -1)
                    *pfinvalid = true;
                break;

            case 5: // 8n+5 base32 characters processed: require '==='
                if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || decode32_table[(unsigned char)p[3]] != -1)
                    *pfinvalid = true;
                break;

            case 7: // 8n+7 base32 characters processed: require '='
                if (left || p[0] != '=' || decode32_table[(unsigned char)p[1]] != -1)
                    *pfinvalid = true;
                break;
        }

    return vchret;
}

string decodebase32(const string& str)
{
    vector<unsigned char> vchret = decodebase32(str.c_str());
    return (vchret.size() == 0) ? string() : string((const char*)&vchret[0], vchret.size());
}

static bool parseprechecks(const std::string& str)
{
    if (str.empty()) // no empty string allowed
        return false;
    if (str.size() >= 1 && (isspace(str[0]) || isspace(str[str.size()-1]))) // no padding allowed
        return false;
    if (str.size() != strlen(str.c_str())) // no embedded nul characters allowed
        return false;
    return true;
}

bool parseint32(const std::string& str, int32_t *out)
{
    if (!parseprechecks(str))
        return false;
    char *endp = null;
    errno = 0; // strtol will not set errno if valid
    long int n = strtol(str.c_str(), &endp, 10);
    if(out) *out = (int32_t)n;
    // note that strtol returns a *long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int32_t*. on 64-bit
    // platforms the size of these types may be different.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int32_t>::min() &&
        n <= std::numeric_limits<int32_t>::max();
}

bool parseint64(const std::string& str, int64_t *out)
{
    if (!parseprechecks(str))
        return false;
    char *endp = null;
    errno = 0; // strtoll will not set errno if valid
    long long int n = strtoll(str.c_str(), &endp, 10);
    if(out) *out = (int64_t)n;
    // note that strtoll returns a *long long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int64_t*.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int64_t>::min() &&
        n <= std::numeric_limits<int64_t>::max();
}

bool parsedouble(const std::string& str, double *out)
{
    if (!parseprechecks(str))
        return false;
    if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') // no hexadecimal floats allowed
        return false;
    char *endp = null;
    errno = 0; // strtod will not set errno if valid
    double n = strtod(str.c_str(), &endp);
    if(out) *out = n;
    return endp && *endp == 0 && !errno;
}

std::string formatparagraph(const std::string& in, size_t width, size_t indent)
{
    std::stringstream out;
    size_t col = 0;
    size_t ptr = 0;
    while(ptr < in.size())
    {
        // find beginning of next word
        ptr = in.find_first_not_of(' ', ptr);
        if (ptr == std::string::npos)
            break;
        // find end of next word
        size_t endword = in.find_first_of(' ', ptr);
        if (endword == std::string::npos)
            endword = in.size();
        // add newline and indentation if this wraps over the allowed width
        if (col > 0)
        {
            if ((col + endword - ptr) > width)
            {
                out << '\n';
                for(size_t i=0; i<indent; ++i)
                    out << ' ';
                col = 0;
            } else
                out << ' ';
        }
        // append word
        out << in.substr(ptr, endword - ptr);
        col += endword - ptr + 1;
        ptr = endword;
    }
    return out.str();
}

std::string i64tostr(int64_t n)
{
    return strprintf("%d", n);
}

std::string itostr(int n)
{
    return strprintf("%d", n);
}

int64_t atoi64(const char* psz)
{
#ifdef _msc_ver
    return _atoi64(psz);
#else
    return strtoll(psz, null, 10);
#endif
}

int64_t atoi64(const std::string& str)
{
#ifdef _msc_ver
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), null, 10);
#endif
}

int atoi(const std::string& str)
{
    return atoi(str.c_str());
}
