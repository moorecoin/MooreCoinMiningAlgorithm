// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_notificator_h
#define moorecoin_qt_notificator_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include <qicon>
#include <qobject>

qt_begin_namespace
class qsystemtrayicon;

#ifdef use_dbus
class qdbusinterface;
#endif
qt_end_namespace

/** cross-platform desktop notification client. */
class notificator: public qobject
{
    q_object

public:
    /** create a new notificator.
       @note ownership of trayicon is not transferred to this object.
    */
    notificator(const qstring &programname, qsystemtrayicon *trayicon, qwidget *parent);
    ~notificator();

    // message class
    enum class
    {
        information,    /**< informational message */
        warning,        /**< notify user of potential problem */
        critical        /**< an error occurred */
    };

public slots:
    /** show notification message.
       @param[in] cls    general message class
       @param[in] title  title shown with message
       @param[in] text   message content
       @param[in] icon   optional icon to show with message
       @param[in] millistimeout notification timeout in milliseconds (defaults to 10 seconds)
       @note platform implementations are free to ignore any of the provided fields except for \a text.
     */
    void notify(class cls, const qstring &title, const qstring &text,
                const qicon &icon = qicon(), int millistimeout = 10000);

private:
    qwidget *parent;
    enum mode {
        none,                       /**< ignore informational notifications, and show a modal pop-up dialog for critical notifications. */
        freedesktop,                /**< use dbus org.freedesktop.notifications */
        qsystemtray,                /**< use qsystemtray::showmessage */
        growl12,                    /**< use the growl 1.2 notification system (mac only) */
        growl13,                    /**< use the growl 1.3 notification system (mac only) */
        usernotificationcenter      /**< use the 10.8+ user notification center (mac only) */
    };
    qstring programname;
    mode mode;
    qsystemtrayicon *trayicon;
#ifdef use_dbus
    qdbusinterface *interface;

    void notifydbus(class cls, const qstring &title, const qstring &text, const qicon &icon, int millistimeout);
#endif
    void notifysystray(class cls, const qstring &title, const qstring &text, const qicon &icon, int millistimeout);
#ifdef q_os_mac
    void notifygrowl(class cls, const qstring &title, const qstring &text, const qicon &icon);
    void notifymacusernotificationcenter(class cls, const qstring &title, const qstring &text, const qicon &icon);
#endif
};

#endif // moorecoin_qt_notificator_h
