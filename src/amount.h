// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_amount_h
#define moorecoin_amount_h

#include "serialize.h"

#include <stdlib.h>
#include <string>

typedef int64_t camount;

static const camount coin = 100000000;
static const camount cent = 1000000;

/** no amount larger than this (in satoshi) is valid.
 *
 * note that this constant is *not* the total money supply, which in moorecoin
 * currently happens to be less than 21,000,000 btc for various reasons, but
 * rather a sanity check. as this sanity check is used by consensus-critical
 * validation code, the exact value of the max_money constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
static const camount max_money = 21000000 * coin;
inline bool moneyrange(const camount& nvalue) { return (nvalue >= 0 && nvalue <= max_money); }

/** type-safe wrapper class to for fee rates
 * (how much to pay based on transaction size)
 */
class cfeerate
{
private:
    camount nsatoshisperk; // unit is satoshis-per-1,000-bytes
public:
    cfeerate() : nsatoshisperk(0) { }
    explicit cfeerate(const camount& _nsatoshisperk): nsatoshisperk(_nsatoshisperk) { }
    cfeerate(const camount& nfeepaid, size_t nsize);
    cfeerate(const cfeerate& other) { nsatoshisperk = other.nsatoshisperk; }

    camount getfee(size_t size) const; // unit returned is satoshis
    camount getfeeperk() const { return getfee(1000); } // satoshis-per-1000-bytes

    friend bool operator<(const cfeerate& a, const cfeerate& b) { return a.nsatoshisperk < b.nsatoshisperk; }
    friend bool operator>(const cfeerate& a, const cfeerate& b) { return a.nsatoshisperk > b.nsatoshisperk; }
    friend bool operator==(const cfeerate& a, const cfeerate& b) { return a.nsatoshisperk == b.nsatoshisperk; }
    friend bool operator<=(const cfeerate& a, const cfeerate& b) { return a.nsatoshisperk <= b.nsatoshisperk; }
    friend bool operator>=(const cfeerate& a, const cfeerate& b) { return a.nsatoshisperk >= b.nsatoshisperk; }
    std::string tostring() const;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(nsatoshisperk);
    }
};

#endif //  moorecoin_amount_h
