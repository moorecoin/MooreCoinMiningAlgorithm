// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_script_script_h
#define moorecoin_script_script_h

#include "crypto/common.h"

#include <assert.h>
#include <climits>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

static const unsigned int max_script_element_size = 520; // bytes

template <typename t>
std::vector<unsigned char> tobytevector(const t& in)
{
    return std::vector<unsigned char>(in.begin(), in.end());
}

/** script opcodes */
enum opcodetype
{
    // push value
    op_0 = 0x00,
    op_false = op_0,
    op_pushdata1 = 0x4c,
    op_pushdata2 = 0x4d,
    op_pushdata4 = 0x4e,
    op_1negate = 0x4f,
    op_reserved = 0x50,
    op_1 = 0x51,
    op_true=op_1,
    op_2 = 0x52,
    op_3 = 0x53,
    op_4 = 0x54,
    op_5 = 0x55,
    op_6 = 0x56,
    op_7 = 0x57,
    op_8 = 0x58,
    op_9 = 0x59,
    op_10 = 0x5a,
    op_11 = 0x5b,
    op_12 = 0x5c,
    op_13 = 0x5d,
    op_14 = 0x5e,
    op_15 = 0x5f,
    op_16 = 0x60,

    // control
    op_nop = 0x61,
    op_ver = 0x62,
    op_if = 0x63,
    op_notif = 0x64,
    op_verif = 0x65,
    op_vernotif = 0x66,
    op_else = 0x67,
    op_endif = 0x68,
    op_verify = 0x69,
    op_return = 0x6a,

    // stack ops
    op_toaltstack = 0x6b,
    op_fromaltstack = 0x6c,
    op_2drop = 0x6d,
    op_2dup = 0x6e,
    op_3dup = 0x6f,
    op_2over = 0x70,
    op_2rot = 0x71,
    op_2swap = 0x72,
    op_ifdup = 0x73,
    op_depth = 0x74,
    op_drop = 0x75,
    op_dup = 0x76,
    op_nip = 0x77,
    op_over = 0x78,
    op_pick = 0x79,
    op_roll = 0x7a,
    op_rot = 0x7b,
    op_swap = 0x7c,
    op_tuck = 0x7d,

    // splice ops
    op_cat = 0x7e,
    op_substr = 0x7f,
    op_left = 0x80,
    op_right = 0x81,
    op_size = 0x82,

    // bit logic
    op_invert = 0x83,
    op_and = 0x84,
    op_or = 0x85,
    op_xor = 0x86,
    op_equal = 0x87,
    op_equalverify = 0x88,
    op_reserved1 = 0x89,
    op_reserved2 = 0x8a,

    // numeric
    op_1add = 0x8b,
    op_1sub = 0x8c,
    op_2mul = 0x8d,
    op_2div = 0x8e,
    op_negate = 0x8f,
    op_abs = 0x90,
    op_not = 0x91,
    op_0notequal = 0x92,

    op_add = 0x93,
    op_sub = 0x94,
    op_mul = 0x95,
    op_div = 0x96,
    op_mod = 0x97,
    op_lshift = 0x98,
    op_rshift = 0x99,

    op_booland = 0x9a,
    op_boolor = 0x9b,
    op_numequal = 0x9c,
    op_numequalverify = 0x9d,
    op_numnotequal = 0x9e,
    op_lessthan = 0x9f,
    op_greaterthan = 0xa0,
    op_lessthanorequal = 0xa1,
    op_greaterthanorequal = 0xa2,
    op_min = 0xa3,
    op_max = 0xa4,

    op_within = 0xa5,

    // crypto
    op_ripemd160 = 0xa6,
    op_sha1 = 0xa7,
    op_sha256 = 0xa8,
    op_hash160 = 0xa9,
    op_hash256 = 0xaa,
    op_codeseparator = 0xab,
    op_checksig = 0xac,
    op_checksigverify = 0xad,
    op_checkmultisig = 0xae,
    op_checkmultisigverify = 0xaf,

    // expansion
    op_nop1 = 0xb0,
    op_nop2 = 0xb1,
    op_nop3 = 0xb2,
    op_nop4 = 0xb3,
    op_nop5 = 0xb4,
    op_nop6 = 0xb5,
    op_nop7 = 0xb6,
    op_nop8 = 0xb7,
    op_nop9 = 0xb8,
    op_nop10 = 0xb9,


    // template matching params
    op_smalldata = 0xf9,
    op_smallinteger = 0xfa,
    op_pubkeys = 0xfb,
    op_pubkeyhash = 0xfd,
    op_pubkey = 0xfe,

    op_invalidopcode = 0xff,
};

const char* getopname(opcodetype opcode);

class scriptnum_error : public std::runtime_error
{
public:
    explicit scriptnum_error(const std::string& str) : std::runtime_error(str) {}
};

class cscriptnum
{
/**
 * numeric opcodes (op_1add, etc) are restricted to operating on 4-byte integers.
 * the semantics are subtle, though: operands must be in the range [-2^31 +1...2^31 -1],
 * but results may overflow (and are valid as long as they are not used in a subsequent
 * numeric operation). cscriptnum enforces those semantics by storing results as
 * an int64 and allowing out-of-range values to be returned as a vector of bytes but
 * throwing an exception if arithmetic is done or the result is interpreted as an integer.
 */
public:

    explicit cscriptnum(const int64_t& n)
    {
        m_value = n;
    }

    explicit cscriptnum(const std::vector<unsigned char>& vch, bool frequireminimal)
    {
        if (vch.size() > nmaxnumsize) {
            throw scriptnum_error("script number overflow");
        }
        if (frequireminimal && vch.size() > 0) {
            // check that the number is encoded with the minimum possible
            // number of bytes.
            //
            // if the most-significant-byte - excluding the sign bit - is zero
            // then we're not minimal. note how this test also rejects the
            // negative-zero encoding, 0x80.
            if ((vch.back() & 0x7f) == 0) {
                // one exception: if there's more than one byte and the most
                // significant bit of the second-most-significant-byte is set
                // it would conflict with the sign bit. an example of this case
                // is +-255, which encode to 0xff00 and 0xff80 respectively.
                // (big-endian).
                if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0) {
                    throw scriptnum_error("non-minimally encoded script number");
                }
            }
        }
        m_value = set_vch(vch);
    }

    inline bool operator==(const int64_t& rhs) const    { return m_value == rhs; }
    inline bool operator!=(const int64_t& rhs) const    { return m_value != rhs; }
    inline bool operator<=(const int64_t& rhs) const    { return m_value <= rhs; }
    inline bool operator< (const int64_t& rhs) const    { return m_value <  rhs; }
    inline bool operator>=(const int64_t& rhs) const    { return m_value >= rhs; }
    inline bool operator> (const int64_t& rhs) const    { return m_value >  rhs; }

    inline bool operator==(const cscriptnum& rhs) const { return operator==(rhs.m_value); }
    inline bool operator!=(const cscriptnum& rhs) const { return operator!=(rhs.m_value); }
    inline bool operator<=(const cscriptnum& rhs) const { return operator<=(rhs.m_value); }
    inline bool operator< (const cscriptnum& rhs) const { return operator< (rhs.m_value); }
    inline bool operator>=(const cscriptnum& rhs) const { return operator>=(rhs.m_value); }
    inline bool operator> (const cscriptnum& rhs) const { return operator> (rhs.m_value); }

    inline cscriptnum operator+(   const int64_t& rhs)    const { return cscriptnum(m_value + rhs);}
    inline cscriptnum operator-(   const int64_t& rhs)    const { return cscriptnum(m_value - rhs);}
    inline cscriptnum operator+(   const cscriptnum& rhs) const { return operator+(rhs.m_value);   }
    inline cscriptnum operator-(   const cscriptnum& rhs) const { return operator-(rhs.m_value);   }

    inline cscriptnum& operator+=( const cscriptnum& rhs)       { return operator+=(rhs.m_value);  }
    inline cscriptnum& operator-=( const cscriptnum& rhs)       { return operator-=(rhs.m_value);  }

    inline cscriptnum operator-()                         const
    {
        assert(m_value != std::numeric_limits<int64_t>::min());
        return cscriptnum(-m_value);
    }

    inline cscriptnum& operator=( const int64_t& rhs)
    {
        m_value = rhs;
        return *this;
    }

    inline cscriptnum& operator+=( const int64_t& rhs)
    {
        assert(rhs == 0 || (rhs > 0 && m_value <= std::numeric_limits<int64_t>::max() - rhs) ||
                           (rhs < 0 && m_value >= std::numeric_limits<int64_t>::min() - rhs));
        m_value += rhs;
        return *this;
    }

    inline cscriptnum& operator-=( const int64_t& rhs)
    {
        assert(rhs == 0 || (rhs > 0 && m_value >= std::numeric_limits<int64_t>::min() + rhs) ||
                           (rhs < 0 && m_value <= std::numeric_limits<int64_t>::max() + rhs));
        m_value -= rhs;
        return *this;
    }

    int getint() const
    {
        if (m_value > std::numeric_limits<int>::max())
            return std::numeric_limits<int>::max();
        else if (m_value < std::numeric_limits<int>::min())
            return std::numeric_limits<int>::min();
        return m_value;
    }

    std::vector<unsigned char> getvch() const
    {
        return serialize(m_value);
    }

    static std::vector<unsigned char> serialize(const int64_t& value)
    {
        if(value == 0)
            return std::vector<unsigned char>();

        std::vector<unsigned char> result;
        const bool neg = value < 0;
        uint64_t absvalue = neg ? -value : value;

        while(absvalue)
        {
            result.push_back(absvalue & 0xff);
            absvalue >>= 8;
        }

//    - if the most significant byte is >= 0x80 and the value is positive, push a
//    new zero-byte to make the significant byte < 0x80 again.

//    - if the most significant byte is >= 0x80 and the value is negative, push a
//    new 0x80 byte that will be popped off when converting to an integral.

//    - if the most significant byte is < 0x80 and the value is negative, add
//    0x80 to it, since it will be subtracted and interpreted as a negative when
//    converting to an integral.

        if (result.back() & 0x80)
            result.push_back(neg ? 0x80 : 0);
        else if (neg)
            result.back() |= 0x80;

        return result;
    }

    static const size_t nmaxnumsize = 4;

private:
    static int64_t set_vch(const std::vector<unsigned char>& vch)
    {
      if (vch.empty())
          return 0;

      int64_t result = 0;
      for (size_t i = 0; i != vch.size(); ++i)
          result |= static_cast<int64_t>(vch[i]) << 8*i;

      // if the input vector's most significant byte is 0x80, remove it from
      // the result's msb and return a negative.
      if (vch.back() & 0x80)
          return -((int64_t)(result & ~(0x80ull << (8 * (vch.size() - 1)))));

      return result;
    }

    int64_t m_value;
};

/** serialized script, used inside transaction inputs and outputs */
class cscript : public std::vector<unsigned char>
{
protected:
    cscript& push_int64(int64_t n)
    {
        if (n == -1 || (n >= 1 && n <= 16))
        {
            push_back(n + (op_1 - 1));
        }
        else if (n == 0)
        {
            push_back(op_0);
        }
        else
        {
            *this << cscriptnum::serialize(n);
        }
        return *this;
    }
public:
    cscript() { }
    cscript(const cscript& b) : std::vector<unsigned char>(b.begin(), b.end()) { }
    cscript(const_iterator pbegin, const_iterator pend) : std::vector<unsigned char>(pbegin, pend) { }
    cscript(const unsigned char* pbegin, const unsigned char* pend) : std::vector<unsigned char>(pbegin, pend) { }

    cscript& operator+=(const cscript& b)
    {
        insert(end(), b.begin(), b.end());
        return *this;
    }

    friend cscript operator+(const cscript& a, const cscript& b)
    {
        cscript ret = a;
        ret += b;
        return ret;
    }

    cscript(int64_t b)        { operator<<(b); }

    explicit cscript(opcodetype b)     { operator<<(b); }
    explicit cscript(const cscriptnum& b) { operator<<(b); }
    explicit cscript(const std::vector<unsigned char>& b) { operator<<(b); }


    cscript& operator<<(int64_t b) { return push_int64(b); }

    cscript& operator<<(opcodetype opcode)
    {
        if (opcode < 0 || opcode > 0xff)
            throw std::runtime_error("cscript::operator<<(): invalid opcode");
        insert(end(), (unsigned char)opcode);
        return *this;
    }

    cscript& operator<<(const cscriptnum& b)
    {
        *this << b.getvch();
        return *this;
    }

    cscript& operator<<(const std::vector<unsigned char>& b)
    {
        if (b.size() < op_pushdata1)
        {
            insert(end(), (unsigned char)b.size());
        }
        else if (b.size() <= 0xff)
        {
            insert(end(), op_pushdata1);
            insert(end(), (unsigned char)b.size());
        }
        else if (b.size() <= 0xffff)
        {
            insert(end(), op_pushdata2);
            uint8_t data[2];
            writele16(data, b.size());
            insert(end(), data, data + sizeof(data));
        }
        else
        {
            insert(end(), op_pushdata4);
            uint8_t data[4];
            writele32(data, b.size());
            insert(end(), data, data + sizeof(data));
        }
        insert(end(), b.begin(), b.end());
        return *this;
    }

    cscript& operator<<(const cscript& b)
    {
        // i'm not sure if this should push the script or concatenate scripts.
        // if there's ever a use for pushing a script onto a script, delete this member fn
        assert(!"warning: pushing a cscript onto a cscript with << is probably not intended, use + to concatenate!");
        return *this;
    }


    bool getop(iterator& pc, opcodetype& opcoderet, std::vector<unsigned char>& vchret)
    {
         // wrapper so it can be called with either iterator or const_iterator
         const_iterator pc2 = pc;
         bool fret = getop2(pc2, opcoderet, &vchret);
         pc = begin() + (pc2 - begin());
         return fret;
    }

    bool getop(iterator& pc, opcodetype& opcoderet)
    {
         const_iterator pc2 = pc;
         bool fret = getop2(pc2, opcoderet, null);
         pc = begin() + (pc2 - begin());
         return fret;
    }

    bool getop(const_iterator& pc, opcodetype& opcoderet, std::vector<unsigned char>& vchret) const
    {
        return getop2(pc, opcoderet, &vchret);
    }

    bool getop(const_iterator& pc, opcodetype& opcoderet) const
    {
        return getop2(pc, opcoderet, null);
    }

    bool getop2(const_iterator& pc, opcodetype& opcoderet, std::vector<unsigned char>* pvchret) const
    {
        opcoderet = op_invalidopcode;
        if (pvchret)
            pvchret->clear();
        if (pc >= end())
            return false;

        // read instruction
        if (end() - pc < 1)
            return false;
        unsigned int opcode = *pc++;

        // immediate operand
        if (opcode <= op_pushdata4)
        {
            unsigned int nsize = 0;
            if (opcode < op_pushdata1)
            {
                nsize = opcode;
            }
            else if (opcode == op_pushdata1)
            {
                if (end() - pc < 1)
                    return false;
                nsize = *pc++;
            }
            else if (opcode == op_pushdata2)
            {
                if (end() - pc < 2)
                    return false;
                nsize = readle16(&pc[0]);
                pc += 2;
            }
            else if (opcode == op_pushdata4)
            {
                if (end() - pc < 4)
                    return false;
                nsize = readle32(&pc[0]);
                pc += 4;
            }
            if (end() - pc < 0 || (unsigned int)(end() - pc) < nsize)
                return false;
            if (pvchret)
                pvchret->assign(pc, pc + nsize);
            pc += nsize;
        }

        opcoderet = (opcodetype)opcode;
        return true;
    }

    /** encode/decode small integers: */
    static int decodeop_n(opcodetype opcode)
    {
        if (opcode == op_0)
            return 0;
        assert(opcode >= op_1 && opcode <= op_16);
        return (int)opcode - (int)(op_1 - 1);
    }
    static opcodetype encodeop_n(int n)
    {
        assert(n >= 0 && n <= 16);
        if (n == 0)
            return op_0;
        return (opcodetype)(op_1+n-1);
    }

    int findanddelete(const cscript& b)
    {
        int nfound = 0;
        if (b.empty())
            return nfound;
        iterator pc = begin();
        opcodetype opcode;
        do
        {
            while (end() - pc >= (long)b.size() && memcmp(&pc[0], &b[0], b.size()) == 0)
            {
                pc = erase(pc, pc + b.size());
                ++nfound;
            }
        }
        while (getop(pc, opcode));
        return nfound;
    }
    int find(opcodetype op) const
    {
        int nfound = 0;
        opcodetype opcode;
        for (const_iterator pc = begin(); pc != end() && getop(pc, opcode);)
            if (opcode == op)
                ++nfound;
        return nfound;
    }

    /**
     * pre-version-0.6, moorecoin always counted checkmultisigs
     * as 20 sigops. with pay-to-script-hash, that changed:
     * checkmultisigs serialized in scriptsigs are
     * counted more accurately, assuming they are of the form
     *  ... op_n checkmultisig ...
     */
    unsigned int getsigopcount(bool faccurate) const;

    /**
     * accurately count sigops, including sigops in
     * pay-to-script-hash transactions:
     */
    unsigned int getsigopcount(const cscript& scriptsig) const;

    bool ispaytoscripthash() const;

    /** called by isstandardtx and p2sh/bip62 verifyscript (which makes it consensus-critical). */
    bool ispushonly() const;

    /**
     * returns whether the script is guaranteed to fail at execution,
     * regardless of the initial stack. this allows outputs to be pruned
     * instantly when entering the utxo set.
     */
    bool isunspendable() const
    {
        return (size() > 0 && *begin() == op_return);
    }

    std::string tostring() const;
    void clear()
    {
        // the default std::vector::clear() does not release memory.
        std::vector<unsigned char>().swap(*this);
    }
};

#endif // moorecoin_script_script_h
