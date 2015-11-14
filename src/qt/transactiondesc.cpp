// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "transactiondesc.h"

#include "moorecoinunits.h"
#include "guiutil.h"
#include "paymentserver.h"
#include "transactionrecord.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "main.h"
#include "script/script.h"
#include "timedata.h"
#include "util.h"
#include "wallet/db.h"
#include "wallet/wallet.h"

#include <stdint.h>
#include <string>

using namespace std;

qstring transactiondesc::formattxstatus(const cwallettx& wtx)
{
    assertlockheld(cs_main);
    if (!checkfinaltx(wtx))
    {
        if (wtx.nlocktime < locktime_threshold)
            return tr("open for %n more block(s)", "", wtx.nlocktime - chainactive.height());
        else
            return tr("open until %1").arg(guiutil::datetimestr(wtx.nlocktime));
    }
    else
    {
        int ndepth = wtx.getdepthinmainchain();
        if (ndepth < 0)
            return tr("conflicted");
        else if (getadjustedtime() - wtx.ntimereceived > 2 * 60 && wtx.getrequestcount() == 0)
            return tr("%1/offline").arg(ndepth);
        else if (ndepth < 6)
            return tr("%1/unconfirmed").arg(ndepth);
        else
            return tr("%1 confirmations").arg(ndepth);
    }
}

qstring transactiondesc::tohtml(cwallet *wallet, cwallettx &wtx, transactionrecord *rec, int unit)
{
    qstring strhtml;

    lock2(cs_main, wallet->cs_wallet);
    strhtml.reserve(4000);
    strhtml += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    int64_t ntime = wtx.gettxtime();
    camount ncredit = wtx.getcredit(ismine_all);
    camount ndebit = wtx.getdebit(ismine_all);
    camount nnet = ncredit - ndebit;

    strhtml += "<b>" + tr("status") + ":</b> " + formattxstatus(wtx);
    int nrequests = wtx.getrequestcount();
    if (nrequests != -1)
    {
        if (nrequests == 0)
            strhtml += tr(", has not been successfully broadcast yet");
        else if (nrequests > 0)
            strhtml += tr(", broadcast through %n node(s)", "", nrequests);
    }
    strhtml += "<br>";

    strhtml += "<b>" + tr("date") + ":</b> " + (ntime ? guiutil::datetimestr(ntime) : "") + "<br>";

    //
    // from
    //
    if (wtx.iscoinbase())
    {
        strhtml += "<b>" + tr("source") + ":</b> " + tr("generated") + "<br>";
    }
    else if (wtx.mapvalue.count("from") && !wtx.mapvalue["from"].empty())
    {
        // online transaction
        strhtml += "<b>" + tr("from") + ":</b> " + guiutil::htmlescape(wtx.mapvalue["from"]) + "<br>";
    }
    else
    {
        // offline transaction
        if (nnet > 0)
        {
            // credit
            if (cmoorecoinaddress(rec->address).isvalid())
            {
                ctxdestination address = cmoorecoinaddress(rec->address).get();
                if (wallet->mapaddressbook.count(address))
                {
                    strhtml += "<b>" + tr("from") + ":</b> " + tr("unknown") + "<br>";
                    strhtml += "<b>" + tr("to") + ":</b> ";
                    strhtml += guiutil::htmlescape(rec->address);
                    qstring addressowned = (::ismine(*wallet, address) == ismine_spendable) ? tr("own address") : tr("watch-only");
                    if (!wallet->mapaddressbook[address].name.empty())
                        strhtml += " (" + addressowned + ", " + tr("label") + ": " + guiutil::htmlescape(wallet->mapaddressbook[address].name) + ")";
                    else
                        strhtml += " (" + addressowned + ")";
                    strhtml += "<br>";
                }
            }
        }
    }

    //
    // to
    //
    if (wtx.mapvalue.count("to") && !wtx.mapvalue["to"].empty())
    {
        // online transaction
        std::string straddress = wtx.mapvalue["to"];
        strhtml += "<b>" + tr("to") + ":</b> ";
        ctxdestination dest = cmoorecoinaddress(straddress).get();
        if (wallet->mapaddressbook.count(dest) && !wallet->mapaddressbook[dest].name.empty())
            strhtml += guiutil::htmlescape(wallet->mapaddressbook[dest].name) + " ";
        strhtml += guiutil::htmlescape(straddress) + "<br>";
    }

    //
    // amount
    //
    if (wtx.iscoinbase() && ncredit == 0)
    {
        //
        // coinbase
        //
        camount nunmatured = 0;
        boost_foreach(const ctxout& txout, wtx.vout)
            nunmatured += wallet->getcredit(txout, ismine_all);
        strhtml += "<b>" + tr("credit") + ":</b> ";
        if (wtx.isinmainchain())
            strhtml += moorecoinunits::formathtmlwithunit(unit, nunmatured)+ " (" + tr("matures in %n more block(s)", "", wtx.getblockstomaturity()) + ")";
        else
            strhtml += "(" + tr("not accepted") + ")";
        strhtml += "<br>";
    }
    else if (nnet > 0)
    {
        //
        // credit
        //
        strhtml += "<b>" + tr("credit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, nnet) + "<br>";
    }
    else
    {
        isminetype fallfromme = ismine_spendable;
        boost_foreach(const ctxin& txin, wtx.vin)
        {
            isminetype mine = wallet->ismine(txin);
            if(fallfromme > mine) fallfromme = mine;
        }

        isminetype falltome = ismine_spendable;
        boost_foreach(const ctxout& txout, wtx.vout)
        {
            isminetype mine = wallet->ismine(txout);
            if(falltome > mine) falltome = mine;
        }

        if (fallfromme)
        {
            if(fallfromme == ismine_watch_only)
                strhtml += "<b>" + tr("from") + ":</b> " + tr("watch-only") + "<br>";

            //
            // debit
            //
            boost_foreach(const ctxout& txout, wtx.vout)
            {
                // ignore change
                isminetype toself = wallet->ismine(txout);
                if ((toself == ismine_spendable) && (fallfromme == ismine_spendable))
                    continue;

                if (!wtx.mapvalue.count("to") || wtx.mapvalue["to"].empty())
                {
                    // offline transaction
                    ctxdestination address;
                    if (extractdestination(txout.scriptpubkey, address))
                    {
                        strhtml += "<b>" + tr("to") + ":</b> ";
                        if (wallet->mapaddressbook.count(address) && !wallet->mapaddressbook[address].name.empty())
                            strhtml += guiutil::htmlescape(wallet->mapaddressbook[address].name) + " ";
                        strhtml += guiutil::htmlescape(cmoorecoinaddress(address).tostring());
                        if(toself == ismine_spendable)
                            strhtml += " (own address)";
                        else if(toself == ismine_watch_only)
                            strhtml += " (watch-only)";
                        strhtml += "<br>";
                    }
                }

                strhtml += "<b>" + tr("debit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, -txout.nvalue) + "<br>";
                if(toself)
                    strhtml += "<b>" + tr("credit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, txout.nvalue) + "<br>";
            }

            if (falltome)
            {
                // payment to self
                camount nchange = wtx.getchange();
                camount nvalue = ncredit - nchange;
                strhtml += "<b>" + tr("total debit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, -nvalue) + "<br>";
                strhtml += "<b>" + tr("total credit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, nvalue) + "<br>";
            }

            camount ntxfee = ndebit - wtx.getvalueout();
            if (ntxfee > 0)
                strhtml += "<b>" + tr("transaction fee") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, -ntxfee) + "<br>";
        }
        else
        {
            //
            // mixed debit transaction
            //
            boost_foreach(const ctxin& txin, wtx.vin)
                if (wallet->ismine(txin))
                    strhtml += "<b>" + tr("debit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, -wallet->getdebit(txin, ismine_all)) + "<br>";
            boost_foreach(const ctxout& txout, wtx.vout)
                if (wallet->ismine(txout))
                    strhtml += "<b>" + tr("credit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, wallet->getcredit(txout, ismine_all)) + "<br>";
        }
    }

    strhtml += "<b>" + tr("net amount") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, nnet, true) + "<br>";

    //
    // message
    //
    if (wtx.mapvalue.count("message") && !wtx.mapvalue["message"].empty())
        strhtml += "<br><b>" + tr("message") + ":</b><br>" + guiutil::htmlescape(wtx.mapvalue["message"], true) + "<br>";
    if (wtx.mapvalue.count("comment") && !wtx.mapvalue["comment"].empty())
        strhtml += "<br><b>" + tr("comment") + ":</b><br>" + guiutil::htmlescape(wtx.mapvalue["comment"], true) + "<br>";

    strhtml += "<b>" + tr("transaction id") + ":</b> " + transactionrecord::formatsubtxid(wtx.gethash(), rec->idx) + "<br>";

    // message from normal moorecoin:uri (moorecoin:123...?message=example)
    foreach (const pairtype(string, string)& r, wtx.vorderform)
        if (r.first == "message")
            strhtml += "<br><b>" + tr("message") + ":</b><br>" + guiutil::htmlescape(r.second, true) + "<br>";

    //
    // paymentrequest info:
    //
    foreach (const pairtype(string, string)& r, wtx.vorderform)
    {
        if (r.first == "paymentrequest")
        {
            paymentrequestplus req;
            req.parse(qbytearray::fromrawdata(r.second.data(), r.second.size()));
            qstring merchant;
            if (req.getmerchant(paymentserver::getcertstore(), merchant))
                strhtml += "<b>" + tr("merchant") + ":</b> " + guiutil::htmlescape(merchant) + "<br>";
        }
    }

    if (wtx.iscoinbase())
    {
        quint32 numblockstomaturity = coinbase_maturity +  1;
        strhtml += "<br>" + tr("generated coins must mature %1 blocks before they can be spent. when you generated this block, it was broadcast to the network to be added to the block chain. if it fails to get into the chain, its state will change to \"not accepted\" and it won't be spendable. this may occasionally happen if another node generates a block within a few seconds of yours.").arg(qstring::number(numblockstomaturity)) + "<br>";
    }

    //
    // debug view
    //
    if (fdebug)
    {
        strhtml += "<hr><br>" + tr("debug information") + "<br><br>";
        boost_foreach(const ctxin& txin, wtx.vin)
            if(wallet->ismine(txin))
                strhtml += "<b>" + tr("debit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, -wallet->getdebit(txin, ismine_all)) + "<br>";
        boost_foreach(const ctxout& txout, wtx.vout)
            if(wallet->ismine(txout))
                strhtml += "<b>" + tr("credit") + ":</b> " + moorecoinunits::formathtmlwithunit(unit, wallet->getcredit(txout, ismine_all)) + "<br>";

        strhtml += "<br><b>" + tr("transaction") + ":</b><br>";
        strhtml += guiutil::htmlescape(wtx.tostring(), true);

        strhtml += "<br><b>" + tr("inputs") + ":</b>";
        strhtml += "<ul>";

        boost_foreach(const ctxin& txin, wtx.vin)
        {
            coutpoint prevout = txin.prevout;

            ccoins prev;
            if(pcoinstip->getcoins(prevout.hash, prev))
            {
                if (prevout.n < prev.vout.size())
                {
                    strhtml += "<li>";
                    const ctxout &vout = prev.vout[prevout.n];
                    ctxdestination address;
                    if (extractdestination(vout.scriptpubkey, address))
                    {
                        if (wallet->mapaddressbook.count(address) && !wallet->mapaddressbook[address].name.empty())
                            strhtml += guiutil::htmlescape(wallet->mapaddressbook[address].name) + " ";
                        strhtml += qstring::fromstdstring(cmoorecoinaddress(address).tostring());
                    }
                    strhtml = strhtml + " " + tr("amount") + "=" + moorecoinunits::formathtmlwithunit(unit, vout.nvalue);
                    strhtml = strhtml + " ismine=" + (wallet->ismine(vout) & ismine_spendable ? tr("true") : tr("false")) + "</li>";
                    strhtml = strhtml + " iswatchonly=" + (wallet->ismine(vout) & ismine_watch_only ? tr("true") : tr("false")) + "</li>";
                }
            }
        }

        strhtml += "</ul>";
    }

    strhtml += "</font></html>";
    return strhtml;
}
