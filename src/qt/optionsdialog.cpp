// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "moorecoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "main.h" // for max_scriptcheck_threads
#include "netbase.h"
#include "txdb.h" // for -dbcache defaults

#ifdef enable_wallet
#include "wallet/wallet.h" // for cwallet::mintxfee
#endif

#include <boost/thread.hpp>

#include <qdatawidgetmapper>
#include <qdir>
#include <qintvalidator>
#include <qlocale>
#include <qmessagebox>
#include <qtimer>

optionsdialog::optionsdialog(qwidget *parent, bool enablewallet) :
    qdialog(parent),
    ui(new ui::optionsdialog),
    model(0),
    mapper(0),
    fproxyipvalid(true)
{
    ui->setupui(this);

    /* main elements init */
    ui->databasecache->setminimum(nmindbcache);
    ui->databasecache->setmaximum(nmaxdbcache);
    ui->threadsscriptverif->setminimum(-(int)boost::thread::hardware_concurrency());
    ui->threadsscriptverif->setmaximum(max_scriptcheck_threads);

    /* network elements init */
#ifndef use_upnp
    ui->mapportupnp->setenabled(false);
#endif

    ui->proxyip->setenabled(false);
    ui->proxyport->setenabled(false);
    ui->proxyport->setvalidator(new qintvalidator(1, 65535, this));

    connect(ui->connectsocks, signal(toggled(bool)), ui->proxyip, slot(setenabled(bool)));
    connect(ui->connectsocks, signal(toggled(bool)), ui->proxyport, slot(setenabled(bool)));

    ui->proxyip->installeventfilter(this);

    /* window elements init */
#ifdef q_os_mac
    /* remove window tab on mac */
    ui->tabwidget->removetab(ui->tabwidget->indexof(ui->tabwindow));
#endif

    /* remove wallet tab in case of -disablewallet */
    if (!enablewallet) {
        ui->tabwidget->removetab(ui->tabwidget->indexof(ui->tabwallet));
    }

    /* display elements init */
    qdir translations(":translations");
    ui->lang->additem(qstring("(") + tr("default") + qstring(")"), qvariant(""));
    foreach(const qstring &langstr, translations.entrylist())
    {
        qlocale locale(langstr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langstr.contains("_"))
        {
#if qt_version >= 0x040800
            /** display language strings as "native language - native country (locale name)", e.g. "deutsch - deutschland (de)" */
            ui->lang->additem(locale.nativelanguagename() + qstring(" - ") + locale.nativecountryname() + qstring(" (") + langstr + qstring(")"), qvariant(langstr));
#else
            /** display language strings as "language - country (locale name)", e.g. "german - germany (de)" */
            ui->lang->additem(qlocale::languagetostring(locale.language()) + qstring(" - ") + qlocale::countrytostring(locale.country()) + qstring(" (") + langstr + qstring(")"), qvariant(langstr));
#endif
        }
        else
        {
#if qt_version >= 0x040800
            /** display language strings as "native language (locale name)", e.g. "deutsch (de)" */
            ui->lang->additem(locale.nativelanguagename() + qstring(" (") + langstr + qstring(")"), qvariant(langstr));
#else
            /** display language strings as "language (locale name)", e.g. "german (de)" */
            ui->lang->additem(qlocale::languagetostring(locale.language()) + qstring(" (") + langstr + qstring(")"), qvariant(langstr));
#endif
        }
    }
#if qt_version >= 0x040700
    ui->thirdpartytxurls->setplaceholdertext("https://example.com/tx/%s");
#endif

    ui->unit->setmodel(new moorecoinunits(this));

    /* widget-to-option mapper */
    mapper = new qdatawidgetmapper(this);
    mapper->setsubmitpolicy(qdatawidgetmapper::manualsubmit);
    mapper->setorientation(qt::vertical);

    /* setup/change ui elements when proxy ip is invalid/valid */
    connect(this, signal(proxyipchecks(qvalidatedlineedit *, int)), this, slot(doproxyipchecks(qvalidatedlineedit *, int)));
}

optionsdialog::~optionsdialog()
{
    delete ui;
}

void optionsdialog::setmodel(optionsmodel *model)
{
    this->model = model;

    if(model)
    {
        /* check if client restart is needed and show persistent message */
        if (model->isrestartrequired())
            showrestartwarning(true);

        qstring strlabel = model->getoverriddenbycommandline();
        if (strlabel.isempty())
            strlabel = tr("none");
        ui->overriddenbycommandlinelabel->settext(strlabel);

        mapper->setmodel(model);
        setmapper();
        mapper->tofirst();
    }

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* main */
    connect(ui->databasecache, signal(valuechanged(int)), this, slot(showrestartwarning()));
    connect(ui->threadsscriptverif, signal(valuechanged(int)), this, slot(showrestartwarning()));
    /* wallet */
    connect(ui->spendzeroconfchange, signal(clicked(bool)), this, slot(showrestartwarning()));
    /* network */
    connect(ui->allowincoming, signal(clicked(bool)), this, slot(showrestartwarning()));
    connect(ui->connectsocks, signal(clicked(bool)), this, slot(showrestartwarning()));
    /* display */
    connect(ui->lang, signal(valuechanged()), this, slot(showrestartwarning()));
    connect(ui->thirdpartytxurls, signal(textchanged(const qstring &)), this, slot(showrestartwarning()));
}

void optionsdialog::setmapper()
{
    /* main */
    mapper->addmapping(ui->moorecoinatstartup, optionsmodel::startatstartup);
    mapper->addmapping(ui->threadsscriptverif, optionsmodel::threadsscriptverif);
    mapper->addmapping(ui->databasecache, optionsmodel::databasecache);

    /* wallet */
    mapper->addmapping(ui->spendzeroconfchange, optionsmodel::spendzeroconfchange);
    mapper->addmapping(ui->coincontrolfeatures, optionsmodel::coincontrolfeatures);

    /* network */
    mapper->addmapping(ui->mapportupnp, optionsmodel::mapportupnp);
    mapper->addmapping(ui->allowincoming, optionsmodel::listen);

    mapper->addmapping(ui->connectsocks, optionsmodel::proxyuse);
    mapper->addmapping(ui->proxyip, optionsmodel::proxyip);
    mapper->addmapping(ui->proxyport, optionsmodel::proxyport);

    /* window */
#ifndef q_os_mac
    mapper->addmapping(ui->minimizetotray, optionsmodel::minimizetotray);
    mapper->addmapping(ui->minimizeonclose, optionsmodel::minimizeonclose);
#endif

    /* display */
    mapper->addmapping(ui->lang, optionsmodel::language);
    mapper->addmapping(ui->unit, optionsmodel::displayunit);
    mapper->addmapping(ui->thirdpartytxurls, optionsmodel::thirdpartytxurls);
}

void optionsdialog::enableokbutton()
{
    /* prevent enabling of the ok button when data modified, if there is an invalid proxy address present */
    if(fproxyipvalid)
        setokbuttonstate(true);
}

void optionsdialog::disableokbutton()
{
    setokbuttonstate(false);
}

void optionsdialog::setokbuttonstate(bool fstate)
{
    ui->okbutton->setenabled(fstate);
}

void optionsdialog::on_resetbutton_clicked()
{
    if(model)
    {
        // confirmation dialog
        qmessagebox::standardbutton btnretval = qmessagebox::question(this, tr("confirm options reset"),
            tr("client restart required to activate changes.") + "<br><br>" + tr("client will be shut down. do you want to proceed?"),
            qmessagebox::yes | qmessagebox::cancel, qmessagebox::cancel);

        if(btnretval == qmessagebox::cancel)
            return;

        /* reset all options and close gui */
        model->reset();
        qapplication::quit();
    }
}

void optionsdialog::on_okbutton_clicked()
{
    mapper->submit();
    accept();
}

void optionsdialog::on_cancelbutton_clicked()
{
    reject();
}

void optionsdialog::showrestartwarning(bool fpersistent)
{
    ui->statuslabel->setstylesheet("qlabel { color: red; }");

    if(fpersistent)
    {
        ui->statuslabel->settext(tr("client restart required to activate changes."));
    }
    else
    {
        ui->statuslabel->settext(tr("this change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // todo: should perhaps be a class attribute, if we extend the use of statuslabel
        qtimer::singleshot(10000, this, slot(clearstatuslabel()));
    }
}

void optionsdialog::clearstatuslabel()
{
    ui->statuslabel->clear();
}

void optionsdialog::doproxyipchecks(qvalidatedlineedit *puiproxyip, int nproxyport)
{
    q_unused(nproxyport);

    const std::string straddrproxy = puiproxyip->text().tostdstring();
    cservice addrproxy;

    /* check for a valid ipv4 / ipv6 address */
    if (!(fproxyipvalid = lookupnumeric(straddrproxy.c_str(), addrproxy)))
    {
        disableokbutton();
        puiproxyip->setvalid(false);
        ui->statuslabel->setstylesheet("qlabel { color: red; }");
        ui->statuslabel->settext(tr("the supplied proxy address is invalid."));
    }
    else
    {
        enableokbutton();
        ui->statuslabel->clear();
    }
}

bool optionsdialog::eventfilter(qobject *object, qevent *event)
{
    if(event->type() == qevent::focusout)
    {
        if(object == ui->proxyip)
        {
            emit proxyipchecks(ui->proxyip, ui->proxyport->text().toint());
        }
    }
    return qdialog::eventfilter(object, event);
}
