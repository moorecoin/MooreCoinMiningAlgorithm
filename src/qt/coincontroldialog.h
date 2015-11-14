// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_coincontroldialog_h
#define moorecoin_qt_coincontroldialog_h

#include "amount.h"

#include <qabstractbutton>
#include <qaction>
#include <qdialog>
#include <qlist>
#include <qmenu>
#include <qpoint>
#include <qstring>
#include <qtreewidgetitem>

class walletmodel;

class ccoincontrol;
class ctxmempool;

namespace ui {
    class coincontroldialog;
}

#define asymp_utf8 "\xe2\x89\x88"

class coincontroldialog : public qdialog
{
    q_object

public:
    explicit coincontroldialog(qwidget *parent = 0);
    ~coincontroldialog();

    void setmodel(walletmodel *model);

    // static because also called from sendcoinsdialog
    static void updatelabels(walletmodel*, qdialog*);
    static qstring getprioritylabel(double dpriority, double mempoolestimatepriority);

    static qlist<camount> payamounts;
    static ccoincontrol *coincontrol;
    static bool fsubtractfeefromamount;

private:
    ui::coincontroldialog *ui;
    walletmodel *model;
    int sortcolumn;
    qt::sortorder sortorder;

    qmenu *contextmenu;
    qtreewidgetitem *contextmenuitem;
    qaction *copytransactionhashaction;
    qaction *lockaction;
    qaction *unlockaction;

    qstring strpad(qstring, int, qstring);
    void sortview(int, qt::sortorder);
    void updateview();

    enum
    {
        column_checkbox,
        column_amount,
        column_label,
        column_address,
        column_date,
        column_confirmations,
        column_priority,
        column_txhash,
        column_vout_index,
        column_amount_int64,
        column_priority_int64,
        column_date_int64
    };

    // some columns have a hidden column containing the value used for sorting
    int getmappedcolumn(int column, bool fvisiblecolumn = true)
    {
        if (fvisiblecolumn)
        {
            if (column == column_amount_int64)
                return column_amount;
            else if (column == column_priority_int64)
                return column_priority;
            else if (column == column_date_int64)
                return column_date;
        }
        else
        {
            if (column == column_amount)
                return column_amount_int64;
            else if (column == column_priority)
                return column_priority_int64;
            else if (column == column_date)
                return column_date_int64;
        }

        return column;
    }

private slots:
    void showmenu(const qpoint &);
    void copyamount();
    void copylabel();
    void copyaddress();
    void copytransactionhash();
    void lockcoin();
    void unlockcoin();
    void clipboardquantity();
    void clipboardamount();
    void clipboardfee();
    void clipboardafterfee();
    void clipboardbytes();
    void clipboardpriority();
    void clipboardlowoutput();
    void clipboardchange();
    void radiotreemode(bool);
    void radiolistmode(bool);
    void viewitemchanged(qtreewidgetitem*, int);
    void headersectionclicked(int);
    void buttonboxclicked(qabstractbutton*);
    void buttonselectallclicked();
    void updatelabellocked();
};

#endif // moorecoin_qt_coincontroldialog_h
