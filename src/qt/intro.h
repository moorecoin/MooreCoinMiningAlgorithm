// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_intro_h
#define moorecoin_qt_intro_h

#include <qdialog>
#include <qmutex>
#include <qthread>

class freespacechecker;

namespace ui {
    class intro;
}

/** introduction screen (pre-gui startup).
  allows the user to choose a data directory,
  in which the wallet and block chain will be stored.
 */
class intro : public qdialog
{
    q_object

public:
    explicit intro(qwidget *parent = 0);
    ~intro();

    qstring getdatadirectory();
    void setdatadirectory(const qstring &datadir);

    /**
     * determine data directory. let the user choose if the current one doesn't exist.
     *
     * @note do not call global getdatadir() before calling this function, this
     * will cause the wrong path to be cached.
     */
    static void pickdatadirectory();

    /**
     * determine default data directory for operating system.
     */
    static qstring getdefaultdatadirectory();

signals:
    void requestcheck();
    void stopthread();

public slots:
    void setstatus(int status, const qstring &message, quint64 bytesavailable);

private slots:
    void on_datadirectory_textchanged(const qstring &arg1);
    void on_ellipsisbutton_clicked();
    void on_datadirdefault_clicked();
    void on_datadircustom_clicked();

private:
    ui::intro *ui;
    qthread *thread;
    qmutex mutex;
    bool signalled;
    qstring pathtocheck;

    void startthread();
    void checkpath(const qstring &datadir);
    qstring getpathtocheck();

    friend class freespacechecker;
};

#endif // moorecoin_qt_intro_h
