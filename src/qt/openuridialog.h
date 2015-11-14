// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_openuridialog_h
#define moorecoin_qt_openuridialog_h

#include <qdialog>

namespace ui {
    class openuridialog;
}

class openuridialog : public qdialog
{
    q_object

public:
    explicit openuridialog(qwidget *parent);
    ~openuridialog();

    qstring geturi();

protected slots:
    void accept();

private slots:
    void on_selectfilebutton_clicked();

private:
    ui::openuridialog *ui;
};

#endif // moorecoin_qt_openuridialog_h
