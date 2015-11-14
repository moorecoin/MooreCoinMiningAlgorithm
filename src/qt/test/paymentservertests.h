// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_test_paymentservertests_h
#define moorecoin_qt_test_paymentservertests_h

#include "../paymentserver.h"

#include <qobject>
#include <qtest>

class paymentservertests : public qobject
{
    q_object

private slots:
    void paymentservertests();
};

// dummy class to receive paymentserver signals.
// if sendcoinsrecipient was a proper qobject, then
// we could use qsignalspy... but it's not.
class recipientcatcher : public qobject
{
    q_object

public slots:
    void getrecipient(sendcoinsrecipient r);

public:
    sendcoinsrecipient recipient;
};

#endif // moorecoin_qt_test_paymentservertests_h
