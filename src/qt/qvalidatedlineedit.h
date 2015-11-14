// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_qvalidatedlineedit_h
#define moorecoin_qt_qvalidatedlineedit_h

#include <qlineedit>

/** line edit that can be marked as "invalid" to show input validation feedback. when marked as invalid,
   it will get a red background until it is focused.
 */
class qvalidatedlineedit : public qlineedit
{
    q_object

public:
    explicit qvalidatedlineedit(qwidget *parent);
    void clear();
    void setcheckvalidator(const qvalidator *v);

protected:
    void focusinevent(qfocusevent *evt);
    void focusoutevent(qfocusevent *evt);

private:
    bool valid;
    const qvalidator *checkvalidator;

public slots:
    void setvalid(bool valid);
    void setenabled(bool enabled);

private slots:
    void markvalid();
    void checkvalidity();
};

#endif // moorecoin_qt_qvalidatedlineedit_h
