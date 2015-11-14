// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_transactiondescdialog_h
#define moorecoin_qt_transactiondescdialog_h

#include <qdialog>

namespace ui {
    class transactiondescdialog;
}

qt_begin_namespace
class qmodelindex;
qt_end_namespace

/** dialog showing transaction details. */
class transactiondescdialog : public qdialog
{
    q_object

public:
    explicit transactiondescdialog(const qmodelindex &idx, qwidget *parent = 0);
    ~transactiondescdialog();

private:
    ui::transactiondescdialog *ui;
};

#endif // moorecoin_qt_transactiondescdialog_h
