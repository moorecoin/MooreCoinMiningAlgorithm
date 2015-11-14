// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodeltransaction.h"

#include "wallet/wallet.h"

walletmodeltransaction::walletmodeltransaction(const qlist<sendcoinsrecipient> &recipients) :
    recipients(recipients),
    wallettransaction(0),
    keychange(0),
    fee(0)
{
    wallettransaction = new cwallettx();
}

walletmodeltransaction::~walletmodeltransaction()
{
    delete keychange;
    delete wallettransaction;
}

qlist<sendcoinsrecipient> walletmodeltransaction::getrecipients()
{
    return recipients;
}

cwallettx *walletmodeltransaction::gettransaction()
{
    return wallettransaction;
}

unsigned int walletmodeltransaction::gettransactionsize()
{
    return (!wallettransaction ? 0 : (::getserializesize(*(ctransaction*)wallettransaction, ser_network, protocol_version)));
}

camount walletmodeltransaction::gettransactionfee()
{
    return fee;
}

void walletmodeltransaction::settransactionfee(const camount& newfee)
{
    fee = newfee;
}

void walletmodeltransaction::reassignamounts(int nchangeposret)
{
    int i = 0;
    for (qlist<sendcoinsrecipient>::iterator it = recipients.begin(); it != recipients.end(); ++it)
    {
        sendcoinsrecipient& rcp = (*it);

        if (rcp.paymentrequest.isinitialized())
        {
            camount subtotal = 0;
            const payments::paymentdetails& details = rcp.paymentrequest.getdetails();
            for (int j = 0; j < details.outputs_size(); j++)
            {
                const payments::output& out = details.outputs(j);
                if (out.amount() <= 0) continue;
                if (i == nchangeposret)
                    i++;
                subtotal += wallettransaction->vout[i].nvalue;
                i++;
            }
            rcp.amount = subtotal;
        }
        else // normal recipient (no payment request)
        {
            if (i == nchangeposret)
                i++;
            rcp.amount = wallettransaction->vout[i].nvalue;
            i++;
        }
    }
}

camount walletmodeltransaction::gettotaltransactionamount()
{
    camount totaltransactionamount = 0;
    foreach(const sendcoinsrecipient &rcp, recipients)
    {
        totaltransactionamount += rcp.amount;
    }
    return totaltransactionamount;
}

void walletmodeltransaction::newpossiblekeychange(cwallet *wallet)
{
    keychange = new creservekey(wallet);
}

creservekey *walletmodeltransaction::getpossiblekeychange()
{
    return keychange;
}
