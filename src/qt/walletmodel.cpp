// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "walletmodel.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "paymentserver.h"
#include "recentrequeststablemodel.h"
#include "transactiontablemodel.h"

#include "base58.h"
#include "keystore.h"
#include "main.h"
#include "sync.h"
#include "ui_interface.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h" // for backupwallet

#include <stdint.h>

#include <qdebug>
#include <qset>
#include <qtimer>

using namespace std;

walletmodel::walletmodel(cwallet *wallet, optionsmodel *optionsmodel, qobject *parent) :
    qobject(parent), wallet(wallet), optionsmodel(optionsmodel), addresstablemodel(0),
    transactiontablemodel(0),
    recentrequeststablemodel(0),
    cachedbalance(0), cachedunconfirmedbalance(0), cachedimmaturebalance(0),
    cachedencryptionstatus(unencrypted),
    cachednumblocks(0)
{
    fhavewatchonly = wallet->havewatchonly();
    fforcecheckbalancechanged = false;

    addresstablemodel = new addresstablemodel(wallet, this);
    transactiontablemodel = new transactiontablemodel(wallet, this);
    recentrequeststablemodel = new recentrequeststablemodel(wallet, this);

    // this timer will be fired repeatedly to update the balance
    polltimer = new qtimer(this);
    connect(polltimer, signal(timeout()), this, slot(pollbalancechanged()));
    polltimer->start(model_update_delay);

    subscribetocoresignals();
}

walletmodel::~walletmodel()
{
    unsubscribefromcoresignals();
}

camount walletmodel::getbalance(const ccoincontrol *coincontrol) const
{
    if (coincontrol)
    {
        camount nbalance = 0;
        std::vector<coutput> vcoins;
        wallet->availablecoins(vcoins, true, coincontrol);
        boost_foreach(const coutput& out, vcoins)
            if(out.fspendable)
                nbalance += out.tx->vout[out.i].nvalue;

        return nbalance;
    }

    return wallet->getbalance();
}

camount walletmodel::getunconfirmedbalance() const
{
    return wallet->getunconfirmedbalance();
}

camount walletmodel::getimmaturebalance() const
{
    return wallet->getimmaturebalance();
}

bool walletmodel::havewatchonly() const
{
    return fhavewatchonly;
}

camount walletmodel::getwatchbalance() const
{
    return wallet->getwatchonlybalance();
}

camount walletmodel::getwatchunconfirmedbalance() const
{
    return wallet->getunconfirmedwatchonlybalance();
}

camount walletmodel::getwatchimmaturebalance() const
{
    return wallet->getimmaturewatchonlybalance();
}

void walletmodel::updatestatus()
{
    encryptionstatus newencryptionstatus = getencryptionstatus();

    if(cachedencryptionstatus != newencryptionstatus)
        emit encryptionstatuschanged(newencryptionstatus);
}

void walletmodel::pollbalancechanged()
{
    // get required locks upfront. this avoids the gui from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    try_lock(cs_main, lockmain);
    if(!lockmain)
        return;
    try_lock(wallet->cs_wallet, lockwallet);
    if(!lockwallet)
        return;

    if(fforcecheckbalancechanged || chainactive.height() != cachednumblocks)
    {
        fforcecheckbalancechanged = false;

        // balance and number of transactions might have changed
        cachednumblocks = chainactive.height();

        checkbalancechanged();
        if(transactiontablemodel)
            transactiontablemodel->updateconfirmations();
    }
}

void walletmodel::checkbalancechanged()
{
    camount newbalance = getbalance();
    camount newunconfirmedbalance = getunconfirmedbalance();
    camount newimmaturebalance = getimmaturebalance();
    camount newwatchonlybalance = 0;
    camount newwatchunconfbalance = 0;
    camount newwatchimmaturebalance = 0;
    if (havewatchonly())
    {
        newwatchonlybalance = getwatchbalance();
        newwatchunconfbalance = getwatchunconfirmedbalance();
        newwatchimmaturebalance = getwatchimmaturebalance();
    }

    if(cachedbalance != newbalance || cachedunconfirmedbalance != newunconfirmedbalance || cachedimmaturebalance != newimmaturebalance ||
        cachedwatchonlybalance != newwatchonlybalance || cachedwatchunconfbalance != newwatchunconfbalance || cachedwatchimmaturebalance != newwatchimmaturebalance)
    {
        cachedbalance = newbalance;
        cachedunconfirmedbalance = newunconfirmedbalance;
        cachedimmaturebalance = newimmaturebalance;
        cachedwatchonlybalance = newwatchonlybalance;
        cachedwatchunconfbalance = newwatchunconfbalance;
        cachedwatchimmaturebalance = newwatchimmaturebalance;
        emit balancechanged(newbalance, newunconfirmedbalance, newimmaturebalance,
                            newwatchonlybalance, newwatchunconfbalance, newwatchimmaturebalance);
    }
}

void walletmodel::updatetransaction()
{
    // balance and number of transactions might have changed
    fforcecheckbalancechanged = true;
}

void walletmodel::updateaddressbook(const qstring &address, const qstring &label,
        bool ismine, const qstring &purpose, int status)
{
    if(addresstablemodel)
        addresstablemodel->updateentry(address, label, ismine, purpose, status);
}

void walletmodel::updatewatchonlyflag(bool fhavewatchonly)
{
    fhavewatchonly = fhavewatchonly;
    emit notifywatchonlychanged(fhavewatchonly);
}

bool walletmodel::validateaddress(const qstring &address)
{
    cmoorecoinaddress addressparsed(address.tostdstring());
    return addressparsed.isvalid();
}

walletmodel::sendcoinsreturn walletmodel::preparetransaction(walletmodeltransaction &transaction, const ccoincontrol *coincontrol)
{
    camount total = 0;
    bool fsubtractfeefromamount = false;
    qlist<sendcoinsrecipient> recipients = transaction.getrecipients();
    std::vector<crecipient> vecsend;

    if(recipients.empty())
    {
        return ok;
    }

    qset<qstring> setaddress; // used to detect duplicates
    int naddresses = 0;

    // pre-check input data for validity
    foreach(const sendcoinsrecipient &rcp, recipients)
    {
        if (rcp.fsubtractfeefromamount)
            fsubtractfeefromamount = true;

        if (rcp.paymentrequest.isinitialized())
        {   // paymentrequest...
            camount subtotal = 0;
            const payments::paymentdetails& details = rcp.paymentrequest.getdetails();
            for (int i = 0; i < details.outputs_size(); i++)
            {
                const payments::output& out = details.outputs(i);
                if (out.amount() <= 0) continue;
                subtotal += out.amount();
                const unsigned char* scriptstr = (const unsigned char*)out.script().data();
                cscript scriptpubkey(scriptstr, scriptstr+out.script().size());
                camount namount = out.amount();
                crecipient recipient = {scriptpubkey, namount, rcp.fsubtractfeefromamount};
                vecsend.push_back(recipient);
            }
            if (subtotal <= 0)
            {
                return invalidamount;
            }
            total += subtotal;
        }
        else
        {   // user-entered moorecoin address / amount:
            if(!validateaddress(rcp.address))
            {
                return invalidaddress;
            }
            if(rcp.amount <= 0)
            {
                return invalidamount;
            }
            setaddress.insert(rcp.address);
            ++naddresses;

            cscript scriptpubkey = getscriptfordestination(cmoorecoinaddress(rcp.address.tostdstring()).get());
            crecipient recipient = {scriptpubkey, rcp.amount, rcp.fsubtractfeefromamount};
            vecsend.push_back(recipient);

            total += rcp.amount;
        }
    }
    if(setaddress.size() != naddresses)
    {
        return duplicateaddress;
    }

    camount nbalance = getbalance(coincontrol);

    if(total > nbalance)
    {
        return amountexceedsbalance;
    }

    {
        lock2(cs_main, wallet->cs_wallet);

        transaction.newpossiblekeychange(wallet);

        camount nfeerequired = 0;
        int nchangeposret = -1;
        std::string strfailreason;

        cwallettx *newtx = transaction.gettransaction();
        creservekey *keychange = transaction.getpossiblekeychange();
        bool fcreated = wallet->createtransaction(vecsend, *newtx, *keychange, nfeerequired, nchangeposret, strfailreason, coincontrol);
        transaction.settransactionfee(nfeerequired);
        if (fsubtractfeefromamount && fcreated)
            transaction.reassignamounts(nchangeposret);

        if(!fcreated)
        {
            if(!fsubtractfeefromamount && (total + nfeerequired) > nbalance)
            {
                return sendcoinsreturn(amountwithfeeexceedsbalance);
            }
            emit message(tr("send coins"), qstring::fromstdstring(strfailreason),
                         cclientuiinterface::msg_error);
            return transactioncreationfailed;
        }

        // reject absurdly high fee > 0.1 moorecoin
        if (nfeerequired > 10000000)
            return absurdfee;
    }

    return sendcoinsreturn(ok);
}

walletmodel::sendcoinsreturn walletmodel::sendcoins(walletmodeltransaction &transaction)
{
    qbytearray transaction_array; /* store serialized transaction */

    {
        lock2(cs_main, wallet->cs_wallet);
        cwallettx *newtx = transaction.gettransaction();

        foreach(const sendcoinsrecipient &rcp, transaction.getrecipients())
        {
            if (rcp.paymentrequest.isinitialized())
            {
                // make sure any payment requests involved are still valid.
                if (paymentserver::verifyexpired(rcp.paymentrequest.getdetails())) {
                    return paymentrequestexpired;
                }

                // store paymentrequests in wtx.vorderform in wallet.
                std::string key("paymentrequest");
                std::string value;
                rcp.paymentrequest.serializetostring(&value);
                newtx->vorderform.push_back(make_pair(key, value));
            }
            else if (!rcp.message.isempty()) // message from normal moorecoin:uri (moorecoin:123...?message=example)
                newtx->vorderform.push_back(make_pair("message", rcp.message.tostdstring()));
        }

        creservekey *keychange = transaction.getpossiblekeychange();
        if(!wallet->committransaction(*newtx, *keychange))
            return transactioncommitfailed;

        ctransaction* t = (ctransaction*)newtx;
        cdatastream sstx(ser_network, protocol_version);
        sstx << *t;
        transaction_array.append(&(sstx[0]), sstx.size());
    }

    // add addresses / update labels that we've sent to to the address book,
    // and emit coinssent signal for each recipient
    foreach(const sendcoinsrecipient &rcp, transaction.getrecipients())
    {
        // don't touch the address book when we have a payment request
        if (!rcp.paymentrequest.isinitialized())
        {
            std::string straddress = rcp.address.tostdstring();
            ctxdestination dest = cmoorecoinaddress(straddress).get();
            std::string strlabel = rcp.label.tostdstring();
            {
                lock(wallet->cs_wallet);

                std::map<ctxdestination, caddressbookdata>::iterator mi = wallet->mapaddressbook.find(dest);

                // check if we have a new address or an updated label
                if (mi == wallet->mapaddressbook.end())
                {
                    wallet->setaddressbook(dest, strlabel, "send");
                }
                else if (mi->second.name != strlabel)
                {
                    wallet->setaddressbook(dest, strlabel, ""); // "" means don't change purpose
                }
            }
        }
        emit coinssent(wallet, rcp, transaction_array);
    }
    checkbalancechanged(); // update balance immediately, otherwise there could be a short noticeable delay until pollbalancechanged hits

    return sendcoinsreturn(ok);
}

optionsmodel *walletmodel::getoptionsmodel()
{
    return optionsmodel;
}

addresstablemodel *walletmodel::getaddresstablemodel()
{
    return addresstablemodel;
}

transactiontablemodel *walletmodel::gettransactiontablemodel()
{
    return transactiontablemodel;
}

recentrequeststablemodel *walletmodel::getrecentrequeststablemodel()
{
    return recentrequeststablemodel;
}

walletmodel::encryptionstatus walletmodel::getencryptionstatus() const
{
    if(!wallet->iscrypted())
    {
        return unencrypted;
    }
    else if(wallet->islocked())
    {
        return locked;
    }
    else
    {
        return unlocked;
    }
}

bool walletmodel::setwalletencrypted(bool encrypted, const securestring &passphrase)
{
    if(encrypted)
    {
        // encrypt
        return wallet->encryptwallet(passphrase);
    }
    else
    {
        // decrypt -- todo; not supported yet
        return false;
    }
}

bool walletmodel::setwalletlocked(bool locked, const securestring &passphrase)
{
    if(locked)
    {
        // lock
        return wallet->lock();
    }
    else
    {
        // unlock
        return wallet->unlock(passphrase);
    }
}

bool walletmodel::changepassphrase(const securestring &oldpass, const securestring &newpass)
{
    bool retval;
    {
        lock(wallet->cs_wallet);
        wallet->lock(); // make sure wallet is locked before attempting pass change
        retval = wallet->changewalletpassphrase(oldpass, newpass);
    }
    return retval;
}

bool walletmodel::backupwallet(const qstring &filename)
{
    return backupwallet(*wallet, filename.tolocal8bit().data());
}

// handlers for core signals
static void notifykeystorestatuschanged(walletmodel *walletmodel, ccryptokeystore *wallet)
{
    qdebug() << "notifykeystorestatuschanged";
    qmetaobject::invokemethod(walletmodel, "updatestatus", qt::queuedconnection);
}

static void notifyaddressbookchanged(walletmodel *walletmodel, cwallet *wallet,
        const ctxdestination &address, const std::string &label, bool ismine,
        const std::string &purpose, changetype status)
{
    qstring straddress = qstring::fromstdstring(cmoorecoinaddress(address).tostring());
    qstring strlabel = qstring::fromstdstring(label);
    qstring strpurpose = qstring::fromstdstring(purpose);

    qdebug() << "notifyaddressbookchanged: " + straddress + " " + strlabel + " ismine=" + qstring::number(ismine) + " purpose=" + strpurpose + " status=" + qstring::number(status);
    qmetaobject::invokemethod(walletmodel, "updateaddressbook", qt::queuedconnection,
                              q_arg(qstring, straddress),
                              q_arg(qstring, strlabel),
                              q_arg(bool, ismine),
                              q_arg(qstring, strpurpose),
                              q_arg(int, status));
}

static void notifytransactionchanged(walletmodel *walletmodel, cwallet *wallet, const uint256 &hash, changetype status)
{
    q_unused(wallet);
    q_unused(hash);
    q_unused(status);
    qmetaobject::invokemethod(walletmodel, "updatetransaction", qt::queuedconnection);
}

static void showprogress(walletmodel *walletmodel, const std::string &title, int nprogress)
{
    // emits signal "showprogress"
    qmetaobject::invokemethod(walletmodel, "showprogress", qt::queuedconnection,
                              q_arg(qstring, qstring::fromstdstring(title)),
                              q_arg(int, nprogress));
}

static void notifywatchonlychanged(walletmodel *walletmodel, bool fhavewatchonly)
{
    qmetaobject::invokemethod(walletmodel, "updatewatchonlyflag", qt::queuedconnection,
                              q_arg(bool, fhavewatchonly));
}

void walletmodel::subscribetocoresignals()
{
    // connect signals to wallet
    wallet->notifystatuschanged.connect(boost::bind(&notifykeystorestatuschanged, this, _1));
    wallet->notifyaddressbookchanged.connect(boost::bind(notifyaddressbookchanged, this, _1, _2, _3, _4, _5, _6));
    wallet->notifytransactionchanged.connect(boost::bind(notifytransactionchanged, this, _1, _2, _3));
    wallet->showprogress.connect(boost::bind(showprogress, this, _1, _2));
    wallet->notifywatchonlychanged.connect(boost::bind(notifywatchonlychanged, this, _1));
}

void walletmodel::unsubscribefromcoresignals()
{
    // disconnect signals from wallet
    wallet->notifystatuschanged.disconnect(boost::bind(&notifykeystorestatuschanged, this, _1));
    wallet->notifyaddressbookchanged.disconnect(boost::bind(notifyaddressbookchanged, this, _1, _2, _3, _4, _5, _6));
    wallet->notifytransactionchanged.disconnect(boost::bind(notifytransactionchanged, this, _1, _2, _3));
    wallet->showprogress.disconnect(boost::bind(showprogress, this, _1, _2));
    wallet->notifywatchonlychanged.disconnect(boost::bind(notifywatchonlychanged, this, _1));
}

// walletmodel::unlockcontext implementation
walletmodel::unlockcontext walletmodel::requestunlock()
{
    bool was_locked = getencryptionstatus() == locked;
    if(was_locked)
    {
        // request ui to unlock wallet
        emit requireunlock();
    }
    // if wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getencryptionstatus() != locked;

    return unlockcontext(this, valid, was_locked);
}

walletmodel::unlockcontext::unlockcontext(walletmodel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

walletmodel::unlockcontext::~unlockcontext()
{
    if(valid && relock)
    {
        wallet->setwalletlocked(true);
    }
}

void walletmodel::unlockcontext::copyfrom(const unlockcontext& rhs)
{
    // transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool walletmodel::getpubkey(const ckeyid &address, cpubkey& vchpubkeyout) const
{
    return wallet->getpubkey(address, vchpubkeyout);
}

// returns a list of coutputs from coutpoints
void walletmodel::getoutputs(const std::vector<coutpoint>& voutpoints, std::vector<coutput>& voutputs)
{
    lock2(cs_main, wallet->cs_wallet);
    boost_foreach(const coutpoint& outpoint, voutpoints)
    {
        if (!wallet->mapwallet.count(outpoint.hash)) continue;
        int ndepth = wallet->mapwallet[outpoint.hash].getdepthinmainchain();
        if (ndepth < 0) continue;
        coutput out(&wallet->mapwallet[outpoint.hash], outpoint.n, ndepth, true);
        voutputs.push_back(out);
    }
}

bool walletmodel::isspent(const coutpoint& outpoint) const
{
    lock2(cs_main, wallet->cs_wallet);
    return wallet->isspent(outpoint.hash, outpoint.n);
}

// availablecoins + lockedcoins grouped by wallet address (put change in one group with wallet address)
void walletmodel::listcoins(std::map<qstring, std::vector<coutput> >& mapcoins) const
{
    std::vector<coutput> vcoins;
    wallet->availablecoins(vcoins);

    lock2(cs_main, wallet->cs_wallet); // listlockedcoins, mapwallet
    std::vector<coutpoint> vlockedcoins;
    wallet->listlockedcoins(vlockedcoins);

    // add locked coins
    boost_foreach(const coutpoint& outpoint, vlockedcoins)
    {
        if (!wallet->mapwallet.count(outpoint.hash)) continue;
        int ndepth = wallet->mapwallet[outpoint.hash].getdepthinmainchain();
        if (ndepth < 0) continue;
        coutput out(&wallet->mapwallet[outpoint.hash], outpoint.n, ndepth, true);
        if (outpoint.n < out.tx->vout.size() && wallet->ismine(out.tx->vout[outpoint.n]) == ismine_spendable)
            vcoins.push_back(out);
    }

    boost_foreach(const coutput& out, vcoins)
    {
        coutput cout = out;

        while (wallet->ischange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 && wallet->ismine(cout.tx->vin[0]))
        {
            if (!wallet->mapwallet.count(cout.tx->vin[0].prevout.hash)) break;
            cout = coutput(&wallet->mapwallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0, true);
        }

        ctxdestination address;
        if(!out.fspendable || !extractdestination(cout.tx->vout[cout.i].scriptpubkey, address))
            continue;
        mapcoins[qstring::fromstdstring(cmoorecoinaddress(address).tostring())].push_back(out);
    }
}

bool walletmodel::islockedcoin(uint256 hash, unsigned int n) const
{
    lock2(cs_main, wallet->cs_wallet);
    return wallet->islockedcoin(hash, n);
}

void walletmodel::lockcoin(coutpoint& output)
{
    lock2(cs_main, wallet->cs_wallet);
    wallet->lockcoin(output);
}

void walletmodel::unlockcoin(coutpoint& output)
{
    lock2(cs_main, wallet->cs_wallet);
    wallet->unlockcoin(output);
}

void walletmodel::listlockedcoins(std::vector<coutpoint>& voutpts)
{
    lock2(cs_main, wallet->cs_wallet);
    wallet->listlockedcoins(voutpts);
}

void walletmodel::loadreceiverequests(std::vector<std::string>& vreceiverequests)
{
    lock(wallet->cs_wallet);
    boost_foreach(const pairtype(ctxdestination, caddressbookdata)& item, wallet->mapaddressbook)
        boost_foreach(const pairtype(std::string, std::string)& item2, item.second.destdata)
            if (item2.first.size() > 2 && item2.first.substr(0,2) == "rr") // receive request
                vreceiverequests.push_back(item2.second);
}

bool walletmodel::savereceiverequest(const std::string &saddress, const int64_t nid, const std::string &srequest)
{
    ctxdestination dest = cmoorecoinaddress(saddress).get();

    std::stringstream ss;
    ss << nid;
    std::string key = "rr" + ss.str(); // "rr" prefix = "receive request" in destdata

    lock(wallet->cs_wallet);
    if (srequest.empty())
        return wallet->erasedestdata(dest, key);
    else
        return wallet->adddestdata(dest, key, srequest);
}
