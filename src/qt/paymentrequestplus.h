// copyright (c) 2011-2014 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_paymentrequestplus_h
#define moorecoin_qt_paymentrequestplus_h

#include "paymentrequest.pb.h"

#include "base58.h"

#include <openssl/x509.h>

#include <qbytearray>
#include <qlist>
#include <qstring>

//
// wraps dumb protocol buffer paymentrequest
// with extra methods
//

class paymentrequestplus
{
public:
    paymentrequestplus() { }

    bool parse(const qbytearray& data);
    bool serializetostring(std::string* output) const;

    bool isinitialized() const;
    // returns true if merchant's identity is authenticated, and
    // returns human-readable merchant identity in merchant
    bool getmerchant(x509_store* certstore, qstring& merchant) const;

    // returns list of outputs, amount
    qlist<std::pair<cscript,camount> > getpayto() const;

    const payments::paymentdetails& getdetails() const { return details; }

private:
    payments::paymentrequest paymentrequest;
    payments::paymentdetails details;
};

#endif // moorecoin_qt_paymentrequestplus_h
