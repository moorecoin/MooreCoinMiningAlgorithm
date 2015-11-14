// copyright (c) 2011-2013 the bitcoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "macdockiconhandler.h"

#include <qimagewriter>
#include <qmenu>
#include <qbuffer>
#include <qwidget>

#undef slots
#include <cocoa/cocoa.h>
#include <objc/objc.h>
#include <objc/message.h>

#if qt_version < 0x050000
extern void qt_mac_set_dock_menu(qmenu *);
#endif

static macdockiconhandler *s_instance = null;

bool dockclickhandler(id self,sel _cmd,...) {
    q_unused(self)
    q_unused(_cmd)
    
    s_instance->handledockiconclickevent();
    
    // return no (false) to suppress the default os x actions
    return false;
}

void setupdockclickhandler() {
    class cls = objc_getclass("nsapplication");
    id appinst = objc_msgsend((id)cls, sel_registername("sharedapplication"));
    
    if (appinst != null) {
        id delegate = objc_msgsend(appinst, sel_registername("delegate"));
        class delclass = (class)objc_msgsend(delegate,  sel_registername("class"));
        sel shouldhandle = sel_registername("applicationshouldhandlereopen:hasvisiblewindows:");
        if (class_getinstancemethod(delclass, shouldhandle))
            class_replacemethod(delclass, shouldhandle, (imp)dockclickhandler, "b@:");
        else
            class_addmethod(delclass, shouldhandle, (imp)dockclickhandler,"b@:");
    }
}


macdockiconhandler::macdockiconhandler() : qobject()
{
    nsautoreleasepool *pool = [[nsautoreleasepool alloc] init];

    setupdockclickhandler();
    this->m_dummywidget = new qwidget();
    this->m_dockmenu = new qmenu(this->m_dummywidget);
    this->setmainwindow(null);
#if qt_version < 0x050000
    qt_mac_set_dock_menu(this->m_dockmenu);
#elif qt_version >= 0x050200
    this->m_dockmenu->setasdockmenu();
#endif
    [pool release];
}

void macdockiconhandler::setmainwindow(qmainwindow *window) {
    this->mainwindow = window;
}

macdockiconhandler::~macdockiconhandler()
{
    delete this->m_dummywidget;
    this->setmainwindow(null);
}

qmenu *macdockiconhandler::dockmenu()
{
    return this->m_dockmenu;
}

void macdockiconhandler::seticon(const qicon &icon)
{
    nsautoreleasepool *pool = [[nsautoreleasepool alloc] init];
    nsimage *image = nil;
    if (icon.isnull())
        image = [[nsimage imagenamed:@"nsapplicationicon"] retain];
    else {
        // generate nsimage from qicon and use this as dock icon.
        qsize size = icon.actualsize(qsize(128, 128));
        qpixmap pixmap = icon.pixmap(size);

        // write image into a r/w buffer from raw pixmap, then save the image.
        qbuffer notificationbuffer;
        if (!pixmap.isnull() && notificationbuffer.open(qiodevice::readwrite)) {
            qimagewriter writer(&notificationbuffer, "png");
            if (writer.write(pixmap.toimage())) {
                nsdata* macimgdata = [nsdata datawithbytes:notificationbuffer.buffer().data()
                                             length:notificationbuffer.buffer().size()];
                image =  [[nsimage alloc] initwithdata:macimgdata];
            }
        }

        if(!image) {
            // if testnet image could not be created, load std. app icon
            image = [[nsimage imagenamed:@"nsapplicationicon"] retain];
        }
    }

    [nsapp setapplicationiconimage:image];
    [image release];
    [pool release];
}

macdockiconhandler *macdockiconhandler::instance()
{
    if (!s_instance)
        s_instance = new macdockiconhandler();
    return s_instance;
}

void macdockiconhandler::cleanup()
{
    delete s_instance;
}

void macdockiconhandler::handledockiconclickevent()
{
    if (this->mainwindow)
    {
        this->mainwindow->activatewindow();
        this->mainwindow->show();
    }

    emit this->dockiconclicked();
}
