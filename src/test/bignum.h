// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_test_bignum_h
#define moorecoin_test_bignum_h

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

#include <openssl/bn.h>

class bignum_error : public std::runtime_error
{
public:
    explicit bignum_error(const std::string& str) : std::runtime_error(str) {}
};


/** c++ wrapper for bignum (openssl bignum) */
class cbignum : public bignum
{
public:
    cbignum()
    {
        bn_init(this);
    }

    cbignum(const cbignum& b)
    {
        bn_init(this);
        if (!bn_copy(this, &b))
        {
            bn_clear_free(this);
            throw bignum_error("cbignum::cbignum(const cbignum&): bn_copy failed");
        }
    }

    cbignum& operator=(const cbignum& b)
    {
        if (!bn_copy(this, &b))
            throw bignum_error("cbignum::operator=: bn_copy failed");
        return (*this);
    }

    ~cbignum()
    {
        bn_clear_free(this);
    }

    cbignum(long long n)          { bn_init(this); setint64(n); }

    explicit cbignum(const std::vector<unsigned char>& vch)
    {
        bn_init(this);
        setvch(vch);
    }

    int getint() const
    {
        bn_ulong n = bn_get_word(this);
        if (!bn_is_negative(this))
            return (n > (bn_ulong)std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : n);
        else
            return (n > (bn_ulong)std::numeric_limits<int>::max() ? std::numeric_limits<int>::min() : -(int)n);
    }

    void setint64(int64_t sn)
    {
        unsigned char pch[sizeof(sn) + 6];
        unsigned char* p = pch + 4;
        bool fnegative;
        uint64_t n;

        if (sn < (int64_t)0)
        {
            // since the minimum signed integer cannot be represented as positive so long as its type is signed, 
            // and it's not well-defined what happens if you make it unsigned before negating it,
            // we instead increment the negative integer by 1, convert it, then increment the (now positive) unsigned integer by 1 to compensate
            n = -(sn + 1);
            ++n;
            fnegative = true;
        } else {
            n = sn;
            fnegative = false;
        }

        bool fleadingzeroes = true;
        for (int i = 0; i < 8; i++)
        {
            unsigned char c = (n >> 56) & 0xff;
            n <<= 8;
            if (fleadingzeroes)
            {
                if (c == 0)
                    continue;
                if (c & 0x80)
                    *p++ = (fnegative ? 0x80 : 0);
                else if (fnegative)
                    c |= 0x80;
                fleadingzeroes = false;
            }
            *p++ = c;
        }
        unsigned int nsize = p - (pch + 4);
        pch[0] = (nsize >> 24) & 0xff;
        pch[1] = (nsize >> 16) & 0xff;
        pch[2] = (nsize >> 8) & 0xff;
        pch[3] = (nsize) & 0xff;
        bn_mpi2bn(pch, p - pch, this);
    }

    void setvch(const std::vector<unsigned char>& vch)
    {
        std::vector<unsigned char> vch2(vch.size() + 4);
        unsigned int nsize = vch.size();
        // bignum's byte stream format expects 4 bytes of
        // big endian size data info at the front
        vch2[0] = (nsize >> 24) & 0xff;
        vch2[1] = (nsize >> 16) & 0xff;
        vch2[2] = (nsize >> 8) & 0xff;
        vch2[3] = (nsize >> 0) & 0xff;
        // swap data to big endian
        reverse_copy(vch.begin(), vch.end(), vch2.begin() + 4);
        bn_mpi2bn(&vch2[0], vch2.size(), this);
    }

    std::vector<unsigned char> getvch() const
    {
        unsigned int nsize = bn_bn2mpi(this, null);
        if (nsize <= 4)
            return std::vector<unsigned char>();
        std::vector<unsigned char> vch(nsize);
        bn_bn2mpi(this, &vch[0]);
        vch.erase(vch.begin(), vch.begin() + 4);
        reverse(vch.begin(), vch.end());
        return vch;
    }

    friend inline const cbignum operator-(const cbignum& a, const cbignum& b);
};



inline const cbignum operator+(const cbignum& a, const cbignum& b)
{
    cbignum r;
    if (!bn_add(&r, &a, &b))
        throw bignum_error("cbignum::operator+: bn_add failed");
    return r;
}

inline const cbignum operator-(const cbignum& a, const cbignum& b)
{
    cbignum r;
    if (!bn_sub(&r, &a, &b))
        throw bignum_error("cbignum::operator-: bn_sub failed");
    return r;
}

inline const cbignum operator-(const cbignum& a)
{
    cbignum r(a);
    bn_set_negative(&r, !bn_is_negative(&r));
    return r;
}

inline bool operator==(const cbignum& a, const cbignum& b) { return (bn_cmp(&a, &b) == 0); }
inline bool operator!=(const cbignum& a, const cbignum& b) { return (bn_cmp(&a, &b) != 0); }
inline bool operator<=(const cbignum& a, const cbignum& b) { return (bn_cmp(&a, &b) <= 0); }
inline bool operator>=(const cbignum& a, const cbignum& b) { return (bn_cmp(&a, &b) >= 0); }
inline bool operator<(const cbignum& a, const cbignum& b)  { return (bn_cmp(&a, &b) < 0); }
inline bool operator>(const cbignum& a, const cbignum& b)  { return (bn_cmp(&a, &b) > 0); }

#endif // moorecoin_test_bignum_h
