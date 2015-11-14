// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "transactiontablemodel.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "scicon.h"
#include "transactiondesc.h"
#include "transactionrecord.h"
#include "walletmodel.h"

#include "main.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"
#include "wallet/wallet.h"

#include <qcolor>
#include <qdatetime>
#include <qdebug>
#include <qicon>
#include <qlist>

// amount column is right-aligned it contains numbers
static int column_alignments[] = {
        qt::alignleft|qt::alignvcenter, /* status */
        qt::alignleft|qt::alignvcenter, /* watchonly */
        qt::alignleft|qt::alignvcenter, /* date */
        qt::alignleft|qt::alignvcenter, /* type */
        qt::alignleft|qt::alignvcenter, /* address */
        qt::alignright|qt::alignvcenter /* amount */
    };

// comparison operator for sort/binary search of model tx list
struct txlessthan
{
    bool operator()(const transactionrecord &a, const transactionrecord &b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const transactionrecord &a, const uint256 &b) const
    {
        return a.hash < b;
    }
    bool operator()(const uint256 &a, const transactionrecord &b) const
    {
        return a < b.hash;
    }
};

// private implementation
class transactiontablepriv
{
public:
    transactiontablepriv(cwallet *wallet, transactiontablemodel *parent) :
        wallet(wallet),
        parent(parent)
    {
    }

    cwallet *wallet;
    transactiontablemodel *parent;

    /* local cache of wallet.
     * as it is in the same order as the cwallet, by definition
     * this is sorted by sha256.
     */
    qlist<transactionrecord> cachedwallet;

    /* query entire wallet anew from core.
     */
    void refreshwallet()
    {
        qdebug() << "transactiontablepriv::refreshwallet";
        cachedwallet.clear();
        {
            lock2(cs_main, wallet->cs_wallet);
            for(std::map<uint256, cwallettx>::iterator it = wallet->mapwallet.begin(); it != wallet->mapwallet.end(); ++it)
            {
                if(transactionrecord::showtransaction(it->second))
                    cachedwallet.append(transactionrecord::decomposetransaction(wallet, it->second));
            }
        }
    }

    /* update our model of the wallet incrementally, to synchronize our model of the wallet
       with that of the core.

       call with transaction that was added, removed or changed.
     */
    void updatewallet(const uint256 &hash, int status, bool showtransaction)
    {
        qdebug() << "transactiontablepriv::updatewallet: " + qstring::fromstdstring(hash.tostring()) + " " + qstring::number(status);

        // find bounds of this transaction in model
        qlist<transactionrecord>::iterator lower = qlowerbound(
            cachedwallet.begin(), cachedwallet.end(), hash, txlessthan());
        qlist<transactionrecord>::iterator upper = qupperbound(
            cachedwallet.begin(), cachedwallet.end(), hash, txlessthan());
        int lowerindex = (lower - cachedwallet.begin());
        int upperindex = (upper - cachedwallet.begin());
        bool inmodel = (lower != upper);

        if(status == ct_updated)
        {
            if(showtransaction && !inmodel)
                status = ct_new; /* not in model, but want to show, treat as new */
            if(!showtransaction && inmodel)
                status = ct_deleted; /* in model, but want to hide, treat as deleted */
        }

        qdebug() << "    inmodel=" + qstring::number(inmodel) +
                    " index=" + qstring::number(lowerindex) + "-" + qstring::number(upperindex) +
                    " showtransaction=" + qstring::number(showtransaction) + " derivedstatus=" + qstring::number(status);

        switch(status)
        {
        case ct_new:
            if(inmodel)
            {
                qwarning() << "transactiontablepriv::updatewallet: warning: got ct_new, but transaction is already in model";
                break;
            }
            if(showtransaction)
            {
                lock2(cs_main, wallet->cs_wallet);
                // find transaction in wallet
                std::map<uint256, cwallettx>::iterator mi = wallet->mapwallet.find(hash);
                if(mi == wallet->mapwallet.end())
                {
                    qwarning() << "transactiontablepriv::updatewallet: warning: got ct_new, but transaction is not in wallet";
                    break;
                }
                // added -- insert at the right position
                qlist<transactionrecord> toinsert =
                        transactionrecord::decomposetransaction(wallet, mi->second);
                if(!toinsert.isempty()) /* only if something to insert */
                {
                    parent->begininsertrows(qmodelindex(), lowerindex, lowerindex+toinsert.size()-1);
                    int insert_idx = lowerindex;
                    foreach(const transactionrecord &rec, toinsert)
                    {
                        cachedwallet.insert(insert_idx, rec);
                        insert_idx += 1;
                    }
                    parent->endinsertrows();
                }
            }
            break;
        case ct_deleted:
            if(!inmodel)
            {
                qwarning() << "transactiontablepriv::updatewallet: warning: got ct_deleted, but transaction is not in model";
                break;
            }
            // removed -- remove entire transaction from table
            parent->beginremoverows(qmodelindex(), lowerindex, upperindex-1);
            cachedwallet.erase(lower, upper);
            parent->endremoverows();
            break;
        case ct_updated:
            // miscellaneous updates -- nothing to do, status update will take care of this, and is only computed for
            // visible transactions.
            break;
        }
    }

    int size()
    {
        return cachedwallet.size();
    }

    transactionrecord *index(int idx)
    {
        if(idx >= 0 && idx < cachedwallet.size())
        {
            transactionrecord *rec = &cachedwallet[idx];

            // get required locks upfront. this avoids the gui from getting
            // stuck if the core is holding the locks for a longer time - for
            // example, during a wallet rescan.
            //
            // if a status update is needed (blocks came in since last check),
            //  update the status of this transaction from the wallet. otherwise,
            // simply re-use the cached status.
            try_lock(cs_main, lockmain);
            if(lockmain)
            {
                try_lock(wallet->cs_wallet, lockwallet);
                if(lockwallet && rec->statusupdateneeded())
                {
                    std::map<uint256, cwallettx>::iterator mi = wallet->mapwallet.find(rec->hash);

                    if(mi != wallet->mapwallet.end())
                    {
                        rec->updatestatus(mi->second);
                    }
                }
            }
            return rec;
        }
        return 0;
    }

    qstring describe(transactionrecord *rec, int unit)
    {
        {
            lock2(cs_main, wallet->cs_wallet);
            std::map<uint256, cwallettx>::iterator mi = wallet->mapwallet.find(rec->hash);
            if(mi != wallet->mapwallet.end())
            {
                return transactiondesc::tohtml(wallet, mi->second, rec, unit);
            }
        }
        return qstring();
    }
};

transactiontablemodel::transactiontablemodel(cwallet* wallet, walletmodel *parent):
        qabstracttablemodel(parent),
        wallet(wallet),
        walletmodel(parent),
        priv(new transactiontablepriv(wallet, this)),
        fprocessingqueuedtransactions(false)
{
    columns << qstring() << qstring() << tr("date") << tr("type") << tr("label") << moorecoinunits::getamountcolumntitle(walletmodel->getoptionsmodel()->getdisplayunit());
    priv->refreshwallet();

    connect(walletmodel->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(updatedisplayunit()));

    subscribetocoresignals();
}

transactiontablemodel::~transactiontablemodel()
{
    unsubscribefromcoresignals();
    delete priv;
}

/** updates the column title to "amount (displayunit)" and emits headerdatachanged() signal for table headers to react. */
void transactiontablemodel::updateamountcolumntitle()
{
    columns[amount] = moorecoinunits::getamountcolumntitle(walletmodel->getoptionsmodel()->getdisplayunit());
    emit headerdatachanged(qt::horizontal,amount,amount);
}

void transactiontablemodel::updatetransaction(const qstring &hash, int status, bool showtransaction)
{
    uint256 updated;
    updated.sethex(hash.tostdstring());

    priv->updatewallet(updated, status, showtransaction);
}

void transactiontablemodel::updateconfirmations()
{
    // blocks came in since last poll.
    // invalidate status (number of confirmations) and (possibly) description
    //  for all rows. qt is smart enough to only actually request the data for the
    //  visible rows.
    emit datachanged(index(0, status), index(priv->size()-1, status));
    emit datachanged(index(0, toaddress), index(priv->size()-1, toaddress));
}

int transactiontablemodel::rowcount(const qmodelindex &parent) const
{
    q_unused(parent);
    return priv->size();
}

int transactiontablemodel::columncount(const qmodelindex &parent) const
{
    q_unused(parent);
    return columns.length();
}

qstring transactiontablemodel::formattxstatus(const transactionrecord *wtx) const
{
    qstring status;

    switch(wtx->status.status)
    {
    case transactionstatus::openuntilblock:
        status = tr("open for %n more block(s)","",wtx->status.open_for);
        break;
    case transactionstatus::openuntildate:
        status = tr("open until %1").arg(guiutil::datetimestr(wtx->status.open_for));
        break;
    case transactionstatus::offline:
        status = tr("offline");
        break;
    case transactionstatus::unconfirmed:
        status = tr("unconfirmed");
        break;
    case transactionstatus::confirming:
        status = tr("confirming (%1 of %2 recommended confirmations)").arg(wtx->status.depth).arg(transactionrecord::recommendednumconfirmations);
        break;
    case transactionstatus::confirmed:
        status = tr("confirmed (%1 confirmations)").arg(wtx->status.depth);
        break;
    case transactionstatus::conflicted:
        status = tr("conflicted");
        break;
    case transactionstatus::immature:
        status = tr("immature (%1 confirmations, will be available after %2)").arg(wtx->status.depth).arg(wtx->status.depth + wtx->status.matures_in);
        break;
    case transactionstatus::matureswarning:
        status = tr("this block was not received by any other nodes and will probably not be accepted!");
        break;
    case transactionstatus::notaccepted:
        status = tr("generated but not accepted");
        break;
    }

    return status;
}

qstring transactiontablemodel::formattxdate(const transactionrecord *wtx) const
{
    if(wtx->time)
    {
        return guiutil::datetimestr(wtx->time);
    }
    return qstring();
}

/* look up address in address book, if found return label (address)
   otherwise just return (address)
 */
qstring transactiontablemodel::lookupaddress(const std::string &address, bool tooltip) const
{
    qstring label = walletmodel->getaddresstablemodel()->labelforaddress(qstring::fromstdstring(address));
    qstring description;
    if(!label.isempty())
    {
        description += label;
    }
    if(label.isempty() || tooltip)
    {
        description += qstring(" (") + qstring::fromstdstring(address) + qstring(")");
    }
    return description;
}

qstring transactiontablemodel::formattxtype(const transactionrecord *wtx) const
{
    switch(wtx->type)
    {
    case transactionrecord::recvwithaddress:
        return tr("received with");
    case transactionrecord::recvfromother:
        return tr("received from");
    case transactionrecord::sendtoaddress:
    case transactionrecord::sendtoother:
        return tr("sent to");
    case transactionrecord::sendtoself:
        return tr("payment to yourself");
    case transactionrecord::generated:
        return tr("mined");
    default:
        return qstring();
    }
}

qvariant transactiontablemodel::txaddressdecoration(const transactionrecord *wtx) const
{
    switch(wtx->type)
    {
    case transactionrecord::generated:
        return qicon(":/icons/tx_mined");
    case transactionrecord::recvwithaddress:
    case transactionrecord::recvfromother:
        return qicon(":/icons/tx_input");
    case transactionrecord::sendtoaddress:
    case transactionrecord::sendtoother:
        return qicon(":/icons/tx_output");
    default:
        return qicon(":/icons/tx_inout");
    }
}

qstring transactiontablemodel::formattxtoaddress(const transactionrecord *wtx, bool tooltip) const
{
    qstring watchaddress;
    if (tooltip) {
        // mark transactions involving watch-only addresses by adding " (watch-only)"
        watchaddress = wtx->involveswatchaddress ? qstring(" (") + tr("watch-only") + qstring(")") : "";
    }

    switch(wtx->type)
    {
    case transactionrecord::recvfromother:
        return qstring::fromstdstring(wtx->address) + watchaddress;
    case transactionrecord::recvwithaddress:
    case transactionrecord::sendtoaddress:
    case transactionrecord::generated:
        return lookupaddress(wtx->address, tooltip) + watchaddress;
    case transactionrecord::sendtoother:
        return qstring::fromstdstring(wtx->address) + watchaddress;
    case transactionrecord::sendtoself:
    default:
        return tr("(n/a)") + watchaddress;
    }
}

qvariant transactiontablemodel::addresscolor(const transactionrecord *wtx) const
{
    // show addresses without label in a less visible color
    switch(wtx->type)
    {
    case transactionrecord::recvwithaddress:
    case transactionrecord::sendtoaddress:
    case transactionrecord::generated:
        {
        qstring label = walletmodel->getaddresstablemodel()->labelforaddress(qstring::fromstdstring(wtx->address));
        if(label.isempty())
            return color_bareaddress;
        } break;
    case transactionrecord::sendtoself:
        return color_bareaddress;
    default:
        break;
    }
    return qvariant();
}

qstring transactiontablemodel::formattxamount(const transactionrecord *wtx, bool showunconfirmed, moorecoinunits::separatorstyle separators) const
{
    qstring str = moorecoinunits::format(walletmodel->getoptionsmodel()->getdisplayunit(), wtx->credit + wtx->debit, false, separators);
    if(showunconfirmed)
    {
        if(!wtx->status.countsforbalance)
        {
            str = qstring("[") + str + qstring("]");
        }
    }
    return qstring(str);
}

qvariant transactiontablemodel::txstatusdecoration(const transactionrecord *wtx) const
{
    switch(wtx->status.status)
    {
    case transactionstatus::openuntilblock:
    case transactionstatus::openuntildate:
        return color_tx_status_openuntildate;
    case transactionstatus::offline:
        return color_tx_status_offline;
    case transactionstatus::unconfirmed:
        return qicon(":/icons/transaction_0");
    case transactionstatus::confirming:
        switch(wtx->status.depth)
        {
        case 1: return qicon(":/icons/transaction_1");
        case 2: return qicon(":/icons/transaction_2");
        case 3: return qicon(":/icons/transaction_3");
        case 4: return qicon(":/icons/transaction_4");
        default: return qicon(":/icons/transaction_5");
        };
    case transactionstatus::confirmed:
        return qicon(":/icons/transaction_confirmed");
    case transactionstatus::conflicted:
        return qicon(":/icons/transaction_conflicted");
    case transactionstatus::immature: {
        int total = wtx->status.depth + wtx->status.matures_in;
        int part = (wtx->status.depth * 4 / total) + 1;
        return qicon(qstring(":/icons/transaction_%1").arg(part));
        }
    case transactionstatus::matureswarning:
    case transactionstatus::notaccepted:
        return qicon(":/icons/transaction_0");
    default:
        return color_black;
    }
}

qvariant transactiontablemodel::txwatchonlydecoration(const transactionrecord *wtx) const
{
    if (wtx->involveswatchaddress)
        return qicon(":/icons/eye");
    else
        return qvariant();
}

qstring transactiontablemodel::formattooltip(const transactionrecord *rec) const
{
    qstring tooltip = formattxstatus(rec) + qstring("\n") + formattxtype(rec);
    if(rec->type==transactionrecord::recvfromother || rec->type==transactionrecord::sendtoother ||
       rec->type==transactionrecord::sendtoaddress || rec->type==transactionrecord::recvwithaddress)
    {
        tooltip += qstring(" ") + formattxtoaddress(rec, true);
    }
    return tooltip;
}

qvariant transactiontablemodel::data(const qmodelindex &index, int role) const
{
    if(!index.isvalid())
        return qvariant();
    transactionrecord *rec = static_cast<transactionrecord*>(index.internalpointer());

    switch(role)
    {
    case rawdecorationrole:
        switch(index.column())
        {
        case status:
            return txstatusdecoration(rec);
        case watchonly:
            return txwatchonlydecoration(rec);
        case toaddress:
            return txaddressdecoration(rec);
        }
        break;
    case qt::decorationrole:
    {
        qicon icon = qvariant_cast<qicon>(index.data(rawdecorationrole));
        return textcoloricon(icon);
    }
    case qt::displayrole:
        switch(index.column())
        {
        case date:
            return formattxdate(rec);
        case type:
            return formattxtype(rec);
        case toaddress:
            return formattxtoaddress(rec, false);
        case amount:
            return formattxamount(rec, true, moorecoinunits::separatoralways);
        }
        break;
    case qt::editrole:
        // edit role is used for sorting, so return the unformatted values
        switch(index.column())
        {
        case status:
            return qstring::fromstdstring(rec->status.sortkey);
        case date:
            return rec->time;
        case type:
            return formattxtype(rec);
        case watchonly:
            return (rec->involveswatchaddress ? 1 : 0);
        case toaddress:
            return formattxtoaddress(rec, true);
        case amount:
            return qint64(rec->credit + rec->debit);
        }
        break;
    case qt::tooltiprole:
        return formattooltip(rec);
    case qt::textalignmentrole:
        return column_alignments[index.column()];
    case qt::foregroundrole:
        // non-confirmed (but not immature) as transactions are grey
        if(!rec->status.countsforbalance && rec->status.status != transactionstatus::immature)
        {
            return color_unconfirmed;
        }
        if(index.column() == amount && (rec->credit+rec->debit) < 0)
        {
            return color_negative;
        }
        if(index.column() == toaddress)
        {
            return addresscolor(rec);
        }
        break;
    case typerole:
        return rec->type;
    case daterole:
        return qdatetime::fromtime_t(static_cast<uint>(rec->time));
    case watchonlyrole:
        return rec->involveswatchaddress;
    case watchonlydecorationrole:
        return txwatchonlydecoration(rec);
    case longdescriptionrole:
        return priv->describe(rec, walletmodel->getoptionsmodel()->getdisplayunit());
    case addressrole:
        return qstring::fromstdstring(rec->address);
    case labelrole:
        return walletmodel->getaddresstablemodel()->labelforaddress(qstring::fromstdstring(rec->address));
    case amountrole:
        return qint64(rec->credit + rec->debit);
    case txidrole:
        return rec->gettxid();
    case txhashrole:
        return qstring::fromstdstring(rec->hash.tostring());
    case confirmedrole:
        return rec->status.countsforbalance;
    case formattedamountrole:
        // used for copy/export, so don't include separators
        return formattxamount(rec, false, moorecoinunits::separatornever);
    case statusrole:
        return rec->status.status;
    }
    return qvariant();
}

qvariant transactiontablemodel::headerdata(int section, qt::orientation orientation, int role) const
{
    if(orientation == qt::horizontal)
    {
        if(role == qt::displayrole)
        {
            return columns[section];
        }
        else if (role == qt::textalignmentrole)
        {
            return column_alignments[section];
        } else if (role == qt::tooltiprole)
        {
            switch(section)
            {
            case status:
                return tr("transaction status. hover over this field to show number of confirmations.");
            case date:
                return tr("date and time that the transaction was received.");
            case type:
                return tr("type of transaction.");
            case watchonly:
                return tr("whether or not a watch-only address is involved in this transaction.");
            case toaddress:
                return tr("user-defined intent/purpose of the transaction.");
            case amount:
                return tr("amount removed from or added to balance.");
            }
        }
    }
    return qvariant();
}

qmodelindex transactiontablemodel::index(int row, int column, const qmodelindex &parent) const
{
    q_unused(parent);
    transactionrecord *data = priv->index(row);
    if(data)
    {
        return createindex(row, column, priv->index(row));
    }
    return qmodelindex();
}

void transactiontablemodel::updatedisplayunit()
{
    // emit datachanged to update amount column with the current unit
    updateamountcolumntitle();
    emit datachanged(index(0, amount), index(priv->size()-1, amount));
}

// queue notifications to show a non freezing progress dialog e.g. for rescan
struct transactionnotification
{
public:
    transactionnotification() {}
    transactionnotification(uint256 hash, changetype status, bool showtransaction):
        hash(hash), status(status), showtransaction(showtransaction) {}

    void invoke(qobject *ttm)
    {
        qstring strhash = qstring::fromstdstring(hash.gethex());
        qdebug() << "notifytransactionchanged: " + strhash + " status= " + qstring::number(status);
        qmetaobject::invokemethod(ttm, "updatetransaction", qt::queuedconnection,
                                  q_arg(qstring, strhash),
                                  q_arg(int, status),
                                  q_arg(bool, showtransaction));
    }
private:
    uint256 hash;
    changetype status;
    bool showtransaction;
};

static bool fqueuenotifications = false;
static std::vector< transactionnotification > vqueuenotifications;

static void notifytransactionchanged(transactiontablemodel *ttm, cwallet *wallet, const uint256 &hash, changetype status)
{
    // find transaction in wallet
    std::map<uint256, cwallettx>::iterator mi = wallet->mapwallet.find(hash);
    // determine whether to show transaction or not (determine this here so that no relocking is needed in gui thread)
    bool inwallet = mi != wallet->mapwallet.end();
    bool showtransaction = (inwallet && transactionrecord::showtransaction(mi->second));

    transactionnotification notification(hash, status, showtransaction);

    if (fqueuenotifications)
    {
        vqueuenotifications.push_back(notification);
        return;
    }
    notification.invoke(ttm);
}

static void showprogress(transactiontablemodel *ttm, const std::string &title, int nprogress)
{
    if (nprogress == 0)
        fqueuenotifications = true;

    if (nprogress == 100)
    {
        fqueuenotifications = false;
        if (vqueuenotifications.size() > 10) // prevent balloon spam, show maximum 10 balloons
            qmetaobject::invokemethod(ttm, "setprocessingqueuedtransactions", qt::queuedconnection, q_arg(bool, true));
        for (unsigned int i = 0; i < vqueuenotifications.size(); ++i)
        {
            if (vqueuenotifications.size() - i <= 10)
                qmetaobject::invokemethod(ttm, "setprocessingqueuedtransactions", qt::queuedconnection, q_arg(bool, false));

            vqueuenotifications[i].invoke(ttm);
        }
        std::vector<transactionnotification >().swap(vqueuenotifications); // clear
    }
}

void transactiontablemodel::subscribetocoresignals()
{
    // connect signals to wallet
    wallet->notifytransactionchanged.connect(boost::bind(notifytransactionchanged, this, _1, _2, _3));
    wallet->showprogress.connect(boost::bind(showprogress, this, _1, _2));
}

void transactiontablemodel::unsubscribefromcoresignals()
{
    // disconnect signals from wallet
    wallet->notifytransactionchanged.disconnect(boost::bind(notifytransactionchanged, this, _1, _2, _3));
    wallet->showprogress.disconnect(boost::bind(showprogress, this, _1, _2));
}
