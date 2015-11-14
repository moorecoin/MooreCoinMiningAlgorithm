// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_receiverequestdialog_h
#define moorecoin_qt_receiverequestdialog_h

#include "walletmodel.h"

#include <qdialog>
#include <qimage>
#include <qlabel>

class optionsmodel;

namespace ui {
    class receiverequestdialog;
}

qt_begin_namespace
class qmenu;
qt_end_namespace

/* label widget for qr code. this image can be dragged, dropped, copied and saved
 * to disk.
 */
class qrimagewidget : public qlabel
{
    q_object

public:
    explicit qrimagewidget(qwidget *parent = 0);
    qimage exportimage();

public slots:
    void saveimage();
    void copyimage();

protected:
    virtual void mousepressevent(qmouseevent *event);
    virtual void contextmenuevent(qcontextmenuevent *event);

private:
    qmenu *contextmenu;
};

class receiverequestdialog : public qdialog
{
    q_object

public:
    explicit receiverequestdialog(qwidget *parent = 0);
    ~receiverequestdialog();

    void setmodel(optionsmodel *model);
    void setinfo(const sendcoinsrecipient &info);

private slots:
    void on_btncopyuri_clicked();
    void on_btncopyaddress_clicked();

    void update();

private:
    ui::receiverequestdialog *ui;
    optionsmodel *model;
    sendcoinsrecipient info;
};

#endif // moorecoin_qt_receiverequestdialog_h
