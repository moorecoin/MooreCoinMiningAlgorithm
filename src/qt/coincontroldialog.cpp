// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "coincontroldialog.h"
#include "ui_coincontroldialog.h"

#include "addresstablemodel.h"
#include "moorecoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "scicon.h"
#include "walletmodel.h"

#include "coincontrol.h"
#include "init.h"
#include "main.h"
#include "wallet/wallet.h"

#include <boost/assign/list_of.hpp> // for 'map_list_of()'

#include <qapplication>
#include <qcheckbox>
#include <qcursor>
#include <qdialogbuttonbox>
#include <qflags>
#include <qicon>
#include <qsettings>
#include <qstring>
#include <qtreewidget>
#include <qtreewidgetitem>

using namespace std;
qlist<camount> coincontroldialog::payamounts;
ccoincontrol* coincontroldialog::coincontrol = new ccoincontrol();
bool coincontroldialog::fsubtractfeefromamount = false;

coincontroldialog::coincontroldialog(qwidget *parent) :
    qdialog(parent),
    ui(new ui::coincontroldialog),
    model(0)
{
    ui->setupui(this);

    // context menu actions
    qaction *copyaddressaction = new qaction(tr("copy address"), this);
    qaction *copylabelaction = new qaction(tr("copy label"), this);
    qaction *copyamountaction = new qaction(tr("copy amount"), this);
             copytransactionhashaction = new qaction(tr("copy transaction id"), this);  // we need to enable/disable this
             lockaction = new qaction(tr("lock unspent"), this);                        // we need to enable/disable this
             unlockaction = new qaction(tr("unlock unspent"), this);                    // we need to enable/disable this

    // context menu
    contextmenu = new qmenu();
    contextmenu->addaction(copyaddressaction);
    contextmenu->addaction(copylabelaction);
    contextmenu->addaction(copyamountaction);
    contextmenu->addaction(copytransactionhashaction);
    contextmenu->addseparator();
    contextmenu->addaction(lockaction);
    contextmenu->addaction(unlockaction);

    // context menu signals
    connect(ui->treewidget, signal(customcontextmenurequested(qpoint)), this, slot(showmenu(qpoint)));
    connect(copyaddressaction, signal(triggered()), this, slot(copyaddress()));
    connect(copylabelaction, signal(triggered()), this, slot(copylabel()));
    connect(copyamountaction, signal(triggered()), this, slot(copyamount()));
    connect(copytransactionhashaction, signal(triggered()), this, slot(copytransactionhash()));
    connect(lockaction, signal(triggered()), this, slot(lockcoin()));
    connect(unlockaction, signal(triggered()), this, slot(unlockcoin()));

    // clipboard actions
    qaction *clipboardquantityaction = new qaction(tr("copy quantity"), this);
    qaction *clipboardamountaction = new qaction(tr("copy amount"), this);
    qaction *clipboardfeeaction = new qaction(tr("copy fee"), this);
    qaction *clipboardafterfeeaction = new qaction(tr("copy after fee"), this);
    qaction *clipboardbytesaction = new qaction(tr("copy bytes"), this);
    qaction *clipboardpriorityaction = new qaction(tr("copy priority"), this);
    qaction *clipboardlowoutputaction = new qaction(tr("copy dust"), this);
    qaction *clipboardchangeaction = new qaction(tr("copy change"), this);

    connect(clipboardquantityaction, signal(triggered()), this, slot(clipboardquantity()));
    connect(clipboardamountaction, signal(triggered()), this, slot(clipboardamount()));
    connect(clipboardfeeaction, signal(triggered()), this, slot(clipboardfee()));
    connect(clipboardafterfeeaction, signal(triggered()), this, slot(clipboardafterfee()));
    connect(clipboardbytesaction, signal(triggered()), this, slot(clipboardbytes()));
    connect(clipboardpriorityaction, signal(triggered()), this, slot(clipboardpriority()));
    connect(clipboardlowoutputaction, signal(triggered()), this, slot(clipboardlowoutput()));
    connect(clipboardchangeaction, signal(triggered()), this, slot(clipboardchange()));

    ui->labelcoincontrolquantity->addaction(clipboardquantityaction);
    ui->labelcoincontrolamount->addaction(clipboardamountaction);
    ui->labelcoincontrolfee->addaction(clipboardfeeaction);
    ui->labelcoincontrolafterfee->addaction(clipboardafterfeeaction);
    ui->labelcoincontrolbytes->addaction(clipboardbytesaction);
    ui->labelcoincontrolpriority->addaction(clipboardpriorityaction);
    ui->labelcoincontrollowoutput->addaction(clipboardlowoutputaction);
    ui->labelcoincontrolchange->addaction(clipboardchangeaction);

    // toggle tree/list mode
    connect(ui->radiotreemode, signal(toggled(bool)), this, slot(radiotreemode(bool)));
    connect(ui->radiolistmode, signal(toggled(bool)), this, slot(radiolistmode(bool)));

    // click on checkbox
    connect(ui->treewidget, signal(itemchanged(qtreewidgetitem*, int)), this, slot(viewitemchanged(qtreewidgetitem*, int)));

    // click on header
#if qt_version < 0x050000
    ui->treewidget->header()->setclickable(true);
#else
    ui->treewidget->header()->setsectionsclickable(true);
#endif
    connect(ui->treewidget->header(), signal(sectionclicked(int)), this, slot(headersectionclicked(int)));

    // ok button
    connect(ui->buttonbox, signal(clicked( qabstractbutton*)), this, slot(buttonboxclicked(qabstractbutton*)));

    // (un)select all
    connect(ui->pushbuttonselectall, signal(clicked()), this, slot(buttonselectallclicked()));

    // change coin control first column label due qt4 bug. 
    // see https://github.com/moorecoin/moorecoin/issues/5716
    ui->treewidget->headeritem()->settext(column_checkbox, qstring());

    ui->treewidget->setcolumnwidth(column_checkbox, 84);
    ui->treewidget->setcolumnwidth(column_amount, 100);
    ui->treewidget->setcolumnwidth(column_label, 170);
    ui->treewidget->setcolumnwidth(column_address, 290);
    ui->treewidget->setcolumnwidth(column_date, 110);
    ui->treewidget->setcolumnwidth(column_confirmations, 100);
    ui->treewidget->setcolumnwidth(column_priority, 100);
    ui->treewidget->setcolumnhidden(column_txhash, true);         // store transacton hash in this column, but don't show it
    ui->treewidget->setcolumnhidden(column_vout_index, true);     // store vout index in this column, but don't show it
    ui->treewidget->setcolumnhidden(column_amount_int64, true);   // store amount int64 in this column, but don't show it
    ui->treewidget->setcolumnhidden(column_priority_int64, true); // store priority int64 in this column, but don't show it
    ui->treewidget->setcolumnhidden(column_date_int64, true);     // store date int64 in this column, but don't show it

    // default view is sorted by amount desc
    sortview(column_amount_int64, qt::descendingorder);

    // restore list mode and sortorder as a convenience feature
    qsettings settings;
    if (settings.contains("ncoincontrolmode") && !settings.value("ncoincontrolmode").tobool())
        ui->radiotreemode->click();
    if (settings.contains("ncoincontrolsortcolumn") && settings.contains("ncoincontrolsortorder"))
        sortview(settings.value("ncoincontrolsortcolumn").toint(), ((qt::sortorder)settings.value("ncoincontrolsortorder").toint()));
}

coincontroldialog::~coincontroldialog()
{
    qsettings settings;
    settings.setvalue("ncoincontrolmode", ui->radiolistmode->ischecked());
    settings.setvalue("ncoincontrolsortcolumn", sortcolumn);
    settings.setvalue("ncoincontrolsortorder", (int)sortorder);

    delete ui;
}

void coincontroldialog::setmodel(walletmodel *model)
{
    this->model = model;

    if(model && model->getoptionsmodel() && model->getaddresstablemodel())
    {
        updateview();
        updatelabellocked();
        coincontroldialog::updatelabels(model, this);
    }
}

// helper function str_pad
qstring coincontroldialog::strpad(qstring s, int npadlength, qstring spadding)
{
    while (s.length() < npadlength)
        s = spadding + s;

    return s;
}

// ok button
void coincontroldialog::buttonboxclicked(qabstractbutton* button)
{
    if (ui->buttonbox->buttonrole(button) == qdialogbuttonbox::acceptrole)
        done(qdialog::accepted); // closes the dialog
}

// (un)select all
void coincontroldialog::buttonselectallclicked()
{
    qt::checkstate state = qt::checked;
    for (int i = 0; i < ui->treewidget->toplevelitemcount(); i++)
    {
        if (ui->treewidget->toplevelitem(i)->checkstate(column_checkbox) != qt::unchecked)
        {
            state = qt::unchecked;
            break;
        }
    }
    ui->treewidget->setenabled(false);
    for (int i = 0; i < ui->treewidget->toplevelitemcount(); i++)
            if (ui->treewidget->toplevelitem(i)->checkstate(column_checkbox) != state)
                ui->treewidget->toplevelitem(i)->setcheckstate(column_checkbox, state);
    ui->treewidget->setenabled(true);
    if (state == qt::unchecked)
        coincontrol->unselectall(); // just to be sure
    coincontroldialog::updatelabels(model, this);
}

// context menu
void coincontroldialog::showmenu(const qpoint &point)
{
    qtreewidgetitem *item = ui->treewidget->itemat(point);
    if(item)
    {
        contextmenuitem = item;

        // disable some items (like copy transaction id, lock, unlock) for tree roots in context menu
        if (item->text(column_txhash).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
        {
            copytransactionhashaction->setenabled(true);
            if (model->islockedcoin(uint256s(item->text(column_txhash).tostdstring()), item->text(column_vout_index).touint()))
            {
                lockaction->setenabled(false);
                unlockaction->setenabled(true);
            }
            else
            {
                lockaction->setenabled(true);
                unlockaction->setenabled(false);
            }
        }
        else // this means click on parent node in tree mode -> disable all
        {
            copytransactionhashaction->setenabled(false);
            lockaction->setenabled(false);
            unlockaction->setenabled(false);
        }

        // show context menu
        contextmenu->exec(qcursor::pos());
    }
}

// context menu action: copy amount
void coincontroldialog::copyamount()
{
    guiutil::setclipboard(moorecoinunits::removespaces(contextmenuitem->text(column_amount)));
}

// context menu action: copy label
void coincontroldialog::copylabel()
{
    if (ui->radiotreemode->ischecked() && contextmenuitem->text(column_label).length() == 0 && contextmenuitem->parent())
        guiutil::setclipboard(contextmenuitem->parent()->text(column_label));
    else
        guiutil::setclipboard(contextmenuitem->text(column_label));
}

// context menu action: copy address
void coincontroldialog::copyaddress()
{
    if (ui->radiotreemode->ischecked() && contextmenuitem->text(column_address).length() == 0 && contextmenuitem->parent())
        guiutil::setclipboard(contextmenuitem->parent()->text(column_address));
    else
        guiutil::setclipboard(contextmenuitem->text(column_address));
}

// context menu action: copy transaction id
void coincontroldialog::copytransactionhash()
{
    guiutil::setclipboard(contextmenuitem->text(column_txhash));
}

// context menu action: lock coin
void coincontroldialog::lockcoin()
{
    if (contextmenuitem->checkstate(column_checkbox) == qt::checked)
        contextmenuitem->setcheckstate(column_checkbox, qt::unchecked);

    coutpoint outpt(uint256s(contextmenuitem->text(column_txhash).tostdstring()), contextmenuitem->text(column_vout_index).touint());
    model->lockcoin(outpt);
    contextmenuitem->setdisabled(true);
    contextmenuitem->seticon(column_checkbox, singlecoloricon(":/icons/lock_closed"));
    updatelabellocked();
}

// context menu action: unlock coin
void coincontroldialog::unlockcoin()
{
    coutpoint outpt(uint256s(contextmenuitem->text(column_txhash).tostdstring()), contextmenuitem->text(column_vout_index).touint());
    model->unlockcoin(outpt);
    contextmenuitem->setdisabled(false);
    contextmenuitem->seticon(column_checkbox, qicon());
    updatelabellocked();
}

// copy label "quantity" to clipboard
void coincontroldialog::clipboardquantity()
{
    guiutil::setclipboard(ui->labelcoincontrolquantity->text());
}

// copy label "amount" to clipboard
void coincontroldialog::clipboardamount()
{
    guiutil::setclipboard(ui->labelcoincontrolamount->text().left(ui->labelcoincontrolamount->text().indexof(" ")));
}

// copy label "fee" to clipboard
void coincontroldialog::clipboardfee()
{
    guiutil::setclipboard(ui->labelcoincontrolfee->text().left(ui->labelcoincontrolfee->text().indexof(" ")).replace(asymp_utf8, ""));
}

// copy label "after fee" to clipboard
void coincontroldialog::clipboardafterfee()
{
    guiutil::setclipboard(ui->labelcoincontrolafterfee->text().left(ui->labelcoincontrolafterfee->text().indexof(" ")).replace(asymp_utf8, ""));
}

// copy label "bytes" to clipboard
void coincontroldialog::clipboardbytes()
{
    guiutil::setclipboard(ui->labelcoincontrolbytes->text().replace(asymp_utf8, ""));
}

// copy label "priority" to clipboard
void coincontroldialog::clipboardpriority()
{
    guiutil::setclipboard(ui->labelcoincontrolpriority->text());
}

// copy label "dust" to clipboard
void coincontroldialog::clipboardlowoutput()
{
    guiutil::setclipboard(ui->labelcoincontrollowoutput->text());
}

// copy label "change" to clipboard
void coincontroldialog::clipboardchange()
{
    guiutil::setclipboard(ui->labelcoincontrolchange->text().left(ui->labelcoincontrolchange->text().indexof(" ")).replace(asymp_utf8, ""));
}

// treeview: sort
void coincontroldialog::sortview(int column, qt::sortorder order)
{
    sortcolumn = column;
    sortorder = order;
    ui->treewidget->sortitems(column, order);
    ui->treewidget->header()->setsortindicator(getmappedcolumn(sortcolumn), sortorder);
}

// treeview: clicked on header
void coincontroldialog::headersectionclicked(int logicalindex)
{
    if (logicalindex == column_checkbox) // click on most left column -> do nothing
    {
        ui->treewidget->header()->setsortindicator(getmappedcolumn(sortcolumn), sortorder);
    }
    else
    {
        logicalindex = getmappedcolumn(logicalindex, false);

        if (sortcolumn == logicalindex)
            sortorder = ((sortorder == qt::ascendingorder) ? qt::descendingorder : qt::ascendingorder);
        else
        {
            sortcolumn = logicalindex;
            sortorder = ((sortcolumn == column_label || sortcolumn == column_address) ? qt::ascendingorder : qt::descendingorder); // if label or address then default => asc, else default => desc
        }

        sortview(sortcolumn, sortorder);
    }
}

// toggle tree mode
void coincontroldialog::radiotreemode(bool checked)
{
    if (checked && model)
        updateview();
}

// toggle list mode
void coincontroldialog::radiolistmode(bool checked)
{
    if (checked && model)
        updateview();
}

// checkbox clicked by user
void coincontroldialog::viewitemchanged(qtreewidgetitem* item, int column)
{
    if (column == column_checkbox && item->text(column_txhash).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
    {
        coutpoint outpt(uint256s(item->text(column_txhash).tostdstring()), item->text(column_vout_index).touint());

        if (item->checkstate(column_checkbox) == qt::unchecked)
            coincontrol->unselect(outpt);
        else if (item->isdisabled()) // locked (this happens if "check all" through parent node)
            item->setcheckstate(column_checkbox, qt::unchecked);
        else
            coincontrol->select(outpt);

        // selection changed -> update labels
        if (ui->treewidget->isenabled()) // do not update on every click for (un)select all
            coincontroldialog::updatelabels(model, this);
    }

    // todo: this is a temporary qt5 fix: when clicking a parent node in tree mode, the parent node
    //       including all children are partially selected. but the parent node should be fully selected
    //       as well as the children. children should never be partially selected in the first place.
    //       please remove this ugly fix, once the bug is solved upstream.
#if qt_version >= 0x050000
    else if (column == column_checkbox && item->childcount() > 0)
    {
        if (item->checkstate(column_checkbox) == qt::partiallychecked && item->child(0)->checkstate(column_checkbox) == qt::partiallychecked)
            item->setcheckstate(column_checkbox, qt::checked);
    }
#endif
}

// return human readable label for priority number
qstring coincontroldialog::getprioritylabel(double dpriority, double mempoolestimatepriority)
{
    double dprioritymedium = mempoolestimatepriority;

    if (dprioritymedium <= 0)
        dprioritymedium = allowfreethreshold(); // not enough data, back to hard-coded

    if      (dpriority / 1000000 > dprioritymedium) return tr("highest");
    else if (dpriority / 100000 > dprioritymedium)  return tr("higher");
    else if (dpriority / 10000 > dprioritymedium)   return tr("high");
    else if (dpriority / 1000 > dprioritymedium)    return tr("medium-high");
    else if (dpriority > dprioritymedium)           return tr("medium");
    else if (dpriority * 10 > dprioritymedium)      return tr("low-medium");
    else if (dpriority * 100 > dprioritymedium)     return tr("low");
    else if (dpriority * 1000 > dprioritymedium)    return tr("lower");
    else                                            return tr("lowest");
}

// shows count of locked unspent outputs
void coincontroldialog::updatelabellocked()
{
    vector<coutpoint> voutpts;
    model->listlockedcoins(voutpts);
    if (voutpts.size() > 0)
    {
       ui->labellocked->settext(tr("(%1 locked)").arg(voutpts.size()));
       ui->labellocked->setvisible(true);
    }
    else ui->labellocked->setvisible(false);
}

void coincontroldialog::updatelabels(walletmodel *model, qdialog* dialog)
{
    if (!model)
        return;

    // npayamount
    camount npayamount = 0;
    bool fdust = false;
    cmutabletransaction txdummy;
    foreach(const camount &amount, coincontroldialog::payamounts)
    {
        npayamount += amount;

        if (amount > 0)
        {
            ctxout txout(amount, (cscript)vector<unsigned char>(24, 0));
            txdummy.vout.push_back(txout);
            if (txout.isdust(::minrelaytxfee))
               fdust = true;
        }
    }

    qstring sprioritylabel      = tr("none");
    camount namount             = 0;
    camount npayfee             = 0;
    camount nafterfee           = 0;
    camount nchange             = 0;
    unsigned int nbytes         = 0;
    unsigned int nbytesinputs   = 0;
    double dpriority            = 0;
    double dpriorityinputs      = 0;
    unsigned int nquantity      = 0;
    int nquantityuncompressed   = 0;
    bool fallowfree             = false;

    vector<coutpoint> vcoincontrol;
    vector<coutput>   voutputs;
    coincontrol->listselected(vcoincontrol);
    model->getoutputs(vcoincontrol, voutputs);

    boost_foreach(const coutput& out, voutputs)
    {
        // unselect already spent, very unlikely scenario, this could happen
        // when selected are spent elsewhere, like rpc or another computer
        uint256 txhash = out.tx->gethash();
        coutpoint outpt(txhash, out.i);
        if (model->isspent(outpt))
        {
            coincontrol->unselect(outpt);
            continue;
        }

        // quantity
        nquantity++;

        // amount
        namount += out.tx->vout[out.i].nvalue;

        // priority
        dpriorityinputs += (double)out.tx->vout[out.i].nvalue * (out.ndepth+1);

        // bytes
        ctxdestination address;
        if(extractdestination(out.tx->vout[out.i].scriptpubkey, address))
        {
            cpubkey pubkey;
            ckeyid *keyid = boost::get<ckeyid>(&address);
            if (keyid && model->getpubkey(*keyid, pubkey))
            {
                nbytesinputs += (pubkey.iscompressed() ? 148 : 180);
                if (!pubkey.iscompressed())
                    nquantityuncompressed++;
            }
            else
                nbytesinputs += 148; // in all error cases, simply assume 148 here
        }
        else nbytesinputs += 148;
    }

    // calculation
    if (nquantity > 0)
    {
        // bytes
        nbytes = nbytesinputs + ((coincontroldialog::payamounts.size() > 0 ? coincontroldialog::payamounts.size() + 1 : 2) * 34) + 10; // always assume +1 output for change here

        // priority
        double mempoolestimatepriority = mempool.estimatepriority(ntxconfirmtarget);
        dpriority = dpriorityinputs / (nbytes - nbytesinputs + (nquantityuncompressed * 29)); // 29 = 180 - 151 (uncompressed public keys are over the limit. max 151 bytes of the input are ignored for priority)
        sprioritylabel = coincontroldialog::getprioritylabel(dpriority, mempoolestimatepriority);

        // in the subtract fee from amount case, we can tell if zero change already and subtract the bytes, so that fee calculation afterwards is accurate
        if (coincontroldialog::fsubtractfeefromamount)
            if (namount - npayamount == 0)
                nbytes -= 34;

        // fee
        npayfee = cwallet::getminimumfee(nbytes, ntxconfirmtarget, mempool);

        // allow free?
        double dpriorityneeded = mempoolestimatepriority;
        if (dpriorityneeded <= 0)
            dpriorityneeded = allowfreethreshold(); // not enough data, back to hard-coded
        fallowfree = (dpriority >= dpriorityneeded);

        if (fsendfreetransactions)
            if (fallowfree && nbytes <= max_free_transaction_create_size)
                npayfee = 0;

        if (npayamount > 0)
        {
            nchange = namount - npayamount;
            if (!coincontroldialog::fsubtractfeefromamount)
                nchange -= npayfee;

            // never create dust outputs; if we would, just add the dust to the fee.
            if (nchange > 0 && nchange < cent)
            {
                ctxout txout(nchange, (cscript)vector<unsigned char>(24, 0));
                if (txout.isdust(::minrelaytxfee))
                {
                    if (coincontroldialog::fsubtractfeefromamount) // dust-change will be raised until no dust
                        nchange = txout.getdustthreshold(::minrelaytxfee);
                    else
                    {
                        npayfee += nchange;
                        nchange = 0;
                    }
                }
            }

            if (nchange == 0 && !coincontroldialog::fsubtractfeefromamount)
                nbytes -= 34;
        }

        // after fee
        nafterfee = namount - npayfee;
        if (nafterfee < 0)
            nafterfee = 0;
    }

    // actually update labels
    int ndisplayunit = moorecoinunits::btc;
    if (model && model->getoptionsmodel())
        ndisplayunit = model->getoptionsmodel()->getdisplayunit();

    qlabel *l1 = dialog->findchild<qlabel *>("labelcoincontrolquantity");
    qlabel *l2 = dialog->findchild<qlabel *>("labelcoincontrolamount");
    qlabel *l3 = dialog->findchild<qlabel *>("labelcoincontrolfee");
    qlabel *l4 = dialog->findchild<qlabel *>("labelcoincontrolafterfee");
    qlabel *l5 = dialog->findchild<qlabel *>("labelcoincontrolbytes");
    qlabel *l6 = dialog->findchild<qlabel *>("labelcoincontrolpriority");
    qlabel *l7 = dialog->findchild<qlabel *>("labelcoincontrollowoutput");
    qlabel *l8 = dialog->findchild<qlabel *>("labelcoincontrolchange");

    // enable/disable "dust" and "change"
    dialog->findchild<qlabel *>("labelcoincontrollowoutputtext")->setenabled(npayamount > 0);
    dialog->findchild<qlabel *>("labelcoincontrollowoutput")    ->setenabled(npayamount > 0);
    dialog->findchild<qlabel *>("labelcoincontrolchangetext")   ->setenabled(npayamount > 0);
    dialog->findchild<qlabel *>("labelcoincontrolchange")       ->setenabled(npayamount > 0);

    // stats
    l1->settext(qstring::number(nquantity));                                 // quantity
    l2->settext(moorecoinunits::formatwithunit(ndisplayunit, namount));        // amount
    l3->settext(moorecoinunits::formatwithunit(ndisplayunit, npayfee));        // fee
    l4->settext(moorecoinunits::formatwithunit(ndisplayunit, nafterfee));      // after fee
    l5->settext(((nbytes > 0) ? asymp_utf8 : "") + qstring::number(nbytes));        // bytes
    l6->settext(sprioritylabel);                                             // priority
    l7->settext(fdust ? tr("yes") : tr("no"));                               // dust
    l8->settext(moorecoinunits::formatwithunit(ndisplayunit, nchange));        // change
    if (npayfee > 0 && !(paytxfee.getfeeperk() > 0 && fpayatleastcustomfee && nbytes < 1000))
    {
        l3->settext(asymp_utf8 + l3->text());
        l4->settext(asymp_utf8 + l4->text());
        if (nchange > 0 && !coincontroldialog::fsubtractfeefromamount)
            l8->settext(asymp_utf8 + l8->text());
    }

    // turn labels "red"
    l5->setstylesheet((nbytes >= max_free_transaction_create_size) ? "color:red;" : "");// bytes >= 1000
    l6->setstylesheet((dpriority > 0 && !fallowfree) ? "color:red;" : "");              // priority < "medium"
    l7->setstylesheet((fdust) ? "color:red;" : "");                                     // dust = "yes"

    // tool tips
    qstring tooltip1 = tr("this label turns red if the transaction size is greater than 1000 bytes.") + "<br /><br />";
    tooltip1 += tr("this means a fee of at least %1 per kb is required.").arg(moorecoinunits::formatwithunit(ndisplayunit, cwallet::mintxfee.getfeeperk())) + "<br /><br />";
    tooltip1 += tr("can vary +/- 1 byte per input.");

    qstring tooltip2 = tr("transactions with higher priority are more likely to get included into a block.") + "<br /><br />";
    tooltip2 += tr("this label turns red if the priority is smaller than \"medium\".") + "<br /><br />";
    tooltip2 += tr("this means a fee of at least %1 per kb is required.").arg(moorecoinunits::formatwithunit(ndisplayunit, cwallet::mintxfee.getfeeperk()));

    qstring tooltip3 = tr("this label turns red if any recipient receives an amount smaller than %1.").arg(moorecoinunits::formatwithunit(ndisplayunit, ::minrelaytxfee.getfee(546)));

    // how many satoshis the estimated fee can vary per byte we guess wrong
    double dfeevary;
    if (paytxfee.getfeeperk() > 0)
        dfeevary = (double)std::max(cwallet::mintxfee.getfeeperk(), paytxfee.getfeeperk()) / 1000;
    else
        dfeevary = (double)std::max(cwallet::mintxfee.getfeeperk(), mempool.estimatefee(ntxconfirmtarget).getfeeperk()) / 1000;
    qstring tooltip4 = tr("can vary +/- %1 satoshi(s) per input.").arg(dfeevary);

    l3->settooltip(tooltip4);
    l4->settooltip(tooltip4);
    l5->settooltip(tooltip1);
    l6->settooltip(tooltip2);
    l7->settooltip(tooltip3);
    l8->settooltip(tooltip4);
    dialog->findchild<qlabel *>("labelcoincontrolfeetext")      ->settooltip(l3->tooltip());
    dialog->findchild<qlabel *>("labelcoincontrolafterfeetext") ->settooltip(l4->tooltip());
    dialog->findchild<qlabel *>("labelcoincontrolbytestext")    ->settooltip(l5->tooltip());
    dialog->findchild<qlabel *>("labelcoincontrolprioritytext") ->settooltip(l6->tooltip());
    dialog->findchild<qlabel *>("labelcoincontrollowoutputtext")->settooltip(l7->tooltip());
    dialog->findchild<qlabel *>("labelcoincontrolchangetext")   ->settooltip(l8->tooltip());

    // insufficient funds
    qlabel *label = dialog->findchild<qlabel *>("labelcoincontrolinsufffunds");
    if (label)
        label->setvisible(nchange < 0);
}

void coincontroldialog::updateview()
{
    if (!model || !model->getoptionsmodel() || !model->getaddresstablemodel())
        return;

    bool treemode = ui->radiotreemode->ischecked();

    ui->treewidget->clear();
    ui->treewidget->setenabled(false); // performance, otherwise updatelabels would be called for every checked checkbox
    ui->treewidget->setalternatingrowcolors(!treemode);
    qflags<qt::itemflag> flgcheckbox = qt::itemisselectable | qt::itemisenabled | qt::itemisusercheckable;
    qflags<qt::itemflag> flgtristate = qt::itemisselectable | qt::itemisenabled | qt::itemisusercheckable | qt::itemistristate;

    int ndisplayunit = model->getoptionsmodel()->getdisplayunit();
    double mempoolestimatepriority = mempool.estimatepriority(ntxconfirmtarget);

    map<qstring, vector<coutput> > mapcoins;
    model->listcoins(mapcoins);

    boost_foreach(pairtype(qstring, vector<coutput>) coins, mapcoins)
    {
        qtreewidgetitem *itemwalletaddress = new qtreewidgetitem();
        itemwalletaddress->setcheckstate(column_checkbox, qt::unchecked);
        qstring swalletaddress = coins.first;
        qstring swalletlabel = model->getaddresstablemodel()->labelforaddress(swalletaddress);
        if (swalletlabel.isempty())
            swalletlabel = tr("(no label)");

        if (treemode)
        {
            // wallet address
            ui->treewidget->addtoplevelitem(itemwalletaddress);

            itemwalletaddress->setflags(flgtristate);
            itemwalletaddress->setcheckstate(column_checkbox, qt::unchecked);

            // label
            itemwalletaddress->settext(column_label, swalletlabel);

            // address
            itemwalletaddress->settext(column_address, swalletaddress);
        }

        camount nsum = 0;
        double dprioritysum = 0;
        int nchildren = 0;
        int ninputsum = 0;
        boost_foreach(const coutput& out, coins.second)
        {
            int ninputsize = 0;
            nsum += out.tx->vout[out.i].nvalue;
            nchildren++;

            qtreewidgetitem *itemoutput;
            if (treemode)    itemoutput = new qtreewidgetitem(itemwalletaddress);
            else             itemoutput = new qtreewidgetitem(ui->treewidget);
            itemoutput->setflags(flgcheckbox);
            itemoutput->setcheckstate(column_checkbox,qt::unchecked);

            // address
            ctxdestination outputaddress;
            qstring saddress = "";
            if(extractdestination(out.tx->vout[out.i].scriptpubkey, outputaddress))
            {
                saddress = qstring::fromstdstring(cmoorecoinaddress(outputaddress).tostring());

                // if listmode or change => show moorecoin address. in tree mode, address is not shown again for direct wallet address outputs
                if (!treemode || (!(saddress == swalletaddress)))
                    itemoutput->settext(column_address, saddress);

                cpubkey pubkey;
                ckeyid *keyid = boost::get<ckeyid>(&outputaddress);
                if (keyid && model->getpubkey(*keyid, pubkey) && !pubkey.iscompressed())
                    ninputsize = 29; // 29 = 180 - 151 (public key is 180 bytes, priority free area is 151 bytes)
            }

            // label
            if (!(saddress == swalletaddress)) // change
            {
                // tooltip from where the change comes from
                itemoutput->settooltip(column_label, tr("change from %1 (%2)").arg(swalletlabel).arg(swalletaddress));
                itemoutput->settext(column_label, tr("(change)"));
            }
            else if (!treemode)
            {
                qstring slabel = model->getaddresstablemodel()->labelforaddress(saddress);
                if (slabel.isempty())
                    slabel = tr("(no label)");
                itemoutput->settext(column_label, slabel);
            }

            // amount
            itemoutput->settext(column_amount, moorecoinunits::format(ndisplayunit, out.tx->vout[out.i].nvalue));
            itemoutput->settext(column_amount_int64, strpad(qstring::number(out.tx->vout[out.i].nvalue), 15, " ")); // padding so that sorting works correctly

            // date
            itemoutput->settext(column_date, guiutil::datetimestr(out.tx->gettxtime()));
            itemoutput->settext(column_date_int64, strpad(qstring::number(out.tx->gettxtime()), 20, " "));

            // confirmations
            itemoutput->settext(column_confirmations, strpad(qstring::number(out.ndepth), 8, " "));

            // priority
            double dpriority = ((double)out.tx->vout[out.i].nvalue  / (ninputsize + 78)) * (out.ndepth+1); // 78 = 2 * 34 + 10
            itemoutput->settext(column_priority, coincontroldialog::getprioritylabel(dpriority, mempoolestimatepriority));
            itemoutput->settext(column_priority_int64, strpad(qstring::number((int64_t)dpriority), 20, " "));
            dprioritysum += (double)out.tx->vout[out.i].nvalue  * (out.ndepth+1);
            ninputsum    += ninputsize;

            // transaction hash
            uint256 txhash = out.tx->gethash();
            itemoutput->settext(column_txhash, qstring::fromstdstring(txhash.gethex()));

            // vout index
            itemoutput->settext(column_vout_index, qstring::number(out.i));

             // disable locked coins
            if (model->islockedcoin(txhash, out.i))
            {
                coutpoint outpt(txhash, out.i);
                coincontrol->unselect(outpt); // just to be sure
                itemoutput->setdisabled(true);
                itemoutput->seticon(column_checkbox, singlecoloricon(":/icons/lock_closed"));
            }

            // set checkbox
            if (coincontrol->isselected(txhash, out.i))
                itemoutput->setcheckstate(column_checkbox, qt::checked);
        }

        // amount
        if (treemode)
        {
            dprioritysum = dprioritysum / (ninputsum + 78);
            itemwalletaddress->settext(column_checkbox, "(" + qstring::number(nchildren) + ")");
            itemwalletaddress->settext(column_amount, moorecoinunits::format(ndisplayunit, nsum));
            itemwalletaddress->settext(column_amount_int64, strpad(qstring::number(nsum), 15, " "));
            itemwalletaddress->settext(column_priority, coincontroldialog::getprioritylabel(dprioritysum, mempoolestimatepriority));
            itemwalletaddress->settext(column_priority_int64, strpad(qstring::number((int64_t)dprioritysum), 20, " "));
        }
    }

    // expand all partially selected
    if (treemode)
    {
        for (int i = 0; i < ui->treewidget->toplevelitemcount(); i++)
            if (ui->treewidget->toplevelitem(i)->checkstate(column_checkbox) == qt::partiallychecked)
                ui->treewidget->toplevelitem(i)->setexpanded(true);
    }

    // sort view
    sortview(sortcolumn, sortorder);
    ui->treewidget->setenabled(true);
}
