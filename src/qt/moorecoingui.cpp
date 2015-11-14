// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "moorecoingui.h"

#include "moorecoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "rpcconsole.h"
#include "scicon.h"
#include "utilitydialog.h"

#ifdef enable_wallet
#include "walletframe.h"
#include "walletmodel.h"
#endif // enable_wallet

#ifdef q_os_mac
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "ui_interface.h"
#include "util.h"

#include <iostream>

#include <qaction>
#include <qapplication>
#include <qdatetime>
#include <qdesktopwidget>
#include <qdragenterevent>
#include <qlistwidget>
#include <qmenubar>
#include <qmessagebox>
#include <qmimedata>
#include <qprogressbar>
#include <qprogressdialog>
#include <qsettings>
#include <qstackedwidget>
#include <qstatusbar>
#include <qstyle>
#include <qtimer>
#include <qtoolbar>
#include <qvboxlayout>

#if qt_version < 0x050000
#include <qtextdocument>
#include <qurl>
#else
#include <qurlquery>
#endif

const qstring moorecoingui::default_wallet = "~default";

moorecoingui::moorecoingui(const networkstyle *networkstyle, qwidget *parent) :
    qmainwindow(parent),
    clientmodel(0),
    walletframe(0),
    unitdisplaycontrol(0),
    labelencryptionicon(0),
    labelconnectionsicon(0),
    labelblocksicon(0),
    progressbarlabel(0),
    progressbar(0),
    progressdialog(0),
    appmenubar(0),
    overviewaction(0),
    historyaction(0),
    quitaction(0),
    sendcoinsaction(0),
    sendcoinsmenuaction(0),
    usedsendingaddressesaction(0),
    usedreceivingaddressesaction(0),
    signmessageaction(0),
    verifymessageaction(0),
    aboutaction(0),
    receivecoinsaction(0),
    receivecoinsmenuaction(0),
    optionsaction(0),
    togglehideaction(0),
    encryptwalletaction(0),
    backupwalletaction(0),
    changepassphraseaction(0),
    aboutqtaction(0),
    openrpcconsoleaction(0),
    openaction(0),
    showhelpmessageaction(0),
    trayicon(0),
    trayiconmenu(0),
    notificator(0),
    rpcconsole(0),
    prevblocks(0),
    spinnerframe(0)
{
    guiutil::restorewindowgeometry("nwindow", qsize(850, 550), this);

    qstring windowtitle = tr("moorecoin core") + " - ";
#ifdef enable_wallet
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enablewallet = !getboolarg("-disablewallet", false);
#else
    enablewallet = false;
#endif // enable_wallet
    if(enablewallet)
    {
        windowtitle += tr("wallet");
    } else {
        windowtitle += tr("node");
    }
    windowtitle += " " + networkstyle->gettitleaddtext();
#ifndef q_os_mac
    qapplication::setwindowicon(networkstyle->gettrayandwindowicon());
    setwindowicon(networkstyle->gettrayandwindowicon());
#else
    macdockiconhandler::instance()->seticon(networkstyle->getappicon());
#endif
    setwindowtitle(windowtitle);

#if defined(q_os_mac) && qt_version < 0x050000
    // this property is not implemented in qt 5. setting it has no effect.
    // a replacement api (qtmacunifiedtoolbar) is available in qtmacextras.
    setunifiedtitleandtoolbaronmac(true);
#endif

    rpcconsole = new rpcconsole(0);
#ifdef enable_wallet
    if(enablewallet)
    {
        /** create wallet frame and make it the central widget */
        walletframe = new walletframe(this);
        setcentralwidget(walletframe);
    } else
#endif // enable_wallet
    {
        /* when compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setcentralwidget(rpcconsole);
    }

    // accept d&d of uris
    setacceptdrops(true);

    // create actions for the toolbar, menu bar and tray/dock icon
    // needs walletframe to be initialized
    createactions();

    // create application menu bar
    createmenubar();

    // create the toolbars
    createtoolbars();

    // create system tray icon and notification
    createtrayicon(networkstyle);

    // create status bar
    statusbar();

    // disable size grip because it looks ugly and nobody needs it
    statusbar()->setsizegripenabled(false);

    // status bar notification icons
    qframe *frameblocks = new qframe();
    frameblocks->setcontentsmargins(0,0,0,0);
    frameblocks->setsizepolicy(qsizepolicy::fixed, qsizepolicy::preferred);
    qhboxlayout *frameblockslayout = new qhboxlayout(frameblocks);
    frameblockslayout->setcontentsmargins(3,0,3,0);
    frameblockslayout->setspacing(3);
    unitdisplaycontrol = new unitdisplaystatusbarcontrol();
    labelencryptionicon = new qlabel();
    labelconnectionsicon = new qlabel();
    labelblocksicon = new qlabel();
    if(enablewallet)
    {
        frameblockslayout->addstretch();
        frameblockslayout->addwidget(unitdisplaycontrol);
        frameblockslayout->addstretch();
        frameblockslayout->addwidget(labelencryptionicon);
    }
    frameblockslayout->addstretch();
    frameblockslayout->addwidget(labelconnectionsicon);
    frameblockslayout->addstretch();
    frameblockslayout->addwidget(labelblocksicon);
    frameblockslayout->addstretch();

    // progress bar and label for blocks download
    progressbarlabel = new qlabel();
    progressbarlabel->setvisible(false);
    progressbar = new guiutil::progressbar();
    progressbar->setalignment(qt::aligncenter);
    progressbar->setvisible(false);

    // override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // see https://qt-project.org/doc/qt-4.8/gallery.html
    qstring curstyle = qapplication::style()->metaobject()->classname();
    if(curstyle == "qwindowsstyle" || curstyle == "qwindowsxpstyle")
    {
        progressbar->setstylesheet("qprogressbar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } qprogressbar::chunk { background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #ff8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusbar()->addwidget(progressbarlabel);
    statusbar()->addwidget(progressbar);
    statusbar()->addpermanentwidget(frameblocks);

    connect(openrpcconsoleaction, signal(triggered()), rpcconsole, slot(show()));

    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitaction, signal(triggered()), rpcconsole, slot(hide()));

    // install event filter to be able to catch status tip events (qevent::statustip)
    this->installeventfilter(this);

    // initially wallet actions should be disabled
    setwalletactionsenabled(false);

    // subscribe to notifications from core
    subscribetocoresignals();
}

moorecoingui::~moorecoingui()
{
    // unsubscribe from notifications from core
    unsubscribefromcoresignals();

    guiutil::savewindowgeometry("nwindow", this);
    if(trayicon) // hide tray icon, as deleting will let it linger until quit (on ubuntu)
        trayicon->hide();
#ifdef q_os_mac
    delete appmenubar;
    macdockiconhandler::cleanup();
#endif

    delete rpcconsole;
}

void moorecoingui::createactions()
{
    qactiongroup *tabgroup = new qactiongroup(this);

    overviewaction = new qaction(singlecoloricon(":/icons/overview"), tr("&overview"), this);
    overviewaction->setstatustip(tr("show general overview of wallet"));
    overviewaction->settooltip(overviewaction->statustip());
    overviewaction->setcheckable(true);
    overviewaction->setshortcut(qkeysequence(qt::alt + qt::key_1));
    tabgroup->addaction(overviewaction);

    sendcoinsaction = new qaction(singlecoloricon(":/icons/send"), tr("&send"), this);
    sendcoinsaction->setstatustip(tr("send coins to a moorecoin address"));
    sendcoinsaction->settooltip(sendcoinsaction->statustip());
    sendcoinsaction->setcheckable(true);
    sendcoinsaction->setshortcut(qkeysequence(qt::alt + qt::key_2));
    tabgroup->addaction(sendcoinsaction);

    sendcoinsmenuaction = new qaction(textcoloricon(":/icons/send"), sendcoinsaction->text(), this);
    sendcoinsmenuaction->setstatustip(sendcoinsaction->statustip());
    sendcoinsmenuaction->settooltip(sendcoinsmenuaction->statustip());

    receivecoinsaction = new qaction(singlecoloricon(":/icons/receiving_addresses"), tr("&receive"), this);
    receivecoinsaction->setstatustip(tr("request payments (generates qr codes and moorecoin: uris)"));
    receivecoinsaction->settooltip(receivecoinsaction->statustip());
    receivecoinsaction->setcheckable(true);
    receivecoinsaction->setshortcut(qkeysequence(qt::alt + qt::key_3));
    tabgroup->addaction(receivecoinsaction);

    receivecoinsmenuaction = new qaction(textcoloricon(":/icons/receiving_addresses"), receivecoinsaction->text(), this);
    receivecoinsmenuaction->setstatustip(receivecoinsaction->statustip());
    receivecoinsmenuaction->settooltip(receivecoinsmenuaction->statustip());

    historyaction = new qaction(singlecoloricon(":/icons/history"), tr("&transactions"), this);
    historyaction->setstatustip(tr("browse transaction history"));
    historyaction->settooltip(historyaction->statustip());
    historyaction->setcheckable(true);
    historyaction->setshortcut(qkeysequence(qt::alt + qt::key_4));
    tabgroup->addaction(historyaction);

#ifdef enable_wallet
    // these shownormalifminimized are needed because send coins and receive coins
    // can be triggered from the tray menu, and need to show the gui to be useful.
    connect(overviewaction, signal(triggered()), this, slot(shownormalifminimized()));
    connect(overviewaction, signal(triggered()), this, slot(gotooverviewpage()));
    connect(sendcoinsaction, signal(triggered()), this, slot(shownormalifminimized()));
    connect(sendcoinsaction, signal(triggered()), this, slot(gotosendcoinspage()));
    connect(sendcoinsmenuaction, signal(triggered()), this, slot(shownormalifminimized()));
    connect(sendcoinsmenuaction, signal(triggered()), this, slot(gotosendcoinspage()));
    connect(receivecoinsaction, signal(triggered()), this, slot(shownormalifminimized()));
    connect(receivecoinsaction, signal(triggered()), this, slot(gotoreceivecoinspage()));
    connect(receivecoinsmenuaction, signal(triggered()), this, slot(shownormalifminimized()));
    connect(receivecoinsmenuaction, signal(triggered()), this, slot(gotoreceivecoinspage()));
    connect(historyaction, signal(triggered()), this, slot(shownormalifminimized()));
    connect(historyaction, signal(triggered()), this, slot(gotohistorypage()));
#endif // enable_wallet

    quitaction = new qaction(textcoloricon(":/icons/quit"), tr("e&xit"), this);
    quitaction->setstatustip(tr("quit application"));
    quitaction->setshortcut(qkeysequence(qt::ctrl + qt::key_q));
    quitaction->setmenurole(qaction::quitrole);
    aboutaction = new qaction(textcoloricon(":/icons/about"), tr("&about moorecoin core"), this);
    aboutaction->setstatustip(tr("show information about moorecoin core"));
    aboutaction->setmenurole(qaction::aboutrole);
    aboutqtaction = new qaction(textcoloricon(":/icons/about_qt"), tr("about &qt"), this);
    aboutqtaction->setstatustip(tr("show information about qt"));
    aboutqtaction->setmenurole(qaction::aboutqtrole);
    optionsaction = new qaction(textcoloricon(":/icons/options"), tr("&options..."), this);
    optionsaction->setstatustip(tr("modify configuration options for moorecoin core"));
    optionsaction->setmenurole(qaction::preferencesrole);
    togglehideaction = new qaction(textcoloricon(":/icons/about"), tr("&show / hide"), this);
    togglehideaction->setstatustip(tr("show or hide the main window"));

    encryptwalletaction = new qaction(textcoloricon(":/icons/lock_closed"), tr("&encrypt wallet..."), this);
    encryptwalletaction->setstatustip(tr("encrypt the private keys that belong to your wallet"));
    encryptwalletaction->setcheckable(true);
    backupwalletaction = new qaction(textcoloricon(":/icons/filesave"), tr("&backup wallet..."), this);
    backupwalletaction->setstatustip(tr("backup wallet to another location"));
    changepassphraseaction = new qaction(textcoloricon(":/icons/key"), tr("&change passphrase..."), this);
    changepassphraseaction->setstatustip(tr("change the passphrase used for wallet encryption"));
    signmessageaction = new qaction(textcoloricon(":/icons/edit"), tr("sign &message..."), this);
    signmessageaction->setstatustip(tr("sign messages with your moorecoin addresses to prove you own them"));
    verifymessageaction = new qaction(textcoloricon(":/icons/verify"), tr("&verify message..."), this);
    verifymessageaction->setstatustip(tr("verify messages to ensure they were signed with specified moorecoin addresses"));

    openrpcconsoleaction = new qaction(textcoloricon(":/icons/debugwindow"), tr("&debug window"), this);
    openrpcconsoleaction->setstatustip(tr("open debugging and diagnostic console"));

    usedsendingaddressesaction = new qaction(textcoloricon(":/icons/address-book"), tr("&sending addresses..."), this);
    usedsendingaddressesaction->setstatustip(tr("show the list of used sending addresses and labels"));
    usedreceivingaddressesaction = new qaction(textcoloricon(":/icons/address-book"), tr("&receiving addresses..."), this);
    usedreceivingaddressesaction->setstatustip(tr("show the list of used receiving addresses and labels"));

    openaction = new qaction(textcoloricon(":/icons/open"), tr("open &uri..."), this);
    openaction->setstatustip(tr("open a moorecoin: uri or payment request"));

    showhelpmessageaction = new qaction(textcoloricon(":/icons/info"), tr("&command-line options"), this);
    showhelpmessageaction->setmenurole(qaction::norole);
    showhelpmessageaction->setstatustip(tr("show the moorecoin core help message to get a list with possible moorecoin command-line options"));

    connect(quitaction, signal(triggered()), qapp, slot(quit()));
    connect(aboutaction, signal(triggered()), this, slot(aboutclicked()));
    connect(aboutqtaction, signal(triggered()), qapp, slot(aboutqt()));
    connect(optionsaction, signal(triggered()), this, slot(optionsclicked()));
    connect(togglehideaction, signal(triggered()), this, slot(togglehidden()));
    connect(showhelpmessageaction, signal(triggered()), this, slot(showhelpmessageclicked()));
#ifdef enable_wallet
    if(walletframe)
    {
        connect(encryptwalletaction, signal(triggered(bool)), walletframe, slot(encryptwallet(bool)));
        connect(backupwalletaction, signal(triggered()), walletframe, slot(backupwallet()));
        connect(changepassphraseaction, signal(triggered()), walletframe, slot(changepassphrase()));
        connect(signmessageaction, signal(triggered()), this, slot(gotosignmessagetab()));
        connect(verifymessageaction, signal(triggered()), this, slot(gotoverifymessagetab()));
        connect(usedsendingaddressesaction, signal(triggered()), walletframe, slot(usedsendingaddresses()));
        connect(usedreceivingaddressesaction, signal(triggered()), walletframe, slot(usedreceivingaddresses()));
        connect(openaction, signal(triggered()), this, slot(openclicked()));
    }
#endif // enable_wallet
}

void moorecoingui::createmenubar()
{
#ifdef q_os_mac
    // create a decoupled menu bar on mac which stays even if the window is closed
    appmenubar = new qmenubar();
#else
    // get the main window's menu bar on other platforms
    appmenubar = menubar();
#endif

    // configure the menus
    qmenu *file = appmenubar->addmenu(tr("&file"));
    if(walletframe)
    {
        file->addaction(openaction);
        file->addaction(backupwalletaction);
        file->addaction(signmessageaction);
        file->addaction(verifymessageaction);
        file->addseparator();
        file->addaction(usedsendingaddressesaction);
        file->addaction(usedreceivingaddressesaction);
        file->addseparator();
    }
    file->addaction(quitaction);

    qmenu *settings = appmenubar->addmenu(tr("&settings"));
    if(walletframe)
    {
        settings->addaction(encryptwalletaction);
        settings->addaction(changepassphraseaction);
        settings->addseparator();
    }
    settings->addaction(optionsaction);

    qmenu *help = appmenubar->addmenu(tr("&help"));
    if(walletframe)
    {
        help->addaction(openrpcconsoleaction);
    }
    help->addaction(showhelpmessageaction);
    help->addseparator();
    help->addaction(aboutaction);
    help->addaction(aboutqtaction);
}

void moorecoingui::createtoolbars()
{
    if(walletframe)
    {
        qtoolbar *toolbar = addtoolbar(tr("tabs toolbar"));
        toolbar->setmovable(false);
        toolbar->settoolbuttonstyle(qt::toolbuttontextbesideicon);
        toolbar->addaction(overviewaction);
        toolbar->addaction(sendcoinsaction);
        toolbar->addaction(receivecoinsaction);
        toolbar->addaction(historyaction);
        overviewaction->setchecked(true);
    }
}

void moorecoingui::setclientmodel(clientmodel *clientmodel)
{
    this->clientmodel = clientmodel;
    if(clientmodel)
    {
        // create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createtrayiconmenu();

        // keep up to date with client
        setnumconnections(clientmodel->getnumconnections());
        connect(clientmodel, signal(numconnectionschanged(int)), this, slot(setnumconnections(int)));

        setnumblocks(clientmodel->getnumblocks(), clientmodel->getlastblockdate());
        connect(clientmodel, signal(numblockschanged(int,qdatetime)), this, slot(setnumblocks(int,qdatetime)));

        // receive and report messages from client model
        connect(clientmodel, signal(message(qstring,qstring,unsigned int)), this, slot(message(qstring,qstring,unsigned int)));

        // show progress dialog
        connect(clientmodel, signal(showprogress(qstring,int)), this, slot(showprogress(qstring,int)));

        rpcconsole->setclientmodel(clientmodel);
#ifdef enable_wallet
        if(walletframe)
        {
            walletframe->setclientmodel(clientmodel);
        }
#endif // enable_wallet
        unitdisplaycontrol->setoptionsmodel(clientmodel->getoptionsmodel());
    } else {
        // disable possibility to show main window via action
        togglehideaction->setenabled(false);
        if(trayiconmenu)
        {
            // disable context menu on tray icon
            trayiconmenu->clear();
        }
    }
}

#ifdef enable_wallet
bool moorecoingui::addwallet(const qstring& name, walletmodel *walletmodel)
{
    if(!walletframe)
        return false;
    setwalletactionsenabled(true);
    return walletframe->addwallet(name, walletmodel);
}

bool moorecoingui::setcurrentwallet(const qstring& name)
{
    if(!walletframe)
        return false;
    return walletframe->setcurrentwallet(name);
}

void moorecoingui::removeallwallets()
{
    if(!walletframe)
        return;
    setwalletactionsenabled(false);
    walletframe->removeallwallets();
}
#endif // enable_wallet

void moorecoingui::setwalletactionsenabled(bool enabled)
{
    overviewaction->setenabled(enabled);
    sendcoinsaction->setenabled(enabled);
    sendcoinsmenuaction->setenabled(enabled);
    receivecoinsaction->setenabled(enabled);
    receivecoinsmenuaction->setenabled(enabled);
    historyaction->setenabled(enabled);
    encryptwalletaction->setenabled(enabled);
    backupwalletaction->setenabled(enabled);
    changepassphraseaction->setenabled(enabled);
    signmessageaction->setenabled(enabled);
    verifymessageaction->setenabled(enabled);
    usedsendingaddressesaction->setenabled(enabled);
    usedreceivingaddressesaction->setenabled(enabled);
    openaction->setenabled(enabled);
}

void moorecoingui::createtrayicon(const networkstyle *networkstyle)
{
#ifndef q_os_mac
    trayicon = new qsystemtrayicon(this);
    qstring tooltip = tr("moorecoin core client") + " " + networkstyle->gettitleaddtext();
    trayicon->settooltip(tooltip);
    trayicon->seticon(networkstyle->gettrayandwindowicon());
    trayicon->show();
#endif

    notificator = new notificator(qapplication::applicationname(), trayicon, this);
}

void moorecoingui::createtrayiconmenu()
{
#ifndef q_os_mac
    // return if trayicon is unset (only on non-mac oses)
    if (!trayicon)
        return;

    trayiconmenu = new qmenu(this);
    trayicon->setcontextmenu(trayiconmenu);

    connect(trayicon, signal(activated(qsystemtrayicon::activationreason)),
            this, slot(trayiconactivated(qsystemtrayicon::activationreason)));
#else
    // note: on mac, the dock icon is used to provide the tray's functionality.
    macdockiconhandler *dockiconhandler = macdockiconhandler::instance();
    dockiconhandler->setmainwindow((qmainwindow *)this);
    trayiconmenu = dockiconhandler->dockmenu();
#endif

    // configuration of the tray icon (or dock icon) icon menu
    trayiconmenu->addaction(togglehideaction);
    trayiconmenu->addseparator();
    trayiconmenu->addaction(sendcoinsmenuaction);
    trayiconmenu->addaction(receivecoinsmenuaction);
    trayiconmenu->addseparator();
    trayiconmenu->addaction(signmessageaction);
    trayiconmenu->addaction(verifymessageaction);
    trayiconmenu->addseparator();
    trayiconmenu->addaction(optionsaction);
    trayiconmenu->addaction(openrpcconsoleaction);
#ifndef q_os_mac // this is built-in on mac
    trayiconmenu->addseparator();
    trayiconmenu->addaction(quitaction);
#endif
}

#ifndef q_os_mac
void moorecoingui::trayiconactivated(qsystemtrayicon::activationreason reason)
{
    if(reason == qsystemtrayicon::trigger)
    {
        // click on system tray icon triggers show/hide of the main window
        togglehidden();
    }
}
#endif

void moorecoingui::optionsclicked()
{
    if(!clientmodel || !clientmodel->getoptionsmodel())
        return;

    optionsdialog dlg(this, enablewallet);
    dlg.setmodel(clientmodel->getoptionsmodel());
    dlg.exec();
}

void moorecoingui::aboutclicked()
{
    if(!clientmodel)
        return;

    helpmessagedialog dlg(this, true);
    dlg.exec();
}

void moorecoingui::showhelpmessageclicked()
{
    helpmessagedialog *help = new helpmessagedialog(this, false);
    help->setattribute(qt::wa_deleteonclose);
    help->show();
}

#ifdef enable_wallet
void moorecoingui::openclicked()
{
    openuridialog dlg(this);
    if(dlg.exec())
    {
        emit receiveduri(dlg.geturi());
    }
}

void moorecoingui::gotooverviewpage()
{
    overviewaction->setchecked(true);
    if (walletframe) walletframe->gotooverviewpage();
}

void moorecoingui::gotohistorypage()
{
    historyaction->setchecked(true);
    if (walletframe) walletframe->gotohistorypage();
}

void moorecoingui::gotoreceivecoinspage()
{
    receivecoinsaction->setchecked(true);
    if (walletframe) walletframe->gotoreceivecoinspage();
}

void moorecoingui::gotosendcoinspage(qstring addr)
{
    sendcoinsaction->setchecked(true);
    if (walletframe) walletframe->gotosendcoinspage(addr);
}

void moorecoingui::gotosignmessagetab(qstring addr)
{
    if (walletframe) walletframe->gotosignmessagetab(addr);
}

void moorecoingui::gotoverifymessagetab(qstring addr)
{
    if (walletframe) walletframe->gotoverifymessagetab(addr);
}
#endif // enable_wallet

void moorecoingui::setnumconnections(int count)
{
    qstring icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelconnectionsicon->setpixmap(singlecoloricon(icon).pixmap(statusbar_iconsize,statusbar_iconsize));
    labelconnectionsicon->settooltip(tr("%n active connection(s) to moorecoin network", "", count));
}

void moorecoingui::setnumblocks(int count, const qdatetime& blockdate)
{
    if(!clientmodel)
        return;

    // prevent orphan statusbar messages (e.g. hover quit in main menu, wait until chain-sync starts -> garbelled text)
    statusbar()->clearmessage();

    // acquire current block source
    enum blocksource blocksource = clientmodel->getblocksource();
    switch (blocksource) {
        case block_source_network:
            progressbarlabel->settext(tr("synchronizing with network..."));
            break;
        case block_source_disk:
            progressbarlabel->settext(tr("importing blocks from disk..."));
            break;
        case block_source_reindex:
            progressbarlabel->settext(tr("reindexing blocks on disk..."));
            break;
        case block_source_none:
            // case: not importing, not reindexing and no network connection
            progressbarlabel->settext(tr("no block source available..."));
            break;
    }

    qstring tooltip;

    qdatetime currentdate = qdatetime::currentdatetime();
    qint64 secs = blockdate.secsto(currentdate);

    tooltip = tr("processed %n block(s) of transaction history.", "", count);

    // set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("up to date") + qstring(".<br>") + tooltip;
        labelblocksicon->setpixmap(singlecoloricon(":/icons/synced").pixmap(statusbar_iconsize, statusbar_iconsize));

#ifdef enable_wallet
        if(walletframe)
            walletframe->showoutofsyncwarning(false);
#endif // enable_wallet

        progressbarlabel->setvisible(false);
        progressbar->setvisible(false);
    }
    else
    {
        // represent time from last generated block in human readable text
        qstring timebehindtext;
        const int hour_in_seconds = 60*60;
        const int day_in_seconds = 24*60*60;
        const int week_in_seconds = 7*24*60*60;
        const int year_in_seconds = 31556952; // average length of year in gregorian calendar
        if(secs < 2*day_in_seconds)
        {
            timebehindtext = tr("%n hour(s)","",secs/hour_in_seconds);
        }
        else if(secs < 2*week_in_seconds)
        {
            timebehindtext = tr("%n day(s)","",secs/day_in_seconds);
        }
        else if(secs < year_in_seconds)
        {
            timebehindtext = tr("%n week(s)","",secs/week_in_seconds);
        }
        else
        {
            qint64 years = secs / year_in_seconds;
            qint64 remainder = secs % year_in_seconds;
            timebehindtext = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(tr("%n week(s)","", remainder/week_in_seconds));
        }

        progressbarlabel->setvisible(true);
        progressbar->setformat(tr("%1 behind").arg(timebehindtext));
        progressbar->setmaximum(1000000000);
        progressbar->setvalue(clientmodel->getverificationprogress() * 1000000000.0 + 0.5);
        progressbar->setvisible(true);

        tooltip = tr("catching up...") + qstring("<br>") + tooltip;
        if(count != prevblocks)
        {
            labelblocksicon->setpixmap(singlecoloricon(qstring(
                ":/movies/spinner-%1").arg(spinnerframe, 3, 10, qchar('0')))
                .pixmap(statusbar_iconsize, statusbar_iconsize));
            spinnerframe = (spinnerframe + 1) % spinner_frames;
        }
        prevblocks = count;

#ifdef enable_wallet
        if(walletframe)
            walletframe->showoutofsyncwarning(true);
#endif // enable_wallet

        tooltip += qstring("<br>");
        tooltip += tr("last received block was generated %1 ago.").arg(timebehindtext);
        tooltip += qstring("<br>");
        tooltip += tr("transactions after this will not yet be visible.");
    }

    // don't word-wrap this (fixed-width) tooltip
    tooltip = qstring("<nobr>") + tooltip + qstring("</nobr>");

    labelblocksicon->settooltip(tooltip);
    progressbarlabel->settooltip(tooltip);
    progressbar->settooltip(tooltip);
}

void moorecoingui::message(const qstring &title, const qstring &message, unsigned int style, bool *ret)
{
    qstring strtitle = tr("moorecoin"); // default title
    // default to information icon
    int nmboxicon = qmessagebox::information;
    int nnotifyicon = notificator::information;

    qstring msgtype;

    // prefer supplied title over style based title
    if (!title.isempty()) {
        msgtype = title;
    }
    else {
        switch (style) {
        case cclientuiinterface::msg_error:
            msgtype = tr("error");
            break;
        case cclientuiinterface::msg_warning:
            msgtype = tr("warning");
            break;
        case cclientuiinterface::msg_information:
            msgtype = tr("information");
            break;
        default:
            break;
        }
    }
    // append title to "moorecoin - "
    if (!msgtype.isempty())
        strtitle += " - " + msgtype;

    // check for error/warning icon
    if (style & cclientuiinterface::icon_error) {
        nmboxicon = qmessagebox::critical;
        nnotifyicon = notificator::critical;
    }
    else if (style & cclientuiinterface::icon_warning) {
        nmboxicon = qmessagebox::warning;
        nnotifyicon = notificator::warning;
    }

    // display message
    if (style & cclientuiinterface::modal) {
        // check for buttons, use ok as default, if none was supplied
        qmessagebox::standardbutton buttons;
        if (!(buttons = (qmessagebox::standardbutton)(style & cclientuiinterface::btn_mask)))
            buttons = qmessagebox::ok;

        shownormalifminimized();
        qmessagebox mbox((qmessagebox::icon)nmboxicon, strtitle, message, buttons, this);
        int r = mbox.exec();
        if (ret != null)
            *ret = r == qmessagebox::ok;
    }
    else
        notificator->notify((notificator::class)nnotifyicon, strtitle, message);
}

void moorecoingui::changeevent(qevent *e)
{
    qmainwindow::changeevent(e);
#ifndef q_os_mac // ignored on mac
    if(e->type() == qevent::windowstatechange)
    {
        if(clientmodel && clientmodel->getoptionsmodel() && clientmodel->getoptionsmodel()->getminimizetotray())
        {
            qwindowstatechangeevent *wsevt = static_cast<qwindowstatechangeevent*>(e);
            if(!(wsevt->oldstate() & qt::windowminimized) && isminimized())
            {
                qtimer::singleshot(0, this, slot(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void moorecoingui::closeevent(qcloseevent *event)
{
#ifndef q_os_mac // ignored on mac
    if(clientmodel && clientmodel->getoptionsmodel())
    {
        if(!clientmodel->getoptionsmodel()->getminimizetotray() &&
           !clientmodel->getoptionsmodel()->getminimizeonclose())
        {
            // close rpcconsole in case it was open to make some space for the shutdown window
            rpcconsole->close();

            qapplication::quit();
        }
    }
#endif
    qmainwindow::closeevent(event);
}

#ifdef enable_wallet
void moorecoingui::incomingtransaction(const qstring& date, int unit, const camount& amount, const qstring& type, const qstring& address, const qstring& label)
{
    // on new transaction, make an info balloon
    qstring msg = tr("date: %1\n").arg(date) +
                  tr("amount: %1\n").arg(moorecoinunits::formatwithunit(unit, amount, true)) +
                  tr("type: %1\n").arg(type);
    if (!label.isempty())
        msg += tr("label: %1\n").arg(label);
    else if (!address.isempty())
        msg += tr("address: %1\n").arg(address);
    message((amount)<0 ? tr("sent transaction") : tr("incoming transaction"),
             msg, cclientuiinterface::msg_information);
}
#endif // enable_wallet

void moorecoingui::dragenterevent(qdragenterevent *event)
{
    // accept only uris
    if(event->mimedata()->hasurls())
        event->acceptproposedaction();
}

void moorecoingui::dropevent(qdropevent *event)
{
    if(event->mimedata()->hasurls())
    {
        foreach(const qurl &uri, event->mimedata()->urls())
        {
            emit receiveduri(uri.tostring());
        }
    }
    event->acceptproposedaction();
}

bool moorecoingui::eventfilter(qobject *object, qevent *event)
{
    // catch status tip events
    if (event->type() == qevent::statustip)
    {
        // prevent adding text from setstatustip(), if we currently use the status bar for displaying other stuff
        if (progressbarlabel->isvisible() || progressbar->isvisible())
            return true;
    }
    return qmainwindow::eventfilter(object, event);
}

#ifdef enable_wallet
bool moorecoingui::handlepaymentrequest(const sendcoinsrecipient& recipient)
{
    // uri has to be valid
    if (walletframe && walletframe->handlepaymentrequest(recipient))
    {
        shownormalifminimized();
        gotosendcoinspage();
        return true;
    }
    return false;
}

void moorecoingui::setencryptionstatus(int status)
{
    switch(status)
    {
    case walletmodel::unencrypted:
        labelencryptionicon->hide();
        encryptwalletaction->setchecked(false);
        changepassphraseaction->setenabled(false);
        encryptwalletaction->setenabled(true);
        break;
    case walletmodel::unlocked:
        labelencryptionicon->show();
        labelencryptionicon->setpixmap(singlecoloricon(":/icons/lock_open").pixmap(statusbar_iconsize,statusbar_iconsize));
        labelencryptionicon->settooltip(tr("wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptwalletaction->setchecked(true);
        changepassphraseaction->setenabled(true);
        encryptwalletaction->setenabled(false); // todo: decrypt currently not supported
        break;
    case walletmodel::locked:
        labelencryptionicon->show();
        labelencryptionicon->setpixmap(singlecoloricon(":/icons/lock_closed").pixmap(statusbar_iconsize,statusbar_iconsize));
        labelencryptionicon->settooltip(tr("wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptwalletaction->setchecked(true);
        changepassphraseaction->setenabled(true);
        encryptwalletaction->setenabled(false); // todo: decrypt currently not supported
        break;
    }
}
#endif // enable_wallet

void moorecoingui::shownormalifminimized(bool ftogglehidden)
{
    if(!clientmodel)
        return;

    // activatewindow() (sometimes) helps with keyboard focus on windows
    if (ishidden())
    {
        show();
        activatewindow();
    }
    else if (isminimized())
    {
        shownormal();
        activatewindow();
    }
    else if (guiutil::isobscured(this))
    {
        raise();
        activatewindow();
    }
    else if(ftogglehidden)
        hide();
}

void moorecoingui::togglehidden()
{
    shownormalifminimized(true);
}

void moorecoingui::detectshutdown()
{
    if (shutdownrequested())
    {
        if(rpcconsole)
            rpcconsole->hide();
        qapp->quit();
    }
}

void moorecoingui::showprogress(const qstring &title, int nprogress)
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

static bool threadsafemessagebox(moorecoingui *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & cclientuiinterface::modal);
    // the secure flag has no effect in the qt gui.
    // bool secure = (style & cclientuiinterface::secure);
    style &= ~cclientuiinterface::secure;
    bool ret = false;
    // in case of modal message, use blocking connection to wait for user to click a button
    qmetaobject::invokemethod(gui, "message",
                               modal ? guiutil::blockingguithreadconnection() : qt::queuedconnection,
                               q_arg(qstring, qstring::fromstdstring(caption)),
                               q_arg(qstring, qstring::fromstdstring(message)),
                               q_arg(unsigned int, style),
                               q_arg(bool*, &ret));
    return ret;
}

void moorecoingui::subscribetocoresignals()
{
    // connect signals to client
    uiinterface.threadsafemessagebox.connect(boost::bind(threadsafemessagebox, this, _1, _2, _3));
}

void moorecoingui::unsubscribefromcoresignals()
{
    // disconnect signals from client
    uiinterface.threadsafemessagebox.disconnect(boost::bind(threadsafemessagebox, this, _1, _2, _3));
}

unitdisplaystatusbarcontrol::unitdisplaystatusbarcontrol() :
    optionsmodel(0),
    menu(0)
{
    createcontextmenu();
    settooltip(tr("unit to show amounts in. click to select another unit."));
    qlist<moorecoinunits::unit> units = moorecoinunits::availableunits();
    int max_width = 0;
    const qfontmetrics fm(font());
    foreach (const moorecoinunits::unit unit, units)
    {
        max_width = qmax(max_width, fm.width(moorecoinunits::name(unit)));
    }
    setminimumsize(max_width, 0);
    setalignment(qt::alignright | qt::alignvcenter);
    setstylesheet(qstring("qlabel { color : %1 }").arg(singlecolor().name()));
}

/** so that it responds to button clicks */
void unitdisplaystatusbarcontrol::mousepressevent(qmouseevent *event)
{
    ondisplayunitsclicked(event->pos());
}

/** creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void unitdisplaystatusbarcontrol::createcontextmenu()
{
    menu = new qmenu();
    foreach(moorecoinunits::unit u, moorecoinunits::availableunits())
    {
        qaction *menuaction = new qaction(qstring(moorecoinunits::name(u)), this);
        menuaction->setdata(qvariant(u));
        menu->addaction(menuaction);
    }
    connect(menu,signal(triggered(qaction*)),this,slot(onmenuselection(qaction*)));
}

/** lets the control know about the options model (and its signals) */
void unitdisplaystatusbarcontrol::setoptionsmodel(optionsmodel *optionsmodel)
{
    if (optionsmodel)
    {
        this->optionsmodel = optionsmodel;

        // be aware of a display unit change reported by the optionsmodel object.
        connect(optionsmodel,signal(displayunitchanged(int)),this,slot(updatedisplayunit(int)));

        // initialize the display units label with the current value in the model.
        updatedisplayunit(optionsmodel->getdisplayunit());
    }
}

/** when display units are changed on optionsmodel it will refresh the display text of the control on the status bar */
void unitdisplaystatusbarcontrol::updatedisplayunit(int newunits)
{
    settext(moorecoinunits::name(newunits));
}

/** shows context menu with display unit options by the mouse coordinates */
void unitdisplaystatusbarcontrol::ondisplayunitsclicked(const qpoint& point)
{
    qpoint globalpos = maptoglobal(point);
    menu->exec(globalpos);
}

/** tells underlying optionsmodel to update its current display unit. */
void unitdisplaystatusbarcontrol::onmenuselection(qaction* action)
{
    if (action)
    {
        optionsmodel->setdisplayunit(action->data());
    }
}
