// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "paymentserver.h"

#include "moorecoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "base58.h"
#include "chainparams.h"
#include "main.h"
#include "ui_interface.h"
#include "util.h"
#include "wallet/wallet.h"

#include <cstdlib>

#include <openssl/x509_vfy.h>

#include <qapplication>
#include <qbytearray>
#include <qdatastream>
#include <qdatetime>
#include <qdebug>
#include <qfile>
#include <qfileopenevent>
#include <qhash>
#include <qlist>
#include <qlocalserver>
#include <qlocalsocket>
#include <qnetworkaccessmanager>
#include <qnetworkproxy>
#include <qnetworkreply>
#include <qnetworkrequest>
#include <qsslcertificate>
#include <qsslerror>
#include <qsslsocket>
#include <qstringlist>
#include <qtextdocument>

#if qt_version < 0x050000
#include <qurl>
#else
#include <qurlquery>
#endif

using namespace std;

const int moorecoin_ipc_connect_timeout = 1000; // milliseconds
const qstring moorecoin_ipc_prefix("moorecoin:");
// bip70 payment protocol messages
const char* bip70_message_paymentack = "paymentack";
const char* bip70_message_paymentrequest = "paymentrequest";
// bip71 payment protocol media types
const char* bip71_mimetype_payment = "application/moorecoin-payment";
const char* bip71_mimetype_paymentack = "application/moorecoin-paymentack";
const char* bip71_mimetype_paymentrequest = "application/moorecoin-paymentrequest";
// bip70 max payment request size in bytes (dos protection)
const qint64 bip70_max_paymentrequest_size = 50000;

x509_store* paymentserver::certstore = null;
void paymentserver::freecertstore()
{
    if (paymentserver::certstore != null)
    {
        x509_store_free(paymentserver::certstore);
        paymentserver::certstore = null;
    }
}

//
// create a name that is unique for:
//  testnet / non-testnet
//  data directory
//
static qstring ipcservername()
{
    qstring name("moorecoinqt");

    // append a simple hash of the datadir
    // note that getdatadir(true) returns a different path
    // for -testnet versus main net
    qstring ddir(qstring::fromstdstring(getdatadir(true).string()));
    name.append(qstring::number(qhash(ddir)));

    return name;
}

//
// we store payment uris and requests received before
// the main gui window is up and ready to ask the user
// to send payment.

static qlist<qstring> savedpaymentrequests;

static void reportinvalidcertificate(const qsslcertificate& cert)
{
#if qt_version < 0x050000
    qdebug() << qstring("%1: payment server found an invalid certificate: ").arg(__func__) << cert.serialnumber() << cert.subjectinfo(qsslcertificate::commonname) << cert.subjectinfo(qsslcertificate::organizationalunitname);
#else
    qdebug() << qstring("%1: payment server found an invalid certificate: ").arg(__func__) << cert.serialnumber() << cert.subjectinfo(qsslcertificate::commonname) << cert.subjectinfo(qsslcertificate::distinguishednamequalifier) << cert.subjectinfo(qsslcertificate::organizationalunitname);
#endif
}

//
// load openssl's list of root certificate authorities
//
void paymentserver::loadrootcas(x509_store* _store)
{
    if (paymentserver::certstore == null)
        atexit(paymentserver::freecertstore);
    else
        freecertstore();

    // unit tests mostly use this, to pass in fake root cas:
    if (_store)
    {
        paymentserver::certstore = _store;
        return;
    }

    // normal execution, use either -rootcertificates or system certs:
    paymentserver::certstore = x509_store_new();

    // note: use "-system-" default here so that users can pass -rootcertificates=""
    // and get 'i don't like x.509 certificates, don't trust anybody' behavior:
    qstring certfile = qstring::fromstdstring(getarg("-rootcertificates", "-system-"));

    // empty store
    if (certfile.isempty()) {
        qdebug() << qstring("paymentserver::%1: payment request authentication via x.509 certificates disabled.").arg(__func__);
        return;
    }

    qlist<qsslcertificate> certlist;

    if (certfile != "-system-") {
            qdebug() << qstring("paymentserver::%1: using \"%2\" as trusted root certificate.").arg(__func__).arg(certfile);

        certlist = qsslcertificate::frompath(certfile);
        // use those certificates when fetching payment requests, too:
        qsslsocket::setdefaultcacertificates(certlist);
    } else
        certlist = qsslsocket::systemcacertificates();

    int nrootcerts = 0;
    const qdatetime currenttime = qdatetime::currentdatetime();

    foreach (const qsslcertificate& cert, certlist) {
        // don't log null certificates
        if (cert.isnull())
            continue;

        // not yet active/valid, or expired certificate
        if (currenttime < cert.effectivedate() || currenttime > cert.expirydate()) {
            reportinvalidcertificate(cert);
            continue;
        }

#if qt_version >= 0x050000
        // blacklisted certificate
        if (cert.isblacklisted()) {
            reportinvalidcertificate(cert);
            continue;
        }
#endif
        qbytearray certdata = cert.toder();
        const unsigned char *data = (const unsigned char *)certdata.data();

        x509* x509 = d2i_x509(0, &data, certdata.size());
        if (x509 && x509_store_add_cert(paymentserver::certstore, x509))
        {
            // note: x509_store_free will free the x509* objects when
            // the paymentserver is destroyed
            ++nrootcerts;
        }
        else
        {
            reportinvalidcertificate(cert);
            continue;
        }
    }
    qwarning() << "paymentserver::loadrootcas: loaded " << nrootcerts << " root certificates";

    // project for another day:
    // fetch certificate revocation lists, and add them to certstore.
    // issues to consider:
    //   performance (start a thread to fetch in background?)
    //   privacy (fetch through tor/proxy so ip address isn't revealed)
    //   would it be easier to just use a compiled-in blacklist?
    //    or use qt's blacklist?
    //   "certificate stapling" with server-side caching is more efficient
}

//
// sending to the server is done synchronously, at startup.
// if the server isn't already running, startup continues,
// and the items in savedpaymentrequest will be handled
// when uiready() is called.
//
// warning: ipcsendcommandline() is called early in init,
// so don't use "emit message()", but "qmessagebox::"!
//
void paymentserver::ipcparsecommandline(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        qstring arg(argv[i]);
        if (arg.startswith("-"))
            continue;

        // if the moorecoin: uri contains a payment request, we are not able to detect the
        // network as that would require fetching and parsing the payment request.
        // that means clicking such an uri which contains a testnet payment request
        // will start a mainnet instance and throw a "wrong network" error.
        if (arg.startswith(moorecoin_ipc_prefix, qt::caseinsensitive)) // moorecoin: uri
        {
            savedpaymentrequests.append(arg);

            sendcoinsrecipient r;
            if (guiutil::parsemoorecoinuri(arg, &r) && !r.address.isempty())
            {
                cmoorecoinaddress address(r.address.tostdstring());

                if (address.isvalid(params(cbasechainparams::main)))
                {
                    selectparams(cbasechainparams::main);
                }
                else if (address.isvalid(params(cbasechainparams::testnet)))
                {
                    selectparams(cbasechainparams::testnet);
                }
            }
        }
        else if (qfile::exists(arg)) // filename
        {
            savedpaymentrequests.append(arg);

            paymentrequestplus request;
            if (readpaymentrequestfromfile(arg, request))
            {
                if (request.getdetails().network() == "main")
                {
                    selectparams(cbasechainparams::main);
                }
                else if (request.getdetails().network() == "test")
                {
                    selectparams(cbasechainparams::testnet);
                }
            }
        }
        else
        {
            // printing to debug.log is about the best we can do here, the
            // gui hasn't started yet so we can't pop up a message box.
            qwarning() << "paymentserver::ipcsendcommandline: payment request file does not exist: " << arg;
        }
    }
}

//
// sending to the server is done synchronously, at startup.
// if the server isn't already running, startup continues,
// and the items in savedpaymentrequest will be handled
// when uiready() is called.
//
bool paymentserver::ipcsendcommandline()
{
    bool fresult = false;
    foreach (const qstring& r, savedpaymentrequests)
    {
        qlocalsocket* socket = new qlocalsocket();
        socket->connecttoserver(ipcservername(), qiodevice::writeonly);
        if (!socket->waitforconnected(moorecoin_ipc_connect_timeout))
        {
            delete socket;
            socket = null;
            return false;
        }

        qbytearray block;
        qdatastream out(&block, qiodevice::writeonly);
        out.setversion(qdatastream::qt_4_0);
        out << r;
        out.device()->seek(0);

        socket->write(block);
        socket->flush();
        socket->waitforbyteswritten(moorecoin_ipc_connect_timeout);
        socket->disconnectfromserver();

        delete socket;
        socket = null;
        fresult = true;
    }

    return fresult;
}

paymentserver::paymentserver(qobject* parent, bool startlocalserver) :
    qobject(parent),
    saveuris(true),
    uriserver(0),
    netmanager(0),
    optionsmodel(0)
{
    // verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    google_protobuf_verify_version;

    // install global event filter to catch qfileopenevents
    // on mac: sent when you click moorecoin: links
    // other oses: helpful when dealing with payment request files
    if (parent)
        parent->installeventfilter(this);

    qstring name = ipcservername();

    // clean up old socket leftover from a crash:
    qlocalserver::removeserver(name);

    if (startlocalserver)
    {
        uriserver = new qlocalserver(this);

        if (!uriserver->listen(name)) {
            // constructor is called early in init, so don't use "emit message()" here
            qmessagebox::critical(0, tr("payment request error"),
                tr("cannot start moorecoin: click-to-pay handler"));
        }
        else {
            connect(uriserver, signal(newconnection()), this, slot(handleuriconnection()));
            connect(this, signal(receivedpaymentack(qstring)), this, slot(handlepaymentack(qstring)));
        }
    }
}

paymentserver::~paymentserver()
{
    google::protobuf::shutdownprotobuflibrary();
}

//
// osx-specific way of handling moorecoin: uris and paymentrequest mime types.
// also used by paymentservertests.cpp and when opening a payment request file
// via "open uri..." menu entry.
//
bool paymentserver::eventfilter(qobject *object, qevent *event)
{
    if (event->type() == qevent::fileopen) {
        qfileopenevent *fileevent = static_cast<qfileopenevent*>(event);
        if (!fileevent->file().isempty())
            handleuriorfile(fileevent->file());
        else if (!fileevent->url().isempty())
            handleuriorfile(fileevent->url().tostring());

        return true;
    }

    return qobject::eventfilter(object, event);
}

void paymentserver::initnetmanager()
{
    if (!optionsmodel)
        return;
    if (netmanager != null)
        delete netmanager;

    // netmanager is used to fetch paymentrequests given in moorecoin: uris
    netmanager = new qnetworkaccessmanager(this);

    qnetworkproxy proxy;

    // query active socks5 proxy
    if (optionsmodel->getproxysettings(proxy)) {
        netmanager->setproxy(proxy);

        qdebug() << "paymentserver::initnetmanager: using socks5 proxy" << proxy.hostname() << ":" << proxy.port();
    }
    else
        qdebug() << "paymentserver::initnetmanager: no active proxy server found.";

    connect(netmanager, signal(finished(qnetworkreply*)),
            this, slot(netrequestfinished(qnetworkreply*)));
    connect(netmanager, signal(sslerrors(qnetworkreply*, const qlist<qsslerror> &)),
            this, slot(reportsslerrors(qnetworkreply*, const qlist<qsslerror> &)));
}

void paymentserver::uiready()
{
    initnetmanager();

    saveuris = false;
    foreach (const qstring& s, savedpaymentrequests)
    {
        handleuriorfile(s);
    }
    savedpaymentrequests.clear();
}

void paymentserver::handleuriorfile(const qstring& s)
{
    if (saveuris)
    {
        savedpaymentrequests.append(s);
        return;
    }

    if (s.startswith(moorecoin_ipc_prefix, qt::caseinsensitive)) // moorecoin: uri
    {
#if qt_version < 0x050000
        qurl uri(s);
#else
        qurlquery uri((qurl(s)));
#endif
        if (uri.hasqueryitem("r")) // payment request uri
        {
            qbytearray temp;
            temp.append(uri.queryitemvalue("r"));
            qstring decoded = qurl::frompercentencoding(temp);
            qurl fetchurl(decoded, qurl::strictmode);

            if (fetchurl.isvalid())
            {
                qdebug() << "paymentserver::handleuriorfile: fetchrequest(" << fetchurl << ")";
                fetchrequest(fetchurl);
            }
            else
            {
                qwarning() << "paymentserver::handleuriorfile: invalid url: " << fetchurl;
                emit message(tr("uri handling"),
                    tr("payment request fetch url is invalid: %1").arg(fetchurl.tostring()),
                    cclientuiinterface::icon_warning);
            }

            return;
        }
        else // normal uri
        {
            sendcoinsrecipient recipient;
            if (guiutil::parsemoorecoinuri(s, &recipient))
            {
                cmoorecoinaddress address(recipient.address.tostdstring());
                if (!address.isvalid()) {
                    emit message(tr("uri handling"), tr("invalid payment address %1").arg(recipient.address),
                        cclientuiinterface::msg_error);
                }
                else
                    emit receivedpaymentrequest(recipient);
            }
            else
                emit message(tr("uri handling"),
                    tr("uri cannot be parsed! this can be caused by an invalid moorecoin address or malformed uri parameters."),
                    cclientuiinterface::icon_warning);

            return;
        }
    }

    if (qfile::exists(s)) // payment request file
    {
        paymentrequestplus request;
        sendcoinsrecipient recipient;
        if (!readpaymentrequestfromfile(s, request))
        {
            emit message(tr("payment request file handling"),
                tr("payment request file cannot be read! this can be caused by an invalid payment request file."),
                cclientuiinterface::icon_warning);
        }
        else if (processpaymentrequest(request, recipient))
            emit receivedpaymentrequest(recipient);

        return;
    }
}

void paymentserver::handleuriconnection()
{
    qlocalsocket *clientconnection = uriserver->nextpendingconnection();

    while (clientconnection->bytesavailable() < (int)sizeof(quint32))
        clientconnection->waitforreadyread();

    connect(clientconnection, signal(disconnected()),
            clientconnection, slot(deletelater()));

    qdatastream in(clientconnection);
    in.setversion(qdatastream::qt_4_0);
    if (clientconnection->bytesavailable() < (int)sizeof(quint16)) {
        return;
    }
    qstring msg;
    in >> msg;

    handleuriorfile(msg);
}

//
// warning: readpaymentrequestfromfile() is used in ipcsendcommandline()
// so don't use "emit message()", but "qmessagebox::"!
//
bool paymentserver::readpaymentrequestfromfile(const qstring& filename, paymentrequestplus& request)
{
    qfile f(filename);
    if (!f.open(qiodevice::readonly)) {
        qwarning() << qstring("paymentserver::%1: failed to open %2").arg(__func__).arg(filename);
        return false;
    }

    // bip70 dos protection
    if (f.size() > bip70_max_paymentrequest_size) {
        qwarning() << qstring("paymentserver::%1: payment request %2 is too large (%3 bytes, allowed %4 bytes).")
            .arg(__func__)
            .arg(filename)
            .arg(f.size())
            .arg(bip70_max_paymentrequest_size);
        return false;
    }

    qbytearray data = f.readall();

    return request.parse(data);
}

bool paymentserver::processpaymentrequest(const paymentrequestplus& request, sendcoinsrecipient& recipient)
{
    if (!optionsmodel)
        return false;

    if (request.isinitialized()) {
        // payment request network matches client network?
        if (!verifynetwork(request.getdetails())) {
            emit message(tr("payment request rejected"), tr("payment request network doesn't match client network."),
                cclientuiinterface::msg_error);

            return false;
        }

        // make sure any payment requests involved are still valid.
        // this is re-checked just before sending coins in walletmodel::sendcoins().
        if (verifyexpired(request.getdetails())) {
            emit message(tr("payment request rejected"), tr("payment request expired."),
                cclientuiinterface::msg_error);

            return false;
        }
    } else {
        emit message(tr("payment request error"), tr("payment request is not initialized."),
            cclientuiinterface::msg_error);

        return false;
    }

    recipient.paymentrequest = request;
    recipient.message = guiutil::htmlescape(request.getdetails().memo());

    request.getmerchant(paymentserver::certstore, recipient.authenticatedmerchant);

    qlist<std::pair<cscript, camount> > sendingtos = request.getpayto();
    qstringlist addresses;

    foreach(const pairtype(cscript, camount)& sendingto, sendingtos) {
        // extract and check destination addresses
        ctxdestination dest;
        if (extractdestination(sendingto.first, dest)) {
            // append destination address
            addresses.append(qstring::fromstdstring(cmoorecoinaddress(dest).tostring()));
        }
        else if (!recipient.authenticatedmerchant.isempty()) {
            // unauthenticated payment requests to custom moorecoin addresses are not supported
            // (there is no good way to tell the user where they are paying in a way they'd
            // have a chance of understanding).
            emit message(tr("payment request rejected"),
                tr("unverified payment requests to custom payment scripts are unsupported."),
                cclientuiinterface::msg_error);
            return false;
        }

        // moorecoin amounts are stored as (optional) uint64 in the protobuf messages (see paymentrequest.proto),
        // but camount is defined as int64_t. because of that we need to verify that amounts are in a valid range
        // and no overflow has happened.
        if (!verifyamount(sendingto.second)) {
            emit message(tr("payment request rejected"), tr("invalid payment request."), cclientuiinterface::msg_error);
            return false;
        }

        // extract and check amounts
        ctxout txout(sendingto.second, sendingto.first);
        if (txout.isdust(::minrelaytxfee)) {
            emit message(tr("payment request error"), tr("requested payment amount of %1 is too small (considered dust).")
                .arg(moorecoinunits::formatwithunit(optionsmodel->getdisplayunit(), sendingto.second)),
                cclientuiinterface::msg_error);

            return false;
        }

        recipient.amount += sendingto.second;
        // also verify that the final amount is still in a valid range after adding additional amounts.
        if (!verifyamount(recipient.amount)) {
            emit message(tr("payment request rejected"), tr("invalid payment request."), cclientuiinterface::msg_error);
            return false;
        }
    }
    // store addresses and format them to fit nicely into the gui
    recipient.address = addresses.join("<br />");

    if (!recipient.authenticatedmerchant.isempty()) {
        qdebug() << "paymentserver::processpaymentrequest: secure payment request from " << recipient.authenticatedmerchant;
    }
    else {
        qdebug() << "paymentserver::processpaymentrequest: insecure payment request to " << addresses.join(", ");
    }

    return true;
}

void paymentserver::fetchrequest(const qurl& url)
{
    qnetworkrequest netrequest;
    netrequest.setattribute(qnetworkrequest::user, bip70_message_paymentrequest);
    netrequest.seturl(url);
    netrequest.setrawheader("user-agent", client_name.c_str());
    netrequest.setrawheader("accept", bip71_mimetype_paymentrequest);
    netmanager->get(netrequest);
}

void paymentserver::fetchpaymentack(cwallet* wallet, sendcoinsrecipient recipient, qbytearray transaction)
{
    const payments::paymentdetails& details = recipient.paymentrequest.getdetails();
    if (!details.has_payment_url())
        return;

    qnetworkrequest netrequest;
    netrequest.setattribute(qnetworkrequest::user, bip70_message_paymentack);
    netrequest.seturl(qstring::fromstdstring(details.payment_url()));
    netrequest.setheader(qnetworkrequest::contenttypeheader, bip71_mimetype_payment);
    netrequest.setrawheader("user-agent", client_name.c_str());
    netrequest.setrawheader("accept", bip71_mimetype_paymentack);

    payments::payment payment;
    payment.set_merchant_data(details.merchant_data());
    payment.add_transactions(transaction.data(), transaction.size());

    // create a new refund address, or re-use:
    qstring account = tr("refund from %1").arg(recipient.authenticatedmerchant);
    std::string straccount = account.tostdstring();
    set<ctxdestination> refundaddresses = wallet->getaccountaddresses(straccount);
    if (!refundaddresses.empty()) {
        cscript s = getscriptfordestination(*refundaddresses.begin());
        payments::output* refund_to = payment.add_refund_to();
        refund_to->set_script(&s[0], s.size());
    }
    else {
        cpubkey newkey;
        if (wallet->getkeyfrompool(newkey)) {
            ckeyid keyid = newkey.getid();
            wallet->setaddressbook(keyid, straccount, "refund");

            cscript s = getscriptfordestination(keyid);
            payments::output* refund_to = payment.add_refund_to();
            refund_to->set_script(&s[0], s.size());
        }
        else {
            // this should never happen, because sending coins should have
            // just unlocked the wallet and refilled the keypool.
            qwarning() << "paymentserver::fetchpaymentack: error getting refund key, refund_to not set";
        }
    }

    int length = payment.bytesize();
    netrequest.setheader(qnetworkrequest::contentlengthheader, length);
    qbytearray serdata(length, '\0');
    if (payment.serializetoarray(serdata.data(), length)) {
        netmanager->post(netrequest, serdata);
    }
    else {
        // this should never happen, either.
        qwarning() << "paymentserver::fetchpaymentack: error serializing payment message";
    }
}

void paymentserver::netrequestfinished(qnetworkreply* reply)
{
    reply->deletelater();

    // bip70 dos protection
    if (reply->size() > bip70_max_paymentrequest_size) {
        qstring msg = tr("payment request %1 is too large (%2 bytes, allowed %3 bytes).")
            .arg(reply->request().url().tostring())
            .arg(reply->size())
            .arg(bip70_max_paymentrequest_size);

        qwarning() << qstring("paymentserver::%1:").arg(__func__) << msg;
        emit message(tr("payment request dos protection"), msg, cclientuiinterface::msg_error);
        return;
    }

    if (reply->error() != qnetworkreply::noerror) {
        qstring msg = tr("error communicating with %1: %2")
            .arg(reply->request().url().tostring())
            .arg(reply->errorstring());

        qwarning() << "paymentserver::netrequestfinished: " << msg;
        emit message(tr("payment request error"), msg, cclientuiinterface::msg_error);
        return;
    }

    qbytearray data = reply->readall();

    qstring requesttype = reply->request().attribute(qnetworkrequest::user).tostring();
    if (requesttype == bip70_message_paymentrequest)
    {
        paymentrequestplus request;
        sendcoinsrecipient recipient;
        if (!request.parse(data))
        {
            qwarning() << "paymentserver::netrequestfinished: error parsing payment request";
            emit message(tr("payment request error"),
                tr("payment request cannot be parsed!"),
                cclientuiinterface::msg_error);
        }
        else if (processpaymentrequest(request, recipient))
            emit receivedpaymentrequest(recipient);

        return;
    }
    else if (requesttype == bip70_message_paymentack)
    {
        payments::paymentack paymentack;
        if (!paymentack.parsefromarray(data.data(), data.size()))
        {
            qstring msg = tr("bad response from server %1")
                .arg(reply->request().url().tostring());

            qwarning() << "paymentserver::netrequestfinished: " << msg;
            emit message(tr("payment request error"), msg, cclientuiinterface::msg_error);
        }
        else
        {
            emit receivedpaymentack(guiutil::htmlescape(paymentack.memo()));
        }
    }
}

void paymentserver::reportsslerrors(qnetworkreply* reply, const qlist<qsslerror> &errs)
{
    q_unused(reply);

    qstring errstring;
    foreach (const qsslerror& err, errs) {
        qwarning() << "paymentserver::reportsslerrors: " << err;
        errstring += err.errorstring() + "\n";
    }
    emit message(tr("network request error"), errstring, cclientuiinterface::msg_error);
}

void paymentserver::setoptionsmodel(optionsmodel *optionsmodel)
{
    this->optionsmodel = optionsmodel;
}

void paymentserver::handlepaymentack(const qstring& paymentackmsg)
{
    // currently we don't futher process or store the paymentack message
    emit message(tr("payment acknowledged"), paymentackmsg, cclientuiinterface::icon_information | cclientuiinterface::modal);
}

bool paymentserver::verifynetwork(const payments::paymentdetails& requestdetails)
{
    bool fverified = requestdetails.network() == params().networkidstring();
    if (!fverified) {
        qwarning() << qstring("paymentserver::%1: payment request network \"%2\" doesn't match client network \"%3\".")
            .arg(__func__)
            .arg(qstring::fromstdstring(requestdetails.network()))
            .arg(qstring::fromstdstring(params().networkidstring()));
    }
    return fverified;
}

bool paymentserver::verifyexpired(const payments::paymentdetails& requestdetails)
{
    bool fverified = (requestdetails.has_expires() && (int64_t)requestdetails.expires() < gettime());
    if (fverified) {
        const qstring requestexpires = qstring::fromstdstring(datetimestrformat("%y-%m-%d %h:%m:%s", (int64_t)requestdetails.expires()));
        qwarning() << qstring("paymentserver::%1: payment request expired \"%2\".")
            .arg(__func__)
            .arg(requestexpires);
    }
    return fverified;
}

bool paymentserver::verifyamount(const camount& requestamount)
{
    bool fverified = moneyrange(requestamount);
    if (!fverified) {
        qwarning() << qstring("paymentserver::%1: payment request amount out of allowed range (%2, allowed 0 - %3).")
            .arg(__func__)
            .arg(requestamount)
            .arg(max_money);
    }
    return fverified;
}
