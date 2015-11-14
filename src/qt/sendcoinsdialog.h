// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_sendcoinsdialog_h
#define moorecoin_qt_sendcoinsdialog_h

#include "walletmodel.h"

#include <qdialog>
#include <qstring>

class clientmodel;
class optionsmodel;
class sendcoinsentry;
class sendcoinsrecipient;

namespace ui {
    class sendcoinsdialog;
}

qt_begin_namespace
class qurl;
qt_end_namespace

const int defaultconfirmtarget = 25;

/** dialog for sending moorecoins */
class sendcoinsdialog : public qdialog
{
    q_object

public:
    explicit sendcoinsdialog(qwidget *parent = 0);
    ~sendcoinsdialog();

    void setclientmodel(clientmodel *clientmodel);
    void setmodel(walletmodel *model);

    /** set up the tab chain manually, as qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/qtbug-10907).
     */
    qwidget *setuptabchain(qwidget *prev);

    void setaddress(const qstring &address);
    void pasteentry(const sendcoinsrecipient &rv);
    bool handlepaymentrequest(const sendcoinsrecipient &recipient);

public slots:
    void clear();
    void reject();
    void accept();
    sendcoinsentry *addentry();
    void updatetabsandlabels();
    void setbalance(const camount& balance, const camount& unconfirmedbalance, const camount& immaturebalance,
                    const camount& watchonlybalance, const camount& watchunconfbalance, const camount& watchimmaturebalance);

private:
    ui::sendcoinsdialog *ui;
    clientmodel *clientmodel;
    walletmodel *model;
    bool fnewrecipientallowed;
    bool ffeeminimized;

    // process walletmodel::sendcoinsreturn and generate a pair consisting
    // of a message and message flags for use in emit message().
    // additional parameter msgarg can be used via .arg(msgarg).
    void processsendcoinsreturn(const walletmodel::sendcoinsreturn &sendcoinsreturn, const qstring &msgarg = qstring());
    void minimizefeesection(bool fminimize);
    void updatefeeminimizedlabel();

private slots:
    void on_sendbutton_clicked();
    void on_buttonchoosefee_clicked();
    void on_buttonminimizefee_clicked();
    void removeentry(sendcoinsentry* entry);
    void updatedisplayunit();
    void coincontrolfeaturechanged(bool);
    void coincontrolbuttonclicked();
    void coincontrolchangechecked(int);
    void coincontrolchangeedited(const qstring &);
    void coincontrolupdatelabels();
    void coincontrolclipboardquantity();
    void coincontrolclipboardamount();
    void coincontrolclipboardfee();
    void coincontrolclipboardafterfee();
    void coincontrolclipboardbytes();
    void coincontrolclipboardpriority();
    void coincontrolclipboardlowoutput();
    void coincontrolclipboardchange();
    void setminimumfee();
    void updatefeesectioncontrols();
    void updateminfeelabel();
    void updatesmartfeelabel();
    void updateglobalfeevariables();

signals:
    // fired when a message should be reported to the user
    void message(const qstring &title, const qstring &message, unsigned int style);
};

#endif // moorecoin_qt_sendcoinsdialog_h
