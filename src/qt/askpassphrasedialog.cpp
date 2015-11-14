// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"

#include "guiconstants.h"
#include "walletmodel.h"

#include "support/allocators/secure.h"

#include <qkeyevent>
#include <qmessagebox>
#include <qpushbutton>

askpassphrasedialog::askpassphrasedialog(mode mode, qwidget *parent) :
    qdialog(parent),
    ui(new ui::askpassphrasedialog),
    mode(mode),
    model(0),
    fcapslock(false)
{
    ui->setupui(this);

    ui->passedit1->setminimumsize(ui->passedit1->sizehint());
    ui->passedit2->setminimumsize(ui->passedit2->sizehint());
    ui->passedit3->setminimumsize(ui->passedit3->sizehint());

    ui->passedit1->setmaxlength(max_passphrase_size);
    ui->passedit2->setmaxlength(max_passphrase_size);
    ui->passedit3->setmaxlength(max_passphrase_size);

    // setup caps lock detection.
    ui->passedit1->installeventfilter(this);
    ui->passedit2->installeventfilter(this);
    ui->passedit3->installeventfilter(this);

    switch(mode)
    {
        case encrypt: // ask passphrase x2
            ui->warninglabel->settext(tr("enter the new passphrase to the wallet.<br/>please use a passphrase of <b>ten or more random characters</b>, or <b>eight or more words</b>."));
            ui->passlabel1->hide();
            ui->passedit1->hide();
            setwindowtitle(tr("encrypt wallet"));
            break;
        case unlock: // ask passphrase
            ui->warninglabel->settext(tr("this operation needs your wallet passphrase to unlock the wallet."));
            ui->passlabel2->hide();
            ui->passedit2->hide();
            ui->passlabel3->hide();
            ui->passedit3->hide();
            setwindowtitle(tr("unlock wallet"));
            break;
        case decrypt:   // ask passphrase
            ui->warninglabel->settext(tr("this operation needs your wallet passphrase to decrypt the wallet."));
            ui->passlabel2->hide();
            ui->passedit2->hide();
            ui->passlabel3->hide();
            ui->passedit3->hide();
            setwindowtitle(tr("decrypt wallet"));
            break;
        case changepass: // ask old passphrase + new passphrase x2
            setwindowtitle(tr("change passphrase"));
            ui->warninglabel->settext(tr("enter the old passphrase and new passphrase to the wallet."));
            break;
    }
    textchanged();
    connect(ui->passedit1, signal(textchanged(qstring)), this, slot(textchanged()));
    connect(ui->passedit2, signal(textchanged(qstring)), this, slot(textchanged()));
    connect(ui->passedit3, signal(textchanged(qstring)), this, slot(textchanged()));
}

askpassphrasedialog::~askpassphrasedialog()
{
    // attempt to overwrite text so that they do not linger around in memory
    ui->passedit1->settext(qstring(" ").repeated(ui->passedit1->text().size()));
    ui->passedit2->settext(qstring(" ").repeated(ui->passedit2->text().size()));
    ui->passedit3->settext(qstring(" ").repeated(ui->passedit3->text().size()));
    delete ui;
}

void askpassphrasedialog::setmodel(walletmodel *model)
{
    this->model = model;
}

void askpassphrasedialog::accept()
{
    securestring oldpass, newpass1, newpass2;
    if(!model)
        return;
    oldpass.reserve(max_passphrase_size);
    newpass1.reserve(max_passphrase_size);
    newpass2.reserve(max_passphrase_size);
    // todo: get rid of this .c_str() by implementing securestring::operator=(std::string)
    // alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->passedit1->text().tostdstring().c_str());
    newpass1.assign(ui->passedit2->text().tostdstring().c_str());
    newpass2.assign(ui->passedit3->text().tostdstring().c_str());

    switch(mode)
    {
    case encrypt: {
        if(newpass1.empty() || newpass2.empty())
        {
            // cannot encrypt with empty passphrase
            break;
        }
        qmessagebox::standardbutton retval = qmessagebox::question(this, tr("confirm wallet encryption"),
                 tr("warning: if you encrypt your wallet and lose your passphrase, you will <b>lose all of your moorecoins</b>!") + "<br><br>" + tr("are you sure you wish to encrypt your wallet?"),
                 qmessagebox::yes|qmessagebox::cancel,
                 qmessagebox::cancel);
        if(retval == qmessagebox::yes)
        {
            if(newpass1 == newpass2)
            {
                if(model->setwalletencrypted(true, newpass1))
                {
                    qmessagebox::warning(this, tr("wallet encrypted"),
                                         "<qt>" +
                                         tr("moorecoin core will close now to finish the encryption process. "
                                         "remember that encrypting your wallet cannot fully protect "
                                         "your moorecoins from being stolen by malware infecting your computer.") +
                                         "<br><br><b>" +
                                         tr("important: any previous backups you have made of your wallet file "
                                         "should be replaced with the newly generated, encrypted wallet file. "
                                         "for security reasons, previous backups of the unencrypted wallet file "
                                         "will become useless as soon as you start using the new, encrypted wallet.") +
                                         "</b></qt>");
                    qapplication::quit();
                }
                else
                {
                    qmessagebox::critical(this, tr("wallet encryption failed"),
                                         tr("wallet encryption failed due to an internal error. your wallet was not encrypted."));
                }
                qdialog::accept(); // success
            }
            else
            {
                qmessagebox::critical(this, tr("wallet encryption failed"),
                                     tr("the supplied passphrases do not match."));
            }
        }
        else
        {
            qdialog::reject(); // cancelled
        }
        } break;
    case unlock:
        if(!model->setwalletlocked(false, oldpass))
        {
            qmessagebox::critical(this, tr("wallet unlock failed"),
                                  tr("the passphrase entered for the wallet decryption was incorrect."));
        }
        else
        {
            qdialog::accept(); // success
        }
        break;
    case decrypt:
        if(!model->setwalletencrypted(false, oldpass))
        {
            qmessagebox::critical(this, tr("wallet decryption failed"),
                                  tr("the passphrase entered for the wallet decryption was incorrect."));
        }
        else
        {
            qdialog::accept(); // success
        }
        break;
    case changepass:
        if(newpass1 == newpass2)
        {
            if(model->changepassphrase(oldpass, newpass1))
            {
                qmessagebox::information(this, tr("wallet encrypted"),
                                     tr("wallet passphrase was successfully changed."));
                qdialog::accept(); // success
            }
            else
            {
                qmessagebox::critical(this, tr("wallet encryption failed"),
                                     tr("the passphrase entered for the wallet decryption was incorrect."));
            }
        }
        else
        {
            qmessagebox::critical(this, tr("wallet encryption failed"),
                                 tr("the supplied passphrases do not match."));
        }
        break;
    }
}

void askpassphrasedialog::textchanged()
{
    // validate input, set ok button to enabled when acceptable
    bool acceptable = false;
    switch(mode)
    {
    case encrypt: // new passphrase x2
        acceptable = !ui->passedit2->text().isempty() && !ui->passedit3->text().isempty();
        break;
    case unlock: // old passphrase x1
    case decrypt:
        acceptable = !ui->passedit1->text().isempty();
        break;
    case changepass: // old passphrase x1, new passphrase x2
        acceptable = !ui->passedit1->text().isempty() && !ui->passedit2->text().isempty() && !ui->passedit3->text().isempty();
        break;
    }
    ui->buttonbox->button(qdialogbuttonbox::ok)->setenabled(acceptable);
}

bool askpassphrasedialog::event(qevent *event)
{
    // detect caps lock key press.
    if (event->type() == qevent::keypress) {
        qkeyevent *ke = static_cast<qkeyevent *>(event);
        if (ke->key() == qt::key_capslock) {
            fcapslock = !fcapslock;
        }
        if (fcapslock) {
            ui->capslabel->settext(tr("warning: the caps lock key is on!"));
        } else {
            ui->capslabel->clear();
        }
    }
    return qwidget::event(event);
}

bool askpassphrasedialog::eventfilter(qobject *object, qevent *event)
{
    /* detect caps lock.
     * there is no good os-independent way to check a key state in qt, but we
     * can detect caps lock by checking for the following condition:
     * shift key is down and the result is a lower case character, or
     * shift key is not down and the result is an upper case character.
     */
    if (event->type() == qevent::keypress) {
        qkeyevent *ke = static_cast<qkeyevent *>(event);
        qstring str = ke->text();
        if (str.length() != 0) {
            const qchar *psz = str.unicode();
            bool fshift = (ke->modifiers() & qt::shiftmodifier) != 0;
            if ((fshift && *psz >= 'a' && *psz <= 'z') || (!fshift && *psz >= 'a' && *psz <= 'z')) {
                fcapslock = true;
                ui->capslabel->settext(tr("warning: the caps lock key is on!"));
            } else if (psz->isletter()) {
                fcapslock = false;
                ui->capslabel->clear();
            }
        }
    }
    return qdialog::eventfilter(object, event);
}
