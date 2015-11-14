// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"

#include "addressbookpage.h"
#include "askpassphrasedialog.h"
#include "moorecoingui.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "receivecoinsdialog.h"
#include "scicon.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"

#include "ui_interface.h"

#include <qaction>
#include <qactiongroup>
#include <qfiledialog>
#include <qhboxlayout>
#include <qprogressdialog>
#include <qpushbutton>
#include <qvboxlayout>

walletview::walletview(qwidget *parent):
    qstackedwidget(parent),
    clientmodel(0),
    walletmodel(0)
{
    // create tabs
    overviewpage = new overviewpage();

    transactionspage = new qwidget(this);
    qvboxlayout *vbox = new qvboxlayout();
    qhboxlayout *hbox_buttons = new qhboxlayout();
    transactionview = new transactionview(this);
    vbox->addwidget(transactionview);
    qpushbutton *exportbutton = new qpushbutton(tr("&export"), this);
    exportbutton->settooltip(tr("export the data in the current tab to a file"));
#ifndef q_os_mac // icons on push buttons are very uncommon on mac
    exportbutton->seticon(singlecoloricon(":/icons/export"));
#endif
    hbox_buttons->addstretch();
    hbox_buttons->addwidget(exportbutton);
    vbox->addlayout(hbox_buttons);
    transactionspage->setlayout(vbox);

    receivecoinspage = new receivecoinsdialog();
    sendcoinspage = new sendcoinsdialog();

    addwidget(overviewpage);
    addwidget(transactionspage);
    addwidget(receivecoinspage);
    addwidget(sendcoinspage);

    // clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewpage, signal(transactionclicked(qmodelindex)), transactionview, slot(focustransaction(qmodelindex)));

    // double-clicking on a transaction on the transaction history page shows details
    connect(transactionview, signal(doubleclicked(qmodelindex)), transactionview, slot(showdetails()));

    // clicking on "export" allows to export the transaction list
    connect(exportbutton, signal(clicked()), transactionview, slot(exportclicked()));

    // pass through messages from sendcoinspage
    connect(sendcoinspage, signal(message(qstring,qstring,unsigned int)), this, signal(message(qstring,qstring,unsigned int)));
    // pass through messages from transactionview
    connect(transactionview, signal(message(qstring,qstring,unsigned int)), this, signal(message(qstring,qstring,unsigned int)));
}

walletview::~walletview()
{
}

void walletview::setmoorecoingui(moorecoingui *gui)
{
    if (gui)
    {
        // clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewpage, signal(transactionclicked(qmodelindex)), gui, slot(gotohistorypage()));

        // receive and report messages
        connect(this, signal(message(qstring,qstring,unsigned int)), gui, slot(message(qstring,qstring,unsigned int)));

        // pass through encryption status changed signals
        connect(this, signal(encryptionstatuschanged(int)), gui, slot(setencryptionstatus(int)));

        // pass through transaction notifications
        connect(this, signal(incomingtransaction(qstring,int,camount,qstring,qstring,qstring)), gui, slot(incomingtransaction(qstring,int,camount,qstring,qstring,qstring)));
    }
}

void walletview::setclientmodel(clientmodel *clientmodel)
{
    this->clientmodel = clientmodel;

    overviewpage->setclientmodel(clientmodel);
    sendcoinspage->setclientmodel(clientmodel);
}

void walletview::setwalletmodel(walletmodel *walletmodel)
{
    this->walletmodel = walletmodel;

    // put transaction list in tabs
    transactionview->setmodel(walletmodel);
    overviewpage->setwalletmodel(walletmodel);
    receivecoinspage->setmodel(walletmodel);
    sendcoinspage->setmodel(walletmodel);

    if (walletmodel)
    {
        // receive and pass through messages from wallet model
        connect(walletmodel, signal(message(qstring,qstring,unsigned int)), this, signal(message(qstring,qstring,unsigned int)));

        // handle changes in encryption status
        connect(walletmodel, signal(encryptionstatuschanged(int)), this, signal(encryptionstatuschanged(int)));
        updateencryptionstatus();

        // balloon pop-up for new transaction
        connect(walletmodel->gettransactiontablemodel(), signal(rowsinserted(qmodelindex,int,int)),
                this, slot(processnewtransaction(qmodelindex,int,int)));

        // ask for passphrase if needed
        connect(walletmodel, signal(requireunlock()), this, slot(unlockwallet()));

        // show progress dialog
        connect(walletmodel, signal(showprogress(qstring,int)), this, slot(showprogress(qstring,int)));
    }
}

void walletview::processnewtransaction(const qmodelindex& parent, int start, int /*end*/)
{
    // prevent balloon-spam when initial block download is in progress
    if (!walletmodel || !clientmodel || clientmodel->ininitialblockdownload())
        return;

    transactiontablemodel *ttm = walletmodel->gettransactiontablemodel();
    if (!ttm || ttm->processingqueuedtransactions())
        return;

    qstring date = ttm->index(start, transactiontablemodel::date, parent).data().tostring();
    qint64 amount = ttm->index(start, transactiontablemodel::amount, parent).data(qt::editrole).toulonglong();
    qstring type = ttm->index(start, transactiontablemodel::type, parent).data().tostring();
    qmodelindex index = ttm->index(start, 0, parent);
    qstring address = ttm->data(index, transactiontablemodel::addressrole).tostring();
    qstring label = ttm->data(index, transactiontablemodel::labelrole).tostring();

    emit incomingtransaction(date, walletmodel->getoptionsmodel()->getdisplayunit(), amount, type, address, label);
}

void walletview::gotooverviewpage()
{
    setcurrentwidget(overviewpage);
}

void walletview::gotohistorypage()
{
    setcurrentwidget(transactionspage);
}

void walletview::gotoreceivecoinspage()
{
    setcurrentwidget(receivecoinspage);
}

void walletview::gotosendcoinspage(qstring addr)
{
    setcurrentwidget(sendcoinspage);

    if (!addr.isempty())
        sendcoinspage->setaddress(addr);
}

void walletview::gotosignmessagetab(qstring addr)
{
    // calls show() in showtab_sm()
    signverifymessagedialog *signverifymessagedialog = new signverifymessagedialog(this);
    signverifymessagedialog->setattribute(qt::wa_deleteonclose);
    signverifymessagedialog->setmodel(walletmodel);
    signverifymessagedialog->showtab_sm(true);

    if (!addr.isempty())
        signverifymessagedialog->setaddress_sm(addr);
}

void walletview::gotoverifymessagetab(qstring addr)
{
    // calls show() in showtab_vm()
    signverifymessagedialog *signverifymessagedialog = new signverifymessagedialog(this);
    signverifymessagedialog->setattribute(qt::wa_deleteonclose);
    signverifymessagedialog->setmodel(walletmodel);
    signverifymessagedialog->showtab_vm(true);

    if (!addr.isempty())
        signverifymessagedialog->setaddress_vm(addr);
}

bool walletview::handlepaymentrequest(const sendcoinsrecipient& recipient)
{
    return sendcoinspage->handlepaymentrequest(recipient);
}

void walletview::showoutofsyncwarning(bool fshow)
{
    overviewpage->showoutofsyncwarning(fshow);
}

void walletview::updateencryptionstatus()
{
    emit encryptionstatuschanged(walletmodel->getencryptionstatus());
}

void walletview::encryptwallet(bool status)
{
    if(!walletmodel)
        return;
    askpassphrasedialog dlg(status ? askpassphrasedialog::encrypt : askpassphrasedialog::decrypt, this);
    dlg.setmodel(walletmodel);
    dlg.exec();

    updateencryptionstatus();
}

void walletview::backupwallet()
{
    qstring filename = guiutil::getsavefilename(this,
        tr("backup wallet"), qstring(),
        tr("wallet data (*.dat)"), null);

    if (filename.isempty())
        return;

    if (!walletmodel->backupwallet(filename)) {
        emit message(tr("backup failed"), tr("there was an error trying to save the wallet data to %1.").arg(filename),
            cclientuiinterface::msg_error);
        }
    else {
        emit message(tr("backup successful"), tr("the wallet data was successfully saved to %1.").arg(filename),
            cclientuiinterface::msg_information);
    }
}

void walletview::changepassphrase()
{
    askpassphrasedialog dlg(askpassphrasedialog::changepass, this);
    dlg.setmodel(walletmodel);
    dlg.exec();
}

void walletview::unlockwallet()
{
    if(!walletmodel)
        return;
    // unlock wallet when requested by wallet model
    if (walletmodel->getencryptionstatus() == walletmodel::locked)
    {
        askpassphrasedialog dlg(askpassphrasedialog::unlock, this);
        dlg.setmodel(walletmodel);
        dlg.exec();
    }
}

void walletview::usedsendingaddresses()
{
    if(!walletmodel)
        return;
    addressbookpage *dlg = new addressbookpage(addressbookpage::forediting, addressbookpage::sendingtab, this);
    dlg->setattribute(qt::wa_deleteonclose);
    dlg->setmodel(walletmodel->getaddresstablemodel());
    dlg->show();
}

void walletview::usedreceivingaddresses()
{
    if(!walletmodel)
        return;
    addressbookpage *dlg = new addressbookpage(addressbookpage::forediting, addressbookpage::receivingtab, this);
    dlg->setattribute(qt::wa_deleteonclose);
    dlg->setmodel(walletmodel->getaddresstablemodel());
    dlg->show();
}

void walletview::showprogress(const qstring &title, int nprogress)
{
    if (nprogress == 0)
    {
        progressdialog = new qprogressdialog(title, "", 0, 100);
        progressdialog->setwindowmodality(qt::applicationmodal);
        progressdialog->setminimumduration(0);
        progressdialog->setcancelbutton(0);
        progressdialog->setautoclose(false);
        progressdialog->setvalue(0);
    }
    else if (nprogress == 100)
    {
        if (progressdialog)
        {
            progressdialog->close();
            progressdialog->deletelater();
        }
    }
    else if (progressdialog)
        progressdialog->setvalue(nprogress);
}
