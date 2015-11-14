// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_recentrequeststablemodel_h
#define moorecoin_qt_recentrequeststablemodel_h

#include "walletmodel.h"

#include <qabstracttablemodel>
#include <qstringlist>
#include <qdatetime>

class cwallet;

class recentrequestentry
{
public:
    recentrequestentry() : nversion(recentrequestentry::current_version), id(0) { }

    static const int current_version = 1;
    int nversion;
    int64_t id;
    qdatetime date;
    sendcoinsrecipient recipient;

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        unsigned int ndate = date.totime_t();

        readwrite(this->nversion);
        nversion = this->nversion;
        readwrite(id);
        readwrite(ndate);
        readwrite(recipient);

        if (ser_action.forread())
            date = qdatetime::fromtime_t(ndate);
    }
};

class recentrequestentrylessthan
{
public:
    recentrequestentrylessthan(int ncolumn, qt::sortorder forder):
        column(ncolumn), order(forder) {}
    bool operator()(recentrequestentry &left, recentrequestentry &right) const;

private:
    int column;
    qt::sortorder order;
};

/** model for list of recently generated payment requests / moorecoin: uris.
 * part of wallet model.
 */
class recentrequeststablemodel: public qabstracttablemodel
{
    q_object

public:
    explicit recentrequeststablemodel(cwallet *wallet, walletmodel *parent);
    ~recentrequeststablemodel();

    enum columnindex {
        date = 0,
        label = 1,
        message = 2,
        amount = 3,
        number_of_columns
    };

    /** @name methods overridden from qabstracttablemodel
        @{*/
    int rowcount(const qmodelindex &parent) const;
    int columncount(const qmodelindex &parent) const;
    qvariant data(const qmodelindex &index, int role) const;
    bool setdata(const qmodelindex &index, const qvariant &value, int role);
    qvariant headerdata(int section, qt::orientation orientation, int role) const;
    qmodelindex index(int row, int column, const qmodelindex &parent) const;
    bool removerows(int row, int count, const qmodelindex &parent = qmodelindex());
    qt::itemflags flags(const qmodelindex &index) const;
    /*@}*/

    const recentrequestentry &entry(int row) const { return list[row]; }
    void addnewrequest(const sendcoinsrecipient &recipient);
    void addnewrequest(const std::string &recipient);
    void addnewrequest(recentrequestentry &recipient);

public slots:
    void sort(int column, qt::sortorder order = qt::ascendingorder);
    void updatedisplayunit();

private:
    walletmodel *walletmodel;
    qstringlist columns;
    qlist<recentrequestentry> list;
    int64_t nreceiverequestsmaxid;

    /** updates the column title to "amount (displayunit)" and emits headerdatachanged() signal for table headers to react. */
    void updateamountcolumntitle();
    /** gets title for amount column including current display unit if optionsmodel reference available. */
    qstring getamounttitle();
};

#endif // moorecoin_qt_recentrequeststablemodel_h
