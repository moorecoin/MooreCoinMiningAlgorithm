// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_networkstyle_h
#define moorecoin_qt_networkstyle_h

#include <qicon>
#include <qpixmap>
#include <qstring>

/* coin network-specific gui style information */
class networkstyle
{
public:
    /** get style associated with provided bip70 network id, or 0 if not known */
    static const networkstyle *instantiate(const qstring &networkid);

    const qstring &getappname() const { return appname; }
    const qicon &getappicon() const { return appicon; }
    const qicon &gettrayandwindowicon() const { return trayandwindowicon; }
    const qstring &gettitleaddtext() const { return titleaddtext; }

private:
    networkstyle(const qstring &appname, const int iconcolorhueshift, const int iconcolorsaturationreduction, const char *titleaddtext);

    qstring appname;
    qicon appicon;
    qicon trayandwindowicon;
    qstring titleaddtext;
};

#endif // moorecoin_qt_networkstyle_h
