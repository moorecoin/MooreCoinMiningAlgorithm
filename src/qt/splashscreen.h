// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_splashscreen_h
#define moorecoin_qt_splashscreen_h

#include <qsplashscreen>

class networkstyle;

/** class for the splashscreen with information of the running client.
 *
 * @note this is intentionally not a qsplashscreen. moorecoin core initialization
 * can take a long time, and in that case a progress window that cannot be
 * moved around and minimized has turned out to be frustrating to the user.
 */
class splashscreen : public qwidget
{
    q_object

public:
    explicit splashscreen(qt::windowflags f, const networkstyle *networkstyle);
    ~splashscreen();

protected:
    void paintevent(qpaintevent *event);
    void closeevent(qcloseevent *event);

public slots:
    /** slot to call finish() method as it's not defined as slot */
    void slotfinish(qwidget *mainwin);

    /** show message and progress */
    void showmessage(const qstring &message, int alignment, const qcolor &color);

private:
    /** connect core signals to splash screen */
    void subscribetocoresignals();
    /** disconnect core signals to splash screen */
    void unsubscribefromcoresignals();

    qpixmap pixmap;
    qstring curmessage;
    qcolor curcolor;
    int curalignment;
};

#endif // moorecoin_qt_splashscreen_h
