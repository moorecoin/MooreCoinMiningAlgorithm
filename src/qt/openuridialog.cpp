// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "openuridialog.h"
#include "ui_openuridialog.h"

#include "guiutil.h"
#include "walletmodel.h"

#include <qurl>

openuridialog::openuridialog(qwidget *parent) :
    qdialog(parent),
    ui(new ui::openuridialog)
{
    ui->setupui(this);
#if qt_version >= 0x040700
    ui->uriedit->setplaceholdertext("moorecoin:");
#endif
}

openuridialog::~openuridialog()
{
    delete ui;
}

qstring openuridialog::geturi()
{
    return ui->uriedit->text();
}

void openuridialog::accept()
{
    sendcoinsrecipient rcp;
    if(guiutil::parsemoorecoinuri(geturi(), &rcp))
    {
        /* only accept value uris */
        qdialog::accept();
    } else {
        ui->uriedit->setvalid(false);
    }
}

void openuridialog::on_selectfilebutton_clicked()
{
    qstring filename = guiutil::getopenfilename(this, tr("select payment request file to open"), "", "", null);
    if(filename.isempty())
        return;
    qurl fileuri = qurl::fromlocalfile(filename);
    ui->uriedit->settext("moorecoin:?r=" + qurl::topercentencoding(fileuri.tostring()));
}
