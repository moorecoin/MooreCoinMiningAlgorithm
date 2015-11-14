// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_optionsdialog_h
#define moorecoin_qt_optionsdialog_h

#include <qdialog>

class optionsmodel;
class qvalidatedlineedit;

qt_begin_namespace
class qdatawidgetmapper;
qt_end_namespace

namespace ui {
class optionsdialog;
}

/** preferences dialog. */
class optionsdialog : public qdialog
{
    q_object

public:
    explicit optionsdialog(qwidget *parent, bool enablewallet);
    ~optionsdialog();

    void setmodel(optionsmodel *model);
    void setmapper();

protected:
    bool eventfilter(qobject *object, qevent *event);

private slots:
    /* enable ok button */
    void enableokbutton();
    /* disable ok button */
    void disableokbutton();
    /* set ok button state (enabled / disabled) */
    void setokbuttonstate(bool fstate);
    void on_resetbutton_clicked();
    void on_okbutton_clicked();
    void on_cancelbutton_clicked();

    void showrestartwarning(bool fpersistent = false);
    void clearstatuslabel();
    void doproxyipchecks(qvalidatedlineedit *puiproxyip, int nproxyport);

signals:
    void proxyipchecks(qvalidatedlineedit *puiproxyip, int nproxyport);

private:
    ui::optionsdialog *ui;
    optionsmodel *model;
    qdatawidgetmapper *mapper;
    bool fproxyipvalid;
};

#endif // moorecoin_qt_optionsdialog_h
