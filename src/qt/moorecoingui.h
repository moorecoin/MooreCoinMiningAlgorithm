// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_moorecoingui_h
#define moorecoin_qt_moorecoingui_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "amount.h"

#include <qlabel>
#include <qmainwindow>
#include <qmap>
#include <qmenu>
#include <qpoint>
#include <qsystemtrayicon>

class clientmodel;
class networkstyle;
class notificator;
class optionsmodel;
class rpcconsole;
class sendcoinsrecipient;
class unitdisplaystatusbarcontrol;
class walletframe;
class walletmodel;

class cwallet;

qt_begin_namespace
class qaction;
class qprogressbar;
class qprogressdialog;
qt_end_namespace

/**
  moorecoin gui main class. this class represents the main window of the moorecoin ui. it communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class moorecoingui : public qmainwindow
{
    q_object

public:
    static const qstring default_wallet;

    explicit moorecoingui(const networkstyle *networkstyle, qwidget *parent = 0);
    ~moorecoingui();

    /** set the client model.
        the client model represents the part of the core that communicates with the p2p network, and is wallet-agnostic.
    */
    void setclientmodel(clientmodel *clientmodel);

#ifdef enable_wallet
    /** set the wallet model.
        the wallet model represents a moorecoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    bool addwallet(const qstring& name, walletmodel *walletmodel);
    bool setcurrentwallet(const qstring& name);
    void removeallwallets();
#endif // enable_wallet
    bool enablewallet;

protected:
    void changeevent(qevent *e);
    void closeevent(qcloseevent *event);
    void dragenterevent(qdragenterevent *event);
    void dropevent(qdropevent *event);
    bool eventfilter(qobject *object, qevent *event);

private:
    clientmodel *clientmodel;
    walletframe *walletframe;

    unitdisplaystatusbarcontrol *unitdisplaycontrol;
    qlabel *labelencryptionicon;
    qlabel *labelconnectionsicon;
    qlabel *labelblocksicon;
    qlabel *progressbarlabel;
    qprogressbar *progressbar;
    qprogressdialog *progressdialog;

    qmenubar *appmenubar;
    qaction *overviewaction;
    qaction *historyaction;
    qaction *quitaction;
    qaction *sendcoinsaction;
    qaction *sendcoinsmenuaction;
    qaction *usedsendingaddressesaction;
    qaction *usedreceivingaddressesaction;
    qaction *signmessageaction;
    qaction *verifymessageaction;
    qaction *aboutaction;
    qaction *receivecoinsaction;
    qaction *receivecoinsmenuaction;
    qaction *optionsaction;
    qaction *togglehideaction;
    qaction *encryptwalletaction;
    qaction *backupwalletaction;
    qaction *changepassphraseaction;
    qaction *aboutqtaction;
    qaction *openrpcconsoleaction;
    qaction *openaction;
    qaction *showhelpmessageaction;

    qsystemtrayicon *trayicon;
    qmenu *trayiconmenu;
    notificator *notificator;
    rpcconsole *rpcconsole;

    /** keep track of previous number of blocks, to detect progress */
    int prevblocks;
    int spinnerframe;

    /** create the main ui actions. */
    void createactions();
    /** create the menu bar and sub-menus. */
    void createmenubar();
    /** create the toolbars */
    void createtoolbars();
    /** create system tray icon and notification */
    void createtrayicon(const networkstyle *networkstyle);
    /** create system tray menu (or setup the dock menu) */
    void createtrayiconmenu();

    /** enable or disable all wallet-related actions */
    void setwalletactionsenabled(bool enabled);

    /** connect core signals to gui client */
    void subscribetocoresignals();
    /** disconnect core signals from gui client */
    void unsubscribefromcoresignals();

signals:
    /** signal raised when a uri was entered or dragged to the gui */
    void receiveduri(const qstring &uri);

public slots:
    /** set number of connections shown in the ui */
    void setnumconnections(int count);
    /** set number of blocks and last block date shown in the ui */
    void setnumblocks(int count, const qdatetime& blockdate);

    /** notify the user of an event from the core network or transaction handling code.
       @param[in] title     the message box / notification title
       @param[in] message   the displayed text
       @param[in] style     modality and style definitions (icon and used buttons - buttons only for message boxes)
                            @see cclientuiinterface::messageboxflags
       @param[in] ret       pointer to a bool that will be modified to whether ok was clicked (modal only)
    */
    void message(const qstring &title, const qstring &message, unsigned int style, bool *ret = null);

#ifdef enable_wallet
    /** set the encryption status as shown in the ui.
       @param[in] status            current encryption status
       @see walletmodel::encryptionstatus
    */
    void setencryptionstatus(int status);

    bool handlepaymentrequest(const sendcoinsrecipient& recipient);

    /** show incoming transaction notification for new transactions. */
    void incomingtransaction(const qstring& date, int unit, const camount& amount, const qstring& type, const qstring& address, const qstring& label);
#endif // enable_wallet

private slots:
#ifdef enable_wallet
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

    /** show open dialog */
    void openclicked();
#endif // enable_wallet
    /** show configuration dialog */
    void optionsclicked();
    /** show about dialog */
    void aboutclicked();
    /** show help message dialog */
    void showhelpmessageclicked();
#ifndef q_os_mac
    /** handle tray icon clicked */
    void trayiconactivated(qsystemtrayicon::activationreason reason);
#endif

    /** show window if hidden, unminimize when minimized, rise when obscured or show if hidden and ftogglehidden is true */
    void shownormalifminimized(bool ftogglehidden = false);
    /** simply calls shownormalifminimized(true) for use in slot() macro */
    void togglehidden();

    /** called by a timer to check if frequestshutdown has been set **/
    void detectshutdown();

    /** show progress dialog e.g. for verifychain */
    void showprogress(const qstring &title, int nprogress);
};

class unitdisplaystatusbarcontrol : public qlabel
{
    q_object

public:
    explicit unitdisplaystatusbarcontrol();
    /** lets the control know about the options model (and its signals) */
    void setoptionsmodel(optionsmodel *optionsmodel);

protected:
    /** so that it responds to left-button clicks */
    void mousepressevent(qmouseevent *event);

private:
    optionsmodel *optionsmodel;
    qmenu* menu;

    /** shows context menu with display unit options by the mouse coordinates */
    void ondisplayunitsclicked(const qpoint& point);
    /** creates context menu, its actions, and wires up all the relevant signals for mouse events. */
    void createcontextmenu();

private slots:
    /** when display units are changed on optionsmodel it will refresh the display text of the control on the status bar */
    void updatedisplayunit(int newunits);
    /** tells underlying optionsmodel to update its current display unit. */
    void onmenuselection(qaction* action);
};

#endif // moorecoin_qt_moorecoingui_h
