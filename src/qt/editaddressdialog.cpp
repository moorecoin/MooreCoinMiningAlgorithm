// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "editaddressdialog.h"
#include "ui_editaddressdialog.h"

#include "addresstablemodel.h"
#include "guiutil.h"

#include <qdatawidgetmapper>
#include <qmessagebox>

editaddressdialog::editaddressdialog(mode mode, qwidget *parent) :
    qdialog(parent),
    ui(new ui::editaddressdialog),
    mapper(0),
    mode(mode),
    model(0)
{
    ui->setupui(this);

    guiutil::setupaddresswidget(ui->addressedit, this);

    switch(mode)
    {
    case newreceivingaddress:
        setwindowtitle(tr("new receiving address"));
        ui->addressedit->setenabled(false);
        break;
    case newsendingaddress:
        setwindowtitle(tr("new sending address"));
        break;
    case editreceivingaddress:
        setwindowtitle(tr("edit receiving address"));
        ui->addressedit->setenabled(false);
        break;
    case editsendingaddress:
        setwindowtitle(tr("edit sending address"));
        break;
    }

    mapper = new qdatawidgetmapper(this);
    mapper->setsubmitpolicy(qdatawidgetmapper::manualsubmit);
}

editaddressdialog::~editaddressdialog()
{
    delete ui;
}

void editaddressdialog::setmodel(addresstablemodel *model)
{
    this->model = model;
    if(!model)
        return;

    mapper->setmodel(model);
    mapper->addmapping(ui->labeledit, addresstablemodel::label);
    mapper->addmapping(ui->addressedit, addresstablemodel::address);
}

void editaddressdialog::loadrow(int row)
{
    mapper->setcurrentindex(row);
}

bool editaddressdialog::savecurrentrow()
{
    if(!model)
        return false;

    switch(mode)
    {
    case newreceivingaddress:
    case newsendingaddress:
        address = model->addrow(
                mode == newsendingaddress ? addresstablemodel::send : addresstablemodel::receive,
                ui->labeledit->text(),
                ui->addressedit->text());
        break;
    case editreceivingaddress:
    case editsendingaddress:
        if(mapper->submit())
        {
            address = ui->addressedit->text();
        }
        break;
    }
    return !address.isempty();
}

void editaddressdialog::accept()
{
    if(!model)
        return;

    if(!savecurrentrow())
    {
        switch(model->geteditstatus())
        {
        case addresstablemodel::ok:
            // failed with unknown reason. just reject.
            break;
        case addresstablemodel::no_changes:
            // no changes were made during edit operation. just reject.
            break;
        case addresstablemodel::invalid_address:
            qmessagebox::warning(this, windowtitle(),
                tr("the entered address \"%1\" is not a valid moorecoin address.").arg(ui->addressedit->text()),
                qmessagebox::ok, qmessagebox::ok);
            break;
        case addresstablemodel::duplicate_address:
            qmessagebox::warning(this, windowtitle(),
                tr("the entered address \"%1\" is already in the address book.").arg(ui->addressedit->text()),
                qmessagebox::ok, qmessagebox::ok);
            break;
        case addresstablemodel::wallet_unlock_failure:
            qmessagebox::critical(this, windowtitle(),
                tr("could not unlock wallet."),
                qmessagebox::ok, qmessagebox::ok);
            break;
        case addresstablemodel::key_generation_failure:
            qmessagebox::critical(this, windowtitle(),
                tr("new key generation failed."),
                qmessagebox::ok, qmessagebox::ok);
            break;

        }
        return;
    }
    qdialog::accept();
}

qstring editaddressdialog::getaddress() const
{
    return address;
}

void editaddressdialog::setaddress(const qstring &address)
{
    this->address = address;
    ui->addressedit->settext(address);
}
