// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "transactionview.h"

#include "addresstablemodel.h"
#include "moorecoinunits.h"
#include "csvmodelwriter.h"
#include "editaddressdialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "scicon.h"
#include "transactiondescdialog.h"
#include "transactionfilterproxy.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include "ui_interface.h"

#include <qcombobox>
#include <qdatetimeedit>
#include <qdesktopservices>
#include <qdoublevalidator>
#include <qhboxlayout>
#include <qheaderview>
#include <qlabel>
#include <qlineedit>
#include <qmenu>
#include <qpoint>
#include <qscrollbar>
#include <qsignalmapper>
#include <qtableview>
#include <qurl>
#include <qvboxlayout>

transactionview::transactionview(qwidget *parent) :
    qwidget(parent), model(0), transactionproxymodel(0),
    transactionview(0)
{
    // build filter row
    setcontentsmargins(0,0,0,0);

    qhboxlayout *hlayout = new qhboxlayout();
    hlayout->setcontentsmargins(0,0,0,0);
#ifdef q_os_mac
    hlayout->setspacing(5);
    hlayout->addspacing(26);
#else
    hlayout->setspacing(0);
    hlayout->addspacing(23);
#endif

    watchonlywidget = new qcombobox(this);
    watchonlywidget->setfixedwidth(24);
    watchonlywidget->additem("", transactionfilterproxy::watchonlyfilter_all);
    watchonlywidget->additem(singlecoloricon(":/icons/eye_plus"), "", transactionfilterproxy::watchonlyfilter_yes);
    watchonlywidget->additem(singlecoloricon(":/icons/eye_minus"), "", transactionfilterproxy::watchonlyfilter_no);
    hlayout->addwidget(watchonlywidget);

    datewidget = new qcombobox(this);
#ifdef q_os_mac
    datewidget->setfixedwidth(121);
#else
    datewidget->setfixedwidth(120);
#endif
    datewidget->additem(tr("all"), all);
    datewidget->additem(tr("today"), today);
    datewidget->additem(tr("this week"), thisweek);
    datewidget->additem(tr("this month"), thismonth);
    datewidget->additem(tr("last month"), lastmonth);
    datewidget->additem(tr("this year"), thisyear);
    datewidget->additem(tr("range..."), range);
    hlayout->addwidget(datewidget);

    typewidget = new qcombobox(this);
#ifdef q_os_mac
    typewidget->setfixedwidth(121);
#else
    typewidget->setfixedwidth(120);
#endif

    typewidget->additem(tr("all"), transactionfilterproxy::all_types);
    typewidget->additem(tr("received with"), transactionfilterproxy::type(transactionrecord::recvwithaddress) |
                                        transactionfilterproxy::type(transactionrecord::recvfromother));
    typewidget->additem(tr("sent to"), transactionfilterproxy::type(transactionrecord::sendtoaddress) |
                                  transactionfilterproxy::type(transactionrecord::sendtoother));
    typewidget->additem(tr("to yourself"), transactionfilterproxy::type(transactionrecord::sendtoself));
    typewidget->additem(tr("mined"), transactionfilterproxy::type(transactionrecord::generated));
    typewidget->additem(tr("other"), transactionfilterproxy::type(transactionrecord::other));

    hlayout->addwidget(typewidget);

    addresswidget = new qlineedit(this);
#if qt_version >= 0x040700
    addresswidget->setplaceholdertext(tr("enter address or label to search"));
#endif
    hlayout->addwidget(addresswidget);

    amountwidget = new qlineedit(this);
#if qt_version >= 0x040700
    amountwidget->setplaceholdertext(tr("min amount"));
#endif
#ifdef q_os_mac
    amountwidget->setfixedwidth(97);
#else
    amountwidget->setfixedwidth(100);
#endif
    amountwidget->setvalidator(new qdoublevalidator(0, 1e20, 8, this));
    hlayout->addwidget(amountwidget);

    qvboxlayout *vlayout = new qvboxlayout(this);
    vlayout->setcontentsmargins(0,0,0,0);
    vlayout->setspacing(0);

    qtableview *view = new qtableview(this);
    vlayout->addlayout(hlayout);
    vlayout->addwidget(createdaterangewidget());
    vlayout->addwidget(view);
    vlayout->setspacing(0);
    int width = view->verticalscrollbar()->sizehint().width();
    // cover scroll bar width with spacing
#ifdef q_os_mac
    hlayout->addspacing(width+2);
#else
    hlayout->addspacing(width);
#endif
    // always show scroll bar
    view->setverticalscrollbarpolicy(qt::scrollbaralwayson);
    view->settabkeynavigation(false);
    view->setcontextmenupolicy(qt::customcontextmenu);

    view->installeventfilter(this);

    transactionview = view;

    // actions
    qaction *copyaddressaction = new qaction(tr("copy address"), this);
    qaction *copylabelaction = new qaction(tr("copy label"), this);
    qaction *copyamountaction = new qaction(tr("copy amount"), this);
    qaction *copytxidaction = new qaction(tr("copy transaction id"), this);
    qaction *editlabelaction = new qaction(tr("edit label"), this);
    qaction *showdetailsaction = new qaction(tr("show transaction details"), this);

    contextmenu = new qmenu();
    contextmenu->addaction(copyaddressaction);
    contextmenu->addaction(copylabelaction);
    contextmenu->addaction(copyamountaction);
    contextmenu->addaction(copytxidaction);
    contextmenu->addaction(editlabelaction);
    contextmenu->addaction(showdetailsaction);

    mapperthirdpartytxurls = new qsignalmapper(this);

    // connect actions
    connect(mapperthirdpartytxurls, signal(mapped(qstring)), this, slot(openthirdpartytxurl(qstring)));

    connect(datewidget, signal(activated(int)), this, slot(choosedate(int)));
    connect(typewidget, signal(activated(int)), this, slot(choosetype(int)));
    connect(watchonlywidget, signal(activated(int)), this, slot(choosewatchonly(int)));
    connect(addresswidget, signal(textchanged(qstring)), this, slot(changedprefix(qstring)));
    connect(amountwidget, signal(textchanged(qstring)), this, slot(changedamount(qstring)));

    connect(view, signal(doubleclicked(qmodelindex)), this, signal(doubleclicked(qmodelindex)));
    connect(view, signal(customcontextmenurequested(qpoint)), this, slot(contextualmenu(qpoint)));

    connect(copyaddressaction, signal(triggered()), this, slot(copyaddress()));
    connect(copylabelaction, signal(triggered()), this, slot(copylabel()));
    connect(copyamountaction, signal(triggered()), this, slot(copyamount()));
    connect(copytxidaction, signal(triggered()), this, slot(copytxid()));
    connect(editlabelaction, signal(triggered()), this, slot(editlabel()));
    connect(showdetailsaction, signal(triggered()), this, slot(showdetails()));
}

void transactionview::setmodel(walletmodel *model)
{
    this->model = model;
    if(model)
    {
        transactionproxymodel = new transactionfilterproxy(this);
        transactionproxymodel->setsourcemodel(model->gettransactiontablemodel());
        transactionproxymodel->setdynamicsortfilter(true);
        transactionproxymodel->setsortcasesensitivity(qt::caseinsensitive);
        transactionproxymodel->setfiltercasesensitivity(qt::caseinsensitive);

        transactionproxymodel->setsortrole(qt::editrole);

        transactionview->sethorizontalscrollbarpolicy(qt::scrollbaralwaysoff);
        transactionview->setmodel(transactionproxymodel);
        transactionview->setalternatingrowcolors(true);
        transactionview->setselectionbehavior(qabstractitemview::selectrows);
        transactionview->setselectionmode(qabstractitemview::extendedselection);
        transactionview->setsortingenabled(true);
        transactionview->sortbycolumn(transactiontablemodel::status, qt::descendingorder);
        transactionview->verticalheader()->hide();

        transactionview->setcolumnwidth(transactiontablemodel::status, status_column_width);
        transactionview->setcolumnwidth(transactiontablemodel::watchonly, watchonly_column_width);
        transactionview->setcolumnwidth(transactiontablemodel::date, date_column_width);
        transactionview->setcolumnwidth(transactiontablemodel::type, type_column_width);
        transactionview->setcolumnwidth(transactiontablemodel::amount, amount_minimum_column_width);

        columnresizingfixer = new guiutil::tableviewlastcolumnresizingfixer(transactionview, amount_minimum_column_width, minimum_column_width);

        if (model->getoptionsmodel())
        {
            // add third party transaction urls to context menu
            qstringlist listurls = model->getoptionsmodel()->getthirdpartytxurls().split("|", qstring::skipemptyparts);
            for (int i = 0; i < listurls.size(); ++i)
            {
                qstring host = qurl(listurls[i].trimmed(), qurl::strictmode).host();
                if (!host.isempty())
                {
                    qaction *thirdpartytxurlaction = new qaction(host, this); // use host as menu item label
                    if (i == 0)
                        contextmenu->addseparator();
                    contextmenu->addaction(thirdpartytxurlaction);
                    connect(thirdpartytxurlaction, signal(triggered()), mapperthirdpartytxurls, slot(map()));
                    mapperthirdpartytxurls->setmapping(thirdpartytxurlaction, listurls[i].trimmed());
                }
            }
        }

        // show/hide column watch-only
        updatewatchonlycolumn(model->havewatchonly());

        // watch-only signal
        connect(model, signal(notifywatchonlychanged(bool)), this, slot(updatewatchonlycolumn(bool)));
    }
}

void transactionview::choosedate(int idx)
{
    if(!transactionproxymodel)
        return;
    qdate current = qdate::currentdate();
    daterangewidget->setvisible(false);
    switch(datewidget->itemdata(idx).toint())
    {
    case all:
        transactionproxymodel->setdaterange(
                transactionfilterproxy::min_date,
                transactionfilterproxy::max_date);
        break;
    case today:
        transactionproxymodel->setdaterange(
                qdatetime(current),
                transactionfilterproxy::max_date);
        break;
    case thisweek: {
        // find last monday
        qdate startofweek = current.adddays(-(current.dayofweek()-1));
        transactionproxymodel->setdaterange(
                qdatetime(startofweek),
                transactionfilterproxy::max_date);

        } break;
    case thismonth:
        transactionproxymodel->setdaterange(
                qdatetime(qdate(current.year(), current.month(), 1)),
                transactionfilterproxy::max_date);
        break;
    case lastmonth:
        transactionproxymodel->setdaterange(
                qdatetime(qdate(current.year(), current.month()-1, 1)),
                qdatetime(qdate(current.year(), current.month(), 1)));
        break;
    case thisyear:
        transactionproxymodel->setdaterange(
                qdatetime(qdate(current.year(), 1, 1)),
                transactionfilterproxy::max_date);
        break;
    case range:
        daterangewidget->setvisible(true);
        daterangechanged();
        break;
    }
}

void transactionview::choosetype(int idx)
{
    if(!transactionproxymodel)
        return;
    transactionproxymodel->settypefilter(
        typewidget->itemdata(idx).toint());
}

void transactionview::choosewatchonly(int idx)
{
    if(!transactionproxymodel)
        return;
    transactionproxymodel->setwatchonlyfilter(
        (transactionfilterproxy::watchonlyfilter)watchonlywidget->itemdata(idx).toint());
}

void transactionview::changedprefix(const qstring &prefix)
{
    if(!transactionproxymodel)
        return;
    transactionproxymodel->setaddressprefix(prefix);
}

void transactionview::changedamount(const qstring &amount)
{
    if(!transactionproxymodel)
        return;
    camount amount_parsed = 0;
    if(moorecoinunits::parse(model->getoptionsmodel()->getdisplayunit(), amount, &amount_parsed))
    {
        transactionproxymodel->setminamount(amount_parsed);
    }
    else
    {
        transactionproxymodel->setminamount(0);
    }
}

void transactionview::exportclicked()
{
    // csv is currently the only supported format
    qstring filename = guiutil::getsavefilename(this,
        tr("export transaction history"), qstring(),
        tr("comma separated file (*.csv)"), null);

    if (filename.isnull())
        return;

    csvmodelwriter writer(filename);

    // name, column, role
    writer.setmodel(transactionproxymodel);
    writer.addcolumn(tr("confirmed"), 0, transactiontablemodel::confirmedrole);
    if (model && model->havewatchonly())
        writer.addcolumn(tr("watch-only"), transactiontablemodel::watchonly);
    writer.addcolumn(tr("date"), 0, transactiontablemodel::daterole);
    writer.addcolumn(tr("type"), transactiontablemodel::type, qt::editrole);
    writer.addcolumn(tr("label"), 0, transactiontablemodel::labelrole);
    writer.addcolumn(tr("address"), 0, transactiontablemodel::addressrole);
    writer.addcolumn(moorecoinunits::getamountcolumntitle(model->getoptionsmodel()->getdisplayunit()), 0, transactiontablemodel::formattedamountrole);
    writer.addcolumn(tr("id"), 0, transactiontablemodel::txidrole);

    if(!writer.write()) {
        emit message(tr("exporting failed"), tr("there was an error trying to save the transaction history to %1.").arg(filename),
            cclientuiinterface::msg_error);
    }
    else {
        emit message(tr("exporting successful"), tr("the transaction history was successfully saved to %1.").arg(filename),
            cclientuiinterface::msg_information);
    }
}

void transactionview::contextualmenu(const qpoint &point)
{
    qmodelindex index = transactionview->indexat(point);
    if(index.isvalid())
    {
        contextmenu->exec(qcursor::pos());
    }
}

void transactionview::copyaddress()
{
    guiutil::copyentrydata(transactionview, 0, transactiontablemodel::addressrole);
}

void transactionview::copylabel()
{
    guiutil::copyentrydata(transactionview, 0, transactiontablemodel::labelrole);
}

void transactionview::copyamount()
{
    guiutil::copyentrydata(transactionview, 0, transactiontablemodel::formattedamountrole);
}

void transactionview::copytxid()
{
    guiutil::copyentrydata(transactionview, 0, transactiontablemodel::txidrole);
}

void transactionview::editlabel()
{
    if(!transactionview->selectionmodel() ||!model)
        return;
    qmodelindexlist selection = transactionview->selectionmodel()->selectedrows();
    if(!selection.isempty())
    {
        addresstablemodel *addressbook = model->getaddresstablemodel();
        if(!addressbook)
            return;
        qstring address = selection.at(0).data(transactiontablemodel::addressrole).tostring();
        if(address.isempty())
        {
            // if this transaction has no associated address, exit
            return;
        }
        // is address in address book? address book can miss address when a transaction is
        // sent from outside the ui.
        int idx = addressbook->lookupaddress(address);
        if(idx != -1)
        {
            // edit sending / receiving address
            qmodelindex modelidx = addressbook->index(idx, 0, qmodelindex());
            // determine type of address, launch appropriate editor dialog type
            qstring type = modelidx.data(addresstablemodel::typerole).tostring();

            editaddressdialog dlg(
                type == addresstablemodel::receive
                ? editaddressdialog::editreceivingaddress
                : editaddressdialog::editsendingaddress, this);
            dlg.setmodel(addressbook);
            dlg.loadrow(idx);
            dlg.exec();
        }
        else
        {
            // add sending address
            editaddressdialog dlg(editaddressdialog::newsendingaddress,
                this);
            dlg.setmodel(addressbook);
            dlg.setaddress(address);
            dlg.exec();
        }
    }
}

void transactionview::showdetails()
{
    if(!transactionview->selectionmodel())
        return;
    qmodelindexlist selection = transactionview->selectionmodel()->selectedrows();
    if(!selection.isempty())
    {
        transactiondescdialog dlg(selection.at(0));
        dlg.exec();
    }
}

void transactionview::openthirdpartytxurl(qstring url)
{
    if(!transactionview || !transactionview->selectionmodel())
        return;
    qmodelindexlist selection = transactionview->selectionmodel()->selectedrows(0);
    if(!selection.isempty())
         qdesktopservices::openurl(qurl::fromuserinput(url.replace("%s", selection.at(0).data(transactiontablemodel::txhashrole).tostring())));
}

qwidget *transactionview::createdaterangewidget()
{
    daterangewidget = new qframe();
    daterangewidget->setframestyle(qframe::panel | qframe::raised);
    daterangewidget->setcontentsmargins(1,1,1,1);
    qhboxlayout *layout = new qhboxlayout(daterangewidget);
    layout->setcontentsmargins(0,0,0,0);
    layout->addspacing(23);
    layout->addwidget(new qlabel(tr("range:")));

    datefrom = new qdatetimeedit(this);
    datefrom->setdisplayformat("dd/mm/yy");
    datefrom->setcalendarpopup(true);
    datefrom->setminimumwidth(100);
    datefrom->setdate(qdate::currentdate().adddays(-7));
    layout->addwidget(datefrom);
    layout->addwidget(new qlabel(tr("to")));

    dateto = new qdatetimeedit(this);
    dateto->setdisplayformat("dd/mm/yy");
    dateto->setcalendarpopup(true);
    dateto->setminimumwidth(100);
    dateto->setdate(qdate::currentdate());
    layout->addwidget(dateto);
    layout->addstretch();

    // hide by default
    daterangewidget->setvisible(false);

    // notify on change
    connect(datefrom, signal(datechanged(qdate)), this, slot(daterangechanged()));
    connect(dateto, signal(datechanged(qdate)), this, slot(daterangechanged()));

    return daterangewidget;
}

void transactionview::daterangechanged()
{
    if(!transactionproxymodel)
        return;
    transactionproxymodel->setdaterange(
            qdatetime(datefrom->date()),
            qdatetime(dateto->date()).adddays(1));
}

void transactionview::focustransaction(const qmodelindex &idx)
{
    if(!transactionproxymodel)
        return;
    qmodelindex targetidx = transactionproxymodel->mapfromsource(idx);
    transactionview->scrollto(targetidx);
    transactionview->setcurrentindex(targetidx);
    transactionview->setfocus();
}

// we override the virtual resizeevent of the qwidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void transactionview::resizeevent(qresizeevent* event)
{
    qwidget::resizeevent(event);
    columnresizingfixer->stretchcolumnwidth(transactiontablemodel::toaddress);
}

// need to override default ctrl+c action for amount as default behaviour is just to copy displayrole text
bool transactionview::eventfilter(qobject *obj, qevent *event)
{
    if (event->type() == qevent::keypress)
    {
        qkeyevent *ke = static_cast<qkeyevent *>(event);
        if (ke->key() == qt::key_c && ke->modifiers().testflag(qt::controlmodifier))
        {
            qmodelindex i = this->transactionview->currentindex();
            if (i.isvalid() && i.column() == transactiontablemodel::amount)
            {
                 guiutil::setclipboard(i.data(transactiontablemodel::formattedamountrole).tostring());
                 return true;
            }
        }
    }
    return qwidget::eventfilter(obj, event);
}

// show/hide column watch-only
void transactionview::updatewatchonlycolumn(bool fhavewatchonly)
{
    watchonlywidget->setvisible(fhavewatchonly);
    transactionview->setcolumnhidden(transactiontablemodel::watchonly, !fhavewatchonly);
}
