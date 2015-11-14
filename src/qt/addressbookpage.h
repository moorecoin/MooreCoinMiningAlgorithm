// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_addressbookpage_h
#define moorecoin_qt_addressbookpage_h

#include <qdialog>

class addresstablemodel;
class optionsmodel;

namespace ui {
    class addressbookpage;
}

qt_begin_namespace
class qitemselection;
class qmenu;
class qmodelindex;
class qsortfilterproxymodel;
class qtableview;
qt_end_namespace

/** widget that shows a list of sending or receiving addresses.
  */
class addressbookpage : public qdialog
{
    q_object

public:
    enum tabs {
        sendingtab = 0,
        receivingtab = 1
    };

    enum mode {
        forselection, /**< open address book to pick address */
        forediting  /**< open address book for editing */
    };

    explicit addressbookpage(mode mode, tabs tab, qwidget *parent);
    ~addressbookpage();

    void setmodel(addresstablemodel *model);
    const qstring &getreturnvalue() const { return returnvalue; }

public slots:
    void done(int retval);

private:
    ui::addressbookpage *ui;
    addresstablemodel *model;
    mode mode;
    tabs tab;
    qstring returnvalue;
    qsortfilterproxymodel *proxymodel;
    qmenu *contextmenu;
    qaction *deleteaction; // to be able to explicitly disable it
    qstring newaddresstoselect;

private slots:
    /** delete currently selected address entry */
    void on_deleteaddress_clicked();
    /** create a new address for receiving coins and / or add a new address book entry */
    void on_newaddress_clicked();
    /** copy address of currently selected address entry to clipboard */
    void on_copyaddress_clicked();
    /** copy label of currently selected address entry to clipboard (no button) */
    void oncopylabelaction();
    /** edit currently selected address entry (no button) */
    void oneditaction();
    /** export button clicked */
    void on_exportbutton_clicked();

    /** set button states based on selected tab and selection */
    void selectionchanged();
    /** spawn contextual menu (right mouse menu) for address book entry */
    void contextualmenu(const qpoint &point);
    /** new entry/entries were added to address table */
    void selectnewaddress(const qmodelindex &parent, int begin, int /*end*/);

signals:
    void sendcoins(qstring addr);
};

#endif // moorecoin_qt_addressbookpage_h
