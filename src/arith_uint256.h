// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_arith_uint256_h
#define moorecoin_arith_uint256_h

#include <assert.h>
#include <cstring>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

class uint256;

class uint_error : public std::runtime_error {
public:
    explicit uint_error(const std::string& str) : std::runtime_error(str) {}
};

/** template base class for unsigned big integers. */
template<unsigned int bits>
class base_uint
{
protected:
    enum { width=bits/32 };
    uint32_t pn[width];
public:

    base_uint()
    {
        for (int i = 0; i < width; i++)
            pn[i] = 0;
    }

    base_uint(const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] = b.pn[i];
    }

    base_uint& operator=(const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] = b.pn[i];
        return *this;
    }

    base_uint(uint64_t b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < width; i++)
            pn[i] = 0;
    }

    explicit base_uint(const std::string& str);

    bool operator!() const
    {
        for (int i = 0; i < width; i++)
            if (pn[i] != 0)
                return false;
        return true;
    }

    const base_uint operator~() const
    {
        base_uint ret;
        for (int i = 0; i < width; i++)
            ret.pn[i] = ~pn[i];
        return ret;
    }

    const base_uint operator-() const
    {
        base_uint ret;
        for (int i = 0; i < width; i++)
            ret.pn[i] = ~pn[i];
        ret++;
        return ret;
    }

    double getdouble() const;

    base_uint& operator=(uint64_t b)
    {
        pn[0] = (unsigned int)b;
        pn[1] = (unsigned int)(b >> 32);
        for (int i = 2; i < width; i++)
            pn[i] = 0;
        return *this;
    }

    base_uint& operator^=(const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] ^= b.pn[i];
        return *this;
    }

    base_uint& operator&=(const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] &= b.pn[i];
        return *this;
    }

    base_uint& operator|=(const base_uint& b)
    {
        for (int i = 0; i < width; i++)
            pn[i] |= b.pn[i];
        return *this;
    }

    base_uint& operator^=(uint64_t b)
    {
        pn[0] ^= (unsigned int)b;
        pn[1] ^= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint& operator|=(uint64_t b)
    {
        pn[0] |= (unsigned int)b;
        pn[1] |= (unsigned int)(b >> 32);
        return *this;
    }

    base_uint& operator<<=(unsigned int shift);
    base_uint& operator>>=(unsigned int shift);

    base_uint& operator+=(const base_uint& b)
    {
        uint64_t carry = 0;
        for (int i = 0; i < width; i++)
        {
            uint64_t n = carry + pn[i] + b.pn[i];
            pn[i] = n & 0xffffffff;
            carry = n >> 32;
        }
        return *this;
    }

    base_uint& operator-=(const base_uint& b)
    {
        *this += -b;
        return *this;
    }

    base_uint& operator+=(uint64_t b64)
    {
        base_uint b;
        b = b64;
        *this += b;
        return *this;
    }

    base_uint& operator-=(uint64_t b64)
    {
        base_uint b;
        b = b64;
        *this += -b;
        return *this;
    }

    base_uint& operator*=(uint32_t b32);
    base_uint& operator*=(const base_uint& b);
    base_uint& operator/=(const base_uint& b);

    base_uint& operator++()
    {
        // prefix operator
        int i = 0;
        while (++pn[i] == 0 && i < width-1)
            i++;
        return *this;
    }

    const base_uint operator++(int)
    {
        // postfix operator
        const base_uint ret = *this;
        ++(*this);
        return ret;
    }

    base_uint& operator--()
    {
        // prefix operator
        int i = 0;
        while (--pn[i] == (uint32_t)-1 && i < width-1)
            i++;
        return *this;
    }

    const base_uint operator--(int)
    {
        // postfix operator
        const base_uint ret = *this;
        --(*this);
        return ret;
    }

    int compareto(const base_uint& b) const;
    bool equalto(uint64_t b) const;

    friend inline const base_uint operator+(const base_uint& a, const base_uint& b) { return base_uint(a) += b; }
    friend inline const base_uint operator-(const base_uint& a, const base_uint& b) { return base_uint(a) -= b; }
    friend inline const base_uint operator*(const base_uint& a, const base_uint& b) { return base_uint(a) *= b; }
    friend inline const base_uint operator/(const base_uint& a, const base_uint& b) { return base_uint(a) /= b; }
    friend inline const base_uint operator|(const base_uint& a, const base_uint& b) { return base_uint(a) |= b; }
    friend inline const base_uint operator&(const base_uint& a, const base_uint& b) { return base_uint(a) &= b; }
    friend inline const base_uint operator^(const base_uint& a, const base_uint& b) { return base_uint(a) ^= b; }
    friend inline const base_uint operator>>(const base_uint& a, int shift) { return base_uint(a) >>= shift; }
    friend inline const base_uint operator<<(const base_uint& a, int shift) { return base_uint(a) <<= shift; }
    friend inline const base_uint operator*(const base_uint& a, uint32_t b) { return base_uint(a) *= b; }
    friend inline bool operator==(const base_uint& a, const base_uint& b) { return memcmp(a.pn, b.pn, sizeof(a.pn)) == 0; }
    friend inline bool operator!=(const base_uint& a, const base_uint& b) { return memcmp(a.pn, b.pn, sizeof(a.pn)) != 0; }
    friend inline bool operator>(const base_uint& a, const base_uint& b) { return a.compareto(b) > 0; }
    friend inline bool operator<(const base_uint& a, const base_uint& b) { return a.compareto(b) < 0; }
    friend inline bool operator>=(const base_uint& a, const base_uint& b) { return a.compareto(b) >= 0; }
    friend inline bool operator<=(const base_uint& a, const base_uint& b) { return a.compareto(b) <= 0; }
    friend inline bool operator==(const base_uint& a, uint64_t b) { return a.equalto(b); }
    friend inline bool operator!=(const base_uint& a, uint64_t b) { return !a.equalto(b); }

    std::string gethex() const;
    void sethex(const char* psz);
    void sethex(const std::string& str);
    std::string tostring() const;

    unsigned int size() const
    {
        return sizeof(pn);
    }

    /**
     * returns the position of the highest bit set plus one, or zero if the
     * value is zero.
     */
    unsigned int bits() const;

    uint64_t getlow64() const
    {
        assert(width >= 2);
        return pn[0] | (uint64_t)pn[1] << 32;
    }
};

/** 256-bit unsigned big integer. */
class arith_uint256 : public base_uint<256> {
public:
    arith_uint256() {}
    arith_uint256(const base_uint<256>& b) : base_uint<256>(b) {}
    arith_uint256(uint64_t b) : base_uint<256>(b) {}
    explicit arith_uint256(const std::string& str) : base_uint<256>(str) {}

    /**
     * the "compact" format is a representation of a whole
     * number n using an unsigned 32bit number similar to a
     * floating point format.
     * the most significant 8 bits are the unsigned exponent of base 256.
     * this exponent can be thought of as "number of bytes of n".
     * the lower 23 bits are the mantissa.
     * bit number 24 (0x800000) represents the sign of n.
     * n = (-1^sign) * mantissa * 256^(exponent-3)
     *
     * satoshi's original implementation used bn_bn2mpi() and bn_mpi2bn().
     * mpi uses the most significant bit of the first byte as sign.
     * thus 0x1234560000 is compact (0x05123456)
     * and  0xc0de000000 is compact (0x0600c0de)
     *
     * moorecoin only uses this "compact" format for encoding difficulty
     * targets, which are unsigned 256bit quantities.  thus, all the
     * complexities of the sign bit and using base 256 are probably an
     * implementation accident.
     */
    arith_uint256& setcompact(uint32_t ncompact, bool *pfnegative = null, bool *pfoverflow = null);
    uint32_t getcompact(bool fnegative = false) const;

    friend uint256 arithtouint256(const arith_uint256 &);
    friend arith_uint256 uinttoarith256(const uint256 &);
};

uint256 arithtouint256(const arith_uint256 &);
arith_uint256 uinttoarith256(const uint256 &);

#endif // moorecoin_arith_uint256_h
