// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_receivecoinsdialog_h
#define moorecoin_qt_receivecoinsdialog_h

#include "guiutil.h"

#include <qdialog>
#include <qheaderview>
#include <qitemselection>
#include <qkeyevent>
#include <qmenu>
#include <qpoint>
#include <qvariant>

class optionsmodel;
class walletmodel;

namespace ui {
    class receivecoinsdialog;
}

qt_begin_namespace
class qmodelindex;
qt_end_namespace

/** dialog for requesting payment of moorecoins */
class receivecoinsdialog : public qdialog
{
    q_object

public:
    enum columnwidths {
        date_column_width = 130,
        label_column_width = 120,
        amount_minimum_column_width = 160,
        minimum_column_width = 130
    };

    explicit receivecoinsdialog(qwidget *parent = 0);
    ~receivecoinsdialog();

    void setmodel(walletmodel *model);

public slots:
    void clear();
    void reject();
    void accept();

protected:
    virtual void keypressevent(qkeyevent *event);

private:
    ui::receivecoinsdialog *ui;
    guiutil::tableviewlastcolumnresizingfixer *columnresizingfixer;
    walletmodel *model;
    qmenu *contextmenu;
    void copycolumntoclipboard(int column);
    virtual void resizeevent(qresizeevent *event);

private slots:
    void on_receivebutton_clicked();
    void on_showrequestbutton_clicked();
    void on_removerequestbutton_clicked();
    void on_recentrequestsview_doubleclicked(const qmodelindex &index);
    void recentrequestsview_selectionchanged(const qitemselection &selected, const qitemselection &deselected);
    void updatedisplayunit();
    void showmenu(const qpoint &point);
    void copylabel();
    void copymessage();
    void copyamount();
};

#endif // moorecoin_qt_receivecoinsdialog_h
