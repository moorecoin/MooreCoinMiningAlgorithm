// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "walletframe.h"

#include "moorecoingui.h"
#include "walletview.h"

#include <cstdio>

#include <qhboxlayout>
#include <qlabel>

walletframe::walletframe(moorecoingui *_gui) :
    qframe(_gui),
    gui(_gui)
{
    // leave hbox hook for adding a list view later
    qhboxlayout *walletframelayout = new qhboxlayout(this);
    setcontentsmargins(0,0,0,0);
    walletstack = new qstackedwidget(this);
    walletframelayout->setcontentsmargins(0,0,0,0);
    walletframelayout->addwidget(walletstack);

    qlabel *nowallet = new qlabel(tr("no wallet has been loaded."));
    nowallet->setalignment(qt::aligncenter);
    walletstack->addwidget(nowallet);
}

walletframe::~walletframe()
{
}

void walletframe::setclientmodel(clientmodel *clientmodel)
{
    this->clientmodel = clientmodel;
}

bool walletframe::addwallet(const qstring& name, walletmodel *walletmodel)
{
    if (!gui || !clientmodel || !walletmodel || mapwalletviews.count(name) > 0)
        return false;

    walletview *walletview = new walletview(this);
    walletview->setmoorecoingui(gui);
    walletview->setclientmodel(clientmodel);
    walletview->setwalletmodel(walletmodel);
    walletview->showoutofsyncwarning(boutofsync);

     /* todo we should goto the currently selected page once dynamically adding wallets is supported */
    walletview->gotooverviewpage();
    walletstack->addwidget(walletview);
    mapwalletviews[name] = walletview;

    // ensure a walletview is able to show the main window
    connect(walletview, signal(shownormalifminimized()), gui, slot(shownormalifminimized()));

    return true;
}

bool walletframe::setcurrentwallet(const qstring& name)
{
    if (mapwalletviews.count(name) == 0)
        return false;

    walletview *walletview = mapwalletviews.value(name);
    walletstack->setcurrentwidget(walletview);
    walletview->updateencryptionstatus();
    return true;
}

bool walletframe::removewallet(const qstring &name)
{
    if (mapwalletviews.count(name) == 0)
        return false;

    walletview *walletview = mapwalletviews.take(name);
    walletstack->removewidget(walletview);
    return true;
}

void walletframe::removeallwallets()
{
    qmap<qstring, walletview*>::const_iterator i;
    for (i = mapwalletviews.constbegin(); i != mapwalletviews.constend(); ++i)
        walletstack->removewidget(i.value());
    mapwalletviews.clear();
}

bool walletframe::handlepaymentrequest(const sendcoinsrecipient &recipient)
{
    walletview *walletview = currentwalletview();
    if (!walletview)
        return false;

    return walletview->handlepaymentrequest(recipient);
}

void walletframe::showoutofsyncwarning(bool fshow)
{
    boutofsync = fshow;
    qmap<qstring, walletview*>::const_iterator i;
    for (i = mapwalletviews.constbegin(); i != mapwalletviews.constend(); ++i)
        i.value()->showoutofsyncwarning(fshow);
}

void walletframe::gotooverviewpage()
{
    qmap<qstring, walletview*>::const_iterator i;
    for (i = mapwalletviews.constbegin(); i != mapwalletviews.constend(); ++i)
        i.value()->gotooverviewpage();
}

void walletframe::gotohistorypage()
{
    qmap<qstring, walletview*>::const_iterator i;
    for (i = mapwalletviews.constbegin(); i != mapwalletviews.constend(); ++i)
        i.value()->gotohistorypage();
}

void walletframe::gotoreceivecoinspage()
{
    qmap<qstring, walletview*>::const_iterator i;
    for (i = mapwalletviews.constbegin(); i != mapwalletviews.constend(); ++i)
        i.value()->gotoreceivecoinspage();
}

void walletframe::gotosendcoinspage(qstring addr)
{
    qmap<qstring, walletview*>::const_iterator i;
    for (i = mapwalletviews.constbegin(); i != mapwalletviews.constend(); ++i)
        i.value()->gotosendcoinspage(addr);
}

void walletframe::gotosignmessagetab(qstring addr)
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->gotosignmessagetab(addr);
}

void walletframe::gotoverifymessagetab(qstring addr)
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->gotoverifymessagetab(addr);
}

void walletframe::encryptwallet(bool status)
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->encryptwallet(status);
}

void walletframe::backupwallet()
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->backupwallet();
}

void walletframe::changepassphrase()
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->changepassphrase();
}

void walletframe::unlockwallet()
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->unlockwallet();
}

void walletframe::usedsendingaddresses()
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->usedsendingaddresses();
}

void walletframe::usedreceivingaddresses()
{
    walletview *walletview = currentwalletview();
    if (walletview)
        walletview->usedreceivingaddresses();
}

walletview *walletframe::currentwalletview()
{
    return qobject_cast<walletview*>(walletstack->currentwidget());
}

