// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "paymentservertests.h"

#include "optionsmodel.h"
#include "paymentrequestdata.h"

#include "amount.h"
#include "random.h"
#include "script/script.h"
#include "script/standard.h"
#include "util.h"
#include "utilstrencodings.h"

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <qfileopenevent>
#include <qtemporaryfile>

x509 *parse_b64der_cert(const char* cert_data)
{
    std::vector<unsigned char> data = decodebase64(cert_data);
    assert(data.size() > 0);
    const unsigned char* dptr = &data[0];
    x509 *cert = d2i_x509(null, &dptr, data.size());
    assert(cert);
    return cert;
}

//
// test payment request handling
//

static sendcoinsrecipient handlerequest(paymentserver* server, std::vector<unsigned char>& data)
{
    recipientcatcher sigcatcher;
    qobject::connect(server, signal(receivedpaymentrequest(sendcoinsrecipient)),
        &sigcatcher, slot(getrecipient(sendcoinsrecipient)));

    // write data to a temp file:
    qtemporaryfile f;
    f.open();
    f.write((const char*)&data[0], data.size());
    f.close();

    // create a qobject, install event filter from paymentserver
    // and send a file open event to the object
    qobject object;
    object.installeventfilter(server);
    qfileopenevent event(f.filename());
    // if sending the event fails, this will cause sigcatcher to be empty,
    // which will lead to a test failure anyway.
    qcoreapplication::sendevent(&object, &event);

    qobject::disconnect(server, signal(receivedpaymentrequest(sendcoinsrecipient)),
        &sigcatcher, slot(getrecipient(sendcoinsrecipient)));

    // return results from sigcatcher
    return sigcatcher.recipient;
}

void paymentservertests::paymentservertests()
{
    selectparams(cbasechainparams::main);
    optionsmodel optionsmodel;
    paymentserver* server = new paymentserver(null, false);
    x509_store* castore = x509_store_new();
    x509_store_add_cert(castore, parse_b64der_cert(cacert1_base64));
    paymentserver::loadrootcas(castore);
    server->setoptionsmodel(&optionsmodel);
    server->uiready();

    std::vector<unsigned char> data;
    sendcoinsrecipient r;
    qstring merchant;

    // now feed paymentrequests to server, and observe signals it produces

    // this payment request validates directly against the
    // cacert1 certificate authority:
    data = decodebase64(paymentrequest1_cert1_base64);
    r = handlerequest(server, data);
    r.paymentrequest.getmerchant(castore, merchant);
    qcompare(merchant, qstring("testmerchant.org"));

    // signed, but expired, merchant cert in the request:
    data = decodebase64(paymentrequest2_cert1_base64);
    r = handlerequest(server, data);
    r.paymentrequest.getmerchant(castore, merchant);
    qcompare(merchant, qstring(""));

    // 10-long certificate chain, all intermediates valid:
    data = decodebase64(paymentrequest3_cert1_base64);
    r = handlerequest(server, data);
    r.paymentrequest.getmerchant(castore, merchant);
    qcompare(merchant, qstring("testmerchant8.org"));

    // long certificate chain, with an expired certificate in the middle:
    data = decodebase64(paymentrequest4_cert1_base64);
    r = handlerequest(server, data);
    r.paymentrequest.getmerchant(castore, merchant);
    qcompare(merchant, qstring(""));

    // validly signed, but by a ca not in our root ca list:
    data = decodebase64(paymentrequest5_cert1_base64);
    r = handlerequest(server, data);
    r.paymentrequest.getmerchant(castore, merchant);
    qcompare(merchant, qstring(""));

    // try again with no root ca's, verifiedmerchant should be empty:
    castore = x509_store_new();
    paymentserver::loadrootcas(castore);
    data = decodebase64(paymentrequest1_cert1_base64);
    r = handlerequest(server, data);
    r.paymentrequest.getmerchant(castore, merchant);
    qcompare(merchant, qstring(""));

    // load second root certificate
    castore = x509_store_new();
    x509_store_add_cert(castore, parse_b64der_cert(cacert2_base64));
    paymentserver::loadrootcas(castore);

    qbytearray bytearray;

    // for the tests below we just need the payment request data from
    // paymentrequestdata.h parsed + stored in r.paymentrequest.
    //
    // these tests require us to bypass the following normal client execution flow
    // shown below to be able to explicitly just trigger a certain condition!
    //
    // handlerequest()
    // -> paymentserver::eventfilter()
    //   -> paymentserver::handleuriorfile()
    //     -> paymentserver::readpaymentrequestfromfile()
    //       -> paymentserver::processpaymentrequest()

    // contains a testnet paytoaddress, so payment request network doesn't match client network:
    data = decodebase64(paymentrequest1_cert2_base64);
    bytearray = qbytearray((const char*)&data[0], data.size());
    r.paymentrequest.parse(bytearray);
    // ensure the request is initialized, because network "main" is default, even for
    // uninizialized payment requests and that will fail our test here.
    qverify(r.paymentrequest.isinitialized());
    qcompare(paymentserver::verifynetwork(r.paymentrequest.getdetails()), false);

    // expired payment request (expires is set to 1 = 1970-01-01 00:00:01):
    data = decodebase64(paymentrequest2_cert2_base64);
    bytearray = qbytearray((const char*)&data[0], data.size());
    r.paymentrequest.parse(bytearray);
    // ensure the request is initialized
    qverify(r.paymentrequest.isinitialized());
    // compares 1 < gettime() == false (treated as expired payment request)
    qcompare(paymentserver::verifyexpired(r.paymentrequest.getdetails()), true);

    // unexpired payment request (expires is set to 0x7fffffffffffffff = max. int64_t):
    // 9223372036854775807 (uint64), 9223372036854775807 (int64_t) and -1 (int32_t)
    // -1 is 1969-12-31 23:59:59 (for a 32 bit time values)
    data = decodebase64(paymentrequest3_cert2_base64);
    bytearray = qbytearray((const char*)&data[0], data.size());
    r.paymentrequest.parse(bytearray);
    // ensure the request is initialized
    qverify(r.paymentrequest.isinitialized());
    // compares 9223372036854775807 < gettime() == false (treated as unexpired payment request)
    qcompare(paymentserver::verifyexpired(r.paymentrequest.getdetails()), false);

    // unexpired payment request (expires is set to 0x8000000000000000 > max. int64_t, allowed uint64):
    // 9223372036854775808 (uint64), -9223372036854775808 (int64_t) and 0 (int32_t)
    // 0 is 1970-01-01 00:00:00 (for a 32 bit time values)
    data = decodebase64(paymentrequest4_cert2_base64);
    bytearray = qbytearray((const char*)&data[0], data.size());
    r.paymentrequest.parse(bytearray);
    // ensure the request is initialized
    qverify(r.paymentrequest.isinitialized());
    // compares -9223372036854775808 < gettime() == true (treated as expired payment request)
    qcompare(paymentserver::verifyexpired(r.paymentrequest.getdetails()), true);

    // test bip70 dos protection:
    unsigned char randdata[bip70_max_paymentrequest_size + 1];
    getrandbytes(randdata, sizeof(randdata));
    // write data to a temp file:
    qtemporaryfile tempfile;
    tempfile.open();
    tempfile.write((const char*)randdata, sizeof(randdata));
    tempfile.close();
    qcompare(paymentserver::readpaymentrequestfromfile(tempfile.filename(), r.paymentrequest), false);

    // payment request with amount overflow (amount is set to 21000001 btc):
    data = decodebase64(paymentrequest5_cert2_base64);
    bytearray = qbytearray((const char*)&data[0], data.size());
    r.paymentrequest.parse(bytearray);
    // ensure the request is initialized
    qverify(r.paymentrequest.isinitialized());
    // extract address and amount from the request
    qlist<std::pair<cscript, camount> > sendingtos = r.paymentrequest.getpayto();
    foreach (const pairtype(cscript, camount)& sendingto, sendingtos) {
        ctxdestination dest;
        if (extractdestination(sendingto.first, dest))
            qcompare(paymentserver::verifyamount(sendingto.second), false);
    }

    delete server;
}

void recipientcatcher::getrecipient(sendcoinsrecipient r)
{
    recipient = r;
}
