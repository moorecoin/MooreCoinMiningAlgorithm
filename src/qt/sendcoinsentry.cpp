// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "sendcoinsentry.h"
#include "ui_sendcoinsentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "scicon.h"
#include "walletmodel.h"

#include <qapplication>
#include <qclipboard>

sendcoinsentry::sendcoinsentry(qwidget *parent) :
    qstackedwidget(parent),
    ui(new ui::sendcoinsentry),
    model(0)
{
    ui->setupui(this);

    ui->addressbookbutton->seticon(singlecoloricon(":/icons/address-book"));
    ui->pastebutton->seticon(singlecoloricon(":/icons/editpaste"));
    ui->deletebutton->seticon(singlecoloricon(":/icons/remove"));
    ui->deletebutton_is->seticon(singlecoloricon(":/icons/remove"));
    ui->deletebutton_s->seticon(singlecoloricon(":/icons/remove"));

    setcurrentwidget(ui->sendcoins);

#ifdef q_os_mac
    ui->paytolayout->setspacing(4);
#endif
#if qt_version >= 0x040700
    ui->addaslabel->setplaceholdertext(tr("enter a label for this address to add it to your address book"));
#endif

    // normal moorecoin address field
    guiutil::setupaddresswidget(ui->payto, this);
    // just a label for displaying moorecoin address(es)
    ui->payto_is->setfont(guiutil::moorecoinaddressfont());

    // connect signals
    connect(ui->payamount, signal(valuechanged()), this, signal(payamountchanged()));
    connect(ui->checkboxsubtractfeefromamount, signal(toggled(bool)), this, signal(subtractfeefromamountchanged()));
    connect(ui->deletebutton, signal(clicked()), this, slot(deleteclicked()));
    connect(ui->deletebutton_is, signal(clicked()), this, slot(deleteclicked()));
    connect(ui->deletebutton_s, signal(clicked()), this, slot(deleteclicked()));
}

sendcoinsentry::~sendcoinsentry()
{
    delete ui;
}

void sendcoinsentry::on_pastebutton_clicked()
{
    // paste text from clipboard into recipient field
    ui->payto->settext(qapplication::clipboard()->text());
}

void sendcoinsentry::on_addressbookbutton_clicked()
{
    if(!model)
        return;
    addressbookpage dlg(addressbookpage::forselection, addressbookpage::sendingtab, this);
    dlg.setmodel(model->getaddresstablemodel());
    if(dlg.exec())
    {
        ui->payto->settext(dlg.getreturnvalue());
        ui->payamount->setfocus();
    }
}

void sendcoinsentry::on_payto_textchanged(const qstring &address)
{
    updatelabel(address);
}

void sendcoinsentry::setmodel(walletmodel *model)
{
    this->model = model;

    if (model && model->getoptionsmodel())
        connect(model->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(updatedisplayunit()));

    clear();
}

void sendcoinsentry::clear()
{
    // clear ui elements for normal payment
    ui->payto->clear();
    ui->addaslabel->clear();
    ui->payamount->clear();
    ui->checkboxsubtractfeefromamount->setcheckstate(qt::unchecked);
    ui->messagetextlabel->clear();
    ui->messagetextlabel->hide();
    ui->messagelabel->hide();
    // clear ui elements for unauthenticated payment request
    ui->payto_is->clear();
    ui->memotextlabel_is->clear();
    ui->payamount_is->clear();
    // clear ui elements for authenticated payment request
    ui->payto_s->clear();
    ui->memotextlabel_s->clear();
    ui->payamount_s->clear();

    // update the display unit, to not use the default ("btc")
    updatedisplayunit();
}

void sendcoinsentry::deleteclicked()
{
    emit removeentry(this);
}

bool sendcoinsentry::validate()
{
    if (!model)
        return false;

    // check input validity
    bool retval = true;

    // skip checks for payment request
    if (recipient.paymentrequest.isinitialized())
        return retval;

    if (!model->validateaddress(ui->payto->text()))
    {
        ui->payto->setvalid(false);
        retval = false;
    }

    if (!ui->payamount->validate())
    {
        retval = false;
    }

    // sending a zero amount is invalid
    if (ui->payamount->value(0) <= 0)
    {
        ui->payamount->setvalid(false);
        retval = false;
    }

    // reject dust outputs:
    if (retval && guiutil::isdust(ui->payto->text(), ui->payamount->value())) {
        ui->payamount->setvalid(false);
        retval = false;
    }

    return retval;
}

sendcoinsrecipient sendcoinsentry::getvalue()
{
    // payment request
    if (recipient.paymentrequest.isinitialized())
        return recipient;

    // normal payment
    recipient.address = ui->payto->text();
    recipient.label = ui->addaslabel->text();
    recipient.amount = ui->payamount->value();
    recipient.message = ui->messagetextlabel->text();
    recipient.fsubtractfeefromamount = (ui->checkboxsubtractfeefromamount->checkstate() == qt::checked);

    return recipient;
}

qwidget *sendcoinsentry::setuptabchain(qwidget *prev)
{
    qwidget::settaborder(prev, ui->payto);
    qwidget::settaborder(ui->payto, ui->addaslabel);
    qwidget *w = ui->payamount->setuptabchain(ui->addaslabel);
    qwidget::settaborder(w, ui->checkboxsubtractfeefromamount);
    qwidget::settaborder(ui->checkboxsubtractfeefromamount, ui->addressbookbutton);
    qwidget::settaborder(ui->addressbookbutton, ui->pastebutton);
    qwidget::settaborder(ui->pastebutton, ui->deletebutton);
    return ui->deletebutton;
}

void sendcoinsentry::setvalue(const sendcoinsrecipient &value)
{
    recipient = value;

    if (recipient.paymentrequest.isinitialized()) // payment request
    {
        if (recipient.authenticatedmerchant.isempty()) // unauthenticated
        {
            ui->payto_is->settext(recipient.address);
            ui->memotextlabel_is->settext(recipient.message);
            ui->payamount_is->setvalue(recipient.amount);
            ui->payamount_is->setreadonly(true);
            setcurrentwidget(ui->sendcoins_unauthenticatedpaymentrequest);
        }
        else // authenticated
        {
            ui->payto_s->settext(recipient.authenticatedmerchant);
            ui->memotextlabel_s->settext(recipient.message);
            ui->payamount_s->setvalue(recipient.amount);
            ui->payamount_s->setreadonly(true);
            setcurrentwidget(ui->sendcoins_authenticatedpaymentrequest);
        }
    }
    else // normal payment
    {
        // message
        ui->messagetextlabel->settext(recipient.message);
        ui->messagetextlabel->setvisible(!recipient.message.isempty());
        ui->messagelabel->setvisible(!recipient.message.isempty());

        ui->addaslabel->clear();
        ui->payto->settext(recipient.address); // this may set a label from addressbook
        if (!recipient.label.isempty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addaslabel->settext(recipient.label);
        ui->payamount->setvalue(recipient.amount);
    }
}

void sendcoinsentry::setaddress(const qstring &address)
{
    ui->payto->settext(address);
    ui->payamount->setfocus();
}

bool sendcoinsentry::isclear()
{
    return ui->payto->text().isempty() && ui->payto_is->text().isempty() && ui->payto_s->text().isempty();
}

void sendcoinsentry::setfocus()
{
    ui->payto->setfocus();
}

void sendcoinsentry::updatedisplayunit()
{
    if(model && model->getoptionsmodel())
    {
        // update payamount with the current unit
        ui->payamount->setdisplayunit(model->getoptionsmodel()->getdisplayunit());
        ui->payamount_is->setdisplayunit(model->getoptionsmodel()->getdisplayunit());
        ui->payamount_s->setdisplayunit(model->getoptionsmodel()->getdisplayunit());
    }
}

bool sendcoinsentry::updatelabel(const qstring &address)
{
    if(!model)
        return false;

    // fill in label from address book, if address has an associated label
    qstring associatedlabel = model->getaddresstablemodel()->labelforaddress(address);
    if(!associatedlabel.isempty())
    {
        ui->addaslabel->settext(associatedlabel);
        return true;
    }

    return false;
}
