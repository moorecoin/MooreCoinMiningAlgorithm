// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "noui.h"

#include "ui_interface.h"
#include "util.h"

#include <cstdio>
#include <stdint.h>
#include <string>

static bool noui_threadsafemessagebox(const std::string& message, const std::string& caption, unsigned int style)
{
    bool fsecure = style & cclientuiinterface::secure;
    style &= ~cclientuiinterface::secure;

    std::string strcaption;
    // check for usage of predefined caption
    switch (style) {
    case cclientuiinterface::msg_error:
        strcaption += _("error");
        break;
    case cclientuiinterface::msg_warning:
        strcaption += _("warning");
        break;
    case cclientuiinterface::msg_information:
        strcaption += _("information");
        break;
    default:
        strcaption += caption; // use supplied caption (can be empty)
    }

    if (!fsecure)
        logprintf("%s: %s\n", strcaption, message);
    fprintf(stderr, "%s: %s\n", strcaption.c_str(), message.c_str());
    return false;
}

static void noui_initmessage(const std::string& message)
{
    logprintf("init message: %s\n", message);
}

void noui_connect()
{
    // connect moorecoind signal handlers
    uiinterface.threadsafemessagebox.connect(noui_threadsafemessagebox);
    uiinterface.initmessage.connect(noui_initmessage);
}
