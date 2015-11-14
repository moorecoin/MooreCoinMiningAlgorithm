// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "receivecoinsdialog.h"
#include "ui_receivecoinsdialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "moorecoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "scicon.h"
#include "walletmodel.h"

#include <qaction>
#include <qcursor>
#include <qitemselection>
#include <qmessagebox>
#include <qscrollbar>
#include <qtextdocument>

receivecoinsdialog::receivecoinsdialog(qwidget *parent) :
    qdialog(parent),
    ui(new ui::receivecoinsdialog),
    model(0)
{
    ui->setupui(this);

#ifdef q_os_mac // icons on push buttons are very uncommon on mac
    ui->clearbutton->seticon(qicon());
    ui->receivebutton->seticon(qicon());
    ui->showrequestbutton->seticon(qicon());
    ui->removerequestbutton->seticon(qicon());
#else
    ui->clearbutton->seticon(singlecoloricon(":/icons/remove"));
    ui->receivebutton->seticon(singlecoloricon(":/icons/receiving_addresses"));
    ui->showrequestbutton->seticon(singlecoloricon(":/icons/edit"));
    ui->removerequestbutton->seticon(singlecoloricon(":/icons/remove"));
#endif

    // context menu actions
    qaction *copylabelaction = new qaction(tr("copy label"), this);
    qaction *copymessageaction = new qaction(tr("copy message"), this);
    qaction *copyamountaction = new qaction(tr("copy amount"), this);

    // context menu
    contextmenu = new qmenu();
    contextmenu->addaction(copylabelaction);
    contextmenu->addaction(copymessageaction);
    contextmenu->addaction(copyamountaction);

    // context menu signals
    connect(ui->recentrequestsview, signal(customcontextmenurequested(qpoint)), this, slot(showmenu(qpoint)));
    connect(copylabelaction, signal(triggered()), this, slot(copylabel()));
    connect(copymessageaction, signal(triggered()), this, slot(copymessage()));
    connect(copyamountaction, signal(triggered()), this, slot(copyamount()));

    connect(ui->clearbutton, signal(clicked()), this, slot(clear()));
}

void receivecoinsdialog::setmodel(walletmodel *model)
{
    this->model = model;

    if(model && model->getoptionsmodel())
    {
        model->getrecentrequeststablemodel()->sort(recentrequeststablemodel::date, qt::descendingorder);
        connect(model->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(updatedisplayunit()));
        updatedisplayunit();

        qtableview* tableview = ui->recentrequestsview;

        tableview->verticalheader()->hide();
        tableview->sethorizontalscrollbarpolicy(qt::scrollbaralwaysoff);
        tableview->setmodel(model->getrecentrequeststablemodel());
        tableview->setalternatingrowcolors(true);
        tableview->setselectionbehavior(qabstractitemview::selectrows);
        tableview->setselectionmode(qabstractitemview::contiguousselection);
        tableview->setcolumnwidth(recentrequeststablemodel::date, date_column_width);
        tableview->setcolumnwidth(recentrequeststablemodel::label, label_column_width);

        connect(tableview->selectionmodel(),
            signal(selectionchanged(qitemselection, qitemselection)), this,
            slot(recentrequestsview_selectionchanged(qitemselection, qitemselection)));
        // last 2 columns are set by the columnresizingfixer, when the table geometry is ready.
        columnresizingfixer = new guiutil::tableviewlastcolumnresizingfixer(tableview, amount_minimum_column_width, date_column_width);
    }
}

receivecoinsdialog::~receivecoinsdialog()
{
    delete ui;
}

void receivecoinsdialog::clear()
{
    ui->reqamount->clear();
    ui->reqlabel->settext("");
    ui->reqmessage->settext("");
    ui->reuseaddress->setchecked(false);
    updatedisplayunit();
}

void receivecoinsdialog::reject()
{
    clear();
}

void receivecoinsdialog::accept()
{
    clear();
}

void receivecoinsdialog::updatedisplayunit()
{
    if(model && model->getoptionsmodel())
    {
        ui->reqamount->setdisplayunit(model->getoptionsmodel()->getdisplayunit());
    }
}

void receivecoinsdialog::on_receivebutton_clicked()
{
    if(!model || !model->getoptionsmodel() || !model->getaddresstablemodel() || !model->getrecentrequeststablemodel())
        return;

    qstring address;
    qstring label = ui->reqlabel->text();
    if(ui->reuseaddress->ischecked())
    {
        /* choose existing receiving address */
        addressbookpage dlg(addressbookpage::forselection, addressbookpage::receivingtab, this);
        dlg.setmodel(model->getaddresstablemodel());
        if(dlg.exec())
        {
            address = dlg.getreturnvalue();
            if(label.isempty()) /* if no label provided, use the previously used label */
            {
                label = model->getaddresstablemodel()->labelforaddress(address);
            }
        } else {
            return;
        }
    } else {
        /* generate new receiving address */
        address = model->getaddresstablemodel()->addrow(addresstablemodel::receive, label, "");
    }
    sendcoinsrecipient info(address, label,
        ui->reqamount->value(), ui->reqmessage->text());
    receiverequestdialog *dialog = new receiverequestdialog(this);
    dialog->setattribute(qt::wa_deleteonclose);
    dialog->setmodel(model->getoptionsmodel());
    dialog->setinfo(info);
    dialog->show();
    clear();

    /* store request for later reference */
    model->getrecentrequeststablemodel()->addnewrequest(info);
}

void receivecoinsdialog::on_recentrequestsview_doubleclicked(const qmodelindex &index)
{
    const recentrequeststablemodel *submodel = model->getrecentrequeststablemodel();
    receiverequestdialog *dialog = new receiverequestdialog(this);
    dialog->setmodel(model->getoptionsmodel());
    dialog->setinfo(submodel->entry(index.row()).recipient);
    dialog->setattribute(qt::wa_deleteonclose);
    dialog->show();
}

void receivecoinsdialog::recentrequestsview_selectionchanged(const qitemselection &selected, const qitemselection &deselected)
{
    // enable show/remove buttons only if anything is selected.
    bool enable = !ui->recentrequestsview->selectionmodel()->selectedrows().isempty();
    ui->showrequestbutton->setenabled(enable);
    ui->removerequestbutton->setenabled(enable);
}

void receivecoinsdialog::on_showrequestbutton_clicked()
{
    if(!model || !model->getrecentrequeststablemodel() || !ui->recentrequestsview->selectionmodel())
        return;
    qmodelindexlist selection = ui->recentrequestsview->selectionmodel()->selectedrows();

    foreach (qmodelindex index, selection)
    {
        on_recentrequestsview_doubleclicked(index);
    }
}

void receivecoinsdialog::on_removerequestbutton_clicked()
{
    if(!model || !model->getrecentrequeststablemodel() || !ui->recentrequestsview->selectionmodel())
        return;
    qmodelindexlist selection = ui->recentrequestsview->selectionmodel()->selectedrows();
    if(selection.empty())
        return;
    // correct for selection mode contiguousselection
    qmodelindex firstindex = selection.at(0);
    model->getrecentrequeststablemodel()->removerows(firstindex.row(), selection.length(), firstindex.parent());
}

// we override the virtual resizeevent of the qwidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void receivecoinsdialog::resizeevent(qresizeevent *event)
{
    qwidget::resizeevent(event);
    columnresizingfixer->stretchcolumnwidth(recentrequeststablemodel::message);
}

void receivecoinsdialog::keypressevent(qkeyevent *event)
{
    if (event->key() == qt::key_return)
    {
        // press return -> submit form
        if (ui->reqlabel->hasfocus() || ui->reqamount->hasfocus() || ui->reqmessage->hasfocus())
        {
            event->ignore();
            on_receivebutton_clicked();
            return;
        }
    }

    this->qdialog::keypressevent(event);
}

// copy column of selected row to clipboard
void receivecoinsdialog::copycolumntoclipboard(int column)
{
    if(!model || !model->getrecentrequeststablemodel() || !ui->recentrequestsview->selectionmodel())
        return;
    qmodelindexlist selection = ui->recentrequestsview->selectionmodel()->selectedrows();
    if(selection.empty())
        return;
    // correct for selection mode contiguousselection
    qmodelindex firstindex = selection.at(0);
    guiutil::setclipboard(model->getrecentrequeststablemodel()->data(firstindex.child(firstindex.row(), column), qt::editrole).tostring());
}

// context menu
void receivecoinsdialog::showmenu(const qpoint &point)
{
    if(!model || !model->getrecentrequeststablemodel() || !ui->recentrequestsview->selectionmodel())
        return;
    qmodelindexlist selection = ui->recentrequestsview->selectionmodel()->selectedrows();
    if(selection.empty())
        return;
    contextmenu->exec(qcursor::pos());
}

// context menu action: copy label
void receivecoinsdialog::copylabel()
{
    copycolumntoclipboard(recentrequeststablemodel::label);
}

// context menu action: copy message
void receivecoinsdialog::copymessage()
{
    copycolumntoclipboard(recentrequeststablemodel::message);
}

// context menu action: copy amount
void receivecoinsdialog::copyamount()
{
    copycolumntoclipboard(recentrequeststablemodel::amount);
}
