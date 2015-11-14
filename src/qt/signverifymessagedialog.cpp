// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "signverifymessagedialog.h"
#include "ui_signverifymessagedialog.h"

#include "addressbookpage.h"
#include "guiutil.h"
#include "scicon.h"
#include "walletmodel.h"

#include "base58.h"
#include "init.h"
#include "main.h" // for strmessagemagic
#include "wallet/wallet.h"

#include <string>
#include <vector>

#include <qclipboard>

signverifymessagedialog::signverifymessagedialog(qwidget *parent) :
    qdialog(parent),
    ui(new ui::signverifymessagedialog),
    model(0)
{
    ui->setupui(this);

    ui->addressbookbutton_sm->seticon(singlecoloricon(":/icons/address-book"));
    ui->pastebutton_sm->seticon(singlecoloricon(":/icons/editpaste"));
    ui->copysignaturebutton_sm->seticon(singlecoloricon(":/icons/editcopy"));
    ui->signmessagebutton_sm->seticon(singlecoloricon(":/icons/edit"));
    ui->clearbutton_sm->seticon(singlecoloricon(":/icons/remove"));
    ui->addressbookbutton_vm->seticon(singlecoloricon(":/icons/address-book"));
    ui->verifymessagebutton_vm->seticon(singlecoloricon(":/icons/transaction_0"));
    ui->clearbutton_vm->seticon(singlecoloricon(":/icons/remove"));

#if qt_version >= 0x040700
    ui->signatureout_sm->setplaceholdertext(tr("click \"sign message\" to generate signature"));
#endif

    guiutil::setupaddresswidget(ui->addressin_sm, this);
    guiutil::setupaddresswidget(ui->addressin_vm, this);

    ui->addressin_sm->installeventfilter(this);
    ui->messagein_sm->installeventfilter(this);
    ui->signatureout_sm->installeventfilter(this);
    ui->addressin_vm->installeventfilter(this);
    ui->messagein_vm->installeventfilter(this);
    ui->signaturein_vm->installeventfilter(this);

    ui->signatureout_sm->setfont(guiutil::moorecoinaddressfont());
    ui->signaturein_vm->setfont(guiutil::moorecoinaddressfont());
}

signverifymessagedialog::~signverifymessagedialog()
{
    delete ui;
}

void signverifymessagedialog::setmodel(walletmodel *model)
{
    this->model = model;
}

void signverifymessagedialog::setaddress_sm(const qstring &address)
{
    ui->addressin_sm->settext(address);
    ui->messagein_sm->setfocus();
}

void signverifymessagedialog::setaddress_vm(const qstring &address)
{
    ui->addressin_vm->settext(address);
    ui->messagein_vm->setfocus();
}

void signverifymessagedialog::showtab_sm(bool fshow)
{
    ui->tabwidget->setcurrentindex(0);
    if (fshow)
        this->show();
}

void signverifymessagedialog::showtab_vm(bool fshow)
{
    ui->tabwidget->setcurrentindex(1);
    if (fshow)
        this->show();
}

void signverifymessagedialog::on_addressbookbutton_sm_clicked()
{
    if (model && model->getaddresstablemodel())
    {
        addressbookpage dlg(addressbookpage::forselection, addressbookpage::receivingtab, this);
        dlg.setmodel(model->getaddresstablemodel());
        if (dlg.exec())
        {
            setaddress_sm(dlg.getreturnvalue());
        }
    }
}

void signverifymessagedialog::on_pastebutton_sm_clicked()
{
    setaddress_sm(qapplication::clipboard()->text());
}

void signverifymessagedialog::on_signmessagebutton_sm_clicked()
{
    if (!model)
        return;

    /* clear old signature to ensure users don't get confused on error with an old signature displayed */
    ui->signatureout_sm->clear();

    cmoorecoinaddress addr(ui->addressin_sm->text().tostdstring());
    if (!addr.isvalid())
    {
        ui->statuslabel_sm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_sm->settext(tr("the entered address is invalid.") + qstring(" ") + tr("please check the address and try again."));
        return;
    }
    ckeyid keyid;
    if (!addr.getkeyid(keyid))
    {
        ui->addressin_sm->setvalid(false);
        ui->statuslabel_sm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_sm->settext(tr("the entered address does not refer to a key.") + qstring(" ") + tr("please check the address and try again."));
        return;
    }

    walletmodel::unlockcontext ctx(model->requestunlock());
    if (!ctx.isvalid())
    {
        ui->statuslabel_sm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_sm->settext(tr("wallet unlock was cancelled."));
        return;
    }

    ckey key;
    if (!pwalletmain->getkey(keyid, key))
    {
        ui->statuslabel_sm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_sm->settext(tr("private key for the entered address is not available."));
        return;
    }

    cdatastream ss(ser_gethash, 0);
    ss << strmessagemagic;
    ss << ui->messagein_sm->document()->toplaintext().tostdstring();

    std::vector<unsigned char> vchsig;
    if (!key.signcompact(hash(ss.begin(), ss.end()), vchsig))
    {
        ui->statuslabel_sm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_sm->settext(qstring("<nobr>") + tr("message signing failed.") + qstring("</nobr>"));
        return;
    }

    ui->statuslabel_sm->setstylesheet("qlabel { color: green; }");
    ui->statuslabel_sm->settext(qstring("<nobr>") + tr("message signed.") + qstring("</nobr>"));

    ui->signatureout_sm->settext(qstring::fromstdstring(encodebase64(&vchsig[0], vchsig.size())));
}

void signverifymessagedialog::on_copysignaturebutton_sm_clicked()
{
    guiutil::setclipboard(ui->signatureout_sm->text());
}

void signverifymessagedialog::on_clearbutton_sm_clicked()
{
    ui->addressin_sm->clear();
    ui->messagein_sm->clear();
    ui->signatureout_sm->clear();
    ui->statuslabel_sm->clear();

    ui->addressin_sm->setfocus();
}

void signverifymessagedialog::on_addressbookbutton_vm_clicked()
{
    if (model && model->getaddresstablemodel())
    {
        addressbookpage dlg(addressbookpage::forselection, addressbookpage::sendingtab, this);
        dlg.setmodel(model->getaddresstablemodel());
        if (dlg.exec())
        {
            setaddress_vm(dlg.getreturnvalue());
        }
    }
}

void signverifymessagedialog::on_verifymessagebutton_vm_clicked()
{
    cmoorecoinaddress addr(ui->addressin_vm->text().tostdstring());
    if (!addr.isvalid())
    {
        ui->statuslabel_vm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_vm->settext(tr("the entered address is invalid.") + qstring(" ") + tr("please check the address and try again."));
        return;
    }
    ckeyid keyid;
    if (!addr.getkeyid(keyid))
    {
        ui->addressin_vm->setvalid(false);
        ui->statuslabel_vm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_vm->settext(tr("the entered address does not refer to a key.") + qstring(" ") + tr("please check the address and try again."));
        return;
    }

    bool finvalid = false;
    std::vector<unsigned char> vchsig = decodebase64(ui->signaturein_vm->text().tostdstring().c_str(), &finvalid);

    if (finvalid)
    {
        ui->signaturein_vm->setvalid(false);
        ui->statuslabel_vm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_vm->settext(tr("the signature could not be decoded.") + qstring(" ") + tr("please check the signature and try again."));
        return;
    }

    cdatastream ss(ser_gethash, 0);
    ss << strmessagemagic;
    ss << ui->messagein_vm->document()->toplaintext().tostdstring();

    cpubkey pubkey;
    if (!pubkey.recovercompact(hash(ss.begin(), ss.end()), vchsig))
    {
        ui->signaturein_vm->setvalid(false);
        ui->statuslabel_vm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_vm->settext(tr("the signature did not match the message digest.") + qstring(" ") + tr("please check the signature and try again."));
        return;
    }

    if (!(cmoorecoinaddress(pubkey.getid()) == addr))
    {
        ui->statuslabel_vm->setstylesheet("qlabel { color: red; }");
        ui->statuslabel_vm->settext(qstring("<nobr>") + tr("message verification failed.") + qstring("</nobr>"));
        return;
    }

    ui->statuslabel_vm->setstylesheet("qlabel { color: green; }");
    ui->statuslabel_vm->settext(qstring("<nobr>") + tr("message verified.") + qstring("</nobr>"));
}

void signverifymessagedialog::on_clearbutton_vm_clicked()
{
    ui->addressin_vm->clear();
    ui->signaturein_vm->clear();
    ui->messagein_vm->clear();
    ui->statuslabel_vm->clear();

    ui->addressin_vm->setfocus();
}

bool signverifymessagedialog::eventfilter(qobject *object, qevent *event)
{
    if (event->type() == qevent::mousebuttonpress || event->type() == qevent::focusin)
    {
        if (ui->tabwidget->currentindex() == 0)
        {
            /* clear status message on focus change */
            ui->statuslabel_sm->clear();

            /* select generated signature */
            if (object == ui->signatureout_sm)
            {
                ui->signatureout_sm->selectall();
                return true;
            }
        }
        else if (ui->tabwidget->currentindex() == 1)
        {
            /* clear status message on focus change */
            ui->statuslabel_vm->clear();
        }
    }
    return qdialog::eventfilter(object, event);
}
