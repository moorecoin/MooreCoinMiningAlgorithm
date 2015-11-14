// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_primitives_transaction_h
#define moorecoin_primitives_transaction_h

#include "amount.h"
#include "script/script.h"
#include "serialize.h"
#include "uint256.h"

/** an outpoint - a combination of a transaction hash and an index n into its vout */
class coutpoint
{
public:
    uint256 hash;
    uint32_t n;

    coutpoint() { setnull(); }
    coutpoint(uint256 hashin, uint32_t nin) { hash = hashin; n = nin; }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(hash);
        readwrite(n);
    }

    void setnull() { hash.setnull(); n = (uint32_t) -1; }
    bool isnull() const { return (hash.isnull() && n == (uint32_t) -1); }

    friend bool operator<(const coutpoint& a, const coutpoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const coutpoint& a, const coutpoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const coutpoint& a, const coutpoint& b)
    {
        return !(a == b);
    }

    std::string tostring() const;
};

/** an input of a transaction.  it contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class ctxin
{
public:
    coutpoint prevout;
    cscript scriptsig;
    uint32_t nsequence;

    ctxin()
    {
        nsequence = std::numeric_limits<unsigned int>::max();
    }

    explicit ctxin(coutpoint prevoutin, cscript scriptsigin=cscript(), uint32_t nsequencein=std::numeric_limits<unsigned int>::max());
    ctxin(uint256 hashprevtx, uint32_t nout, cscript scriptsigin=cscript(), uint32_t nsequencein=std::numeric_limits<uint32_t>::max());

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(prevout);
        readwrite(scriptsig);
        readwrite(nsequence);
    }

    bool isfinal() const
    {
        return (nsequence == std::numeric_limits<uint32_t>::max());
    }

    friend bool operator==(const ctxin& a, const ctxin& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptsig == b.scriptsig &&
                a.nsequence == b.nsequence);
    }

    friend bool operator!=(const ctxin& a, const ctxin& b)
    {
        return !(a == b);
    }

    std::string tostring() const;
};

/** an output of a transaction.  it contains the public key that the next input
 * must be able to sign with to claim it.
 */
class ctxout
{
public:
    camount nvalue;
    cscript scriptpubkey;

    ctxout()
    {
        setnull();
    }

    ctxout(const camount& nvaluein, cscript scriptpubkeyin);

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(nvalue);
        readwrite(scriptpubkey);
    }

    void setnull()
    {
        nvalue = -1;
        scriptpubkey.clear();
    }

    bool isnull() const
    {
        return (nvalue == -1);
    }

    uint256 gethash() const;

    camount getdustthreshold(const cfeerate &minrelaytxfee) const
    {
        // "dust" is defined in terms of ctransaction::minrelaytxfee,
        // which has units satoshis-per-kilobyte.
        // if you'd pay more than 1/3 in fees
        // to spend something, then we consider it dust.
        // a typical txout is 34 bytes big, and will
        // need a ctxin of at least 148 bytes to spend:
        // so dust is a txout less than 546 satoshis 
        // with default minrelaytxfee.
        size_t nsize = getserializesize(ser_disk,0)+148u;
        return 3*minrelaytxfee.getfee(nsize);
    }

    bool isdust(const cfeerate &minrelaytxfee) const
    {
        return (nvalue < getdustthreshold(minrelaytxfee));
    }

    friend bool operator==(const ctxout& a, const ctxout& b)
    {
        return (a.nvalue       == b.nvalue &&
                a.scriptpubkey == b.scriptpubkey);
    }

    friend bool operator!=(const ctxout& a, const ctxout& b)
    {
        return !(a == b);
    }

    std::string tostring() const;
};

struct cmutabletransaction;

/** the basic transaction that is broadcasted on the network and contained in
 * blocks.  a transaction can contain multiple inputs and outputs.
 */
class ctransaction
{
private:
    /** memory only. */
    const uint256 hash;
    void updatehash() const;

public:
    static const int32_t current_version=1;

    // the local variables are made const to prevent unintended modification
    // without updating the cached hash value. however, ctransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. this is safe, as they update the entire
    // structure, including the hash.
    const int32_t nversion;
    const std::vector<ctxin> vin;
    const std::vector<ctxout> vout;
    const uint32_t nlocktime;

    /** construct a ctransaction that qualifies as isnull() */
    ctransaction();

    /** convert a cmutabletransaction into a ctransaction. */
    ctransaction(const cmutabletransaction &tx);

    ctransaction& operator=(const ctransaction& tx);

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(*const_cast<int32_t*>(&this->nversion));
        nversion = this->nversion;
        readwrite(*const_cast<std::vector<ctxin>*>(&vin));
        readwrite(*const_cast<std::vector<ctxout>*>(&vout));
        readwrite(*const_cast<uint32_t*>(&nlocktime));
        if (ser_action.forread())
            updatehash();
    }

    bool isnull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& gethash() const {
        return hash;
    }

    // return sum of txouts.
    camount getvalueout() const;
    // getvaluein() is a method on ccoinsviewcache, because
    // inputs must be known to compute value in.

    // compute priority, given priority of inputs and (optionally) tx size
    double computepriority(double dpriorityinputs, unsigned int ntxsize=0) const;

    // compute modified tx size for priority calculation (optionally given tx size)
    unsigned int calculatemodifiedsize(unsigned int ntxsize=0) const;

    bool iscoinbase() const
    {
        return (vin.size() == 1 && vin[0].prevout.isnull());
    }

    friend bool operator==(const ctransaction& a, const ctransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const ctransaction& a, const ctransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string tostring() const;
};

/** a mutable version of ctransaction. */
struct cmutabletransaction
{
    int32_t nversion;
    std::vector<ctxin> vin;
    std::vector<ctxout> vout;
    uint32_t nlocktime;

    cmutabletransaction();
    cmutabletransaction(const ctransaction& tx);

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(this->nversion);
        nversion = this->nversion;
        readwrite(vin);
        readwrite(vout);
        readwrite(nlocktime);
    }

    /** compute the hash of this cmutabletransaction. this is computed on the
     * fly, as opposed to gethash() in ctransaction, which uses a cached result.
     */
    uint256 gethash() const;
};

#endif // moorecoin_primitives_transaction_h
