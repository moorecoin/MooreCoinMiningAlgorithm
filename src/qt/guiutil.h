// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_guiutil_h
#define moorecoin_qt_guiutil_h

#include "amount.h"

#include <qevent>
#include <qheaderview>
#include <qmessagebox>
#include <qobject>
#include <qprogressbar>
#include <qstring>
#include <qtableview>

#include <boost/filesystem.hpp>

class qvalidatedlineedit;
class sendcoinsrecipient;

qt_begin_namespace
class qabstractitemview;
class qdatetime;
class qfont;
class qlineedit;
class qurl;
class qwidget;
qt_end_namespace

/** utility functions used by the moorecoin qt ui.
 */
namespace guiutil
{
    // create human-readable string from date
    qstring datetimestr(const qdatetime &datetime);
    qstring datetimestr(qint64 ntime);

    // render moorecoin addresses in monospace font
    qfont moorecoinaddressfont();

    // set up widgets for address and amounts
    void setupaddresswidget(qvalidatedlineedit *widget, qwidget *parent);
    void setupamountwidget(qlineedit *widget, qwidget *parent);

    // parse "moorecoin:" uri into recipient object, return true on successful parsing
    bool parsemoorecoinuri(const qurl &uri, sendcoinsrecipient *out);
    bool parsemoorecoinuri(qstring uri, sendcoinsrecipient *out);
    qstring formatmoorecoinuri(const sendcoinsrecipient &info);

    // returns true if given address+amount meets "dust" definition
    bool isdust(const qstring& address, const camount& amount);

    // html escaping for rich text controls
    qstring htmlescape(const qstring& str, bool fmultiline=false);
    qstring htmlescape(const std::string& str, bool fmultiline=false);

    /** copy a field of the currently selected entry of a view to the clipboard. does nothing if nothing
        is selected.
       @param[in] column  data column to extract from the model
       @param[in] role    data role to extract from the model
       @see  transactionview::copylabel, transactionview::copyamount, transactionview::copyaddress
     */
    void copyentrydata(qabstractitemview *view, int column, int role=qt::editrole);

    /** return a field of the currently selected entry as a qstring. does nothing if nothing
        is selected.
       @param[in] column  data column to extract from the model
       @param[in] role    data role to extract from the model
       @see  transactionview::copylabel, transactionview::copyamount, transactionview::copyaddress
     */
    qstring getentrydata(qabstractitemview *view, int column, int role);

    void setclipboard(const qstring& str);

    /** get save filename, mimics qfiledialog::getsavefilename, except that it appends a default suffix
        when no suffix is provided by the user.

      @param[in] parent  parent window (or 0)
      @param[in] caption window caption (or empty, for default)
      @param[in] dir     starting directory (or empty, to default to documents directory)
      @param[in] filter  filter specification such as "comma separated files (*.csv)"
      @param[out] selectedsuffixout  pointer to return the suffix (file type) that was selected (or 0).
                  can be useful when choosing the save file format based on suffix.
     */
    qstring getsavefilename(qwidget *parent, const qstring &caption, const qstring &dir,
        const qstring &filter,
        qstring *selectedsuffixout);

    /** get open filename, convenience wrapper for qfiledialog::getopenfilename.

      @param[in] parent  parent window (or 0)
      @param[in] caption window caption (or empty, for default)
      @param[in] dir     starting directory (or empty, to default to documents directory)
      @param[in] filter  filter specification such as "comma separated files (*.csv)"
      @param[out] selectedsuffixout  pointer to return the suffix (file type) that was selected (or 0).
                  can be useful when choosing the save file format based on suffix.
     */
    qstring getopenfilename(qwidget *parent, const qstring &caption, const qstring &dir,
        const qstring &filter,
        qstring *selectedsuffixout);

    /** get connection type to call object slot in gui thread with invokemethod. the call will be blocking.

       @returns if called from the gui thread, return a qt::directconnection.
                if called from another thread, return a qt::blockingqueuedconnection.
    */
    qt::connectiontype blockingguithreadconnection();

    // determine whether a widget is hidden behind other windows
    bool isobscured(qwidget *w);

    // open debug.log
    void opendebuglogfile();

    // replace invalid default fonts with known good ones
    void substitutefonts(const qstring& language);

    /** qt event filter that intercepts tooltipchange events, and replaces the tooltip with a rich text
      representation if needed. this assures that qt can word-wrap long tooltip messages.
      tooltips longer than the provided size threshold (in characters) are wrapped.
     */
    class tooltiptorichtextfilter : public qobject
    {
        q_object

    public:
        explicit tooltiptorichtextfilter(int size_threshold, qobject *parent = 0);

    protected:
        bool eventfilter(qobject *obj, qevent *evt);

    private:
        int size_threshold;
    };

    /**
     * makes a qtableview last column feel as if it was being resized from its left border.
     * also makes sure the column widths are never larger than the table's viewport.
     * in qt, all columns are resizable from the right, but it's not intuitive resizing the last column from the right.
     * usually our second to last columns behave as if stretched, and when on strech mode, columns aren't resizable
     * interactively or programatically.
     *
     * this helper object takes care of this issue.
     *
     */
    class tableviewlastcolumnresizingfixer: public qobject
    {
        q_object

        public:
            tableviewlastcolumnresizingfixer(qtableview* table, int lastcolminimumwidth, int allcolsminimumwidth);
            void stretchcolumnwidth(int column);

        private:
            qtableview* tableview;
            int lastcolumnminimumwidth;
            int allcolumnsminimumwidth;
            int lastcolumnindex;
            int columncount;
            int secondtolastcolumnindex;

            void adjusttablecolumnswidth();
            int getavailablewidthforcolumn(int column);
            int getcolumnswidth();
            void connectviewheaderssignals();
            void disconnectviewheaderssignals();
            void setviewheaderresizemode(int logicalindex, qheaderview::resizemode resizemode);
            void resizecolumn(int ncolumnindex, int width);

        private slots:
            void on_sectionresized(int logicalindex, int oldsize, int newsize);
            void on_geometrieschanged();
    };

    bool getstartonsystemstartup();
    bool setstartonsystemstartup(bool fautostart);

    /** save window size and position */
    void savewindowgeometry(const qstring& strsetting, qwidget *parent);
    /** restore window size and position */
    void restorewindowgeometry(const qstring& strsetting, const qsize &defaultsizein, qwidget *parent);

    /* convert qstring to os specific boost path through utf-8 */
    boost::filesystem::path qstringtoboostpath(const qstring &path);

    /* convert os specific boost path to qstring through utf-8 */
    qstring boostpathtoqstring(const boost::filesystem::path &path);

    /* convert seconds into a qstring with days, hours, mins, secs */
    qstring formatdurationstr(int secs);

    /* format cnodestats.nservices bitmask into a user-readable string */
    qstring formatservicesstr(quint64 mask);

    /* format a cnodecombinedstats.dpingtime into a user-readable string or display n/a, if 0*/
    qstring formatpingtime(double dpingtime);

    /* format a cnodecombinedstats.ntimeoffset into a user-readable string. */
    qstring formattimeoffset(int64_t ntimeoffset);

#if defined(q_os_mac) && qt_version >= 0x050000
    // workaround for qt osx bug:
    // https://bugreports.qt-project.org/browse/qtbug-15631
    // qprogressbar uses around 10% cpu even when app is in background
    class progressbar : public qprogressbar
    {
        bool event(qevent *e) {
            return (e->type() != qevent::styleanimationupdate) ? qprogressbar::event(e) : false;
        }
    };
#else
    typedef qprogressbar progressbar;
#endif

} // namespace guiutil

#endif // moorecoin_qt_guiutil_h
