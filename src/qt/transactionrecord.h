// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_transactionrecord_h
#define moorecoin_qt_transactionrecord_h

#include "amount.h"
#include "uint256.h"

#include <qlist>
#include <qstring>

class cwallet;
class cwallettx;

/** ui model for transaction status. the transaction status is the part of a transaction that will change over time.
 */
class transactionstatus
{
public:
    transactionstatus():
        countsforbalance(false), sortkey(""),
        matures_in(0), status(offline), depth(0), open_for(0), cur_num_blocks(-1)
    { }

    enum status {
        confirmed,          /**< have 6 or more confirmations (normal tx) or fully mature (mined tx) **/
        /// normal (sent/received) transactions
        openuntildate,      /**< transaction not yet final, waiting for date */
        openuntilblock,     /**< transaction not yet final, waiting for block */
        offline,            /**< not sent to any other nodes **/
        unconfirmed,        /**< not yet mined into a block **/
        confirming,         /**< confirmed, but waiting for the recommended number of confirmations **/
        conflicted,         /**< conflicts with other transaction or mempool **/
        /// generated (mined) transactions
        immature,           /**< mined but waiting for maturity */
        matureswarning,     /**< transaction will likely not mature because no nodes have confirmed */
        notaccepted         /**< mined but not accepted */
    };

    /// transaction counts towards available balance
    bool countsforbalance;
    /// sorting key based on status
    std::string sortkey;

    /** @name generated (mined) transactions
       @{*/
    int matures_in;
    /**@}*/

    /** @name reported status
       @{*/
    status status;
    qint64 depth;
    qint64 open_for; /**< timestamp if status==openuntildate, otherwise number
                      of additional blocks that need to be mined before
                      finalization */
    /**@}*/

    /** current number of blocks (to know whether cached status is still valid) */
    int cur_num_blocks;
};

/** ui model for a transaction. a core transaction can be represented by multiple ui transactions if it has
    multiple outputs.
 */
class transactionrecord
{
public:
    enum type
    {
        other,
        generated,
        sendtoaddress,
        sendtoother,
        recvwithaddress,
        recvfromother,
        sendtoself
    };

    /** number of confirmation recommended for accepting a transaction */
    static const int recommendednumconfirmations = 6;

    transactionrecord():
            hash(), time(0), type(other), address(""), debit(0), credit(0), idx(0)
    {
    }

    transactionrecord(uint256 hash, qint64 time):
            hash(hash), time(time), type(other), address(""), debit(0),
            credit(0), idx(0)
    {
    }

    transactionrecord(uint256 hash, qint64 time,
                type type, const std::string &address,
                const camount& debit, const camount& credit):
            hash(hash), time(time), type(type), address(address), debit(debit), credit(credit),
            idx(0)
    {
    }

    /** decompose cwallet transaction to model transaction records.
     */
    static bool showtransaction(const cwallettx &wtx);
    static qlist<transactionrecord> decomposetransaction(const cwallet *wallet, const cwallettx &wtx);

    /** @name immutable transaction attributes
      @{*/
    uint256 hash;
    qint64 time;
    type type;
    std::string address;
    camount debit;
    camount credit;
    /**@}*/

    /** subtransaction index, for sort key */
    int idx;

    /** status: can change with block chain update */
    transactionstatus status;

    /** whether the transaction was sent/received with a watch-only address */
    bool involveswatchaddress;

    /** return the unique identifier for this transaction (part) */
    qstring gettxid() const;

    /** format subtransaction id */
    static qstring formatsubtxid(const uint256 &hash, int vout);

    /** update status from core wallet tx.
     */
    void updatestatus(const cwallettx &wtx);

    /** return whether a status update is needed.
     */
    bool statusupdateneeded();
};

#endif // moorecoin_qt_transactionrecord_h
