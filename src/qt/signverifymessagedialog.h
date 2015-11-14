// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_signverifymessagedialog_h
#define moorecoin_qt_signverifymessagedialog_h

#include <qdialog>

class walletmodel;

namespace ui {
    class signverifymessagedialog;
}

class signverifymessagedialog : public qdialog
{
    q_object

public:
    explicit signverifymessagedialog(qwidget *parent);
    ~signverifymessagedialog();

    void setmodel(walletmodel *model);
    void setaddress_sm(const qstring &address);
    void setaddress_vm(const qstring &address);

    void showtab_sm(bool fshow);
    void showtab_vm(bool fshow);

protected:
    bool eventfilter(qobject *object, qevent *event);

private:
    ui::signverifymessagedialog *ui;
    walletmodel *model;

private slots:
    /* sign message */
    void on_addressbookbutton_sm_clicked();
    void on_pastebutton_sm_clicked();
    void on_signmessagebutton_sm_clicked();
    void on_copysignaturebutton_sm_clicked();
    void on_clearbutton_sm_clicked();
    /* verify message */
    void on_addressbookbutton_vm_clicked();
    void on_verifymessagebutton_vm_clicked();
    void on_clearbutton_vm_clicked();
};

#endif // moorecoin_qt_signverifymessagedialog_h
