// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "splashscreen.h"

#include "networkstyle.h"

#include "clientversion.h"
#include "init.h"
#include "util.h"
#include "ui_interface.h"
#include "version.h"

#ifdef enable_wallet
#include "wallet/wallet.h"
#endif

#include <qapplication>
#include <qcloseevent>
#include <qdesktopwidget>
#include <qpainter>
#include <qradialgradient>

splashscreen::splashscreen(qt::windowflags f, const networkstyle *networkstyle) :
    qwidget(0, f), curalignment(0)
{
    // set reference point, paddings
    int paddingright            = 50;
    int paddingtop              = 50;
    int titleversionvspace      = 17;
    int titlecopyrightvspace    = 40;

    float fontfactor            = 1.0;
    float devicepixelratio      = 1.0;
#if qt_version > 0x050100
    devicepixelratio = ((qguiapplication*)qcoreapplication::instance())->devicepixelratio();
#endif

    // define text to place
    qstring titletext       = tr("moorecoin core");
    qstring versiontext     = qstring("version %1").arg(qstring::fromstdstring(formatfullversion()));
    qstring copyrighttext   = qchar(0xa9)+qstring(" 2009-%1 ").arg(copyright_year) + qstring(tr("the moorecoin core developers"));
    qstring titleaddtext    = networkstyle->gettitleaddtext();

    qstring font            = qapplication::font().tostring();

    // create a bitmap according to device pixelratio
    qsize splashsize(480*devicepixelratio,320*devicepixelratio);
    pixmap = qpixmap(splashsize);

#if qt_version > 0x050100
    // change to hidpi if it makes sense
    pixmap.setdevicepixelratio(devicepixelratio);
#endif

    qpainter pixpaint(&pixmap);
    pixpaint.setpen(qcolor(100,100,100));

    // draw a slighly radial gradient
    qradialgradient gradient(qpoint(0,0), splashsize.width()/devicepixelratio);
    gradient.setcolorat(0, qt::white);
    gradient.setcolorat(1, qcolor(247,247,247));
    qrect rgradient(qpoint(0,0), splashsize);
    pixpaint.fillrect(rgradient, gradient);

    // draw the moorecoin icon, expected size of png: 1024x1024
    qrect recticon(qpoint(-150,-122), qsize(430,430));

    const qsize requiredsize(1024,1024);
    qpixmap icon(networkstyle->getappicon().pixmap(requiredsize));

    pixpaint.drawpixmap(recticon, icon);

    // check font size and drawing with
    pixpaint.setfont(qfont(font, 33*fontfactor));
    qfontmetrics fm = pixpaint.fontmetrics();
    int titletextwidth  = fm.width(titletext);
    if(titletextwidth > 160) {
        // strange font rendering, arial probably not found
        fontfactor = 0.75;
    }

    pixpaint.setfont(qfont(font, 33*fontfactor));
    fm = pixpaint.fontmetrics();
    titletextwidth  = fm.width(titletext);
    pixpaint.drawtext(pixmap.width()/devicepixelratio-titletextwidth-paddingright,paddingtop,titletext);

    pixpaint.setfont(qfont(font, 15*fontfactor));

    // if the version string is to long, reduce size
    fm = pixpaint.fontmetrics();
    int versiontextwidth  = fm.width(versiontext);
    if(versiontextwidth > titletextwidth+paddingright-10) {
        pixpaint.setfont(qfont(font, 10*fontfactor));
        titleversionvspace -= 5;
    }
    pixpaint.drawtext(pixmap.width()/devicepixelratio-titletextwidth-paddingright+2,paddingtop+titleversionvspace,versiontext);

    // draw copyright stuff
    pixpaint.setfont(qfont(font, 10*fontfactor));
    pixpaint.drawtext(pixmap.width()/devicepixelratio-titletextwidth-paddingright,paddingtop+titlecopyrightvspace,copyrighttext);

    // draw additional text if special network
    if(!titleaddtext.isempty()) {
        qfont boldfont = qfont(font, 10*fontfactor);
        boldfont.setweight(qfont::bold);
        pixpaint.setfont(boldfont);
        fm = pixpaint.fontmetrics();
        int titleaddtextwidth  = fm.width(titleaddtext);
        pixpaint.drawtext(pixmap.width()/devicepixelratio-titleaddtextwidth-10,15,titleaddtext);
    }

    pixpaint.end();

    // set window title
    setwindowtitle(titletext + " " + titleaddtext);

    // resize window and move to center of desktop, disallow resizing
    qrect r(qpoint(), qsize(pixmap.size().width()/devicepixelratio,pixmap.size().height()/devicepixelratio));
    resize(r.size());
    setfixedsize(r.size());
    move(qapplication::desktop()->screengeometry().center() - r.center());

    subscribetocoresignals();
}

splashscreen::~splashscreen()
{
    unsubscribefromcoresignals();
}

void splashscreen::slotfinish(qwidget *mainwin)
{
    q_unused(mainwin);
    hide();
}

static void initmessage(splashscreen *splash, const std::string &message)
{
    qmetaobject::invokemethod(splash, "showmessage",
        qt::queuedconnection,
        q_arg(qstring, qstring::fromstdstring(message)),
        q_arg(int, qt::alignbottom|qt::alignhcenter),
        q_arg(qcolor, qcolor(55,55,55)));
}

static void showprogress(splashscreen *splash, const std::string &title, int nprogress)
{
    initmessage(splash, title + strprintf("%d", nprogress) + "%");
}

#ifdef enable_wallet
static void connectwallet(splashscreen *splash, cwallet* wallet)
{
    wallet->showprogress.connect(boost::bind(showprogress, splash, _1, _2));
}
#endif

void splashscreen::subscribetocoresignals()
{
    // connect signals to client
    uiinterface.initmessage.connect(boost::bind(initmessage, this, _1));
    uiinterface.showprogress.connect(boost::bind(showprogress, this, _1, _2));
#ifdef enable_wallet
    uiinterface.loadwallet.connect(boost::bind(connectwallet, this, _1));
#endif
}

void splashscreen::unsubscribefromcoresignals()
{
    // disconnect signals from client
    uiinterface.initmessage.disconnect(boost::bind(initmessage, this, _1));
    uiinterface.showprogress.disconnect(boost::bind(showprogress, this, _1, _2));
#ifdef enable_wallet
    if(pwalletmain)
        pwalletmain->showprogress.disconnect(boost::bind(showprogress, this, _1, _2));
#endif
}

void splashscreen::showmessage(const qstring &message, int alignment, const qcolor &color)
{
    curmessage = message;
    curalignment = alignment;
    curcolor = color;
    update();
}

void splashscreen::paintevent(qpaintevent *event)
{
    qpainter painter(this);
    painter.drawpixmap(0, 0, pixmap);
    qrect r = rect().adjusted(5, 5, -5, -5);
    painter.setpen(curcolor);
    painter.drawtext(r, curalignment, curmessage);
}

void splashscreen::closeevent(qcloseevent *event)
{
    startshutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
