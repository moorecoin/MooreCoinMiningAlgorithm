// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_walletframe_h
#define moorecoin_qt_walletframe_h

#include <qframe>
#include <qmap>

class moorecoingui;
class clientmodel;
class sendcoinsrecipient;
class walletmodel;
class walletview;

qt_begin_namespace
class qstackedwidget;
qt_end_namespace

class walletframe : public qframe
{
    q_object

public:
    explicit walletframe(moorecoingui *_gui = 0);
    ~walletframe();

    void setclientmodel(clientmodel *clientmodel);

    bool addwallet(const qstring& name, walletmodel *walletmodel);
    bool setcurrentwallet(const qstring& name);
    bool removewallet(const qstring &name);
    void removeallwallets();

    bool handlepaymentrequest(const sendcoinsrecipient& recipient);

    void showoutofsyncwarning(bool fshow);

private:
    qstackedwidget *walletstack;
    moorecoingui *gui;
    clientmodel *clientmodel;
    qmap<qstring, walletview*> mapwalletviews;

    bool boutofsync;

    walletview *currentwalletview();

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
};

#endif // moorecoin_qt_walletframe_h
