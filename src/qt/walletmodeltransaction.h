// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_walletmodeltransaction_h
#define moorecoin_qt_walletmodeltransaction_h

#include "walletmodel.h"

#include <qobject>

class sendcoinsrecipient;

class creservekey;
class cwallet;
class cwallettx;

/** data model for a walletmodel transaction. */
class walletmodeltransaction
{
public:
    explicit walletmodeltransaction(const qlist<sendcoinsrecipient> &recipients);
    ~walletmodeltransaction();

    qlist<sendcoinsrecipient> getrecipients();

    cwallettx *gettransaction();
    unsigned int gettransactionsize();

    void settransactionfee(const camount& newfee);
    camount gettransactionfee();

    camount gettotaltransactionamount();

    void newpossiblekeychange(cwallet *wallet);
    creservekey *getpossiblekeychange();

    void reassignamounts(int nchangeposret); // needed for the subtract-fee-from-amount feature

private:
    qlist<sendcoinsrecipient> recipients;
    cwallettx *wallettransaction;
    creservekey *keychange;
    camount fee;
};

#endif // moorecoin_qt_walletmodeltransaction_h
