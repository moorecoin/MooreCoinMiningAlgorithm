// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_sendcoinsentry_h
#define moorecoin_qt_sendcoinsentry_h

#include "walletmodel.h"

#include <qstackedwidget>

class walletmodel;

namespace ui {
    class sendcoinsentry;
}

/**
 * a single entry in the dialog for sending moorecoins.
 * stacked widget, with different uis for payment requests
 * with a strong payee identity.
 */
class sendcoinsentry : public qstackedwidget
{
    q_object

public:
    explicit sendcoinsentry(qwidget *parent = 0);
    ~sendcoinsentry();

    void setmodel(walletmodel *model);
    bool validate();
    sendcoinsrecipient getvalue();

    /** return whether the entry is still empty and unedited */
    bool isclear();

    void setvalue(const sendcoinsrecipient &value);
    void setaddress(const qstring &address);

    /** set up the tab chain manually, as qt messes up the tab chain by default in some cases
     *  (issue https://bugreports.qt-project.org/browse/qtbug-10907).
     */
    qwidget *setuptabchain(qwidget *prev);

    void setfocus();

public slots:
    void clear();

signals:
    void removeentry(sendcoinsentry *entry);
    void payamountchanged();
    void subtractfeefromamountchanged();

private slots:
    void deleteclicked();
    void on_payto_textchanged(const qstring &address);
    void on_addressbookbutton_clicked();
    void on_pastebutton_clicked();
    void updatedisplayunit();

private:
    sendcoinsrecipient recipient;
    ui::sendcoinsentry *ui;
    walletmodel *model;

    bool updatelabel(const qstring &address);
};

#endif // moorecoin_qt_sendcoinsentry_h
