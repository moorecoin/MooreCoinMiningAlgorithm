// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_winshutdownmonitor_h
#define moorecoin_qt_winshutdownmonitor_h

#ifdef win32
#include <qbytearray>
#include <qstring>

#if qt_version >= 0x050000
#include <windef.h> // for hwnd

#include <qabstractnativeeventfilter>

class winshutdownmonitor : public qabstractnativeeventfilter
{
public:
    /** implements qabstractnativeeventfilter interface for processing windows messages */
    bool nativeeventfilter(const qbytearray &eventtype, void *pmessage, long *pnresult);

    /** register the reason for blocking shutdown on windows to allow clean client exit */
    static void registershutdownblockreason(const qstring& strreason, const hwnd& mainwinid);
};
#endif
#endif

#endif // moorecoin_qt_winshutdownmonitor_h
