// copyright (c) 2014 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "scicon.h"

#include <qapplication>
#include <qcolor>
#include <qicon>
#include <qimage>
#include <qpalette>
#include <qpixmap>

namespace {

void makesinglecolorimage(qimage& img, const qcolor& colorbase)
{
    img = img.converttoformat(qimage::format_argb32);
    for (int x = img.width(); x--; )
    {
        for (int y = img.height(); y--; )
        {
            const qrgb rgb = img.pixel(x, y);
            img.setpixel(x, y, qrgba(colorbase.red(), colorbase.green(), colorbase.blue(), qalpha(rgb)));
        }
    }
}

}

qimage singlecolorimage(const qstring& filename, const qcolor& colorbase)
{
    qimage img(filename);
#if !defined(win32) && !defined(mac_osx)
    makesinglecolorimage(img, colorbase);
#endif
    return img;
}

qicon singlecoloricon(const qicon& ico, const qcolor& colorbase)
{
#if defined(win32) || defined(mac_osx)
    return ico;
#else
    qicon new_ico;
    qsize sz;
    q_foreach(sz, ico.availablesizes())
    {
        qimage img(ico.pixmap(sz).toimage());
        makesinglecolorimage(img, colorbase);
        new_ico.addpixmap(qpixmap::fromimage(img));
    }
    return new_ico;
#endif
}

qicon singlecoloricon(const qstring& filename, const qcolor& colorbase)
{
    return qicon(qpixmap::fromimage(singlecolorimage(filename, colorbase)));
}

qcolor singlecolor()
{
#if defined(win32) || defined(mac_osx)
    return qcolor(0,0,0);
#else
    const qcolor colorhighlightbg(qapplication::palette().color(qpalette::highlight));
    const qcolor colorhighlightfg(qapplication::palette().color(qpalette::highlightedtext));
    const qcolor colortext(qapplication::palette().color(qpalette::windowtext));
    const int colortextlightness = colortext.lightness();
    qcolor colorbase;
    if (abs(colorhighlightbg.lightness() - colortextlightness) < abs(colorhighlightfg.lightness() - colortextlightness))
        colorbase = colorhighlightbg;
    else
        colorbase = colorhighlightfg;
    return colorbase;
#endif
}

qicon singlecoloricon(const qstring& filename)
{
    return singlecoloricon(filename, singlecolor());
}

static qcolor textcolor()
{
    return qcolor(qapplication::palette().color(qpalette::windowtext));
}

qicon textcoloricon(const qstring& filename)
{
    return singlecoloricon(filename, textcolor());
}

qicon textcoloricon(const qicon& ico)
{
    return singlecoloricon(ico, textcolor());
}
