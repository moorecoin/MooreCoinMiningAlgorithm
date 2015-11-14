// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"

#include "transactiontablemodel.h"

#include <qmodelindex>

transactiondescdialog::transactiondescdialog(const qmodelindex &idx, qwidget *parent) :
    qdialog(parent),
    ui(new ui::transactiondescdialog)
{
    ui->setupui(this);
    qstring desc = idx.data(transactiontablemodel::longdescriptionrole).tostring();
    ui->detailtext->sethtml(desc);
}

transactiondescdialog::~transactiondescdialog()
{
    delete ui;
}
