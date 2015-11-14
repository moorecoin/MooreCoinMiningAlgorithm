// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "notificator.h"

#include <qapplication>
#include <qbytearray>
#include <qicon>
#include <qimagewriter>
#include <qmessagebox>
#include <qmetatype>
#include <qstyle>
#include <qsystemtrayicon>
#include <qtemporaryfile>
#include <qvariant>
#ifdef use_dbus
#include <stdint.h>
#include <qtdbus>
#endif
// include applicationservices.h after qtdbus to avoid redefinition of check().
// this affects at least osx 10.6. see /usr/include/assertmacros.h for details.
// note: this could also be worked around using:
// #define __assert_macros_define_versions_without_underscores 0
#ifdef q_os_mac
#include <applicationservices/applicationservices.h>
#include "macnotificationhandler.h"
#endif


#ifdef use_dbus
// https://wiki.ubuntu.com/notificationdevelopmentguidelines recommends at least 128
const int freedesktop_notification_icon_size = 128;
#endif

notificator::notificator(const qstring &programname, qsystemtrayicon *trayicon, qwidget *parent) :
    qobject(parent),
    parent(parent),
    programname(programname),
    mode(none),
    trayicon(trayicon)
#ifdef use_dbus
    ,interface(0)
#endif
{
    if(trayicon && trayicon->supportsmessages())
    {
        mode = qsystemtray;
    }
#ifdef use_dbus
    interface = new qdbusinterface("org.freedesktop.notifications",
        "/org/freedesktop/notifications", "org.freedesktop.notifications");
    if(interface->isvalid())
    {
        mode = freedesktop;
    }
#endif
#ifdef q_os_mac
    // check if users os has support for nsusernotification
    if( macnotificationhandler::instance()->hasusernotificationcentersupport()) {
        mode = usernotificationcenter;
    }
    else {
        // check if growl is installed (based on qt's tray icon implementation)
        cfurlref cfurl;
        osstatus status = lsgetapplicationforinfo(klsunknowntype, klsunknowncreator, cfstr("growlticket"), klsrolesall, 0, &cfurl);
        if (status != klsapplicationnotfounderr) {
            cfbundleref bundle = cfbundlecreate(0, cfurl);
            if (cfstringcompare(cfbundlegetidentifier(bundle), cfstr("com.growl.growlhelperapp"), kcfcomparecaseinsensitive | kcfcomparebackwards) == kcfcompareequalto) {
                if (cfstringhassuffix(cfurlgetstring(cfurl), cfstr("/growl.app/")))
                    mode = growl13;
                else
                    mode = growl12;
            }
            cfrelease(cfurl);
            cfrelease(bundle);
        }
    }
#endif
}

notificator::~notificator()
{
#ifdef use_dbus
    delete interface;
#endif
}

#ifdef use_dbus

// loosely based on http://www.qtcentre.org/archive/index.php/t-25879.html
class freedesktopimage
{
public:
    freedesktopimage() {}
    freedesktopimage(const qimage &img);

    static int metatype();

    // image to variant that can be marshalled over dbus
    static qvariant tovariant(const qimage &img);

private:
    int width, height, stride;
    bool hasalpha;
    int channels;
    int bitspersample;
    qbytearray image;

    friend qdbusargument &operator<<(qdbusargument &a, const freedesktopimage &i);
    friend const qdbusargument &operator>>(const qdbusargument &a, freedesktopimage &i);
};

q_declare_metatype(freedesktopimage);

// image configuration settings
const int channels = 4;
const int bytes_per_pixel = 4;
const int bits_per_sample = 8;

freedesktopimage::freedesktopimage(const qimage &img):
    width(img.width()),
    height(img.height()),
    stride(img.width() * bytes_per_pixel),
    hasalpha(true),
    channels(channels),
    bitspersample(bits_per_sample)
{
    // convert 00xaarrggbb to rgba bytewise (endian-independent) format
    qimage tmp = img.converttoformat(qimage::format_argb32);
    const uint32_t *data = reinterpret_cast<const uint32_t*>(tmp.bits());

    unsigned int num_pixels = width * height;
    image.resize(num_pixels * bytes_per_pixel);

    for(unsigned int ptr = 0; ptr < num_pixels; ++ptr)
    {
        image[ptr*bytes_per_pixel+0] = data[ptr] >> 16; // r
        image[ptr*bytes_per_pixel+1] = data[ptr] >> 8;  // g
        image[ptr*bytes_per_pixel+2] = data[ptr];       // b
        image[ptr*bytes_per_pixel+3] = data[ptr] >> 24; // a
    }
}

qdbusargument &operator<<(qdbusargument &a, const freedesktopimage &i)
{
    a.beginstructure();
    a << i.width << i.height << i.stride << i.hasalpha << i.bitspersample << i.channels << i.image;
    a.endstructure();
    return a;
}

const qdbusargument &operator>>(const qdbusargument &a, freedesktopimage &i)
{
    a.beginstructure();
    a >> i.width >> i.height >> i.stride >> i.hasalpha >> i.bitspersample >> i.channels >> i.image;
    a.endstructure();
    return a;
}

int freedesktopimage::metatype()
{
    return qdbusregistermetatype<freedesktopimage>();
}

qvariant freedesktopimage::tovariant(const qimage &img)
{
    freedesktopimage fimg(img);
    return qvariant(freedesktopimage::metatype(), &fimg);
}

void notificator::notifydbus(class cls, const qstring &title, const qstring &text, const qicon &icon, int millistimeout)
{
    q_unused(cls);
    // arguments for dbus call:
    qlist<qvariant> args;

    // program name:
    args.append(programname);

    // unique id of this notification type:
    args.append(0u);

    // application icon, empty string
    args.append(qstring());

    // summary
    args.append(title);

    // body
    args.append(text);

    // actions (none, actions are deprecated)
    qstringlist actions;
    args.append(actions);

    // hints
    qvariantmap hints;

    // if no icon specified, set icon based on class
    qicon tmpicon;
    if(icon.isnull())
    {
        qstyle::standardpixmap sicon = qstyle::sp_messageboxquestion;
        switch(cls)
        {
        case information: sicon = qstyle::sp_messageboxinformation; break;
        case warning: sicon = qstyle::sp_messageboxwarning; break;
        case critical: sicon = qstyle::sp_messageboxcritical; break;
        default: break;
        }
        tmpicon = qapplication::style()->standardicon(sicon);
    }
    else
    {
        tmpicon = icon;
    }
    hints["icon_data"] = freedesktopimage::tovariant(tmpicon.pixmap(freedesktop_notification_icon_size).toimage());
    args.append(hints);

    // timeout (in msec)
    args.append(millistimeout);

    // "fire and forget"
    interface->callwithargumentlist(qdbus::noblock, "notify", args);
}
#endif

void notificator::notifysystray(class cls, const qstring &title, const qstring &text, const qicon &icon, int millistimeout)
{
    q_unused(icon);
    qsystemtrayicon::messageicon sicon = qsystemtrayicon::noicon;
    switch(cls) // set icon based on class
    {
    case information: sicon = qsystemtrayicon::information; break;
    case warning: sicon = qsystemtrayicon::warning; break;
    case critical: sicon = qsystemtrayicon::critical; break;
    }
    trayicon->showmessage(title, text, sicon, millistimeout);
}

// based on qt's tray icon implementation
#ifdef q_os_mac
void notificator::notifygrowl(class cls, const qstring &title, const qstring &text, const qicon &icon)
{
    const qstring script(
        "tell application \"%5\"\n"
        "  set the allnotificationslist to {\"notification\"}\n" // -- make a list of all the notification types (all)
        "  set the enablednotificationslist to {\"notification\"}\n" // -- make a list of the notifications (enabled)
        "  register as application \"%1\" all notifications allnotificationslist default notifications enablednotificationslist\n" // -- register our script with growl
        "  notify with name \"notification\" title \"%2\" description \"%3\" application name \"%1\"%4\n" // -- send a notification
        "end tell"
    );

    qstring notificationapp(qapplication::applicationname());
    if (notificationapp.isempty())
        notificationapp = "application";

    qpixmap notificationiconpixmap;
    if (icon.isnull()) { // if no icon specified, set icon based on class
        qstyle::standardpixmap sicon = qstyle::sp_messageboxquestion;
        switch (cls)
        {
        case information: sicon = qstyle::sp_messageboxinformation; break;
        case warning: sicon = qstyle::sp_messageboxwarning; break;
        case critical: sicon = qstyle::sp_messageboxcritical; break;
        }
        notificationiconpixmap = qapplication::style()->standardpixmap(sicon);
    }
    else {
        qsize size = icon.actualsize(qsize(48, 48));
        notificationiconpixmap = icon.pixmap(size);
    }

    qstring notificationicon;
    qtemporaryfile notificationiconfile;
    if (!notificationiconpixmap.isnull() && notificationiconfile.open()) {
        qimagewriter writer(&notificationiconfile, "png");
        if (writer.write(notificationiconpixmap.toimage()))
            notificationicon = qstring(" image from location \"file://%1\"").arg(notificationiconfile.filename());
    }

    qstring quotedtitle(title), quotedtext(text);
    quotedtitle.replace("\\", "\\\\").replace("\"", "\\");
    quotedtext.replace("\\", "\\\\").replace("\"", "\\");
    qstring growlapp(this->mode == notificator::growl13 ? "growl" : "growlhelperapp");
    macnotificationhandler::instance()->sendapplescript(script.arg(notificationapp, quotedtitle, quotedtext, notificationicon, growlapp));
}

void notificator::notifymacusernotificationcenter(class cls, const qstring &title, const qstring &text, const qicon &icon) {
    // icon is not supported by the user notification center yet. osx will use the app icon.
    macnotificationhandler::instance()->shownotification(title, text);
}

#endif

void notificator::notify(class cls, const qstring &title, const qstring &text, const qicon &icon, int millistimeout)
{
    switch(mode)
    {
#ifdef use_dbus
    case freedesktop:
        notifydbus(cls, title, text, icon, millistimeout);
        break;
#endif
    case qsystemtray:
        notifysystray(cls, title, text, icon, millistimeout);
        break;
#ifdef q_os_mac
    case usernotificationcenter:
        notifymacusernotificationcenter(cls, title, text, icon);
        break;
    case growl12:
    case growl13:
        notifygrowl(cls, title, text, icon);
        break;
#endif
    default:
        if(cls == critical)
        {
            // fall back to old fashioned pop-up dialog if critical and no other notification available
            qmessagebox::critical(parent, title, text, qmessagebox::ok, qmessagebox::ok);
        }
        break;
    }
}
