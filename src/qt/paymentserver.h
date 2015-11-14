// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_paymentserver_h
#define moorecoin_qt_paymentserver_h

// this class handles payment requests from clicking on
// moorecoin: uris
//
// this is somewhat tricky, because we have to deal with
// the situation where the user clicks on a link during
// startup/initialization, when the splash-screen is up
// but the main window (and the send coins tab) is not.
//
// so, the strategy is:
//
// create the server, and register the event handler,
// when the application is created. save any uris
// received at or during startup in a list.
//
// when startup is finished and the main window is
// shown, a signal is sent to slot uiready(), which
// emits a receivedurl() signal for any payment
// requests that happened during startup.
//
// after startup, receivedurl() happens as usual.
//
// this class has one more feature: a static
// method that finds uris passed in the command line
// and, if a server is running in another process,
// sends them to the server.
//

#include "paymentrequestplus.h"
#include "walletmodel.h"

#include <qobject>
#include <qstring>

class optionsmodel;

class cwallet;

qt_begin_namespace
class qapplication;
class qbytearray;
class qlocalserver;
class qnetworkaccessmanager;
class qnetworkreply;
class qsslerror;
class qurl;
qt_end_namespace

// bip70 max payment request size in bytes (dos protection)
extern const qint64 bip70_max_paymentrequest_size;

class paymentserver : public qobject
{
    q_object

public:
    // parse uris on command line
    // returns false on error
    static void ipcparsecommandline(int argc, char *argv[]);

    // returns true if there were uris on the command line
    // which were successfully sent to an already-running
    // process.
    // note: if a payment request is given, selectparams(main/testnet)
    // will be called so we startup in the right mode.
    static bool ipcsendcommandline();

    // parent should be qapplication object
    paymentserver(qobject* parent, bool startlocalserver = true);
    ~paymentserver();

    // load root certificate authorities. pass null (default)
    // to read from the file specified in the -rootcertificates setting,
    // or, if that's not set, to use the system default root certificates.
    // if you pass in a store, you should not x509_store_free it: it will be
    // freed either at exit or when another set of cas are loaded.
    static void loadrootcas(x509_store* store = null);

    // return certificate store
    static x509_store* getcertstore() { return certstore; }

    // optionsmodel is used for getting proxy settings and display unit
    void setoptionsmodel(optionsmodel *optionsmodel);

    // this is now public, because we use it in paymentservertests.cpp
    static bool readpaymentrequestfromfile(const qstring& filename, paymentrequestplus& request);

    // verify that the payment request network matches the client network
    static bool verifynetwork(const payments::paymentdetails& requestdetails);
    // verify if the payment request is expired
    static bool verifyexpired(const payments::paymentdetails& requestdetails);
    // verify the payment request amount is valid
    static bool verifyamount(const camount& requestamount);

signals:
    // fired when a valid payment request is received
    void receivedpaymentrequest(sendcoinsrecipient);

    // fired when a valid paymentack is received
    void receivedpaymentack(const qstring &paymentackmsg);

    // fired when a message should be reported to the user
    void message(const qstring &title, const qstring &message, unsigned int style);

public slots:
    // signal this when the main window's ui is ready
    // to display payment requests to the user
    void uiready();

    // submit payment message to a merchant, get back paymentack:
    void fetchpaymentack(cwallet* wallet, sendcoinsrecipient recipient, qbytearray transaction);

    // handle an incoming uri, uri with local file scheme or file
    void handleuriorfile(const qstring& s);

private slots:
    void handleuriconnection();
    void netrequestfinished(qnetworkreply*);
    void reportsslerrors(qnetworkreply*, const qlist<qsslerror> &);
    void handlepaymentack(const qstring& paymentackmsg);

protected:
    // constructor registers this on the parent qapplication to
    // receive qevent::fileopen and qevent:drop events
    bool eventfilter(qobject *object, qevent *event);

private:
    bool processpaymentrequest(const paymentrequestplus& request, sendcoinsrecipient& recipient);
    void fetchrequest(const qurl& url);

    // setup networking
    void initnetmanager();

    bool saveuris;                      // true during startup
    qlocalserver* uriserver;

    static x509_store* certstore;       // trusted root certificates
    static void freecertstore();

    qnetworkaccessmanager* netmanager;  // used to fetch payment requests

    optionsmodel *optionsmodel;
};

#endif // moorecoin_qt_paymentserver_h
