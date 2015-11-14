// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "networkstyle.h"

#include "guiconstants.h"
#include "scicon.h"

#include <qapplication>

static const struct {
    const char *networkid;
    const char *appname;
    const int iconcolorhueshift;
    const int iconcolorsaturationreduction;
    const char *titleaddtext;
} network_styles[] = {
    {"main", qapp_app_name_default, 0, 0, ""},
    {"test", qapp_app_name_testnet, 70, 30, qt_translate_noop("splashscreen", "[testnet]")},
    {"regtest", qapp_app_name_testnet, 160, 30, "[regtest]"}
};
static const unsigned network_styles_count = sizeof(network_styles)/sizeof(*network_styles);

// titleaddtext needs to be const char* for tr()
networkstyle::networkstyle(const qstring &appname, const int iconcolorhueshift, const int iconcolorsaturationreduction, const char *titleaddtext):
    appname(appname),
    titleaddtext(qapp->translate("splashscreen", titleaddtext))
{
    // load pixmap
    qpixmap pixmap(":/icons/moorecoin");

    if(iconcolorhueshift != 0 && iconcolorsaturationreduction != 0)
    {
        // generate qimage from qpixmap
        qimage img = pixmap.toimage();

        int h,s,l,a;

        // traverse though lines
        for(int y=0;y<img.height();y++)
        {
            qrgb *scl = reinterpret_cast< qrgb *>( img.scanline( y ) );

            // loop through pixels
            for(int x=0;x<img.width();x++)
            {
                // preserve alpha because qcolor::gethsl doesen't return the alpha value
                a = qalpha(scl[x]);
                qcolor col(scl[x]);

                // get hue value
                col.gethsl(&h,&s,&l);

                // rotate color on rgb color circle
                // 70掳 should end up with the typical "testnet" green
                h+=iconcolorhueshift;

                // change saturation value
                if(s>iconcolorsaturationreduction)
                {
                    s -= iconcolorsaturationreduction;
                }
                col.sethsl(h,s,l,a);

                // set the pixel
                scl[x] = col.rgba();
            }
        }

        //convert back to qpixmap
#if qt_version >= 0x040700
        pixmap.convertfromimage(img);
#else
        pixmap = qpixmap::fromimage(img);
#endif
    }

    appicon             = qicon(pixmap);
    trayandwindowicon   = qicon(pixmap.scaled(qsize(256,256)));
}

const networkstyle *networkstyle::instantiate(const qstring &networkid)
{
    for (unsigned x=0; x<network_styles_count; ++x)
    {
        if (networkid == network_styles[x].networkid)
        {
            return new networkstyle(
                    network_styles[x].appname,
                    network_styles[x].iconcolorhueshift,
                    network_styles[x].iconcolorsaturationreduction,
                    network_styles[x].titleaddtext);
        }
    }
    return 0;
}
