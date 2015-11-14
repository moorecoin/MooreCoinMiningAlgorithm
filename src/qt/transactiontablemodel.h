// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_transactiontablemodel_h
#define moorecoin_qt_transactiontablemodel_h

#include "moorecoinunits.h"

#include <qabstracttablemodel>
#include <qstringlist>

class transactionrecord;
class transactiontablepriv;
class walletmodel;

class cwallet;

/** ui model for the transaction table of a wallet.
 */
class transactiontablemodel : public qabstracttablemodel
{
    q_object

public:
    explicit transactiontablemodel(cwallet* wallet, walletmodel *parent = 0);
    ~transactiontablemodel();

    enum columnindex {
        status = 0,
        watchonly = 1,
        date = 2,
        type = 3,
        toaddress = 4,
        amount = 5
    };

    /** roles to get specific information from a transaction row.
        these are independent of column.
    */
    enum roleindex {
        /** type of transaction */
        typerole = qt::userrole,
        /** date and time this transaction was created */
        daterole,
        /** watch-only boolean */
        watchonlyrole,
        /** watch-only icon */
        watchonlydecorationrole,
        /** long description (html format) */
        longdescriptionrole,
        /** address of transaction */
        addressrole,
        /** label of address related to transaction */
        labelrole,
        /** net amount of transaction */
        amountrole,
        /** unique identifier */
        txidrole,
        /** transaction hash */
        txhashrole,
        /** is transaction confirmed? */
        confirmedrole,
        /** formatted amount, without brackets when unconfirmed */
        formattedamountrole,
        /** transaction status (transactionrecord::status) */
        statusrole,
        /** unprocessed icon */
        rawdecorationrole,
    };

    int rowcount(const qmodelindex &parent) const;
    int columncount(const qmodelindex &parent) const;
    qvariant data(const qmodelindex &index, int role) const;
    qvariant headerdata(int section, qt::orientation orientation, int role) const;
    qmodelindex index(int row, int column, const qmodelindex & parent = qmodelindex()) const;
    bool processingqueuedtransactions() { return fprocessingqueuedtransactions; }

private:
    cwallet* wallet;
    walletmodel *walletmodel;
    qstringlist columns;
    transactiontablepriv *priv;
    bool fprocessingqueuedtransactions;

    void subscribetocoresignals();
    void unsubscribefromcoresignals();

    qstring lookupaddress(const std::string &address, bool tooltip) const;
    qvariant addresscolor(const transactionrecord *wtx) const;
    qstring formattxstatus(const transactionrecord *wtx) const;
    qstring formattxdate(const transactionrecord *wtx) const;
    qstring formattxtype(const transactionrecord *wtx) const;
    qstring formattxtoaddress(const transactionrecord *wtx, bool tooltip) const;
    qstring formattxamount(const transactionrecord *wtx, bool showunconfirmed=true, moorecoinunits::separatorstyle separators=moorecoinunits::separatorstandard) const;
    qstring formattooltip(const transactionrecord *rec) const;
    qvariant txstatusdecoration(const transactionrecord *wtx) const;
    qvariant txwatchonlydecoration(const transactionrecord *wtx) const;
    qvariant txaddressdecoration(const transactionrecord *wtx) const;

public slots:
    /* new transaction, or transaction changed status */
    void updatetransaction(const qstring &hash, int status, bool showtransaction);
    void updateconfirmations();
    void updatedisplayunit();
    /** updates the column title to "amount (displayunit)" and emits headerdatachanged() signal for table headers to react. */
    void updateamountcolumntitle();
    /* needed to update fprocessingqueuedtransactions through a queuedconnection */
    void setprocessingqueuedtransactions(bool value) { fprocessingqueuedtransactions = value; }

    friend class transactiontablepriv;
};

#endif // moorecoin_qt_transactiontablemodel_h
