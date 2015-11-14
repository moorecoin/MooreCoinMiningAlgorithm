// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "optionsmodel.h"

#include "moorecoinunits.h"
#include "guiutil.h"

#include "amount.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "txdb.h" // for -dbcache defaults

#ifdef enable_wallet
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#endif

#include <qnetworkproxy>
#include <qsettings>
#include <qstringlist>

optionsmodel::optionsmodel(qobject *parent) :
    qabstractlistmodel(parent)
{
    init();
}

void optionsmodel::addoverriddenoption(const std::string &option)
{
    stroverriddenbycommandline += qstring::fromstdstring(option) + "=" + qstring::fromstdstring(mapargs[option]) + " ";
}

// writes all missing qsettings with their default values
void optionsmodel::init()
{
    qsettings settings;

    // ensure restart flag is unset on client startup
    setrestartrequired(false);

    // these are qt-only settings:

    // window
    if (!settings.contains("fminimizetotray"))
        settings.setvalue("fminimizetotray", false);
    fminimizetotray = settings.value("fminimizetotray").tobool();

    if (!settings.contains("fminimizeonclose"))
        settings.setvalue("fminimizeonclose", false);
    fminimizeonclose = settings.value("fminimizeonclose").tobool();

    // display
    if (!settings.contains("ndisplayunit"))
        settings.setvalue("ndisplayunit", moorecoinunits::btc);
    ndisplayunit = settings.value("ndisplayunit").toint();

    if (!settings.contains("strthirdpartytxurls"))
        settings.setvalue("strthirdpartytxurls", "");
    strthirdpartytxurls = settings.value("strthirdpartytxurls", "").tostring();

    if (!settings.contains("fcoincontrolfeatures"))
        settings.setvalue("fcoincontrolfeatures", false);
    fcoincontrolfeatures = settings.value("fcoincontrolfeatures", false).tobool();

    // these are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the gui settings.
    //
    // if setting doesn't exist create it with defaults.
    //
    // if softsetarg() or softsetboolarg() return false we were overridden
    // by command-line and show this in the ui.

    // main
    if (!settings.contains("ndatabasecache"))
        settings.setvalue("ndatabasecache", (qint64)ndefaultdbcache);
    if (!softsetarg("-dbcache", settings.value("ndatabasecache").tostring().tostdstring()))
        addoverriddenoption("-dbcache");

    if (!settings.contains("nthreadsscriptverif"))
        settings.setvalue("nthreadsscriptverif", default_scriptcheck_threads);
    if (!softsetarg("-par", settings.value("nthreadsscriptverif").tostring().tostdstring()))
        addoverriddenoption("-par");

    // wallet
#ifdef enable_wallet
    if (!settings.contains("bspendzeroconfchange"))
        settings.setvalue("bspendzeroconfchange", true);
    if (!softsetboolarg("-spendzeroconfchange", settings.value("bspendzeroconfchange").tobool()))
        addoverriddenoption("-spendzeroconfchange");
#endif

    // network
    if (!settings.contains("fuseupnp"))
        settings.setvalue("fuseupnp", default_upnp);
    if (!softsetboolarg("-upnp", settings.value("fuseupnp").tobool()))
        addoverriddenoption("-upnp");

    if (!settings.contains("flisten"))
        settings.setvalue("flisten", default_listen);
    if (!softsetboolarg("-listen", settings.value("flisten").tobool()))
        addoverriddenoption("-listen");

    if (!settings.contains("fuseproxy"))
        settings.setvalue("fuseproxy", false);
    if (!settings.contains("addrproxy"))
        settings.setvalue("addrproxy", "127.0.0.1:9050");
    // only try to set -proxy, if user has enabled fuseproxy
    if (settings.value("fuseproxy").tobool() && !softsetarg("-proxy", settings.value("addrproxy").tostring().tostdstring()))
        addoverriddenoption("-proxy");
    else if(!settings.value("fuseproxy").tobool() && !getarg("-proxy", "").empty())
        addoverriddenoption("-proxy");

    // display
    if (!settings.contains("language"))
        settings.setvalue("language", "");
    if (!softsetarg("-lang", settings.value("language").tostring().tostdstring()))
        addoverriddenoption("-lang");

    language = settings.value("language").tostring();
}

void optionsmodel::reset()
{
    qsettings settings;

    // remove all entries from our qsettings object
    settings.clear();

    // default setting for optionsmodel::startatstartup - disabled
    if (guiutil::getstartonsystemstartup())
        guiutil::setstartonsystemstartup(false);
}

int optionsmodel::rowcount(const qmodelindex & parent) const
{
    return optionidrowcount;
}

// read qsettings values and return them
qvariant optionsmodel::data(const qmodelindex & index, int role) const
{
    if(role == qt::editrole)
    {
        qsettings settings;
        switch(index.row())
        {
        case startatstartup:
            return guiutil::getstartonsystemstartup();
        case minimizetotray:
            return fminimizetotray;
        case mapportupnp:
#ifdef use_upnp
            return settings.value("fuseupnp");
#else
            return false;
#endif
        case minimizeonclose:
            return fminimizeonclose;

        // default proxy
        case proxyuse:
            return settings.value("fuseproxy", false);
        case proxyip: {
            // contains ip at index 0 and port at index 1
            qstringlist strlipport = settings.value("addrproxy").tostring().split(":", qstring::skipemptyparts);
            return strlipport.at(0);
        }
        case proxyport: {
            // contains ip at index 0 and port at index 1
            qstringlist strlipport = settings.value("addrproxy").tostring().split(":", qstring::skipemptyparts);
            return strlipport.at(1);
        }

#ifdef enable_wallet
        case spendzeroconfchange:
            return settings.value("bspendzeroconfchange");
#endif
        case displayunit:
            return ndisplayunit;
        case thirdpartytxurls:
            return strthirdpartytxurls;
        case language:
            return settings.value("language");
        case coincontrolfeatures:
            return fcoincontrolfeatures;
        case databasecache:
            return settings.value("ndatabasecache");
        case threadsscriptverif:
            return settings.value("nthreadsscriptverif");
        case listen:
            return settings.value("flisten");
        default:
            return qvariant();
        }
    }
    return qvariant();
}

// write qsettings values
bool optionsmodel::setdata(const qmodelindex & index, const qvariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == qt::editrole)
    {
        qsettings settings;
        switch(index.row())
        {
        case startatstartup:
            successful = guiutil::setstartonsystemstartup(value.tobool());
            break;
        case minimizetotray:
            fminimizetotray = value.tobool();
            settings.setvalue("fminimizetotray", fminimizetotray);
            break;
        case mapportupnp: // core option - can be changed on-the-fly
            settings.setvalue("fuseupnp", value.tobool());
            mapport(value.tobool());
            break;
        case minimizeonclose:
            fminimizeonclose = value.tobool();
            settings.setvalue("fminimizeonclose", fminimizeonclose);
            break;

        // default proxy
        case proxyuse:
            if (settings.value("fuseproxy") != value) {
                settings.setvalue("fuseproxy", value.tobool());
                setrestartrequired(true);
            }
            break;
        case proxyip: {
            // contains current ip at index 0 and current port at index 1
            qstringlist strlipport = settings.value("addrproxy").tostring().split(":", qstring::skipemptyparts);
            // if that key doesn't exist or has a changed ip
            if (!settings.contains("addrproxy") || strlipport.at(0) != value.tostring()) {
                // construct new value from new ip and current port
                qstring strnewvalue = value.tostring() + ":" + strlipport.at(1);
                settings.setvalue("addrproxy", strnewvalue);
                setrestartrequired(true);
            }
        }
        break;
        case proxyport: {
            // contains current ip at index 0 and current port at index 1
            qstringlist strlipport = settings.value("addrproxy").tostring().split(":", qstring::skipemptyparts);
            // if that key doesn't exist or has a changed port
            if (!settings.contains("addrproxy") || strlipport.at(1) != value.tostring()) {
                // construct new value from current ip and new port
                qstring strnewvalue = strlipport.at(0) + ":" + value.tostring();
                settings.setvalue("addrproxy", strnewvalue);
                setrestartrequired(true);
            }
        }
        break;
#ifdef enable_wallet
        case spendzeroconfchange:
            if (settings.value("bspendzeroconfchange") != value) {
                settings.setvalue("bspendzeroconfchange", value);
                setrestartrequired(true);
            }
            break;
#endif
        case displayunit:
            setdisplayunit(value);
            break;
        case thirdpartytxurls:
            if (strthirdpartytxurls != value.tostring()) {
                strthirdpartytxurls = value.tostring();
                settings.setvalue("strthirdpartytxurls", strthirdpartytxurls);
                setrestartrequired(true);
            }
            break;
        case language:
            if (settings.value("language") != value) {
                settings.setvalue("language", value);
                setrestartrequired(true);
            }
            break;
        case coincontrolfeatures:
            fcoincontrolfeatures = value.tobool();
            settings.setvalue("fcoincontrolfeatures", fcoincontrolfeatures);
            emit coincontrolfeatureschanged(fcoincontrolfeatures);
            break;
        case databasecache:
            if (settings.value("ndatabasecache") != value) {
                settings.setvalue("ndatabasecache", value);
                setrestartrequired(true);
            }
            break;
        case threadsscriptverif:
            if (settings.value("nthreadsscriptverif") != value) {
                settings.setvalue("nthreadsscriptverif", value);
                setrestartrequired(true);
            }
            break;
        case listen:
            if (settings.value("flisten") != value) {
                settings.setvalue("flisten", value);
                setrestartrequired(true);
            }
            break;
        default:
            break;
        }
    }

    emit datachanged(index, index);

    return successful;
}

/** updates current unit in memory, settings and emits displayunitchanged(newunit) signal */
void optionsmodel::setdisplayunit(const qvariant &value)
{
    if (!value.isnull())
    {
        qsettings settings;
        ndisplayunit = value.toint();
        settings.setvalue("ndisplayunit", ndisplayunit);
        emit displayunitchanged(ndisplayunit);
    }
}

bool optionsmodel::getproxysettings(qnetworkproxy& proxy) const
{
    // directly query current base proxy, because
    // gui settings can be overridden with -proxy.
    proxytype curproxy;
    if (getproxy(net_ipv4, curproxy)) {
        proxy.settype(qnetworkproxy::socks5proxy);
        proxy.sethostname(qstring::fromstdstring(curproxy.proxy.tostringip()));
        proxy.setport(curproxy.proxy.getport());

        return true;
    }
    else
        proxy.settype(qnetworkproxy::noproxy);

    return false;
}

void optionsmodel::setrestartrequired(bool frequired)
{
    qsettings settings;
    return settings.setvalue("frestartrequired", frequired);
}

bool optionsmodel::isrestartrequired()
{
    qsettings settings;
    return settings.value("frestartrequired", false).tobool();
}
