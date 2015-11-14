// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "moorecoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "scicon.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "walletmodel.h"

#include <qabstractitemdelegate>
#include <qpainter>

#define decoration_size 54
#define num_items 5

class txviewdelegate : public qabstractitemdelegate
{
    q_object
public:
    txviewdelegate(): qabstractitemdelegate(), unit(moorecoinunits::btc)
    {

    }

    inline void paint(qpainter *painter, const qstyleoptionviewitem &option,
                      const qmodelindex &index ) const
    {
        painter->save();

        qicon icon = qvariant_cast<qicon>(index.data(transactiontablemodel::rawdecorationrole));
        qrect mainrect = option.rect;
        qrect decorationrect(mainrect.topleft(), qsize(decoration_size, decoration_size));
        int xspace = decoration_size + 8;
        int ypad = 6;
        int halfheight = (mainrect.height() - 2*ypad)/2;
        qrect amountrect(mainrect.left() + xspace, mainrect.top()+ypad, mainrect.width() - xspace, halfheight);
        qrect addressrect(mainrect.left() + xspace, mainrect.top()+ypad+halfheight, mainrect.width() - xspace, halfheight);
        icon = singlecoloricon(icon, singlecolor());
        icon.paint(painter, decorationrect);

        qdatetime date = index.data(transactiontablemodel::daterole).todatetime();
        qstring address = index.data(qt::displayrole).tostring();
        qint64 amount = index.data(transactiontablemodel::amountrole).tolonglong();
        bool confirmed = index.data(transactiontablemodel::confirmedrole).tobool();
        qvariant value = index.data(qt::foregroundrole);
        qcolor foreground = option.palette.color(qpalette::text);
        if(value.canconvert<qbrush>())
        {
            qbrush brush = qvariant_cast<qbrush>(value);
            foreground = brush.color();
        }

        painter->setpen(foreground);
        qrect boundingrect;
        painter->drawtext(addressrect, qt::alignleft|qt::alignvcenter, address, &boundingrect);

        if (index.data(transactiontablemodel::watchonlyrole).tobool())
        {
            qicon iconwatchonly = qvariant_cast<qicon>(index.data(transactiontablemodel::watchonlydecorationrole));
            qrect watchonlyrect(boundingrect.right() + 5, mainrect.top()+ypad+halfheight, 16, halfheight);
            iconwatchonly.paint(painter, watchonlyrect);
        }

        if(amount < 0)
        {
            foreground = color_negative;
        }
        else if(!confirmed)
        {
            foreground = color_unconfirmed;
        }
        else
        {
            foreground = option.palette.color(qpalette::text);
        }
        painter->setpen(foreground);
        qstring amounttext = moorecoinunits::formatwithunit(unit, amount, true, moorecoinunits::separatoralways);
        if(!confirmed)
        {
            amounttext = qstring("[") + amounttext + qstring("]");
        }
        painter->drawtext(amountrect, qt::alignright|qt::alignvcenter, amounttext);

        painter->setpen(option.palette.color(qpalette::text));
        painter->drawtext(amountrect, qt::alignleft|qt::alignvcenter, guiutil::datetimestr(date));

        painter->restore();
    }

    inline qsize sizehint(const qstyleoptionviewitem &option, const qmodelindex &index) const
    {
        return qsize(decoration_size, decoration_size);
    }

    int unit;

};
#include "overviewpage.moc"

overviewpage::overviewpage(qwidget *parent) :
    qwidget(parent),
    ui(new ui::overviewpage),
    clientmodel(0),
    walletmodel(0),
    currentbalance(-1),
    currentunconfirmedbalance(-1),
    currentimmaturebalance(-1),
    currentwatchonlybalance(-1),
    currentwatchunconfbalance(-1),
    currentwatchimmaturebalance(-1),
    txdelegate(new txviewdelegate()),
    filter(0)
{
    ui->setupui(this);

    // use a singlecoloricon for the "out of sync warning" icon
    qicon icon = singlecoloricon(":/icons/warning");
    icon.addpixmap(icon.pixmap(qsize(64,64), qicon::normal), qicon::disabled); // also set the disabled icon because we are using a disabled qpushbutton to work around missing hidpi support of qlabel (https://bugreports.qt.io/browse/qtbug-42503)
    ui->labeltransactionsstatus->seticon(icon);
    ui->labelwalletstatus->seticon(icon);

    // recent transactions
    ui->listtransactions->setitemdelegate(txdelegate);
    ui->listtransactions->seticonsize(qsize(decoration_size, decoration_size));
    ui->listtransactions->setminimumheight(num_items * (decoration_size + 2));
    ui->listtransactions->setattribute(qt::wa_macshowfocusrect, false);

    connect(ui->listtransactions, signal(clicked(qmodelindex)), this, slot(handletransactionclicked(qmodelindex)));

    // start with displaying the "out of sync" warnings
    showoutofsyncwarning(true);
}

void overviewpage::handletransactionclicked(const qmodelindex &index)
{
    if(filter)
        emit transactionclicked(filter->maptosource(index));
}

overviewpage::~overviewpage()
{
    delete ui;
}

void overviewpage::setbalance(const camount& balance, const camount& unconfirmedbalance, const camount& immaturebalance, const camount& watchonlybalance, const camount& watchunconfbalance, const camount& watchimmaturebalance)
{
    int unit = walletmodel->getoptionsmodel()->getdisplayunit();
    currentbalance = balance;
    currentunconfirmedbalance = unconfirmedbalance;
    currentimmaturebalance = immaturebalance;
    currentwatchonlybalance = watchonlybalance;
    currentwatchunconfbalance = watchunconfbalance;
    currentwatchimmaturebalance = watchimmaturebalance;
    ui->labelbalance->settext(moorecoinunits::formatwithunit(unit, balance, false, moorecoinunits::separatoralways));
    ui->labelunconfirmed->settext(moorecoinunits::formatwithunit(unit, unconfirmedbalance, false, moorecoinunits::separatoralways));
    ui->labelimmature->settext(moorecoinunits::formatwithunit(unit, immaturebalance, false, moorecoinunits::separatoralways));
    ui->labeltotal->settext(moorecoinunits::formatwithunit(unit, balance + unconfirmedbalance + immaturebalance, false, moorecoinunits::separatoralways));
    ui->labelwatchavailable->settext(moorecoinunits::formatwithunit(unit, watchonlybalance, false, moorecoinunits::separatoralways));
    ui->labelwatchpending->settext(moorecoinunits::formatwithunit(unit, watchunconfbalance, false, moorecoinunits::separatoralways));
    ui->labelwatchimmature->settext(moorecoinunits::formatwithunit(unit, watchimmaturebalance, false, moorecoinunits::separatoralways));
    ui->labelwatchtotal->settext(moorecoinunits::formatwithunit(unit, watchonlybalance + watchunconfbalance + watchimmaturebalance, false, moorecoinunits::separatoralways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showimmature = immaturebalance != 0;
    bool showwatchonlyimmature = watchimmaturebalance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelimmature->setvisible(showimmature || showwatchonlyimmature);
    ui->labelimmaturetext->setvisible(showimmature || showwatchonlyimmature);
    ui->labelwatchimmature->setvisible(showwatchonlyimmature); // show watch-only immature balance
}

// show/hide watch-only labels
void overviewpage::updatewatchonlylabels(bool showwatchonly)
{
    ui->labelspendable->setvisible(showwatchonly);      // show spendable label (only when watch-only is active)
    ui->labelwatchonly->setvisible(showwatchonly);      // show watch-only label
    ui->linewatchbalance->setvisible(showwatchonly);    // show watch-only balance separator line
    ui->labelwatchavailable->setvisible(showwatchonly); // show watch-only available balance
    ui->labelwatchpending->setvisible(showwatchonly);   // show watch-only pending balance
    ui->labelwatchtotal->setvisible(showwatchonly);     // show watch-only total balance

    if (!showwatchonly)
        ui->labelwatchimmature->hide();
}

void overviewpage::setclientmodel(clientmodel *model)
{
    this->clientmodel = model;
    if(model)
    {
        // show warning if this is a prerelease version
        connect(model, signal(alertschanged(qstring)), this, slot(updatealerts(qstring)));
        updatealerts(model->getstatusbarwarnings());
    }
}

void overviewpage::setwalletmodel(walletmodel *model)
{
    this->walletmodel = model;
    if(model && model->getoptionsmodel())
    {
        // set up transaction list
        filter = new transactionfilterproxy();
        filter->setsourcemodel(model->gettransactiontablemodel());
        filter->setlimit(num_items);
        filter->setdynamicsortfilter(true);
        filter->setsortrole(qt::editrole);
        filter->setshowinactive(false);
        filter->sort(transactiontablemodel::status, qt::descendingorder);

        ui->listtransactions->setmodel(filter);
        ui->listtransactions->setmodelcolumn(transactiontablemodel::toaddress);

        // keep up to date with wallet
        setbalance(model->getbalance(), model->getunconfirmedbalance(), model->getimmaturebalance(),
                   model->getwatchbalance(), model->getwatchunconfirmedbalance(), model->getwatchimmaturebalance());
        connect(model, signal(balancechanged(camount,camount,camount,camount,camount,camount)), this, slot(setbalance(camount,camount,camount,camount,camount,camount)));

        connect(model->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(updatedisplayunit()));

        updatewatchonlylabels(model->havewatchonly());
        connect(model, signal(notifywatchonlychanged(bool)), this, slot(updatewatchonlylabels(bool)));
    }

    // update the display unit, to not use the default ("btc")
    updatedisplayunit();
}

void overviewpage::updatedisplayunit()
{
    if(walletmodel && walletmodel->getoptionsmodel())
    {
        if(currentbalance != -1)
            setbalance(currentbalance, currentunconfirmedbalance, currentimmaturebalance,
                       currentwatchonlybalance, currentwatchunconfbalance, currentwatchimmaturebalance);

        // update txdelegate->unit with the current unit
        txdelegate->unit = walletmodel->getoptionsmodel()->getdisplayunit();

        ui->listtransactions->update();
    }
}

void overviewpage::updatealerts(const qstring &warnings)
{
    this->ui->labelalerts->setvisible(!warnings.isempty());
    this->ui->labelalerts->settext(warnings);
}

void overviewpage::showoutofsyncwarning(bool fshow)
{
    ui->labelwalletstatus->setvisible(fshow);
    ui->labeltransactionsstatus->setvisible(fshow);
}
