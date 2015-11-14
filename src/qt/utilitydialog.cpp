// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "utilitydialog.h"

#include "ui_helpmessagedialog.h"

#include "moorecoingui.h"
#include "clientmodel.h"
#include "guiutil.h"

#include "clientversion.h"
#include "init.h"
#include "util.h"

#include <stdio.h>

#include <qcloseevent>
#include <qlabel>
#include <qregexp>
#include <qtexttable>
#include <qtextcursor>
#include <qvboxlayout>

/** "help message" or "about" dialog box */
helpmessagedialog::helpmessagedialog(qwidget *parent, bool about) :
    qdialog(parent),
    ui(new ui::helpmessagedialog)
{
    ui->setupui(this);

    qstring version = tr("moorecoin core") + " " + tr("version") + " " + qstring::fromstdstring(formatfullversion());
    /* on x86 add a bit specifier to the version so that users can distinguish between
     * 32 and 64 bit builds. on other architectures, 32/64 bit may be more ambigious.
     */
#if defined(__x86_64__)
    version += " " + tr("(%1-bit)").arg(64);
#elif defined(__i386__ )
    version += " " + tr("(%1-bit)").arg(32);
#endif

    if (about)
    {
        setwindowtitle(tr("about moorecoin core"));

        /// html-format the license message from the core
        qstring licenseinfo = qstring::fromstdstring(licenseinfo());
        qstring licenseinfohtml = licenseinfo;
        // make urls clickable
        qregexp uri("<(.*)>", qt::casesensitive, qregexp::regexp2);
        uri.setminimal(true); // use non-greedy matching
        licenseinfohtml.replace(uri, "<a href=\"\\1\">\\1</a>");
        // replace newlines with html breaks
        licenseinfohtml.replace("\n\n", "<br><br>");

        ui->aboutmessage->settextformat(qt::richtext);
        ui->scrollarea->setverticalscrollbarpolicy(qt::scrollbarasneeded);
        text = version + "\n" + licenseinfo;
        ui->aboutmessage->settext(version + "<br><br>" + licenseinfohtml);
        ui->aboutmessage->setwordwrap(true);
        ui->helpmessage->setvisible(false);
    } else {
        setwindowtitle(tr("command-line options"));
        qstring header = tr("usage:") + "\n" +
            "  moorecoin-qt [" + tr("command-line options") + "]                     " + "\n";
        qtextcursor cursor(ui->helpmessage->document());
        cursor.inserttext(version);
        cursor.insertblock();
        cursor.inserttext(header);
        cursor.insertblock();

        qstring coreoptions = qstring::fromstdstring(helpmessage(hmm_moorecoin_qt));
        text = version + "\n" + header + "\n" + coreoptions;

        qtexttableformat tf;
        tf.setborderstyle(qtextframeformat::borderstyle_none);
        tf.setcellpadding(2);
        qvector<qtextlength> widths;
        widths << qtextlength(qtextlength::percentagelength, 35);
        widths << qtextlength(qtextlength::percentagelength, 65);
        tf.setcolumnwidthconstraints(widths);

        qtextcharformat bold;
        bold.setfontweight(qfont::bold);

        foreach (const qstring &line, coreoptions.split("\n")) {
            if (line.startswith("  -"))
            {
                cursor.currenttable()->appendrows(1);
                cursor.moveposition(qtextcursor::previouscell);
                cursor.moveposition(qtextcursor::nextrow);
                cursor.inserttext(line.trimmed());
                cursor.moveposition(qtextcursor::nextcell);
            } else if (line.startswith("   ")) {
                cursor.inserttext(line.trimmed()+' ');
            } else if (line.size() > 0) {
                //title of a group
                if (cursor.currenttable())
                    cursor.currenttable()->appendrows(1);
                cursor.moveposition(qtextcursor::down);
                cursor.inserttext(line.trimmed(), bold);
                cursor.inserttable(1, 2, tf);
            }
        }

        ui->helpmessage->movecursor(qtextcursor::start);
        ui->scrollarea->setvisible(false);
        ui->aboutlogo->setvisible(false);
    }
}

helpmessagedialog::~helpmessagedialog()
{
    delete ui;
}

void helpmessagedialog::printtoconsole()
{
    // on other operating systems, the expected action is to print the message to the console.
    fprintf(stdout, "%s\n", qprintable(text));
}

void helpmessagedialog::showorprint()
{
#if defined(win32)
    // on windows, show a message box, as there is no stderr/stdout in windowed applications
    exec();
#else
    // on other operating systems, print help text to console
    printtoconsole();
#endif
}

void helpmessagedialog::on_okbutton_accepted()
{
    close();
}


/** "shutdown" window */
shutdownwindow::shutdownwindow(qwidget *parent, qt::windowflags f):
    qwidget(parent, f)
{
    qvboxlayout *layout = new qvboxlayout();
    layout->addwidget(new qlabel(
        tr("moorecoin core is shutting down...") + "<br /><br />" +
        tr("do not shut down the computer until this window disappears.")));
    setlayout(layout);
}

void shutdownwindow::showshutdownwindow(moorecoingui *window)
{
    if (!window)
        return;

    // show a simple window indicating shutdown status
    qwidget *shutdownwindow = new shutdownwindow();
    // we don't hold a direct pointer to the shutdown window after creation, so use
    // qt::wa_deleteonclose to make sure that the window will be deleted eventually.
    shutdownwindow->setattribute(qt::wa_deleteonclose);
    shutdownwindow->setwindowtitle(window->windowtitle());

    // center shutdown window at where main window was
    const qpoint global = window->maptoglobal(window->rect().center());
    shutdownwindow->move(global.x() - shutdownwindow->width() / 2, global.y() - shutdownwindow->height() / 2);
    shutdownwindow->show();
}

void shutdownwindow::closeevent(qcloseevent *event)
{
    event->ignore();
}
