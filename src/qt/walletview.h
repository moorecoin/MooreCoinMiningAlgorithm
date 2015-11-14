// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_walletview_h
#define moorecoin_qt_walletview_h

#include "amount.h"

#include <qstackedwidget>

class moorecoingui;
class clientmodel;
class overviewpage;
class receivecoinsdialog;
class sendcoinsdialog;
class sendcoinsrecipient;
class transactionview;
class walletmodel;

qt_begin_namespace
class qmodelindex;
class qprogressdialog;
qt_end_namespace

/*
  walletview class. this class represents the view to a single wallet.
  it was added to support multiple wallet functionality. each wallet gets its own walletview instance.
  it communicates with both the client and the wallet models to give the user an up-to-date view of the
  current core state.
*/
class walletview : public qstackedwidget
{
    q_object

public:
    explicit walletview(qwidget *parent);
    ~walletview();

    void setmoorecoingui(moorecoingui *gui);
    /** set the client model.
        the client model represents the part of the core that communicates with the p2p network, and is wallet-agnostic.
    */
    void setclientmodel(clientmodel *clientmodel);
    /** set the wallet model.
        the wallet model represents a moorecoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    void setwalletmodel(walletmodel *walletmodel);

    bool handlepaymentrequest(const sendcoinsrecipient& recipient);

    void showoutofsyncwarning(bool fshow);

private:
    clientmodel *clientmodel;
    walletmodel *walletmodel;

    overviewpage *overviewpage;
    qwidget *transactionspage;
    receivecoinsdialog *receivecoinspage;
    sendcoinsdialog *sendcoinspage;

    transactionview *transactionview;

    qprogressdialog *progressdialog;

public slots:
    /** switch to overview (home) page */
    void gotooverviewpage();
    /** switch to history (transactions) page */
    void gotohistorypage();
    /** switch to receive coins page */
    void gotoreceivecoinspage();
    /** switch to send coins page */
    void gotosendcoinspage(qstring addr = "");

    /** show sign/verify message dialog and switch to sign message tab */
    void gotosignmessagetab(qstring addr = "");
    /** show sign/verify message dialog and switch to verify message tab */
    void gotoverifymessagetab(qstring addr = "");

    /** show incoming transaction notification for new transactions.

        the new items are those between start and end inclusive, under the given parent item.
    */
    void processnewtransaction(const qmodelindex& parent, int start, int /*end*/);
    /** encrypt the wallet */
    void encryptwallet(bool status);
    /** backup the wallet */
    void backupwallet();
    /** change encrypted wallet passphrase */
    void changepassphrase();
    /** ask for passphrase to unlock wallet temporarily */
    void unlockwallet();

    /** show used sending addresses */
    void usedsendingaddresses();
    /** show used receiving addresses */
    void usedreceivingaddresses();

    /** re-emit encryption status signal */
    void updateencryptionstatus();

    /** show progress dialog e.g. for rescan */
    void showprogress(const qstring &title, int nprogress);

signals:
    /** signal that we want to show the main window */
    void shownormalifminimized();
    /**  fired when a message should be reported to the user */
    void message(const qstring &title, const qstring &message, unsigned int style);
    /** encryption status of wallet changed */
    void encryptionstatuschanged(int status);
    /** notify that a new transaction appeared */
    void incomingtransaction(const qstring& date, int unit, const camount& amount, const qstring& type, const qstring& address, const qstring& label);
};

#endif // moorecoin_qt_walletview_h
