// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2012 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_ui_interface_h
#define moorecoin_ui_interface_h

#include <stdint.h>
#include <string>

#include <boost/signals2/last_value.hpp>
#include <boost/signals2/signal.hpp>

class cbasickeystore;
class cwallet;
class uint256;

/** general change type (added, updated, removed). */
enum changetype
{
    ct_new,
    ct_updated,
    ct_deleted
};

/** signals for ui communication. */
class cclientuiinterface
{
public:
    /** flags for cclientuiinterface::threadsafemessagebox */
    enum messageboxflags
    {
        icon_information    = 0,
        icon_warning        = (1u << 0),
        icon_error          = (1u << 1),
        /**
         * mask of all available icons in cclientuiinterface::messageboxflags
         * this needs to be updated, when icons are changed there!
         */
        icon_mask = (icon_information | icon_warning | icon_error),

        /** these values are taken from qmessagebox.h "enum standardbutton" to be directly usable */
        btn_ok      = 0x00000400u, // qmessagebox::ok
        btn_yes     = 0x00004000u, // qmessagebox::yes
        btn_no      = 0x00010000u, // qmessagebox::no
        btn_abort   = 0x00040000u, // qmessagebox::abort
        btn_retry   = 0x00080000u, // qmessagebox::retry
        btn_ignore  = 0x00100000u, // qmessagebox::ignore
        btn_close   = 0x00200000u, // qmessagebox::close
        btn_cancel  = 0x00400000u, // qmessagebox::cancel
        btn_discard = 0x00800000u, // qmessagebox::discard
        btn_help    = 0x01000000u, // qmessagebox::help
        btn_apply   = 0x02000000u, // qmessagebox::apply
        btn_reset   = 0x04000000u, // qmessagebox::reset
        /**
         * mask of all available buttons in cclientuiinterface::messageboxflags
         * this needs to be updated, when buttons are changed there!
         */
        btn_mask = (btn_ok | btn_yes | btn_no | btn_abort | btn_retry | btn_ignore |
                    btn_close | btn_cancel | btn_discard | btn_help | btn_apply | btn_reset),

        /** force blocking, modal message box dialog (not just os notification) */
        modal               = 0x10000000u,

        /** do not print contents of message to debug log */
        secure              = 0x40000000u,

        /** predefined combinations for certain default usage cases */
        msg_information = icon_information,
        msg_warning = (icon_warning | btn_ok | modal),
        msg_error = (icon_error | btn_ok | modal)
    };

    /** show message box. */
    boost::signals2::signal<bool (const std::string& message, const std::string& caption, unsigned int style), boost::signals2::last_value<bool> > threadsafemessagebox;

    /** progress message during initialization. */
    boost::signals2::signal<void (const std::string &message)> initmessage;

    /** number of network connections changed. */
    boost::signals2::signal<void (int newnumconnections)> notifynumconnectionschanged;

    /**
     * new, updated or cancelled alert.
     * @note called with lock cs_mapalerts held.
     */
    boost::signals2::signal<void (const uint256 &hash, changetype status)> notifyalertchanged;

    /** a wallet has been loaded. */
    boost::signals2::signal<void (cwallet* wallet)> loadwallet;

    /** show progress e.g. for verifychain */
    boost::signals2::signal<void (const std::string &title, int nprogress)> showprogress;

    /** new block has been accepted */
    boost::signals2::signal<void (const uint256& hash)> notifyblocktip;
};

extern cclientuiinterface uiinterface;

#endif // moorecoin_ui_interface_h
