// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_askpassphrasedialog_h
#define moorecoin_qt_askpassphrasedialog_h

#include <qdialog>

class walletmodel;

namespace ui {
    class askpassphrasedialog;
}

/** multifunctional dialog to ask for passphrases. used for encryption, unlocking, and changing the passphrase.
 */
class askpassphrasedialog : public qdialog
{
    q_object

public:
    enum mode {
        encrypt,    /**< ask passphrase twice and encrypt */
        unlock,     /**< ask passphrase and unlock */
        changepass, /**< ask old passphrase + new passphrase twice */
        decrypt     /**< ask passphrase and decrypt wallet */
    };

    explicit askpassphrasedialog(mode mode, qwidget *parent);
    ~askpassphrasedialog();

    void accept();

    void setmodel(walletmodel *model);

private:
    ui::askpassphrasedialog *ui;
    mode mode;
    walletmodel *model;
    bool fcapslock;

private slots:
    void textchanged();

protected:
    bool event(qevent *event);
    bool eventfilter(qobject *object, qevent *event);
};

#endif // moorecoin_qt_askpassphrasedialog_h
