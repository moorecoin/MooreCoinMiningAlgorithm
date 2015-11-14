// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "guiutil.h"

#include "moorecoinaddressvalidator.h"
#include "moorecoinunits.h"
#include "qvalidatedlineedit.h"
#include "walletmodel.h"

#include "primitives/transaction.h"
#include "init.h"
#include "main.h"
#include "protocol.h"
#include "script/script.h"
#include "script/standard.h"
#include "util.h"

#ifdef win32
#ifdef _win32_winnt
#undef _win32_winnt
#endif
#define _win32_winnt 0x0501
#ifdef _win32_ie
#undef _win32_ie
#endif
#define _win32_ie 0x0501
#define win32_lean_and_mean 1
#ifndef nominmax
#define nominmax
#endif
#include "shellapi.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#if boost_filesystem_version >= 3
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#endif
#include <boost/scoped_array.hpp>

#include <qabstractitemview>
#include <qapplication>
#include <qclipboard>
#include <qdatetime>
#include <qdesktopservices>
#include <qdesktopwidget>
#include <qdoublevalidator>
#include <qfiledialog>
#include <qfont>
#include <qlineedit>
#include <qsettings>
#include <qtextdocument> // for qt::mightberichtext
#include <qthread>

#if qt_version < 0x050000
#include <qurl>
#else
#include <qurlquery>
#endif

#if boost_filesystem_version >= 3
static boost::filesystem::detail::utf8_codecvt_facet utf8;
#endif

#if defined(q_os_mac)
extern double nsappkitversionnumber;
#if !defined(nsappkitversionnumber10_8)
#define nsappkitversionnumber10_8 1187
#endif
#if !defined(nsappkitversionnumber10_9)
#define nsappkitversionnumber10_9 1265
#endif
#endif

namespace guiutil {

qstring datetimestr(const qdatetime &date)
{
    return date.date().tostring(qt::systemlocaleshortdate) + qstring(" ") + date.tostring("hh:mm");
}

qstring datetimestr(qint64 ntime)
{
    return datetimestr(qdatetime::fromtime_t((qint32)ntime));
}

qfont moorecoinaddressfont()
{
    qfont font("monospace");
#if qt_version >= 0x040800
    font.setstylehint(qfont::monospace);
#else
    font.setstylehint(qfont::typewriter);
#endif
    return font;
}

void setupaddresswidget(qvalidatedlineedit *widget, qwidget *parent)
{
    parent->setfocusproxy(widget);

    widget->setfont(moorecoinaddressfont());
#if qt_version >= 0x040700
    // we don't want translators to use own addresses in translations
    // and this is the only place, where this address is supplied.
    widget->setplaceholdertext(qobject::tr("enter a moorecoin address (e.g. %1)").arg("1ns17iag9jjgthd1vxjvlcenzuq3rjde9l"));
#endif
    widget->setvalidator(new moorecoinaddressentryvalidator(parent));
    widget->setcheckvalidator(new moorecoinaddresscheckvalidator(parent));
}

void setupamountwidget(qlineedit *widget, qwidget *parent)
{
    qdoublevalidator *amountvalidator = new qdoublevalidator(parent);
    amountvalidator->setdecimals(8);
    amountvalidator->setbottom(0.0);
    widget->setvalidator(amountvalidator);
    widget->setalignment(qt::alignright|qt::alignvcenter);
}

bool parsemoorecoinuri(const qurl &uri, sendcoinsrecipient *out)
{
    // return if uri is not valid or is no moorecoin: uri
    if(!uri.isvalid() || uri.scheme() != qstring("moorecoin"))
        return false;

    sendcoinsrecipient rv;
    rv.address = uri.path();
    // trim any following forward slash which may have been added by the os
    if (rv.address.endswith("/")) {
        rv.address.truncate(rv.address.length() - 1);
    }
    rv.amount = 0;

#if qt_version < 0x050000
    qlist<qpair<qstring, qstring> > items = uri.queryitems();
#else
    qurlquery uriquery(uri);
    qlist<qpair<qstring, qstring> > items = uriquery.queryitems();
#endif
    for (qlist<qpair<qstring, qstring> >::iterator i = items.begin(); i != items.end(); i++)
    {
        bool fshouldreturnfalse = false;
        if (i->first.startswith("req-"))
        {
            i->first.remove(0, 4);
            fshouldreturnfalse = true;
        }

        if (i->first == "label")
        {
            rv.label = i->second;
            fshouldreturnfalse = false;
        }
        if (i->first == "message")
        {
            rv.message = i->second;
            fshouldreturnfalse = false;
        }
        else if (i->first == "amount")
        {
            if(!i->second.isempty())
            {
                if(!moorecoinunits::parse(moorecoinunits::btc, i->second, &rv.amount))
                {
                    return false;
                }
            }
            fshouldreturnfalse = false;
        }

        if (fshouldreturnfalse)
            return false;
    }
    if(out)
    {
        *out = rv;
    }
    return true;
}

bool parsemoorecoinuri(qstring uri, sendcoinsrecipient *out)
{
    // convert moorecoin:// to moorecoin:
    //
    //    cannot handle this later, because moorecoin:// will cause qt to see the part after // as host,
    //    which will lower-case it (and thus invalidate the address).
    if(uri.startswith("moorecoin://", qt::caseinsensitive))
    {
        uri.replace(0, 10, "moorecoin:");
    }
    qurl uriinstance(uri);
    return parsemoorecoinuri(uriinstance, out);
}

qstring formatmoorecoinuri(const sendcoinsrecipient &info)
{
    qstring ret = qstring("moorecoin:%1").arg(info.address);
    int paramcount = 0;

    if (info.amount)
    {
        ret += qstring("?amount=%1").arg(moorecoinunits::format(moorecoinunits::btc, info.amount, false, moorecoinunits::separatornever));
        paramcount++;
    }

    if (!info.label.isempty())
    {
        qstring lbl(qurl::topercentencoding(info.label));
        ret += qstring("%1label=%2").arg(paramcount == 0 ? "?" : "&").arg(lbl);
        paramcount++;
    }

    if (!info.message.isempty())
    {
        qstring msg(qurl::topercentencoding(info.message));;
        ret += qstring("%1message=%2").arg(paramcount == 0 ? "?" : "&").arg(msg);
        paramcount++;
    }

    return ret;
}

bool isdust(const qstring& address, const camount& amount)
{
    ctxdestination dest = cmoorecoinaddress(address.tostdstring()).get();
    cscript script = getscriptfordestination(dest);
    ctxout txout(amount, script);
    return txout.isdust(::minrelaytxfee);
}

qstring htmlescape(const qstring& str, bool fmultiline)
{
#if qt_version < 0x050000
    qstring escaped = qt::escape(str);
#else
    qstring escaped = str.tohtmlescaped();
#endif
    if(fmultiline)
    {
        escaped = escaped.replace("\n", "<br>\n");
    }
    return escaped;
}

qstring htmlescape(const std::string& str, bool fmultiline)
{
    return htmlescape(qstring::fromstdstring(str), fmultiline);
}

void copyentrydata(qabstractitemview *view, int column, int role)
{
    if(!view || !view->selectionmodel())
        return;
    qmodelindexlist selection = view->selectionmodel()->selectedrows(column);

    if(!selection.isempty())
    {
        // copy first item
        setclipboard(selection.at(0).data(role).tostring());
    }
}

qstring getentrydata(qabstractitemview *view, int column, int role)
{
    if(!view || !view->selectionmodel())
        return qstring();
    qmodelindexlist selection = view->selectionmodel()->selectedrows(column);

    if(!selection.isempty()) {
        // return first item
        return (selection.at(0).data(role).tostring());
    }
    return qstring();
}

qstring getsavefilename(qwidget *parent, const qstring &caption, const qstring &dir,
    const qstring &filter,
    qstring *selectedsuffixout)
{
    qstring selectedfilter;
    qstring mydir;
    if(dir.isempty()) // default to user documents location
    {
#if qt_version < 0x050000
        mydir = qdesktopservices::storagelocation(qdesktopservices::documentslocation);
#else
        mydir = qstandardpaths::writablelocation(qstandardpaths::documentslocation);
#endif
    }
    else
    {
        mydir = dir;
    }
    /* directly convert path to native os path separators */
    qstring result = qdir::tonativeseparators(qfiledialog::getsavefilename(parent, caption, mydir, filter, &selectedfilter));

    /* extract first suffix from filter pattern "description (*.foo)" or "description (*.foo *.bar ...) */
    qregexp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    qstring selectedsuffix;
    if(filter_re.exactmatch(selectedfilter))
    {
        selectedsuffix = filter_re.cap(1);
    }

    /* add suffix if needed */
    qfileinfo info(result);
    if(!result.isempty())
    {
        if(info.suffix().isempty() && !selectedsuffix.isempty())
        {
            /* no suffix specified, add selected suffix */
            if(!result.endswith("."))
                result.append(".");
            result.append(selectedsuffix);
        }
    }

    /* return selected suffix if asked to */
    if(selectedsuffixout)
    {
        *selectedsuffixout = selectedsuffix;
    }
    return result;
}

qstring getopenfilename(qwidget *parent, const qstring &caption, const qstring &dir,
    const qstring &filter,
    qstring *selectedsuffixout)
{
    qstring selectedfilter;
    qstring mydir;
    if(dir.isempty()) // default to user documents location
    {
#if qt_version < 0x050000
        mydir = qdesktopservices::storagelocation(qdesktopservices::documentslocation);
#else
        mydir = qstandardpaths::writablelocation(qstandardpaths::documentslocation);
#endif
    }
    else
    {
        mydir = dir;
    }
    /* directly convert path to native os path separators */
    qstring result = qdir::tonativeseparators(qfiledialog::getopenfilename(parent, caption, mydir, filter, &selectedfilter));

    if(selectedsuffixout)
    {
        /* extract first suffix from filter pattern "description (*.foo)" or "description (*.foo *.bar ...) */
        qregexp filter_re(".* \\(\\*\\.(.*)[ \\)]");
        qstring selectedsuffix;
        if(filter_re.exactmatch(selectedfilter))
        {
            selectedsuffix = filter_re.cap(1);
        }
        *selectedsuffixout = selectedsuffix;
    }
    return result;
}

qt::connectiontype blockingguithreadconnection()
{
    if(qthread::currentthread() != qapp->thread())
    {
        return qt::blockingqueuedconnection;
    }
    else
    {
        return qt::directconnection;
    }
}

bool checkpoint(const qpoint &p, const qwidget *w)
{
    qwidget *atw = qapplication::widgetat(w->maptoglobal(p));
    if (!atw) return false;
    return atw->toplevelwidget() == w;
}

bool isobscured(qwidget *w)
{
    return !(checkpoint(qpoint(0, 0), w)
        && checkpoint(qpoint(w->width() - 1, 0), w)
        && checkpoint(qpoint(0, w->height() - 1), w)
        && checkpoint(qpoint(w->width() - 1, w->height() - 1), w)
        && checkpoint(qpoint(w->width() / 2, w->height() / 2), w));
}

void opendebuglogfile()
{
    boost::filesystem::path pathdebug = getdatadir() / "debug.log";

    /* open debug.log with the associated application */
    if (boost::filesystem::exists(pathdebug))
        qdesktopservices::openurl(qurl::fromlocalfile(boostpathtoqstring(pathdebug)));
}

void substitutefonts(const qstring& language)
{
#if defined(q_os_mac)
// background:
// osx's default font changed in 10.9 and qt is unable to find it with its
// usual fallback methods when building against the 10.7 sdk or lower.
// the 10.8 sdk added a function to let it find the correct fallback font.
// if this fallback is not properly loaded, some characters may fail to
// render correctly.
//
// the same thing happened with 10.10. .helvetica neue deskinterface is now default.
//
// solution: if building with the 10.7 sdk or lower and the user's platform
// is 10.9 or higher at runtime, substitute the correct font. this needs to
// happen before the qapplication is created.
#if defined(mac_os_x_version_max_allowed) && mac_os_x_version_max_allowed < mac_os_x_version_10_8
    if (floor(nsappkitversionnumber) > nsappkitversionnumber10_8)
    {
        if (floor(nsappkitversionnumber) <= nsappkitversionnumber10_9)
            /* on a 10.9 - 10.9.x system */
            qfont::insertsubstitution(".lucida grande ui", "lucida grande");
        else
        {
            /* 10.10 or later system */
            if (language == "zh_cn" || language == "zh_tw" || language == "zh_hk") // traditional or simplified chinese
              qfont::insertsubstitution(".helvetica neue deskinterface", "heiti sc");
            else if (language == "ja") // japanesee
              qfont::insertsubstitution(".helvetica neue deskinterface", "songti sc");
            else
              qfont::insertsubstitution(".helvetica neue deskinterface", "lucida grande");
        }
    }
#endif
#endif
}

tooltiptorichtextfilter::tooltiptorichtextfilter(int size_threshold, qobject *parent) :
    qobject(parent),
    size_threshold(size_threshold)
{

}

bool tooltiptorichtextfilter::eventfilter(qobject *obj, qevent *evt)
{
    if(evt->type() == qevent::tooltipchange)
    {
        qwidget *widget = static_cast<qwidget*>(obj);
        qstring tooltip = widget->tooltip();
        if(tooltip.size() > size_threshold && !tooltip.startswith("<qt") && !qt::mightberichtext(tooltip))
        {
            // envelop with <qt></qt> to make sure qt detects this as rich text
            // escape the current message as html and replace \n by <br>
            tooltip = "<qt>" + htmlescape(tooltip, true) + "</qt>";
            widget->settooltip(tooltip);
            return true;
        }
    }
    return qobject::eventfilter(obj, evt);
}

void tableviewlastcolumnresizingfixer::connectviewheaderssignals()
{
    connect(tableview->horizontalheader(), signal(sectionresized(int,int,int)), this, slot(on_sectionresized(int,int,int)));
    connect(tableview->horizontalheader(), signal(geometrieschanged()), this, slot(on_geometrieschanged()));
}

// we need to disconnect these while handling the resize events, otherwise we can enter infinite loops.
void tableviewlastcolumnresizingfixer::disconnectviewheaderssignals()
{
    disconnect(tableview->horizontalheader(), signal(sectionresized(int,int,int)), this, slot(on_sectionresized(int,int,int)));
    disconnect(tableview->horizontalheader(), signal(geometrieschanged()), this, slot(on_geometrieschanged()));
}

// setup the resize mode, handles compatibility for qt5 and below as the method signatures changed.
// refactored here for readability.
void tableviewlastcolumnresizingfixer::setviewheaderresizemode(int logicalindex, qheaderview::resizemode resizemode)
{
#if qt_version < 0x050000
    tableview->horizontalheader()->setresizemode(logicalindex, resizemode);
#else
    tableview->horizontalheader()->setsectionresizemode(logicalindex, resizemode);
#endif
}

void tableviewlastcolumnresizingfixer::resizecolumn(int ncolumnindex, int width)
{
    tableview->setcolumnwidth(ncolumnindex, width);
    tableview->horizontalheader()->resizesection(ncolumnindex, width);
}

int tableviewlastcolumnresizingfixer::getcolumnswidth()
{
    int ncolumnswidthsum = 0;
    for (int i = 0; i < columncount; i++)
    {
        ncolumnswidthsum += tableview->horizontalheader()->sectionsize(i);
    }
    return ncolumnswidthsum;
}

int tableviewlastcolumnresizingfixer::getavailablewidthforcolumn(int column)
{
    int nresult = lastcolumnminimumwidth;
    int ntablewidth = tableview->horizontalheader()->width();

    if (ntablewidth > 0)
    {
        int nothercolswidth = getcolumnswidth() - tableview->horizontalheader()->sectionsize(column);
        nresult = std::max(nresult, ntablewidth - nothercolswidth);
    }

    return nresult;
}

// make sure we don't make the columns wider than the tables viewport width.
void tableviewlastcolumnresizingfixer::adjusttablecolumnswidth()
{
    disconnectviewheaderssignals();
    resizecolumn(lastcolumnindex, getavailablewidthforcolumn(lastcolumnindex));
    connectviewheaderssignals();

    int ntablewidth = tableview->horizontalheader()->width();
    int ncolswidth = getcolumnswidth();
    if (ncolswidth > ntablewidth)
    {
        resizecolumn(secondtolastcolumnindex,getavailablewidthforcolumn(secondtolastcolumnindex));
    }
}

// make column use all the space available, useful during window resizing.
void tableviewlastcolumnresizingfixer::stretchcolumnwidth(int column)
{
    disconnectviewheaderssignals();
    resizecolumn(column, getavailablewidthforcolumn(column));
    connectviewheaderssignals();
}

// when a section is resized this is a slot-proxy for ajustamountcolumnwidth().
void tableviewlastcolumnresizingfixer::on_sectionresized(int logicalindex, int oldsize, int newsize)
{
    adjusttablecolumnswidth();
    int remainingwidth = getavailablewidthforcolumn(logicalindex);
    if (newsize > remainingwidth)
    {
       resizecolumn(logicalindex, remainingwidth);
    }
}

// when the tabless geometry is ready, we manually perform the stretch of the "message" column,
// as the "stretch" resize mode does not allow for interactive resizing.
void tableviewlastcolumnresizingfixer::on_geometrieschanged()
{
    if ((getcolumnswidth() - this->tableview->horizontalheader()->width()) != 0)
    {
        disconnectviewheaderssignals();
        resizecolumn(secondtolastcolumnindex, getavailablewidthforcolumn(secondtolastcolumnindex));
        connectviewheaderssignals();
    }
}

/**
 * initializes all internal variables and prepares the
 * the resize modes of the last 2 columns of the table and
 */
tableviewlastcolumnresizingfixer::tableviewlastcolumnresizingfixer(qtableview* table, int lastcolminimumwidth, int allcolsminimumwidth) :
    tableview(table),
    lastcolumnminimumwidth(lastcolminimumwidth),
    allcolumnsminimumwidth(allcolsminimumwidth)
{
    columncount = tableview->horizontalheader()->count();
    lastcolumnindex = columncount - 1;
    secondtolastcolumnindex = columncount - 2;
    tableview->horizontalheader()->setminimumsectionsize(allcolumnsminimumwidth);
    setviewheaderresizemode(secondtolastcolumnindex, qheaderview::interactive);
    setviewheaderresizemode(lastcolumnindex, qheaderview::interactive);
}

#ifdef win32
boost::filesystem::path static startupshortcutpath()
{
    if (getboolarg("-testnet", false))
        return getspecialfolderpath(csidl_startup) / "moorecoin (testnet).lnk";
    else if (getboolarg("-regtest", false))
        return getspecialfolderpath(csidl_startup) / "moorecoin (regtest).lnk";

    return getspecialfolderpath(csidl_startup) / "moorecoin.lnk";
}

bool getstartonsystemstartup()
{
    // check for moorecoin*.lnk
    return boost::filesystem::exists(startupshortcutpath());
}

bool setstartonsystemstartup(bool fautostart)
{
    // if the shortcut exists already, remove it for updating
    boost::filesystem::remove(startupshortcutpath());

    if (fautostart)
    {
        coinitialize(null);

        // get a pointer to the ishelllink interface.
        ishelllink* psl = null;
        hresult hres = cocreateinstance(clsid_shelllink, null,
            clsctx_inproc_server, iid_ishelllink,
            reinterpret_cast<void**>(&psl));

        if (succeeded(hres))
        {
            // get the current executable path
            tchar pszexepath[max_path];
            getmodulefilename(null, pszexepath, sizeof(pszexepath));

            // start client minimized
            qstring strargs = "-min";
            // set -testnet /-regtest options
            strargs += qstring::fromstdstring(strprintf(" -testnet=%d -regtest=%d", getboolarg("-testnet", false), getboolarg("-regtest", false)));

#ifdef unicode
            boost::scoped_array<tchar> args(new tchar[strargs.length() + 1]);
            // convert the qstring to tchar*
            strargs.towchararray(args.get());
            // add missing '\0'-termination to string
            args[strargs.length()] = '\0';
#endif

            // set the path to the shortcut target
            psl->setpath(pszexepath);
            pathremovefilespec(pszexepath);
            psl->setworkingdirectory(pszexepath);
            psl->setshowcmd(sw_showminnoactive);
#ifndef unicode
            psl->setarguments(strargs.tostdstring().c_str());
#else
            psl->setarguments(args.get());
#endif

            // query ishelllink for the ipersistfile interface for
            // saving the shortcut in persistent storage.
            ipersistfile* ppf = null;
            hres = psl->queryinterface(iid_ipersistfile, reinterpret_cast<void**>(&ppf));
            if (succeeded(hres))
            {
                wchar pwsz[max_path];
                // ensure that the string is ansi.
                multibytetowidechar(cp_acp, 0, startupshortcutpath().string().c_str(), -1, pwsz, max_path);
                // save the link by calling ipersistfile::save.
                hres = ppf->save(pwsz, true);
                ppf->release();
                psl->release();
                couninitialize();
                return true;
            }
            psl->release();
        }
        couninitialize();
        return false;
    }
    return true;
}
#elif defined(q_os_linux)

// follow the desktop application autostart spec:
// http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

boost::filesystem::path static getautostartdir()
{
    namespace fs = boost::filesystem;

    char* pszconfighome = getenv("xdg_config_home");
    if (pszconfighome) return fs::path(pszconfighome) / "autostart";
    char* pszhome = getenv("home");
    if (pszhome) return fs::path(pszhome) / ".config" / "autostart";
    return fs::path();
}

boost::filesystem::path static getautostartfilepath()
{
    return getautostartdir() / "moorecoin.desktop";
}

bool getstartonsystemstartup()
{
    boost::filesystem::ifstream optionfile(getautostartfilepath());
    if (!optionfile.good())
        return false;
    // scan through file for "hidden=true":
    std::string line;
    while (!optionfile.eof())
    {
        getline(optionfile, line);
        if (line.find("hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionfile.close();

    return true;
}

bool setstartonsystemstartup(bool fautostart)
{
    if (!fautostart)
        boost::filesystem::remove(getautostartfilepath());
    else
    {
        char pszexepath[max_path+1];
        memset(pszexepath, 0, sizeof(pszexepath));
        if (readlink("/proc/self/exe", pszexepath, sizeof(pszexepath)-1) == -1)
            return false;

        boost::filesystem::create_directories(getautostartdir());

        boost::filesystem::ofstream optionfile(getautostartfilepath(), std::ios_base::out|std::ios_base::trunc);
        if (!optionfile.good())
            return false;
        // write a moorecoin.desktop file to the autostart directory:
        optionfile << "[desktop entry]\n";
        optionfile << "type=application\n";
        if (getboolarg("-testnet", false))
            optionfile << "name=moorecoin (testnet)\n";
        else if (getboolarg("-regtest", false))
            optionfile << "name=moorecoin (regtest)\n";
        else
            optionfile << "name=moorecoin\n";
        optionfile << "exec=" << pszexepath << strprintf(" -min -testnet=%d -regtest=%d\n", getboolarg("-testnet", false), getboolarg("-regtest", false));
        optionfile << "terminal=false\n";
        optionfile << "hidden=false\n";
        optionfile.close();
    }
    return true;
}


#elif defined(q_os_mac)
// based on: https://github.com/mozketo/launchatlogincontroller/blob/master/launchatlogincontroller.m

#include <corefoundation/corefoundation.h>
#include <coreservices/coreservices.h>

lssharedfilelistitemref findstartupiteminlist(lssharedfilelistref list, cfurlref findurl);
lssharedfilelistitemref findstartupiteminlist(lssharedfilelistref list, cfurlref findurl)
{
    // loop through the list of startup items and try to find the moorecoin app
    cfarrayref listsnapshot = lssharedfilelistcopysnapshot(list, null);
    for(int i = 0; i < cfarraygetcount(listsnapshot); i++) {
        lssharedfilelistitemref item = (lssharedfilelistitemref)cfarraygetvalueatindex(listsnapshot, i);
        uint32 resolutionflags = klssharedfilelistnouserinteraction | klssharedfilelistdonotmountvolumes;
        cfurlref currentitemurl = null;

#if defined(mac_os_x_version_max_allowed) && mac_os_x_version_max_allowed >= 10100
    if(&lssharedfilelistitemcopyresolvedurl)
        currentitemurl = lssharedfilelistitemcopyresolvedurl(item, resolutionflags, null);
#if defined(mac_os_x_version_min_required) && mac_os_x_version_min_required < 10100
    else
        lssharedfilelistitemresolve(item, resolutionflags, &currentitemurl, null);
#endif
#else
    lssharedfilelistitemresolve(item, resolutionflags, &currentitemurl, null);
#endif

        if(currentitemurl && cfequal(currentitemurl, findurl)) {
            // found
            cfrelease(currentitemurl);
            return item;
        }
        if(currentitemurl) {
            cfrelease(currentitemurl);
        }
    }
    return null;
}

bool getstartonsystemstartup()
{
    cfurlref moorecoinappurl = cfbundlecopybundleurl(cfbundlegetmainbundle());
    lssharedfilelistref loginitems = lssharedfilelistcreate(null, klssharedfilelistsessionloginitems, null);
    lssharedfilelistitemref founditem = findstartupiteminlist(loginitems, moorecoinappurl);
    return !!founditem; // return boolified object
}

bool setstartonsystemstartup(bool fautostart)
{
    cfurlref moorecoinappurl = cfbundlecopybundleurl(cfbundlegetmainbundle());
    lssharedfilelistref loginitems = lssharedfilelistcreate(null, klssharedfilelistsessionloginitems, null);
    lssharedfilelistitemref founditem = findstartupiteminlist(loginitems, moorecoinappurl);

    if(fautostart && !founditem) {
        // add moorecoin app to startup item list
        lssharedfilelistinsertitemurl(loginitems, klssharedfilelistitembeforefirst, null, null, moorecoinappurl, null, null);
    }
    else if(!fautostart && founditem) {
        // remove item
        lssharedfilelistitemremove(loginitems, founditem);
    }
    return true;
}
#else

bool getstartonsystemstartup() { return false; }
bool setstartonsystemstartup(bool fautostart) { return false; }

#endif

void savewindowgeometry(const qstring& strsetting, qwidget *parent)
{
    qsettings settings;
    settings.setvalue(strsetting + "pos", parent->pos());
    settings.setvalue(strsetting + "size", parent->size());
}

void restorewindowgeometry(const qstring& strsetting, const qsize& defaultsize, qwidget *parent)
{
    qsettings settings;
    qpoint pos = settings.value(strsetting + "pos").topoint();
    qsize size = settings.value(strsetting + "size", defaultsize).tosize();

    if (!pos.x() && !pos.y()) {
        qrect screen = qapplication::desktop()->screengeometry();
        pos.setx((screen.width() - size.width()) / 2);
        pos.sety((screen.height() - size.height()) / 2);
    }

    parent->resize(size);
    parent->move(pos);
}

void setclipboard(const qstring& str)
{
    qapplication::clipboard()->settext(str, qclipboard::clipboard);
    qapplication::clipboard()->settext(str, qclipboard::selection);
}

#if boost_filesystem_version >= 3
boost::filesystem::path qstringtoboostpath(const qstring &path)
{
    return boost::filesystem::path(path.tostdstring(), utf8);
}

qstring boostpathtoqstring(const boost::filesystem::path &path)
{
    return qstring::fromstdstring(path.string(utf8));
}
#else
#warning conversion between boost path and qstring can use invalid character encoding with boost_filesystem v2 and older
boost::filesystem::path qstringtoboostpath(const qstring &path)
{
    return boost::filesystem::path(path.tostdstring());
}

qstring boostpathtoqstring(const boost::filesystem::path &path)
{
    return qstring::fromstdstring(path.string());
}
#endif

qstring formatdurationstr(int secs)
{
    qstringlist strlist;
    int days = secs / 86400;
    int hours = (secs % 86400) / 3600;
    int mins = (secs % 3600) / 60;
    int seconds = secs % 60;

    if (days)
        strlist.append(qstring(qobject::tr("%1 d")).arg(days));
    if (hours)
        strlist.append(qstring(qobject::tr("%1 h")).arg(hours));
    if (mins)
        strlist.append(qstring(qobject::tr("%1 m")).arg(mins));
    if (seconds || (!days && !hours && !mins))
        strlist.append(qstring(qobject::tr("%1 s")).arg(seconds));

    return strlist.join(" ");
}

qstring formatservicesstr(quint64 mask)
{
    qstringlist strlist;

    // just scan the last 8 bits for now.
    for (int i = 0; i < 8; i++) {
        uint64_t check = 1 << i;
        if (mask & check)
        {
            switch (check)
            {
            case node_network:
                strlist.append("network");
                break;
            case node_getutxo:
                strlist.append("getutxo");
                break;
            default:
                strlist.append(qstring("%1[%2]").arg("unknown").arg(check));
            }
        }
    }

    if (strlist.size())
        return strlist.join(" & ");
    else
        return qobject::tr("none");
}

qstring formatpingtime(double dpingtime)
{
    return dpingtime == 0 ? qobject::tr("n/a") : qstring(qobject::tr("%1 ms")).arg(qstring::number((int)(dpingtime * 1000), 10));
}

qstring formattimeoffset(int64_t ntimeoffset)
{
  return qstring(qobject::tr("%1 s")).arg(qstring::number((int)ntimeoffset, 10));
}

} // namespace guiutil
