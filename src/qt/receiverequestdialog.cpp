// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "receiverequestdialog.h"
#include "ui_receiverequestdialog.h"

#include "moorecoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <qclipboard>
#include <qdrag>
#include <qmenu>
#include <qmimedata>
#include <qmouseevent>
#include <qpixmap>
#if qt_version < 0x050000
#include <qurl>
#endif

#if defined(have_config_h)
#include "config/moorecoin-config.h" /* for use_qrcode */
#endif

#ifdef use_qrcode
#include <qrencode.h>
#endif

qrimagewidget::qrimagewidget(qwidget *parent):
    qlabel(parent), contextmenu(0)
{
    contextmenu = new qmenu();
    qaction *saveimageaction = new qaction(tr("&save image..."), this);
    connect(saveimageaction, signal(triggered()), this, slot(saveimage()));
    contextmenu->addaction(saveimageaction);
    qaction *copyimageaction = new qaction(tr("&copy image"), this);
    connect(copyimageaction, signal(triggered()), this, slot(copyimage()));
    contextmenu->addaction(copyimageaction);
}

qimage qrimagewidget::exportimage()
{
    if(!pixmap())
        return qimage();
    return pixmap()->toimage().scaled(export_image_size, export_image_size);
}

void qrimagewidget::mousepressevent(qmouseevent *event)
{
    if(event->button() == qt::leftbutton && pixmap())
    {
        event->accept();
        qmimedata *mimedata = new qmimedata;
        mimedata->setimagedata(exportimage());

        qdrag *drag = new qdrag(this);
        drag->setmimedata(mimedata);
        drag->exec();
    } else {
        qlabel::mousepressevent(event);
    }
}

void qrimagewidget::saveimage()
{
    if(!pixmap())
        return;
    qstring fn = guiutil::getsavefilename(this, tr("save qr code"), qstring(), tr("png image (*.png)"), null);
    if (!fn.isempty())
    {
        exportimage().save(fn);
    }
}

void qrimagewidget::copyimage()
{
    if(!pixmap())
        return;
    qapplication::clipboard()->setimage(exportimage());
}

void qrimagewidget::contextmenuevent(qcontextmenuevent *event)
{
    if(!pixmap())
        return;
    contextmenu->exec(event->globalpos());
}

receiverequestdialog::receiverequestdialog(qwidget *parent) :
    qdialog(parent),
    ui(new ui::receiverequestdialog),
    model(0)
{
    ui->setupui(this);

#ifndef use_qrcode
    ui->btnsaveas->setvisible(false);
    ui->lblqrcode->setvisible(false);
#endif

    connect(ui->btnsaveas, signal(clicked()), ui->lblqrcode, slot(saveimage()));
}

receiverequestdialog::~receiverequestdialog()
{
    delete ui;
}

void receiverequestdialog::setmodel(optionsmodel *model)
{
    this->model = model;

    if (model)
        connect(model, signal(displayunitchanged(int)), this, slot(update()));

    // update the display unit if necessary
    update();
}

void receiverequestdialog::setinfo(const sendcoinsrecipient &info)
{
    this->info = info;
    update();
}

void receiverequestdialog::update()
{
    if(!model)
        return;
    qstring target = info.label;
    if(target.isempty())
        target = info.address;
    setwindowtitle(tr("request payment to %1").arg(target));

    qstring uri = guiutil::formatmoorecoinuri(info);
    ui->btnsaveas->setenabled(false);
    qstring html;
    html += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    html += "<b>"+tr("payment information")+"</b><br>";
    html += "<b>"+tr("uri")+"</b>: ";
    html += "<a href=\""+uri+"\">" + guiutil::htmlescape(uri) + "</a><br>";
    html += "<b>"+tr("address")+"</b>: " + guiutil::htmlescape(info.address) + "<br>";
    if(info.amount)
        html += "<b>"+tr("amount")+"</b>: " + moorecoinunits::formatwithunit(model->getdisplayunit(), info.amount) + "<br>";
    if(!info.label.isempty())
        html += "<b>"+tr("label")+"</b>: " + guiutil::htmlescape(info.label) + "<br>";
    if(!info.message.isempty())
        html += "<b>"+tr("message")+"</b>: " + guiutil::htmlescape(info.message) + "<br>";
    ui->outuri->settext(html);

#ifdef use_qrcode
    ui->lblqrcode->settext("");
    if(!uri.isempty())
    {
        // limit uri length
        if (uri.length() > max_uri_length)
        {
            ui->lblqrcode->settext(tr("resulting uri too long, try to reduce the text for label / message."));
        } else {
            qrcode *code = qrcode_encodestring(uri.toutf8().constdata(), 0, qr_eclevel_l, qr_mode_8, 1);
            if (!code)
            {
                ui->lblqrcode->settext(tr("error encoding uri into qr code."));
                return;
            }
            qimage myimage = qimage(code->width + 8, code->width + 8, qimage::format_rgb32);
            myimage.fill(0xffffff);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++)
            {
                for (int x = 0; x < code->width; x++)
                {
                    myimage.setpixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                    p++;
                }
            }
            qrcode_free(code);

            ui->lblqrcode->setpixmap(qpixmap::fromimage(myimage).scaled(300, 300));
            ui->btnsaveas->setenabled(true);
        }
    }
#endif
}

void receiverequestdialog::on_btncopyuri_clicked()
{
    guiutil::setclipboard(guiutil::formatmoorecoinuri(info));
}

void receiverequestdialog::on_btncopyaddress_clicked()
{
    guiutil::setclipboard(info.address);
}
