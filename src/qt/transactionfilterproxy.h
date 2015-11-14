// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_transactionfilterproxy_h
#define moorecoin_qt_transactionfilterproxy_h

#include "amount.h"

#include <qdatetime>
#include <qsortfilterproxymodel>

/** filter the transaction list according to pre-specified rules. */
class transactionfilterproxy : public qsortfilterproxymodel
{
    q_object

public:
    explicit transactionfilterproxy(qobject *parent = 0);

    /** earliest date that can be represented (far in the past) */
    static const qdatetime min_date;
    /** last date that can be represented (far in the future) */
    static const qdatetime max_date;
    /** type filter bit field (all types) */
    static const quint32 all_types = 0xffffffff;

    static quint32 type(int type) { return 1<<type; }

    enum watchonlyfilter
    {
        watchonlyfilter_all,
        watchonlyfilter_yes,
        watchonlyfilter_no
    };

    void setdaterange(const qdatetime &from, const qdatetime &to);
    void setaddressprefix(const qstring &addrprefix);
    /**
      @note type filter takes a bit field created with type() or all_types
     */
    void settypefilter(quint32 modes);
    void setminamount(const camount& minimum);
    void setwatchonlyfilter(watchonlyfilter filter);

    /** set maximum number of rows returned, -1 if unlimited. */
    void setlimit(int limit);

    /** set whether to show conflicted transactions. */
    void setshowinactive(bool showinactive);

    int rowcount(const qmodelindex &parent = qmodelindex()) const;

protected:
    bool filteracceptsrow(int source_row, const qmodelindex & source_parent) const;

private:
    qdatetime datefrom;
    qdatetime dateto;
    qstring addrprefix;
    quint32 typefilter;
    watchonlyfilter watchonlyfilter;
    camount minamount;
    int limitrows;
    bool showinactive;
};

#endif // moorecoin_qt_transactionfilterproxy_h
