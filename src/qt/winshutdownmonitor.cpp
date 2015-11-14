// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "winshutdownmonitor.h"

#if defined(q_os_win) && qt_version >= 0x050000
#include "init.h"
#include "util.h"

#include <windows.h>

#include <qdebug>

#include <openssl/rand.h>

// if we don't want a message to be processed by qt, return true and set result to
// the value that the window procedure should return. otherwise return false.
bool winshutdownmonitor::nativeeventfilter(const qbytearray &eventtype, void *pmessage, long *pnresult)
{
       q_unused(eventtype);

       msg *pmsg = static_cast<msg *>(pmessage);

       // seed openssl prng with windows event data (e.g.  mouse movements and other user interactions)
       if (rand_event(pmsg->message, pmsg->wparam, pmsg->lparam) == 0) {
            // warn only once as this is performance-critical
            static bool warned = false;
            if (!warned) {
                logprint("%s: openssl rand_event() failed to seed openssl prng with enough data.\n", __func__);
                warned = true;
            }
       }

       switch(pmsg->message)
       {
           case wm_queryendsession:
           {
               // initiate a client shutdown after receiving a wm_queryendsession and block
               // windows session end until we have finished client shutdown.
               startshutdown();
               *pnresult = false;
               return true;
           }

           case wm_endsession:
           {
               *pnresult = false;
               return true;
           }
       }

       return false;
}

void winshutdownmonitor::registershutdownblockreason(const qstring& strreason, const hwnd& mainwinid)
{
    typedef bool (winapi *pshutdownbrcreate)(hwnd, lpcwstr);
    pshutdownbrcreate shutdownbrcreate = (pshutdownbrcreate)getprocaddress(getmodulehandlea("user32.dll"), "shutdownblockreasoncreate");
    if (shutdownbrcreate == null) {
        qwarning() << "registershutdownblockreason: getprocaddress for shutdownblockreasoncreate failed";
        return;
    }

    if (shutdownbrcreate(mainwinid, strreason.tostdwstring().c_str()))
        qwarning() << "registershutdownblockreason: successfully registered: " + strreason;
    else
        qwarning() << "registershutdownblockreason: failed to register: " + strreason;
}
#endif
