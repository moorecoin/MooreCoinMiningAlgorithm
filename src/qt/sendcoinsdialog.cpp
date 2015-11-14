// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "sendcoinsdialog.h"
#include "ui_sendcoinsdialog.h"

#include "addresstablemodel.h"
#include "moorecoinunits.h"
#include "clientmodel.h"
#include "coincontroldialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "scicon.h"
#include "sendcoinsentry.h"
#include "walletmodel.h"

#include "base58.h"
#include "coincontrol.h"
#include "main.h"
#include "ui_interface.h"
#include "wallet/wallet.h"

#include <qmessagebox>
#include <qscrollbar>
#include <qsettings>
#include <qtextdocument>

sendcoinsdialog::sendcoinsdialog(qwidget *parent) :
    qdialog(parent),
    ui(new ui::sendcoinsdialog),
    clientmodel(0),
    model(0),
    fnewrecipientallowed(true),
    ffeeminimized(true)
{
    ui->setupui(this);

#ifdef q_os_mac // icons on push buttons are very uncommon on mac
    ui->addbutton->seticon(qicon());
    ui->clearbutton->seticon(qicon());
    ui->sendbutton->seticon(qicon());
#else
    ui->addbutton->seticon(singlecoloricon(":/icons/add"));
    ui->clearbutton->seticon(singlecoloricon(":/icons/remove"));
    ui->sendbutton->seticon(singlecoloricon(":/icons/send"));
#endif

    guiutil::setupaddresswidget(ui->lineeditcoincontrolchange, this);

    addentry();

    connect(ui->addbutton, signal(clicked()), this, slot(addentry()));
    connect(ui->clearbutton, signal(clicked()), this, slot(clear()));

    // coin control
    connect(ui->pushbuttoncoincontrol, signal(clicked()), this, slot(coincontrolbuttonclicked()));
    connect(ui->checkbomoorecoincontrolchange, signal(statechanged(int)), this, slot(coincontrolchangechecked(int)));
    connect(ui->lineeditcoincontrolchange, signal(textedited(const qstring &)), this, slot(coincontrolchangeedited(const qstring &)));

    // coin control: clipboard actions
    qaction *clipboardquantityaction = new qaction(tr("copy quantity"), this);
    qaction *clipboardamountaction = new qaction(tr("copy amount"), this);
    qaction *clipboardfeeaction = new qaction(tr("copy fee"), this);
    qaction *clipboardafterfeeaction = new qaction(tr("copy after fee"), this);
    qaction *clipboardbytesaction = new qaction(tr("copy bytes"), this);
    qaction *clipboardpriorityaction = new qaction(tr("copy priority"), this);
    qaction *clipboardlowoutputaction = new qaction(tr("copy dust"), this);
    qaction *clipboardchangeaction = new qaction(tr("copy change"), this);
    connect(clipboardquantityaction, signal(triggered()), this, slot(coincontrolclipboardquantity()));
    connect(clipboardamountaction, signal(triggered()), this, slot(coincontrolclipboardamount()));
    connect(clipboardfeeaction, signal(triggered()), this, slot(coincontrolclipboardfee()));
    connect(clipboardafterfeeaction, signal(triggered()), this, slot(coincontrolclipboardafterfee()));
    connect(clipboardbytesaction, signal(triggered()), this, slot(coincontrolclipboardbytes()));
    connect(clipboardpriorityaction, signal(triggered()), this, slot(coincontrolclipboardpriority()));
    connect(clipboardlowoutputaction, signal(triggered()), this, slot(coincontrolclipboardlowoutput()));
    connect(clipboardchangeaction, signal(triggered()), this, slot(coincontrolclipboardchange()));
    ui->labelcoincontrolquantity->addaction(clipboardquantityaction);
    ui->labelcoincontrolamount->addaction(clipboardamountaction);
    ui->labelcoincontrolfee->addaction(clipboardfeeaction);
    ui->labelcoincontrolafterfee->addaction(clipboardafterfeeaction);
    ui->labelcoincontrolbytes->addaction(clipboardbytesaction);
    ui->labelcoincontrolpriority->addaction(clipboardpriorityaction);
    ui->labelcoincontrollowoutput->addaction(clipboardlowoutputaction);
    ui->labelcoincontrolchange->addaction(clipboardchangeaction);

    // init transaction fee section
    qsettings settings;
    if (!settings.contains("ffeesectionminimized"))
        settings.setvalue("ffeesectionminimized", true);
    if (!settings.contains("nfeeradio") && settings.contains("ntransactionfee") && settings.value("ntransactionfee").tolonglong() > 0) // compatibility
        settings.setvalue("nfeeradio", 1); // custom
    if (!settings.contains("nfeeradio"))
        settings.setvalue("nfeeradio", 0); // recommended
    if (!settings.contains("ncustomfeeradio") && settings.contains("ntransactionfee") && settings.value("ntransactionfee").tolonglong() > 0) // compatibility
        settings.setvalue("ncustomfeeradio", 1); // total at least
    if (!settings.contains("ncustomfeeradio"))
        settings.setvalue("ncustomfeeradio", 0); // per kilobyte
    if (!settings.contains("nsmartfeesliderposition"))
        settings.setvalue("nsmartfeesliderposition", 0);
    if (!settings.contains("ntransactionfee"))
        settings.setvalue("ntransactionfee", (qint64)default_transaction_fee);
    if (!settings.contains("fpayonlyminfee"))
        settings.setvalue("fpayonlyminfee", false);
    if (!settings.contains("fsendfreetransactions"))
        settings.setvalue("fsendfreetransactions", false);
    ui->groupfee->setid(ui->radiosmartfee, 0);
    ui->groupfee->setid(ui->radiocustomfee, 1);
    ui->groupfee->button((int)std::max(0, std::min(1, settings.value("nfeeradio").toint())))->setchecked(true);
    ui->groupcustomfee->setid(ui->radiocustomperkilobyte, 0);
    ui->groupcustomfee->setid(ui->radiocustomatleast, 1);
    ui->groupcustomfee->button((int)std::max(0, std::min(1, settings.value("ncustomfeeradio").toint())))->setchecked(true);
    ui->slidersmartfee->setvalue(settings.value("nsmartfeesliderposition").toint());
    ui->customfee->setvalue(settings.value("ntransactionfee").tolonglong());
    ui->checkboxminimumfee->setchecked(settings.value("fpayonlyminfee").tobool());
    ui->checkboxfreetx->setchecked(settings.value("fsendfreetransactions").tobool());
    minimizefeesection(settings.value("ffeesectionminimized").tobool());
}

void sendcoinsdialog::setclientmodel(clientmodel *clientmodel)
{
    this->clientmodel = clientmodel;

    if (clientmodel) {
        connect(clientmodel, signal(numblockschanged(int,qdatetime)), this, slot(updatesmartfeelabel()));
    }
}

void sendcoinsdialog::setmodel(walletmodel *model)
{
    this->model = model;

    if(model && model->getoptionsmodel())
    {
        for(int i = 0; i < ui->entries->count(); ++i)
        {
            sendcoinsentry *entry = qobject_cast<sendcoinsentry*>(ui->entries->itemat(i)->widget());
            if(entry)
            {
                entry->setmodel(model);
            }
        }

        setbalance(model->getbalance(), model->getunconfirmedbalance(), model->getimmaturebalance(),
                   model->getwatchbalance(), model->getwatchunconfirmedbalance(), model->getwatchimmaturebalance());
        connect(model, signal(balancechanged(camount,camount,camount,camount,camount,camount)), this, slot(setbalance(camount,camount,camount,camount,camount,camount)));
        connect(model->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(updatedisplayunit()));
        updatedisplayunit();

        // coin control
        connect(model->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(coincontrolupdatelabels()));
        connect(model->getoptionsmodel(), signal(coincontrolfeatureschanged(bool)), this, slot(coincontrolfeaturechanged(bool)));
        ui->framecoincontrol->setvisible(model->getoptionsmodel()->getcoincontrolfeatures());
        coincontrolupdatelabels();

        // fee section
        connect(ui->slidersmartfee, signal(valuechanged(int)), this, slot(updatesmartfeelabel()));
        connect(ui->slidersmartfee, signal(valuechanged(int)), this, slot(updateglobalfeevariables()));
        connect(ui->slidersmartfee, signal(valuechanged(int)), this, slot(coincontrolupdatelabels()));
        connect(ui->groupfee, signal(buttonclicked(int)), this, slot(updatefeesectioncontrols()));
        connect(ui->groupfee, signal(buttonclicked(int)), this, slot(updateglobalfeevariables()));
        connect(ui->groupfee, signal(buttonclicked(int)), this, slot(coincontrolupdatelabels()));
        connect(ui->groupcustomfee, signal(buttonclicked(int)), this, slot(updateglobalfeevariables()));
        connect(ui->groupcustomfee, signal(buttonclicked(int)), this, slot(coincontrolupdatelabels()));
        connect(ui->customfee, signal(valuechanged()), this, slot(updateglobalfeevariables()));
        connect(ui->customfee, signal(valuechanged()), this, slot(coincontrolupdatelabels()));
        connect(ui->checkboxminimumfee, signal(statechanged(int)), this, slot(setminimumfee()));
        connect(ui->checkboxminimumfee, signal(statechanged(int)), this, slot(updatefeesectioncontrols()));
        connect(ui->checkboxminimumfee, signal(statechanged(int)), this, slot(updateglobalfeevariables()));
        connect(ui->checkboxminimumfee, signal(statechanged(int)), this, slot(coincontrolupdatelabels()));
        connect(ui->checkboxfreetx, signal(statechanged(int)), this, slot(updateglobalfeevariables()));
        connect(ui->checkboxfreetx, signal(statechanged(int)), this, slot(coincontrolupdatelabels()));
        ui->customfee->setsinglestep(cwallet::mintxfee.getfeeperk());
        updatefeesectioncontrols();
        updateminfeelabel();
        updatesmartfeelabel();
        updateglobalfeevariables();
    }
}

sendcoinsdialog::~sendcoinsdialog()
{
    qsettings settings;
    settings.setvalue("ffeesectionminimized", ffeeminimized);
    settings.setvalue("nfeeradio", ui->groupfee->checkedid());
    settings.setvalue("ncustomfeeradio", ui->groupcustomfee->checkedid());
    settings.setvalue("nsmartfeesliderposition", ui->slidersmartfee->value());
    settings.setvalue("ntransactionfee", (qint64)ui->customfee->value());
    settings.setvalue("fpayonlyminfee", ui->checkboxminimumfee->ischecked());
    settings.setvalue("fsendfreetransactions", ui->checkboxfreetx->ischecked());

    delete ui;
}

void sendcoinsdialog::on_sendbutton_clicked()
{
    if(!model || !model->getoptionsmodel())
        return;

    qlist<sendcoinsrecipient> recipients;
    bool valid = true;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        sendcoinsentry *entry = qobject_cast<sendcoinsentry*>(ui->entries->itemat(i)->widget());
        if(entry)
        {
            if(entry->validate())
            {
                recipients.append(entry->getvalue());
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isempty())
    {
        return;
    }

    fnewrecipientallowed = false;
    walletmodel::unlockcontext ctx(model->requestunlock());
    if(!ctx.isvalid())
    {
        // unlock wallet was cancelled
        fnewrecipientallowed = true;
        return;
    }

    // prepare transaction for getting txfee earlier
    walletmodeltransaction currenttransaction(recipients);
    walletmodel::sendcoinsreturn preparestatus;
    if (model->getoptionsmodel()->getcoincontrolfeatures()) // coin control enabled
        preparestatus = model->preparetransaction(currenttransaction, coincontroldialog::coincontrol);
    else
        preparestatus = model->preparetransaction(currenttransaction);

    // process preparestatus and on error generate message shown to user
    processsendcoinsreturn(preparestatus,
        moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), currenttransaction.gettransactionfee()));

    if(preparestatus.status != walletmodel::ok) {
        fnewrecipientallowed = true;
        return;
    }

    camount txfee = currenttransaction.gettransactionfee();

    // format confirmation message
    qstringlist formatted;
    foreach(const sendcoinsrecipient &rcp, currenttransaction.getrecipients())
    {
        // generate bold amount string
        qstring amount = "<b>" + moorecoinunits::formathtmlwithunit(model->getoptionsmodel()->getdisplayunit(), rcp.amount);
        amount.append("</b>");
        // generate monospace address string
        qstring address = "<span style='font-family: monospace;'>" + rcp.address;
        address.append("</span>");

        qstring recipientelement;

        if (!rcp.paymentrequest.isinitialized()) // normal payment
        {
            if(rcp.label.length() > 0) // label with address
            {
                recipientelement = tr("%1 to %2").arg(amount, guiutil::htmlescape(rcp.label));
                recipientelement.append(qstring(" (%1)").arg(address));
            }
            else // just address
            {
                recipientelement = tr("%1 to %2").arg(amount, address);
            }
        }
        else if(!rcp.authenticatedmerchant.isempty()) // authenticated payment request
        {
            recipientelement = tr("%1 to %2").arg(amount, guiutil::htmlescape(rcp.authenticatedmerchant));
        }
        else // unauthenticated payment request
        {
            recipientelement = tr("%1 to %2").arg(amount, address);
        }

        formatted.append(recipientelement);
    }

    qstring questionstring = tr("are you sure you want to send?");
    questionstring.append("<br /><br />%1");

    if(txfee > 0)
    {
        // append fee string if a fee is required
        questionstring.append("<hr /><span style='color:#aa0000;'>");
        questionstring.append(moorecoinunits::formathtmlwithunit(model->getoptionsmodel()->getdisplayunit(), txfee));
        questionstring.append("</span> ");
        questionstring.append(tr("added as transaction fee"));

        // append transaction size
        questionstring.append(" (" + qstring::number((double)currenttransaction.gettransactionsize() / 1000) + " kb)");
    }

    // add total amount in all subdivision units
    questionstring.append("<hr />");
    camount totalamount = currenttransaction.gettotaltransactionamount() + txfee;
    qstringlist alternativeunits;
    foreach(moorecoinunits::unit u, moorecoinunits::availableunits())
    {
        if(u != model->getoptionsmodel()->getdisplayunit())
            alternativeunits.append(moorecoinunits::formathtmlwithunit(u, totalamount));
    }
    questionstring.append(tr("total amount %1 (= %2)")
        .arg(moorecoinunits::formathtmlwithunit(model->getoptionsmodel()->getdisplayunit(), totalamount))
        .arg(alternativeunits.join(" " + tr("or") + " ")));

    qmessagebox::standardbutton retval = qmessagebox::question(this, tr("confirm send coins"),
        questionstring.arg(formatted.join("<br />")),
        qmessagebox::yes | qmessagebox::cancel,
        qmessagebox::cancel);

    if(retval != qmessagebox::yes)
    {
        fnewrecipientallowed = true;
        return;
    }

    // now send the prepared transaction
    walletmodel::sendcoinsreturn sendstatus = model->sendcoins(currenttransaction);
    // process sendstatus and on error generate message shown to user
    processsendcoinsreturn(sendstatus);

    if (sendstatus.status == walletmodel::ok)
    {
        accept();
        coincontroldialog::coincontrol->unselectall();
        coincontrolupdatelabels();
    }
    fnewrecipientallowed = true;
}

void sendcoinsdialog::clear()
{
    // remove entries until only one left
    while(ui->entries->count())
    {
        ui->entries->takeat(0)->widget()->deletelater();
    }
    addentry();

    updatetabsandlabels();
}

void sendcoinsdialog::reject()
{
    clear();
}

void sendcoinsdialog::accept()
{
    clear();
}

sendcoinsentry *sendcoinsdialog::addentry()
{
    sendcoinsentry *entry = new sendcoinsentry(this);
    entry->setmodel(model);
    ui->entries->addwidget(entry);
    connect(entry, signal(removeentry(sendcoinsentry*)), this, slot(removeentry(sendcoinsentry*)));
    connect(entry, signal(payamountchanged()), this, slot(coincontrolupdatelabels()));
    connect(entry, signal(subtractfeefromamountchanged()), this, slot(coincontrolupdatelabels()));

    updatetabsandlabels();

    // focus the field, so that entry can start immediately
    entry->clear();
    entry->setfocus();
    ui->scrollareawidgetcontents->resize(ui->scrollareawidgetcontents->sizehint());
    qapp->processevents();
    qscrollbar* bar = ui->scrollarea->verticalscrollbar();
    if(bar)
        bar->setsliderposition(bar->maximum());
    return entry;
}

void sendcoinsdialog::updatetabsandlabels()
{
    setuptabchain(0);
    coincontrolupdatelabels();
}

void sendcoinsdialog::removeentry(sendcoinsentry* entry)
{
    entry->hide();

    // if the last entry is about to be removed add an empty one
    if (ui->entries->count() == 1)
        addentry();

    entry->deletelater();

    updatetabsandlabels();
}

qwidget *sendcoinsdialog::setuptabchain(qwidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        sendcoinsentry *entry = qobject_cast<sendcoinsentry*>(ui->entries->itemat(i)->widget());
        if(entry)
        {
            prev = entry->setuptabchain(prev);
        }
    }
    qwidget::settaborder(prev, ui->sendbutton);
    qwidget::settaborder(ui->sendbutton, ui->clearbutton);
    qwidget::settaborder(ui->clearbutton, ui->addbutton);
    return ui->addbutton;
}

void sendcoinsdialog::setaddress(const qstring &address)
{
    sendcoinsentry *entry = 0;
    // replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        sendcoinsentry *first = qobject_cast<sendcoinsentry*>(ui->entries->itemat(0)->widget());
        if(first->isclear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addentry();
    }

    entry->setaddress(address);
}

void sendcoinsdialog::pasteentry(const sendcoinsrecipient &rv)
{
    if(!fnewrecipientallowed)
        return;

    sendcoinsentry *entry = 0;
    // replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        sendcoinsentry *first = qobject_cast<sendcoinsentry*>(ui->entries->itemat(0)->widget());
        if(first->isclear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addentry();
    }

    entry->setvalue(rv);
    updatetabsandlabels();
}

bool sendcoinsdialog::handlepaymentrequest(const sendcoinsrecipient &rv)
{
    // just paste the entry, all pre-checks
    // are done in paymentserver.cpp.
    pasteentry(rv);
    return true;
}

void sendcoinsdialog::setbalance(const camount& balance, const camount& unconfirmedbalance, const camount& immaturebalance,
                                 const camount& watchbalance, const camount& watchunconfirmedbalance, const camount& watchimmaturebalance)
{
    q_unused(unconfirmedbalance);
    q_unused(immaturebalance);
    q_unused(watchbalance);
    q_unused(watchunconfirmedbalance);
    q_unused(watchimmaturebalance);

    if(model && model->getoptionsmodel())
    {
        ui->labelbalance->settext(moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), balance));
    }
}

void sendcoinsdialog::updatedisplayunit()
{
    setbalance(model->getbalance(), 0, 0, 0, 0, 0);
    ui->customfee->setdisplayunit(model->getoptionsmodel()->getdisplayunit());
    updateminfeelabel();
    updatesmartfeelabel();
}

void sendcoinsdialog::processsendcoinsreturn(const walletmodel::sendcoinsreturn &sendcoinsreturn, const qstring &msgarg)
{
    qpair<qstring, cclientuiinterface::messageboxflags> msgparams;
    // default to a warning message, override if error message is needed
    msgparams.second = cclientuiinterface::msg_warning;

    // this comment is specific to sendcoinsdialog usage of walletmodel::sendcoinsreturn.
    // walletmodel::transactioncommitfailed is used only in walletmodel::sendcoins()
    // all others are used only in walletmodel::preparetransaction()
    switch(sendcoinsreturn.status)
    {
    case walletmodel::invalidaddress:
        msgparams.first = tr("the recipient address is not valid. please recheck.");
        break;
    case walletmodel::invalidamount:
        msgparams.first = tr("the amount to pay must be larger than 0.");
        break;
    case walletmodel::amountexceedsbalance:
        msgparams.first = tr("the amount exceeds your balance.");
        break;
    case walletmodel::amountwithfeeexceedsbalance:
        msgparams.first = tr("the total exceeds your balance when the %1 transaction fee is included.").arg(msgarg);
        break;
    case walletmodel::duplicateaddress:
        msgparams.first = tr("duplicate address found: addresses should only be used once each.");
        break;
    case walletmodel::transactioncreationfailed:
        msgparams.first = tr("transaction creation failed!");
        msgparams.second = cclientuiinterface::msg_error;
        break;
    case walletmodel::transactioncommitfailed:
        msgparams.first = tr("the transaction was rejected! this might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
        msgparams.second = cclientuiinterface::msg_error;
        break;
    case walletmodel::absurdfee:
        msgparams.first = tr("a fee higher than %1 is considered an absurdly high fee.").arg(moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), 10000000));
        break;
    case walletmodel::paymentrequestexpired:
        msgparams.first = tr("payment request expired.");
        msgparams.second = cclientuiinterface::msg_error;
        break;
    // included to prevent a compiler warning.
    case walletmodel::ok:
    default:
        return;
    }

    emit message(tr("send coins"), msgparams.first, msgparams.second);
}

void sendcoinsdialog::minimizefeesection(bool fminimize)
{
    ui->labelfeeminimized->setvisible(fminimize);
    ui->buttonchoosefee  ->setvisible(fminimize);
    ui->buttonminimizefee->setvisible(!fminimize);
    ui->framefeeselection->setvisible(!fminimize);
    ui->horizontallayoutsmartfee->setcontentsmargins(0, (fminimize ? 0 : 6), 0, 0);
    ffeeminimized = fminimize;
}

void sendcoinsdialog::on_buttonchoosefee_clicked()
{
    minimizefeesection(false);
}

void sendcoinsdialog::on_buttonminimizefee_clicked()
{
    updatefeeminimizedlabel();
    minimizefeesection(true);
}

void sendcoinsdialog::setminimumfee()
{
    ui->radiocustomperkilobyte->setchecked(true);
    ui->customfee->setvalue(cwallet::mintxfee.getfeeperk());
}

void sendcoinsdialog::updatefeesectioncontrols()
{
    ui->slidersmartfee          ->setenabled(ui->radiosmartfee->ischecked());
    ui->labelsmartfee           ->setenabled(ui->radiosmartfee->ischecked());
    ui->labelsmartfee2          ->setenabled(ui->radiosmartfee->ischecked());
    ui->labelsmartfee3          ->setenabled(ui->radiosmartfee->ischecked());
    ui->labelfeeestimation      ->setenabled(ui->radiosmartfee->ischecked());
    ui->labelsmartfeenormal     ->setenabled(ui->radiosmartfee->ischecked());
    ui->labelsmartfeefast       ->setenabled(ui->radiosmartfee->ischecked());
    ui->checkboxminimumfee      ->setenabled(ui->radiocustomfee->ischecked());
    ui->labelminfeewarning      ->setenabled(ui->radiocustomfee->ischecked());
    ui->radiocustomperkilobyte  ->setenabled(ui->radiocustomfee->ischecked() && !ui->checkboxminimumfee->ischecked());
    ui->radiocustomatleast      ->setenabled(ui->radiocustomfee->ischecked() && !ui->checkboxminimumfee->ischecked());
    ui->customfee               ->setenabled(ui->radiocustomfee->ischecked() && !ui->checkboxminimumfee->ischecked());
}

void sendcoinsdialog::updateglobalfeevariables()
{
    if (ui->radiosmartfee->ischecked())
    {
        ntxconfirmtarget = defaultconfirmtarget - ui->slidersmartfee->value();
        paytxfee = cfeerate(0);
    }
    else
    {
        ntxconfirmtarget = defaultconfirmtarget;
        paytxfee = cfeerate(ui->customfee->value());
        fpayatleastcustomfee = ui->radiocustomatleast->ischecked();
    }

    fsendfreetransactions = ui->checkboxfreetx->ischecked();
}

void sendcoinsdialog::updatefeeminimizedlabel()
{
    if(!model || !model->getoptionsmodel())
        return;

    if (ui->radiosmartfee->ischecked())
        ui->labelfeeminimized->settext(ui->labelsmartfee->text());
    else {
        ui->labelfeeminimized->settext(moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), ui->customfee->value()) +
            ((ui->radiocustomperkilobyte->ischecked()) ? "/kb" : ""));
    }
}

void sendcoinsdialog::updateminfeelabel()
{
    if (model && model->getoptionsmodel())
        ui->checkboxminimumfee->settext(tr("pay only the minimum fee of %1").arg(
            moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), cwallet::mintxfee.getfeeperk()) + "/kb")
        );
}

void sendcoinsdialog::updatesmartfeelabel()
{
    if(!model || !model->getoptionsmodel())
        return;

    int nblockstoconfirm = defaultconfirmtarget - ui->slidersmartfee->value();
    cfeerate feerate = mempool.estimatefee(nblockstoconfirm);
    if (feerate <= cfeerate(0)) // not enough data => minfee
    {
        ui->labelsmartfee->settext(moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), cwallet::mintxfee.getfeeperk()) + "/kb");
        ui->labelsmartfee2->show(); // (smart fee not initialized yet. this usually takes a few blocks...)
        ui->labelfeeestimation->settext("");
    }
    else
    {
        ui->labelsmartfee->settext(moorecoinunits::formatwithunit(model->getoptionsmodel()->getdisplayunit(), feerate.getfeeperk()) + "/kb");
        ui->labelsmartfee2->hide();
        ui->labelfeeestimation->settext(tr("estimated to begin confirmation within %n block(s).", "", nblockstoconfirm));
    }

    updatefeeminimizedlabel();
}

// coin control: copy label "quantity" to clipboard
void sendcoinsdialog::coincontrolclipboardquantity()
{
    guiutil::setclipboard(ui->labelcoincontrolquantity->text());
}

// coin control: copy label "amount" to clipboard
void sendcoinsdialog::coincontrolclipboardamount()
{
    guiutil::setclipboard(ui->labelcoincontrolamount->text().left(ui->labelcoincontrolamount->text().indexof(" ")));
}

// coin control: copy label "fee" to clipboard
void sendcoinsdialog::coincontrolclipboardfee()
{
    guiutil::setclipboard(ui->labelcoincontrolfee->text().left(ui->labelcoincontrolfee->text().indexof(" ")).replace(asymp_utf8, ""));
}

// coin control: copy label "after fee" to clipboard
void sendcoinsdialog::coincontrolclipboardafterfee()
{
    guiutil::setclipboard(ui->labelcoincontrolafterfee->text().left(ui->labelcoincontrolafterfee->text().indexof(" ")).replace(asymp_utf8, ""));
}

// coin control: copy label "bytes" to clipboard
void sendcoinsdialog::coincontrolclipboardbytes()
{
    guiutil::setclipboard(ui->labelcoincontrolbytes->text().replace(asymp_utf8, ""));
}

// coin control: copy label "priority" to clipboard
void sendcoinsdialog::coincontrolclipboardpriority()
{
    guiutil::setclipboard(ui->labelcoincontrolpriority->text());
}

// coin control: copy label "dust" to clipboard
void sendcoinsdialog::coincontrolclipboardlowoutput()
{
    guiutil::setclipboard(ui->labelcoincontrollowoutput->text());
}

// coin control: copy label "change" to clipboard
void sendcoinsdialog::coincontrolclipboardchange()
{
    guiutil::setclipboard(ui->labelcoincontrolchange->text().left(ui->labelcoincontrolchange->text().indexof(" ")).replace(asymp_utf8, ""));
}

// coin control: settings menu - coin control enabled/disabled by user
void sendcoinsdialog::coincontrolfeaturechanged(bool checked)
{
    ui->framecoincontrol->setvisible(checked);

    if (!checked && model) // coin control features disabled
        coincontroldialog::coincontrol->setnull();

    if (checked)
        coincontrolupdatelabels();
}

// coin control: button inputs -> show actual coin control dialog
void sendcoinsdialog::coincontrolbuttonclicked()
{
    coincontroldialog dlg;
    dlg.setmodel(model);
    dlg.exec();
    coincontrolupdatelabels();
}

// coin control: checkbox custom change address
void sendcoinsdialog::coincontrolchangechecked(int state)
{
    if (state == qt::unchecked)
    {
        coincontroldialog::coincontrol->destchange = cnodestination();
        ui->labelcoincontrolchangelabel->clear();
    }
    else
        // use this to re-validate an already entered address
        coincontrolchangeedited(ui->lineeditcoincontrolchange->text());

    ui->lineeditcoincontrolchange->setenabled((state == qt::checked));
}

// coin control: custom change address changed
void sendcoinsdialog::coincontrolchangeedited(const qstring& text)
{
    if (model && model->getaddresstablemodel())
    {
        // default to no change address until verified
        coincontroldialog::coincontrol->destchange = cnodestination();
        ui->labelcoincontrolchangelabel->setstylesheet("qlabel{color:red;}");

        cmoorecoinaddress addr = cmoorecoinaddress(text.tostdstring());

        if (text.isempty()) // nothing entered
        {
            ui->labelcoincontrolchangelabel->settext("");
        }
        else if (!addr.isvalid()) // invalid address
        {
            ui->labelcoincontrolchangelabel->settext(tr("warning: invalid moorecoin address"));
        }
        else // valid address
        {
            cpubkey pubkey;
            ckeyid keyid;
            addr.getkeyid(keyid);
            if (!model->getpubkey(keyid, pubkey)) // unknown change address
            {
                ui->labelcoincontrolchangelabel->settext(tr("warning: unknown change address"));
            }
            else // known change address
            {
                ui->labelcoincontrolchangelabel->setstylesheet("qlabel{color:black;}");

                // query label
                qstring associatedlabel = model->getaddresstablemodel()->labelforaddress(text);
                if (!associatedlabel.isempty())
                    ui->labelcoincontrolchangelabel->settext(associatedlabel);
                else
                    ui->labelcoincontrolchangelabel->settext(tr("(no label)"));

                coincontroldialog::coincontrol->destchange = addr.get();
            }
        }
    }
}

// coin control: update labels
void sendcoinsdialog::coincontrolupdatelabels()
{
    if (!model || !model->getoptionsmodel() || !model->getoptionsmodel()->getcoincontrolfeatures())
        return;

    // set pay amounts
    coincontroldialog::payamounts.clear();
    coincontroldialog::fsubtractfeefromamount = false;
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        sendcoinsentry *entry = qobject_cast<sendcoinsentry*>(ui->entries->itemat(i)->widget());
        if(entry)
        {
            sendcoinsrecipient rcp = entry->getvalue();
            coincontroldialog::payamounts.append(rcp.amount);
            if (rcp.fsubtractfeefromamount)
                coincontroldialog::fsubtractfeefromamount = true;
        }
    }

    if (coincontroldialog::coincontrol->hasselected())
    {
        // actual coin control calculation
        coincontroldialog::updatelabels(model, this);

        // show coin control stats
        ui->labelcoincontrolautomaticallyselected->hide();
        ui->widgetcoincontrol->show();
    }
    else
    {
        // hide coin control stats
        ui->labelcoincontrolautomaticallyselected->show();
        ui->widgetcoincontrol->hide();
        ui->labelcoincontrolinsufffunds->hide();
    }
}
