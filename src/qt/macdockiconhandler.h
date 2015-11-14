// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_macdockiconhandler_h
#define moorecoin_qt_macdockiconhandler_h

#include <qmainwindow>
#include <qobject>

qt_begin_namespace
class qicon;
class qmenu;
class qwidget;
qt_end_namespace

/** macintosh-specific dock icon handler.
 */
class macdockiconhandler : public qobject
{
    q_object

public:
    ~macdockiconhandler();

    qmenu *dockmenu();
    void seticon(const qicon &icon);
    void setmainwindow(qmainwindow *window);
    static macdockiconhandler *instance();
    static void cleanup();
    void handledockiconclickevent();

signals:
    void dockiconclicked();

private:
    macdockiconhandler();

    qwidget *m_dummywidget;
    qmenu *m_dockmenu;
    qmainwindow *mainwindow;
};

#endif // moorecoin_qt_macdockiconhandler_h
