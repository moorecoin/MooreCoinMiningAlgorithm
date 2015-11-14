// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_moorecoinamountfield_h
#define moorecoin_qt_moorecoinamountfield_h

#include "amount.h"

#include <qwidget>

class amountspinbox;

qt_begin_namespace
class qvaluecombobox;
qt_end_namespace

/** widget for entering moorecoin amounts.
  */
class moorecoinamountfield: public qwidget
{
    q_object

    // ugly hack: for some unknown reason camount (instead of qint64) does not work here as expected
    // discussion: https://github.com/moorecoin/moorecoin/pull/5117
    q_property(qint64 value read value write setvalue notify valuechanged user true)

public:
    explicit moorecoinamountfield(qwidget *parent = 0);

    camount value(bool *value=0) const;
    void setvalue(const camount& value);

    /** set single step in satoshis **/
    void setsinglestep(const camount& step);

    /** make read-only **/
    void setreadonly(bool freadonly);

    /** mark current value as invalid in ui. */
    void setvalid(bool valid);
    /** perform input validation, mark field as invalid if entered value is not valid. */
    bool validate();

    /** change unit used to display amount. */
    void setdisplayunit(int unit);

    /** make field empty and ready for new input. */
    void clear();

    /** enable/disable. */
    void setenabled(bool fenabled);

    /** qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/qtbug-10907),
        in these cases we have to set it up manually.
    */
    qwidget *setuptabchain(qwidget *prev);

signals:
    void valuechanged();

protected:
    /** intercept focus-in event and ',' key presses */
    bool eventfilter(qobject *object, qevent *event);

private:
    amountspinbox *amount;
    qvaluecombobox *unit;

private slots:
    void unitchanged(int idx);

};

#endif // moorecoin_qt_moorecoinamountfield_h
