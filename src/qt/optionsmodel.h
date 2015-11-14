// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_optionsmodel_h
#define moorecoin_qt_optionsmodel_h

#include "amount.h"

#include <qabstractlistmodel>

qt_begin_namespace
class qnetworkproxy;
qt_end_namespace

/** interface from qt to configuration data structure for moorecoin client.
   to qt, the options are presented as a list with the different options
   laid out vertically.
   this can be changed to a tree once the settings become sufficiently
   complex.
 */
class optionsmodel : public qabstractlistmodel
{
    q_object

public:
    explicit optionsmodel(qobject *parent = 0);

    enum optionid {
        startatstartup,         // bool
        minimizetotray,         // bool
        mapportupnp,            // bool
        minimizeonclose,        // bool
        proxyuse,               // bool
        proxyip,                // qstring
        proxyport,              // int
        displayunit,            // moorecoinunits::unit
        thirdpartytxurls,       // qstring
        language,               // qstring
        coincontrolfeatures,    // bool
        threadsscriptverif,     // int
        databasecache,          // int
        spendzeroconfchange,    // bool
        listen,                 // bool
        optionidrowcount,
    };

    void init();
    void reset();

    int rowcount(const qmodelindex & parent = qmodelindex()) const;
    qvariant data(const qmodelindex & index, int role = qt::displayrole) const;
    bool setdata(const qmodelindex & index, const qvariant & value, int role = qt::editrole);
    /** updates current unit in memory, settings and emits displayunitchanged(newunit) signal */
    void setdisplayunit(const qvariant &value);

    /* explicit getters */
    bool getminimizetotray() { return fminimizetotray; }
    bool getminimizeonclose() { return fminimizeonclose; }
    int getdisplayunit() { return ndisplayunit; }
    qstring getthirdpartytxurls() { return strthirdpartytxurls; }
    bool getproxysettings(qnetworkproxy& proxy) const;
    bool getcoincontrolfeatures() { return fcoincontrolfeatures; }
    const qstring& getoverriddenbycommandline() { return stroverriddenbycommandline; }

    /* restart flag helper */
    void setrestartrequired(bool frequired);
    bool isrestartrequired();

private:
    /* qt-only settings */
    bool fminimizetotray;
    bool fminimizeonclose;
    qstring language;
    int ndisplayunit;
    qstring strthirdpartytxurls;
    bool fcoincontrolfeatures;
    /* settings that were overriden by command-line */
    qstring stroverriddenbycommandline;

    /// add option to list of gui options overridden through command line/config file
    void addoverriddenoption(const std::string &option);

signals:
    void displayunitchanged(int unit);
    void coincontrolfeatureschanged(bool);
};

#endif // moorecoin_qt_optionsmodel_h
