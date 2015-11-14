// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "moorecoingui.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "intro.h"
#include "networkstyle.h"
#include "optionsmodel.h"
#include "splashscreen.h"
#include "utilitydialog.h"
#include "winshutdownmonitor.h"

#ifdef enable_wallet
#include "paymentserver.h"
#include "walletmodel.h"
#endif

#include "init.h"
#include "main.h"
#include "rpcserver.h"
#include "scheduler.h"
#include "ui_interface.h"
#include "util.h"

#ifdef enable_wallet
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/filesystem/operations.hpp>
#include <boost/thread.hpp>

#include <qapplication>
#include <qdebug>
#include <qlibraryinfo>
#include <qlocale>
#include <qmessagebox>
#include <qsettings>
#include <qthread>
#include <qtimer>
#include <qtranslator>

#if defined(qt_staticplugin)
#include <qtplugin>
#if qt_version < 0x050000
q_import_plugin(qcncodecs)
q_import_plugin(qjpcodecs)
q_import_plugin(qtwcodecs)
q_import_plugin(qkrcodecs)
q_import_plugin(qtaccessiblewidgets)
#else
q_import_plugin(accessiblefactory)
#if defined(qt_qpa_platform_xcb)
q_import_plugin(qxcbintegrationplugin);
#elif defined(qt_qpa_platform_windows)
q_import_plugin(qwindowsintegrationplugin);
#elif defined(qt_qpa_platform_cocoa)
q_import_plugin(qcocoaintegrationplugin);
#endif
#endif
#endif

#if qt_version < 0x050000
#include <qtextcodec>
#endif

// declare meta types used for qmetaobject::invokemethod
q_declare_metatype(bool*)
q_declare_metatype(camount)

static void initmessage(const std::string &message)
{
    logprintf("init message: %s\n", message);
}

/*
   translate string to current locale using qt.
 */
static std::string translate(const char* psz)
{
    return qcoreapplication::translate("moorecoin-core", psz).tostdstring();
}

static qstring getlangterritory()
{
    qsettings settings;
    // get desired locale (e.g. "de_de")
    // 1) system default language
    qstring lang_territory = qlocale::system().name();
    // 2) language from qsettings
    qstring lang_territory_qsettings = settings.value("language", "").tostring();
    if(!lang_territory_qsettings.isempty())
        lang_territory = lang_territory_qsettings;
    // 3) -lang command line argument
    lang_territory = qstring::fromstdstring(getarg("-lang", lang_territory.tostdstring()));
    return lang_territory;
}

/** set up translations */
static void inittranslations(qtranslator &qttranslatorbase, qtranslator &qttranslator, qtranslator &translatorbase, qtranslator &translator)
{
    // remove old translators
    qapplication::removetranslator(&qttranslatorbase);
    qapplication::removetranslator(&qttranslator);
    qapplication::removetranslator(&translatorbase);
    qapplication::removetranslator(&translator);

    // get desired locale (e.g. "de_de")
    // 1) system default language
    qstring lang_territory = getlangterritory();

    // convert to "de" only by truncating "_de"
    qstring lang = lang_territory;
    lang.truncate(lang_territory.lastindexof('_'));

    // load language files for configured locale:
    // - first load the translator for the base language, without territory
    // - then load the more specific locale translator

    // load e.g. qt_de.qm
    if (qttranslatorbase.load("qt_" + lang, qlibraryinfo::location(qlibraryinfo::translationspath)))
        qapplication::installtranslator(&qttranslatorbase);

    // load e.g. qt_de_de.qm
    if (qttranslator.load("qt_" + lang_territory, qlibraryinfo::location(qlibraryinfo::translationspath)))
        qapplication::installtranslator(&qttranslator);

    // load e.g. moorecoin_de.qm (shortcut "de" needs to be defined in moorecoin.qrc)
    if (translatorbase.load(lang, ":/translations/"))
        qapplication::installtranslator(&translatorbase);

    // load e.g. moorecoin_de_de.qm (shortcut "de_de" needs to be defined in moorecoin.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        qapplication::installtranslator(&translator);
}

/* qdebug() message handler --> debug.log */
#if qt_version < 0x050000
void debugmessagehandler(qtmsgtype type, const char *msg)
{
    const char *category = (type == qtdebugmsg) ? "qt" : null;
    logprint(category, "gui: %s\n", msg);
}
#else
void debugmessagehandler(qtmsgtype type, const qmessagelogcontext& context, const qstring &msg)
{
    q_unused(context);
    const char *category = (type == qtdebugmsg) ? "qt" : null;
    logprint(category, "gui: %s\n", msg.tostdstring());
}
#endif

/** class encapsulating moorecoin core startup and shutdown.
 * allows running startup and shutdown in a different thread from the ui thread.
 */
class moorecoincore: public qobject
{
    q_object
public:
    explicit moorecoincore();

public slots:
    void initialize();
    void shutdown();

signals:
    void initializeresult(int retval);
    void shutdownresult(int retval);
    void runawayexception(const qstring &message);

private:
    boost::thread_group threadgroup;
    cscheduler scheduler;

    /// pass fatal exception message to ui thread
    void handlerunawayexception(const std::exception *e);
};

/** main moorecoin application object */
class moorecoinapplication: public qapplication
{
    q_object
public:
    explicit moorecoinapplication(int &argc, char **argv);
    ~moorecoinapplication();

#ifdef enable_wallet
    /// create payment server
    void createpaymentserver();
#endif
    /// create options model
    void createoptionsmodel();
    /// create main window
    void createwindow(const networkstyle *networkstyle);
    /// create splash screen
    void createsplashscreen(const networkstyle *networkstyle);

    /// request core initialization
    void requestinitialize();
    /// request core shutdown
    void requestshutdown();

    /// get process return value
    int getreturnvalue() { return returnvalue; }

    /// get window identifier of qmainwindow (moorecoingui)
    wid getmainwinid() const;

public slots:
    void initializeresult(int retval);
    void shutdownresult(int retval);
    /// handle runaway exceptions. shows a message box with the problem and quits the program.
    void handlerunawayexception(const qstring &message);

signals:
    void requestedinitialize();
    void requestedshutdown();
    void stopthread();
    void splashfinished(qwidget *window);

private:
    qthread *corethread;
    optionsmodel *optionsmodel;
    clientmodel *clientmodel;
    moorecoingui *window;
    qtimer *pollshutdowntimer;
#ifdef enable_wallet
    paymentserver* paymentserver;
    walletmodel *walletmodel;
#endif
    int returnvalue;

    void startthread();
};

#include "moorecoin.moc"

moorecoincore::moorecoincore():
    qobject()
{
}

void moorecoincore::handlerunawayexception(const std::exception *e)
{
    printexceptioncontinue(e, "runaway exception");
    emit runawayexception(qstring::fromstdstring(strmiscwarning));
}

void moorecoincore::initialize()
{
    try
    {
        qdebug() << __func__ << ": running appinit2 in thread";
        int rv = appinit2(threadgroup, scheduler);
        if(rv)
        {
            /* start a dummy rpc thread if no rpc thread is active yet
             * to handle timeouts.
             */
            startdummyrpcthread();
        }
        emit initializeresult(rv);
    } catch (const std::exception& e) {
        handlerunawayexception(&e);
    } catch (...) {
        handlerunawayexception(null);
    }
}

void moorecoincore::shutdown()
{
    try
    {
        qdebug() << __func__ << ": running shutdown in thread";
        threadgroup.interrupt_all();
        threadgroup.join_all();
        shutdown();
        qdebug() << __func__ << ": shutdown finished";
        emit shutdownresult(1);
    } catch (const std::exception& e) {
        handlerunawayexception(&e);
    } catch (...) {
        handlerunawayexception(null);
    }
}

moorecoinapplication::moorecoinapplication(int &argc, char **argv):
    qapplication(argc, argv),
    corethread(0),
    optionsmodel(0),
    clientmodel(0),
    window(0),
    pollshutdowntimer(0),
#ifdef enable_wallet
    paymentserver(0),
    walletmodel(0),
#endif
    returnvalue(0)
{
    setquitonlastwindowclosed(false);
}

moorecoinapplication::~moorecoinapplication()
{
    if(corethread)
    {
        qdebug() << __func__ << ": stopping thread";
        emit stopthread();
        corethread->wait();
        qdebug() << __func__ << ": stopped thread";
    }

    delete window;
    window = 0;
#ifdef enable_wallet
    delete paymentserver;
    paymentserver = 0;
#endif
    delete optionsmodel;
    optionsmodel = 0;
}

#ifdef enable_wallet
void moorecoinapplication::createpaymentserver()
{
    paymentserver = new paymentserver(this);
}
#endif

void moorecoinapplication::createoptionsmodel()
{
    optionsmodel = new optionsmodel();
}

void moorecoinapplication::createwindow(const networkstyle *networkstyle)
{
    window = new moorecoingui(networkstyle, 0);

    pollshutdowntimer = new qtimer(window);
    connect(pollshutdowntimer, signal(timeout()), window, slot(detectshutdown()));
    pollshutdowntimer->start(200);
}

void moorecoinapplication::createsplashscreen(const networkstyle *networkstyle)
{
    splashscreen *splash = new splashscreen(0, networkstyle);
    // we don't hold a direct pointer to the splash screen after creation, so use
    // qt::wa_deleteonclose to make sure that the window will be deleted eventually.
    splash->setattribute(qt::wa_deleteonclose);
    splash->show();
    connect(this, signal(splashfinished(qwidget*)), splash, slot(slotfinish(qwidget*)));
}

void moorecoinapplication::startthread()
{
    if(corethread)
        return;
    corethread = new qthread(this);
    moorecoincore *executor = new moorecoincore();
    executor->movetothread(corethread);

    /*  communication to and from thread */
    connect(executor, signal(initializeresult(int)), this, slot(initializeresult(int)));
    connect(executor, signal(shutdownresult(int)), this, slot(shutdownresult(int)));
    connect(executor, signal(runawayexception(qstring)), this, slot(handlerunawayexception(qstring)));
    connect(this, signal(requestedinitialize()), executor, slot(initialize()));
    connect(this, signal(requestedshutdown()), executor, slot(shutdown()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, signal(stopthread()), executor, slot(deletelater()));
    connect(this, signal(stopthread()), corethread, slot(quit()));

    corethread->start();
}

void moorecoinapplication::requestinitialize()
{
    qdebug() << __func__ << ": requesting initialize";
    startthread();
    emit requestedinitialize();
}

void moorecoinapplication::requestshutdown()
{
    qdebug() << __func__ << ": requesting shutdown";
    startthread();
    window->hide();
    window->setclientmodel(0);
    pollshutdowntimer->stop();

#ifdef enable_wallet
    window->removeallwallets();
    delete walletmodel;
    walletmodel = 0;
#endif
    delete clientmodel;
    clientmodel = 0;

    // show a simple window indicating shutdown status
    shutdownwindow::showshutdownwindow(window);

    // request shutdown from core thread
    emit requestedshutdown();
}

void moorecoinapplication::initializeresult(int retval)
{
    qdebug() << __func__ << ": initialization result: " << retval;
    // set exit result: 0 if successful, 1 if failure
    returnvalue = retval ? 0 : 1;
    if(retval)
    {
#ifdef enable_wallet
        paymentserver::loadrootcas();
        paymentserver->setoptionsmodel(optionsmodel);
#endif

        clientmodel = new clientmodel(optionsmodel);
        window->setclientmodel(clientmodel);

#ifdef enable_wallet
        if(pwalletmain)
        {
            walletmodel = new walletmodel(pwalletmain, optionsmodel);

            window->addwallet(moorecoingui::default_wallet, walletmodel);
            window->setcurrentwallet(moorecoingui::default_wallet);

            connect(walletmodel, signal(coinssent(cwallet*,sendcoinsrecipient,qbytearray)),
                             paymentserver, slot(fetchpaymentack(cwallet*,const sendcoinsrecipient&,qbytearray)));
        }
#endif

        // if -min option passed, start window minimized.
        if(getboolarg("-min", false))
        {
            window->showminimized();
        }
        else
        {
            window->show();
        }
        emit splashfinished(window);

#ifdef enable_wallet
        // now that initialization/startup is done, process any command-line
        // moorecoin: uris or payment requests:
        connect(paymentserver, signal(receivedpaymentrequest(sendcoinsrecipient)),
                         window, slot(handlepaymentrequest(sendcoinsrecipient)));
        connect(window, signal(receiveduri(qstring)),
                         paymentserver, slot(handleuriorfile(qstring)));
        connect(paymentserver, signal(message(qstring,qstring,unsigned int)),
                         window, slot(message(qstring,qstring,unsigned int)));
        qtimer::singleshot(100, paymentserver, slot(uiready()));
#endif
    } else {
        quit(); // exit main loop
    }
}

void moorecoinapplication::shutdownresult(int retval)
{
    qdebug() << __func__ << ": shutdown result: " << retval;
    quit(); // exit main loop after shutdown finished
}

void moorecoinapplication::handlerunawayexception(const qstring &message)
{
    qmessagebox::critical(0, "runaway exception", moorecoingui::tr("a fatal error occurred. moorecoin can no longer continue safely and will quit.") + qstring("\n\n") + message);
    ::exit(1);
}

wid moorecoinapplication::getmainwinid() const
{
    if (!window)
        return 0;

    return window->winid();
}

#ifndef moorecoin_qt_test
int main(int argc, char *argv[])
{
    setupenvironment();

    /// 1. parse command-line options. these take precedence over anything else.
    // command-line options take precedence:
    parseparameters(argc, argv);

    // do not refer to data directory yet, this can be overridden by intro::pickdatadirectory

    /// 2. basic qt initialization (not dependent on parameters or configuration)
#if qt_version < 0x050000
    // internal string conversion is all utf-8
    qtextcodec::setcodecfortr(qtextcodec::codecforname("utf-8"));
    qtextcodec::setcodecforcstrings(qtextcodec::codecfortr());
#endif

    q_init_resource(moorecoin);
    q_init_resource(moorecoin_locale);

    moorecoinapplication app(argc, argv);
#if qt_version > 0x050100
    // generate high-dpi pixmaps
    qapplication::setattribute(qt::aa_usehighdpipixmaps);
#endif
#ifdef q_os_mac
    qapplication::setattribute(qt::aa_dontshowiconsinmenus);
#endif

    // register meta types used for qmetaobject::invokemethod
    qregistermetatype< bool* >();
    //   need to pass name here as camount is a typedef (see http://qt-project.org/doc/qt-5/qmetatype.html#qregistermetatype)
    //   important if it is no longer a typedef use the normal variant above
    qregistermetatype< camount >("camount");

    /// 3. application identification
    // must be set before optionsmodel is initialized or translations are loaded,
    // as it is used to locate qsettings
    qapplication::setorganizationname(qapp_org_name);
    qapplication::setorganizationdomain(qapp_org_domain);
    qapplication::setapplicationname(qapp_app_name_default);
    guiutil::substitutefonts(getlangterritory());

    /// 4. initialization of translations, so that intro dialog is in user's language
    // now that qsettings are accessible, initialize translations
    qtranslator qttranslatorbase, qttranslator, translatorbase, translator;
    inittranslations(qttranslatorbase, qttranslator, translatorbase, translator);
    translationinterface.translate.connect(translate);

    // show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (mapargs.count("-?") || mapargs.count("-help") || mapargs.count("-version"))
    {
        helpmessagedialog help(null, mapargs.count("-version"));
        help.showorprint();
        return 1;
    }

    /// 5. now that settings and translations are available, ask user for data directory
    // user language is set up: pick a data directory
    intro::pickdatadirectory();

    /// 6. determine availability of data directory and parse moorecoin.conf
    /// - do not call getdatadir(true) before this step finishes
    if (!boost::filesystem::is_directory(getdatadir(false)))
    {
        qmessagebox::critical(0, qobject::tr("moorecoin core"),
                              qobject::tr("error: specified data directory \"%1\" does not exist.").arg(qstring::fromstdstring(mapargs["-datadir"])));
        return 1;
    }
    try {
        readconfigfile(mapargs, mapmultiargs);
    } catch (const std::exception& e) {
        qmessagebox::critical(0, qobject::tr("moorecoin core"),
                              qobject::tr("error: cannot parse configuration file: %1. only use key=value syntax.").arg(e.what()));
        return false;
    }

    /// 7. determine network (and switch to network specific options)
    // - do not call params() before this step
    // - do this after parsing the configuration file, as the network can be switched there
    // - qsettings() will use the new application name after this, resulting in network-specific settings
    // - needs to be done before createoptionsmodel

    // check for -testnet or -regtest parameter (params() calls are only valid after this clause)
    if (!selectparamsfromcommandline()) {
        qmessagebox::critical(0, qobject::tr("moorecoin core"), qobject::tr("error: invalid combination of -regtest and -testnet."));
        return 1;
    }
#ifdef enable_wallet
    // parse uris on command line -- this can affect params()
    paymentserver::ipcparsecommandline(argc, argv);
#endif

    qscopedpointer<const networkstyle> networkstyle(networkstyle::instantiate(qstring::fromstdstring(params().networkidstring())));
    assert(!networkstyle.isnull());
    // allow for separate ui settings for testnets
    qapplication::setapplicationname(networkstyle->getappname());
    // re-initialize translations after changing application name (language in network-specific settings can be different)
    inittranslations(qttranslatorbase, qttranslator, translatorbase, translator);

#ifdef enable_wallet
    /// 8. uri ipc sending
    // - do this early as we don't want to bother initializing if we are just calling ipc
    // - do this *after* setting up the data directory, as the data directory hash is used in the name
    // of the server.
    // - do this after creating app and setting up translations, so errors are
    // translated properly.
    if (paymentserver::ipcsendcommandline())
        exit(0);

    // start up the payment server early, too, so impatient users that click on
    // moorecoin: links repeatedly have their payment requests routed to this process:
    app.createpaymentserver();
#endif

    /// 9. main gui initialization
    // install global event filter that makes sure that long tooltips can be word-wrapped
    app.installeventfilter(new guiutil::tooltiptorichtextfilter(tooltip_wrap_threshold, &app));
#if qt_version < 0x050000
    // install qdebug() message handler to route to debug.log
    qinstallmsghandler(debugmessagehandler);
#else
#if defined(q_os_win)
    // install global event filter for processing windows session related windows messages (wm_queryendsession and wm_endsession)
    qapp->installnativeeventfilter(new winshutdownmonitor());
#endif
    // install qdebug() message handler to route to debug.log
    qinstallmessagehandler(debugmessagehandler);
#endif
    // load gui settings from qsettings
    app.createoptionsmodel();

    // subscribe to global signals from core
    uiinterface.initmessage.connect(initmessage);

    if (getboolarg("-splash", true) && !getboolarg("-min", false))
        app.createsplashscreen(networkstyle.data());

    try
    {
        app.createwindow(networkstyle.data());
        app.requestinitialize();
#if defined(q_os_win) && qt_version >= 0x050000
        winshutdownmonitor::registershutdownblockreason(qobject::tr("moorecoin core didn't yet exit safely..."), (hwnd)app.getmainwinid());
#endif
        app.exec();
        app.requestshutdown();
        app.exec();
    } catch (const std::exception& e) {
        printexceptioncontinue(&e, "runaway exception");
        app.handlerunawayexception(qstring::fromstdstring(strmiscwarning));
    } catch (...) {
        printexceptioncontinue(null, "runaway exception");
        app.handlerunawayexception(qstring::fromstdstring(strmiscwarning));
    }
    return app.getreturnvalue();
}
#endif // moorecoin_qt_test
