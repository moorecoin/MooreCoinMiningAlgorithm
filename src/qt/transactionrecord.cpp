// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "transactionrecord.h"

#include "base58.h"
#include "consensus/consensus.h"
#include "main.h"
#include "timedata.h"
#include "wallet/wallet.h"

#include <stdint.h>

#include <boost/foreach.hpp>

/* return positive answer if transaction should be shown in list.
 */
bool transactionrecord::showtransaction(const cwallettx &wtx)
{
    if (wtx.iscoinbase())
    {
        // ensures we show generated coins / mined transactions at depth 1
        if (!wtx.isinmainchain())
        {
            return false;
        }
    }
    return true;
}

/*
 * decompose cwallet transaction to model transaction records.
 */
qlist<transactionrecord> transactionrecord::decomposetransaction(const cwallet *wallet, const cwallettx &wtx)
{
    qlist<transactionrecord> parts;
    int64_t ntime = wtx.gettxtime();
    camount ncredit = wtx.getcredit(ismine_all);
    camount ndebit = wtx.getdebit(ismine_all);
    camount nnet = ncredit - ndebit;
    uint256 hash = wtx.gethash();
    std::map<std::string, std::string> mapvalue = wtx.mapvalue;

    if (nnet > 0 || wtx.iscoinbase())
    {
        //
        // credit
        //
        boost_foreach(const ctxout& txout, wtx.vout)
        {
            isminetype mine = wallet->ismine(txout);
            if(mine)
            {
                transactionrecord sub(hash, ntime);
                ctxdestination address;
                sub.idx = parts.size(); // sequence number
                sub.credit = txout.nvalue;
                sub.involveswatchaddress = mine == ismine_watch_only;
                if (extractdestination(txout.scriptpubkey, address) && ismine(*wallet, address))
                {
                    // received by moorecoin address
                    sub.type = transactionrecord::recvwithaddress;
                    sub.address = cmoorecoinaddress(address).tostring();
                }
                else
                {
                    // received by ip connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = transactionrecord::recvfromother;
                    sub.address = mapvalue["from"];
                }
                if (wtx.iscoinbase())
                {
                    // generated
                    sub.type = transactionrecord::generated;
                }

                parts.append(sub);
            }
        }
    }
    else
    {
        bool involveswatchaddress = false;
        isminetype fallfromme = ismine_spendable;
        boost_foreach(const ctxin& txin, wtx.vin)
        {
            isminetype mine = wallet->ismine(txin);
            if(mine == ismine_watch_only) involveswatchaddress = true;
            if(fallfromme > mine) fallfromme = mine;
        }

        isminetype falltome = ismine_spendable;
        boost_foreach(const ctxout& txout, wtx.vout)
        {
            isminetype mine = wallet->ismine(txout);
            if(mine == ismine_watch_only) involveswatchaddress = true;
            if(falltome > mine) falltome = mine;
        }

        if (fallfromme && falltome)
        {
            // payment to self
            camount nchange = wtx.getchange();

            parts.append(transactionrecord(hash, ntime, transactionrecord::sendtoself, "",
                            -(ndebit - nchange), ncredit - nchange));
            parts.last().involveswatchaddress = involveswatchaddress;   // maybe pass to transactionrecord as constructor argument
        }
        else if (fallfromme)
        {
            //
            // debit
            //
            camount ntxfee = ndebit - wtx.getvalueout();

            for (unsigned int nout = 0; nout < wtx.vout.size(); nout++)
            {
                const ctxout& txout = wtx.vout[nout];
                transactionrecord sub(hash, ntime);
                sub.idx = parts.size();
                sub.involveswatchaddress = involveswatchaddress;

                if(wallet->ismine(txout))
                {
                    // ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                ctxdestination address;
                if (extractdestination(txout.scriptpubkey, address))
                {
                    // sent to moorecoin address
                    sub.type = transactionrecord::sendtoaddress;
                    sub.address = cmoorecoinaddress(address).tostring();
                }
                else
                {
                    // sent to ip, or other non-address transaction like op_eval
                    sub.type = transactionrecord::sendtoother;
                    sub.address = mapvalue["to"];
                }

                camount nvalue = txout.nvalue;
                /* add fee to first output */
                if (ntxfee > 0)
                {
                    nvalue += ntxfee;
                    ntxfee = 0;
                }
                sub.debit = -nvalue;

                parts.append(sub);
            }
        }
        else
        {
            //
            // mixed debit transaction, can't break down payees
            //
            parts.append(transactionrecord(hash, ntime, transactionrecord::other, "", nnet, 0));
            parts.last().involveswatchaddress = involveswatchaddress;
        }
    }

    return parts;
}

void transactionrecord::updatestatus(const cwallettx &wtx)
{
    assertlockheld(cs_main);
    // determine transaction status

    // find the block the tx is in
    cblockindex* pindex = null;
    blockmap::iterator mi = mapblockindex.find(wtx.hashblock);
    if (mi != mapblockindex.end())
        pindex = (*mi).second;

    // sort order, unrecorded transactions sort to the top
    status.sortkey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nheight : std::numeric_limits<int>::max()),
        (wtx.iscoinbase() ? 1 : 0),
        wtx.ntimereceived,
        idx);
    status.countsforbalance = wtx.istrusted() && !(wtx.getblockstomaturity() > 0);
    status.depth = wtx.getdepthinmainchain();
    status.cur_num_blocks = chainactive.height();

    if (!checkfinaltx(wtx))
    {
        if (wtx.nlocktime < locktime_threshold)
        {
            status.status = transactionstatus::openuntilblock;
            status.open_for = wtx.nlocktime - chainactive.height();
        }
        else
        {
            status.status = transactionstatus::openuntildate;
            status.open_for = wtx.nlocktime;
        }
    }
    // for generated transactions, determine maturity
    else if(type == transactionrecord::generated)
    {
        if (wtx.getblockstomaturity() > 0)
        {
            status.status = transactionstatus::immature;

            if (wtx.isinmainchain())
            {
                status.matures_in = wtx.getblockstomaturity();

                // check if the block was requested by anyone
                if (getadjustedtime() - wtx.ntimereceived > 2 * 60 && wtx.getrequestcount() == 0)
                    status.status = transactionstatus::matureswarning;
            }
            else
            {
                status.status = transactionstatus::notaccepted;
            }
        }
        else
        {
            status.status = transactionstatus::confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = transactionstatus::conflicted;
        }
        else if (getadjustedtime() - wtx.ntimereceived > 2 * 60 && wtx.getrequestcount() == 0)
        {
            status.status = transactionstatus::offline;
        }
        else if (status.depth == 0)
        {
            status.status = transactionstatus::unconfirmed;
        }
        else if (status.depth < recommendednumconfirmations)
        {
            status.status = transactionstatus::confirming;
        }
        else
        {
            status.status = transactionstatus::confirmed;
        }
    }

}

bool transactionrecord::statusupdateneeded()
{
    assertlockheld(cs_main);
    return status.cur_num_blocks != chainactive.height();
}

qstring transactionrecord::gettxid() const
{
    return formatsubtxid(hash, idx);
}

qstring transactionrecord::formatsubtxid(const uint256 &hash, int vout)
{
    return qstring::fromstdstring(hash.tostring() + strprintf("-%03d", vout));
}

