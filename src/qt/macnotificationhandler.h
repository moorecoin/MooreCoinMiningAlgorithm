// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_macnotificationhandler_h
#define moorecoin_qt_macnotificationhandler_h

#include <qobject>

/** macintosh-specific notification handler (supports usernotificationcenter and growl).
 */
class macnotificationhandler : public qobject
{
    q_object

public:
    /** shows a 10.8+ usernotification in the usernotificationcenter
     */
    void shownotification(const qstring &title, const qstring &text);

    /** executes applescript */
    void sendapplescript(const qstring &script);

    /** check if os can handle usernotifications */
    bool hasusernotificationcentersupport(void);
    static macnotificationhandler *instance();
};


#endif // moorecoin_qt_macnotificationhandler_h
