// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "addressbookpage.h"
#include "ui_addressbookpage.h"

#include "addresstablemodel.h"
#include "moorecoingui.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "scicon.h"

#include <qicon>
#include <qmenu>
#include <qmessagebox>
#include <qsortfilterproxymodel>

addressbookpage::addressbookpage(mode mode, tabs tab, qwidget *parent) :
    qdialog(parent),
    ui(new ui::addressbookpage),
    model(0),
    mode(mode),
    tab(tab)
{
    ui->setupui(this);

#ifdef q_os_mac // icons on push buttons are very uncommon on mac
    ui->newaddress->seticon(qicon());
    ui->copyaddress->seticon(qicon());
    ui->deleteaddress->seticon(qicon());
    ui->exportbutton->seticon(qicon());
#else
    ui->newaddress->seticon(singlecoloricon(":/icons/add"));
    ui->copyaddress->seticon(singlecoloricon(":/icons/editcopy"));
    ui->deleteaddress->seticon(singlecoloricon(":/icons/remove"));
    ui->exportbutton->seticon(singlecoloricon(":/icons/export"));
#endif

    switch(mode)
    {
    case forselection:
        switch(tab)
        {
        case sendingtab: setwindowtitle(tr("choose the address to send coins to")); break;
        case receivingtab: setwindowtitle(tr("choose the address to receive coins with")); break;
        }
        connect(ui->tableview, signal(doubleclicked(qmodelindex)), this, slot(accept()));
        ui->tableview->setedittriggers(qabstractitemview::noedittriggers);
        ui->tableview->setfocus();
        ui->closebutton->settext(tr("c&hoose"));
        ui->exportbutton->hide();
        break;
    case forediting:
        switch(tab)
        {
        case sendingtab: setwindowtitle(tr("sending addresses")); break;
        case receivingtab: setwindowtitle(tr("receiving addresses")); break;
        }
        break;
    }
    switch(tab)
    {
    case sendingtab:
        ui->labelexplanation->settext(tr("these are your moorecoin addresses for sending payments. always check the amount and the receiving address before sending coins."));
        ui->deleteaddress->setvisible(true);
        break;
    case receivingtab:
        ui->labelexplanation->settext(tr("these are your moorecoin addresses for receiving payments. it is recommended to use a new receiving address for each transaction."));
        ui->deleteaddress->setvisible(false);
        break;
    }

    // context menu actions
    qaction *copyaddressaction = new qaction(tr("&copy address"), this);
    qaction *copylabelaction = new qaction(tr("copy &label"), this);
    qaction *editaction = new qaction(tr("&edit"), this);
    deleteaction = new qaction(ui->deleteaddress->text(), this);

    // build context menu
    contextmenu = new qmenu();
    contextmenu->addaction(copyaddressaction);
    contextmenu->addaction(copylabelaction);
    contextmenu->addaction(editaction);
    if(tab == sendingtab)
        contextmenu->addaction(deleteaction);
    contextmenu->addseparator();

    // connect signals for context menu actions
    connect(copyaddressaction, signal(triggered()), this, slot(on_copyaddress_clicked()));
    connect(copylabelaction, signal(triggered()), this, slot(oncopylabelaction()));
    connect(editaction, signal(triggered()), this, slot(oneditaction()));
    connect(deleteaction, signal(triggered()), this, slot(on_deleteaddress_clicked()));

    connect(ui->tableview, signal(customcontextmenurequested(qpoint)), this, slot(contextualmenu(qpoint)));

    connect(ui->closebutton, signal(clicked()), this, slot(accept()));
}

addressbookpage::~addressbookpage()
{
    delete ui;
}

void addressbookpage::setmodel(addresstablemodel *model)
{
    this->model = model;
    if(!model)
        return;

    proxymodel = new qsortfilterproxymodel(this);
    proxymodel->setsourcemodel(model);
    proxymodel->setdynamicsortfilter(true);
    proxymodel->setsortcasesensitivity(qt::caseinsensitive);
    proxymodel->setfiltercasesensitivity(qt::caseinsensitive);
    switch(tab)
    {
    case receivingtab:
        // receive filter
        proxymodel->setfilterrole(addresstablemodel::typerole);
        proxymodel->setfilterfixedstring(addresstablemodel::receive);
        break;
    case sendingtab:
        // send filter
        proxymodel->setfilterrole(addresstablemodel::typerole);
        proxymodel->setfilterfixedstring(addresstablemodel::send);
        break;
    }
    ui->tableview->setmodel(proxymodel);
    ui->tableview->sortbycolumn(0, qt::ascendingorder);

    // set column widths
#if qt_version < 0x050000
    ui->tableview->horizontalheader()->setresizemode(addresstablemodel::label, qheaderview::stretch);
    ui->tableview->horizontalheader()->setresizemode(addresstablemodel::address, qheaderview::resizetocontents);
#else
    ui->tableview->horizontalheader()->setsectionresizemode(addresstablemodel::label, qheaderview::stretch);
    ui->tableview->horizontalheader()->setsectionresizemode(addresstablemodel::address, qheaderview::resizetocontents);
#endif

    connect(ui->tableview->selectionmodel(), signal(selectionchanged(qitemselection,qitemselection)),
        this, slot(selectionchanged()));

    // select row for newly created address
    connect(model, signal(rowsinserted(qmodelindex,int,int)), this, slot(selectnewaddress(qmodelindex,int,int)));

    selectionchanged();
}

void addressbookpage::on_copyaddress_clicked()
{
    guiutil::copyentrydata(ui->tableview, addresstablemodel::address);
}

void addressbookpage::oncopylabelaction()
{
    guiutil::copyentrydata(ui->tableview, addresstablemodel::label);
}

void addressbookpage::oneditaction()
{
    if(!model)
        return;

    if(!ui->tableview->selectionmodel())
        return;
    qmodelindexlist indexes = ui->tableview->selectionmodel()->selectedrows();
    if(indexes.isempty())
        return;

    editaddressdialog dlg(
        tab == sendingtab ?
        editaddressdialog::editsendingaddress :
        editaddressdialog::editreceivingaddress, this);
    dlg.setmodel(model);
    qmodelindex origindex = proxymodel->maptosource(indexes.at(0));
    dlg.loadrow(origindex.row());
    dlg.exec();
}

void addressbookpage::on_newaddress_clicked()
{
    if(!model)
        return;

    editaddressdialog dlg(
        tab == sendingtab ?
        editaddressdialog::newsendingaddress :
        editaddressdialog::newreceivingaddress, this);
    dlg.setmodel(model);
    if(dlg.exec())
    {
        newaddresstoselect = dlg.getaddress();
    }
}

void addressbookpage::on_deleteaddress_clicked()
{
    qtableview *table = ui->tableview;
    if(!table->selectionmodel())
        return;

    qmodelindexlist indexes = table->selectionmodel()->selectedrows();
    if(!indexes.isempty())
    {
        table->model()->removerow(indexes.at(0).row());
    }
}

void addressbookpage::selectionchanged()
{
    // set button states based on selected tab and selection
    qtableview *table = ui->tableview;
    if(!table->selectionmodel())
        return;

    if(table->selectionmodel()->hasselection())
    {
        switch(tab)
        {
        case sendingtab:
            // in sending tab, allow deletion of selection
            ui->deleteaddress->setenabled(true);
            ui->deleteaddress->setvisible(true);
            deleteaction->setenabled(true);
            break;
        case receivingtab:
            // deleting receiving addresses, however, is not allowed
            ui->deleteaddress->setenabled(false);
            ui->deleteaddress->setvisible(false);
            deleteaction->setenabled(false);
            break;
        }
        ui->copyaddress->setenabled(true);
    }
    else
    {
        ui->deleteaddress->setenabled(false);
        ui->copyaddress->setenabled(false);
    }
}

void addressbookpage::done(int retval)
{
    qtableview *table = ui->tableview;
    if(!table->selectionmodel() || !table->model())
        return;

    // figure out which address was selected, and return it
    qmodelindexlist indexes = table->selectionmodel()->selectedrows(addresstablemodel::address);

    foreach (qmodelindex index, indexes)
    {
        qvariant address = table->model()->data(index);
        returnvalue = address.tostring();
    }

    if(returnvalue.isempty())
    {
        // if no address entry selected, return rejected
        retval = rejected;
    }

    qdialog::done(retval);
}

void addressbookpage::on_exportbutton_clicked()
{
    // csv is currently the only supported format
    qstring filename = guiutil::getsavefilename(this,
        tr("export address list"), qstring(),
        tr("comma separated file (*.csv)"), null);

    if (filename.isnull())
        return;

    csvmodelwriter writer(filename);

    // name, column, role
    writer.setmodel(proxymodel);
    writer.addcolumn("label", addresstablemodel::label, qt::editrole);
    writer.addcolumn("address", addresstablemodel::address, qt::editrole);

    if(!writer.write()) {
        qmessagebox::critical(this, tr("exporting failed"),
            tr("there was an error trying to save the address list to %1. please try again.").arg(filename));
    }
}

void addressbookpage::contextualmenu(const qpoint &point)
{
    qmodelindex index = ui->tableview->indexat(point);
    if(index.isvalid())
    {
        contextmenu->exec(qcursor::pos());
    }
}

void addressbookpage::selectnewaddress(const qmodelindex &parent, int begin, int /*end*/)
{
    qmodelindex idx = proxymodel->mapfromsource(model->index(begin, addresstablemodel::address, parent));
    if(idx.isvalid() && (idx.data(qt::editrole).tostring() == newaddresstoselect))
    {
        // select row of newly created address, once
        ui->tableview->setfocus();
        ui->tableview->selectrow(idx.row());
        newaddresstoselect.clear();
    }
}
