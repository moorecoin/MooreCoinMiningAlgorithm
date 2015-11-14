// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_rpcconsole_h
#define moorecoin_qt_rpcconsole_h

#include "guiutil.h"
#include "peertablemodel.h"

#include "net.h"

#include <qwidget>

class clientmodel;

namespace ui {
    class rpcconsole;
}

qt_begin_namespace
class qmenu;
class qitemselection;
qt_end_namespace

/** local moorecoin rpc console. */
class rpcconsole: public qwidget
{
    q_object

public:
    explicit rpcconsole(qwidget *parent);
    ~rpcconsole();

    void setclientmodel(clientmodel *model);

    enum messageclass {
        mc_error,
        mc_debug,
        cmd_request,
        cmd_reply,
        cmd_error
    };

protected:
    virtual bool eventfilter(qobject* obj, qevent *event);
    void keypressevent(qkeyevent *);

private slots:
    void on_lineedit_returnpressed();
    void on_tabwidget_currentchanged(int index);
    /** open the debug.log from the current datadir */
    void on_opendebuglogfilebutton_clicked();
    /** change the time range of the network traffic graph */
    void on_sldgraphrange_valuechanged(int value);
    /** update traffic statistics */
    void updatetrafficstats(quint64 totalbytesin, quint64 totalbytesout);
    void resizeevent(qresizeevent *event);
    void showevent(qshowevent *event);
    void hideevent(qhideevent *event);
    /** show custom context menu on peers tab */
    void showmenu(const qpoint& point);

public slots:
    void clear();
    void message(int category, const qstring &message, bool html = false);
    /** set number of connections shown in the ui */
    void setnumconnections(int count);
    /** set number of blocks and last block date shown in the ui */
    void setnumblocks(int count, const qdatetime& blockdate);
    /** go forward or back in history */
    void browsehistory(int offset);
    /** scroll console view to end */
    void scrolltoend();
    /** handle selection of peer in peers list */
    void peerselected(const qitemselection &selected, const qitemselection &deselected);
    /** handle updated peer information */
    void peerlayoutchanged();
    /** disconnect a selected node on the peers tab */
    void disconnectselectednode();

signals:
    // for rpc command executor
    void stopexecutor();
    void cmdrequest(const qstring &command);

private:
    static qstring formatbytes(quint64 bytes);
    void startexecutor();
    void settrafficgraphrange(int mins);
    /** show detailed information on ui about selected node */
    void updatenodedetail(const cnodecombinedstats *stats);
    /** clear the selected node */
    void clearselectednode();

    enum columnwidths
    {
        address_column_width = 200,
        subversion_column_width = 100,
        ping_column_width = 80
    };

    ui::rpcconsole *ui;
    clientmodel *clientmodel;
    qstringlist history;
    int historyptr;
    nodeid cachednodeid;
    qmenu *contextmenu;
};

#endif // moorecoin_qt_rpcconsole_h
