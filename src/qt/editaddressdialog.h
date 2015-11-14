// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_editaddressdialog_h
#define moorecoin_qt_editaddressdialog_h

#include <qdialog>

class addresstablemodel;

namespace ui {
    class editaddressdialog;
}

qt_begin_namespace
class qdatawidgetmapper;
qt_end_namespace

/** dialog for editing an address and associated information.
 */
class editaddressdialog : public qdialog
{
    q_object

public:
    enum mode {
        newreceivingaddress,
        newsendingaddress,
        editreceivingaddress,
        editsendingaddress
    };

    explicit editaddressdialog(mode mode, qwidget *parent);
    ~editaddressdialog();

    void setmodel(addresstablemodel *model);
    void loadrow(int row);

    qstring getaddress() const;
    void setaddress(const qstring &address);

public slots:
    void accept();

private:
    bool savecurrentrow();

    ui::editaddressdialog *ui;
    qdatawidgetmapper *mapper;
    mode mode;
    addresstablemodel *model;

    qstring address;
};

#endif // moorecoin_qt_editaddressdialog_h
