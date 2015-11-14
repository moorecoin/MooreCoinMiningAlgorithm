// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_transactionview_h
#define moorecoin_qt_transactionview_h

#include "guiutil.h"

#include <qwidget>
#include <qkeyevent>

class transactionfilterproxy;
class walletmodel;

qt_begin_namespace
class qcombobox;
class qdatetimeedit;
class qframe;
class qlineedit;
class qmenu;
class qmodelindex;
class qsignalmapper;
class qtableview;
qt_end_namespace

/** widget showing the transaction list for a wallet, including a filter row.
    using the filter row, the user can view or export a subset of the transactions.
  */
class transactionview : public qwidget
{
    q_object

public:
    explicit transactionview(qwidget *parent = 0);

    void setmodel(walletmodel *model);

    // date ranges for filter
    enum dateenum
    {
        all,
        today,
        thisweek,
        thismonth,
        lastmonth,
        thisyear,
        range
    };

    enum columnwidths {
        status_column_width = 30,
        watchonly_column_width = 23,
        date_column_width = 120,
        type_column_width = 113,
        amount_minimum_column_width = 120,
        minimum_column_width = 23
    };

private:
    walletmodel *model;
    transactionfilterproxy *transactionproxymodel;
    qtableview *transactionview;

    qcombobox *datewidget;
    qcombobox *typewidget;
    qcombobox *watchonlywidget;
    qlineedit *addresswidget;
    qlineedit *amountwidget;

    qmenu *contextmenu;
    qsignalmapper *mapperthirdpartytxurls;

    qframe *daterangewidget;
    qdatetimeedit *datefrom;
    qdatetimeedit *dateto;

    qwidget *createdaterangewidget();

    guiutil::tableviewlastcolumnresizingfixer *columnresizingfixer;

    virtual void resizeevent(qresizeevent* event);

    bool eventfilter(qobject *obj, qevent *event);

private slots:
    void contextualmenu(const qpoint &);
    void daterangechanged();
    void showdetails();
    void copyaddress();
    void editlabel();
    void copylabel();
    void copyamount();
    void copytxid();
    void openthirdpartytxurl(qstring url);
    void updatewatchonlycolumn(bool fhavewatchonly);

signals:
    void doubleclicked(const qmodelindex&);

    /**  fired when a message should be reported to the user */
    void message(const qstring &title, const qstring &message, unsigned int style);

public slots:
    void choosedate(int idx);
    void choosetype(int idx);
    void choosewatchonly(int idx);
    void changedprefix(const qstring &prefix);
    void changedamount(const qstring &amount);
    void exportclicked();
    void focustransaction(const qmodelindex&);

};

#endif // moorecoin_qt_transactionview_h
