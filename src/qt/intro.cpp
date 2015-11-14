// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "intro.h"
#include "ui_intro.h"

#include "guiutil.h"
#include "scicon.h"

#include "util.h"

#include <boost/filesystem.hpp>

#include <qfiledialog>
#include <qsettings>
#include <qmessagebox>

/* minimum free space (in bytes) needed for data directory */
static const uint64_t gb_bytes = 1000000000ll;
static const uint64_t block_chain_size = 20ll * gb_bytes;

/* check free space asynchronously to prevent hanging the ui thread.

   up to one request to check a path is in flight to this thread; when the check()
   function runs, the current path is requested from the associated intro object.
   the reply is sent back through a signal.

   this ensures that no queue of checking requests is built up while the user is
   still entering the path, and that always the most recently entered path is checked as
   soon as the thread becomes available.
*/
class freespacechecker : public qobject
{
    q_object

public:
    freespacechecker(intro *intro);

    enum status {
        st_ok,
        st_error
    };

public slots:
    void check();

signals:
    void reply(int status, const qstring &message, quint64 available);

private:
    intro *intro;
};

#include "intro.moc"

freespacechecker::freespacechecker(intro *intro)
{
    this->intro = intro;
}

void freespacechecker::check()
{
    namespace fs = boost::filesystem;
    qstring datadirstr = intro->getpathtocheck();
    fs::path datadir = guiutil::qstringtoboostpath(datadirstr);
    uint64_t freebytesavailable = 0;
    int replystatus = st_ok;
    qstring replymessage = tr("a new data directory will be created.");

    /* find first parent that exists, so that fs::space does not fail */
    fs::path parentdir = datadir;
    fs::path parentdirold = fs::path();
    while(parentdir.has_parent_path() && !fs::exists(parentdir))
    {
        parentdir = parentdir.parent_path();

        /* check if we make any progress, break if not to prevent an infinite loop here */
        if (parentdirold == parentdir)
            break;

        parentdirold = parentdir;
    }

    try {
        freebytesavailable = fs::space(parentdir).available;
        if(fs::exists(datadir))
        {
            if(fs::is_directory(datadir))
            {
                qstring separator = "<code>" + qdir::tonativeseparators("/") + tr("name") + "</code>";
                replystatus = st_ok;
                replymessage = tr("directory already exists. add %1 if you intend to create a new directory here.").arg(separator);
            } else {
                replystatus = st_error;
                replymessage = tr("path already exists, and is not a directory.");
            }
        }
    } catch (const fs::filesystem_error&)
    {
        /* parent directory does not exist or is not accessible */
        replystatus = st_error;
        replymessage = tr("cannot create data directory here.");
    }
    emit reply(replystatus, replymessage, freebytesavailable);
}


intro::intro(qwidget *parent) :
    qdialog(parent),
    ui(new ui::intro),
    thread(0),
    signalled(false)
{
    ui->setupui(this);
    ui->sizewarninglabel->settext(ui->sizewarninglabel->text().arg(block_chain_size/gb_bytes));
    startthread();
}

intro::~intro()
{
    delete ui;
    /* ensure thread is finished before it is deleted */
    emit stopthread();
    thread->wait();
}

qstring intro::getdatadirectory()
{
    return ui->datadirectory->text();
}

void intro::setdatadirectory(const qstring &datadir)
{
    ui->datadirectory->settext(datadir);
    if(datadir == getdefaultdatadirectory())
    {
        ui->datadirdefault->setchecked(true);
        ui->datadirectory->setenabled(false);
        ui->ellipsisbutton->setenabled(false);
    } else {
        ui->datadircustom->setchecked(true);
        ui->datadirectory->setenabled(true);
        ui->ellipsisbutton->setenabled(true);
    }
}

qstring intro::getdefaultdatadirectory()
{
    return guiutil::boostpathtoqstring(getdefaultdatadir());
}

void intro::pickdatadirectory()
{
    namespace fs = boost::filesystem;
    qsettings settings;
    /* if data directory provided on command line, no need to look at settings
       or show a picking dialog */
    if(!getarg("-datadir", "").empty())
        return;
    /* 1) default data directory for operating system */
    qstring datadir = getdefaultdatadirectory();
    /* 2) allow qsettings to override default dir */
    datadir = settings.value("strdatadir", datadir).tostring();

    if(!fs::exists(guiutil::qstringtoboostpath(datadir)) || getboolarg("-choosedatadir", false))
    {
        /* if current default data directory does not exist, let the user choose one */
        intro intro;
        intro.setdatadirectory(datadir);
        intro.setwindowicon(singlecoloricon(":icons/moorecoin"));

        while(true)
        {
            if(!intro.exec())
            {
                /* cancel clicked */
                exit(0);
            }
            datadir = intro.getdatadirectory();
            try {
                trycreatedirectory(guiutil::qstringtoboostpath(datadir));
                break;
            } catch (const fs::filesystem_error&) {
                qmessagebox::critical(0, tr("moorecoin core"),
                    tr("error: specified data directory \"%1\" cannot be created.").arg(datadir));
                /* fall through, back to choosing screen */
            }
        }

        settings.setvalue("strdatadir", datadir);
    }
    /* only override -datadir if different from the default, to make it possible to
     * override -datadir in the moorecoin.conf file in the default data directory
     * (to be consistent with moorecoind behavior)
     */
    if(datadir != getdefaultdatadirectory())
        softsetarg("-datadir", guiutil::qstringtoboostpath(datadir).string()); // use os locale for path setting
}

void intro::setstatus(int status, const qstring &message, quint64 bytesavailable)
{
    switch(status)
    {
    case freespacechecker::st_ok:
        ui->errormessage->settext(message);
        ui->errormessage->setstylesheet("");
        break;
    case freespacechecker::st_error:
        ui->errormessage->settext(tr("error") + ": " + message);
        ui->errormessage->setstylesheet("qlabel { color: #800000 }");
        break;
    }
    /* indicate number of bytes available */
    if(status == freespacechecker::st_error)
    {
        ui->freespace->settext("");
    } else {
        qstring freestring = tr("%n gb of free space available", "", bytesavailable/gb_bytes);
        if(bytesavailable < block_chain_size)
        {
            freestring += " " + tr("(of %n gb needed)", "", block_chain_size/gb_bytes);
            ui->freespace->setstylesheet("qlabel { color: #800000 }");
        } else {
            ui->freespace->setstylesheet("");
        }
        ui->freespace->settext(freestring + ".");
    }
    /* don't allow confirm in error state */
    ui->buttonbox->button(qdialogbuttonbox::ok)->setenabled(status != freespacechecker::st_error);
}

void intro::on_datadirectory_textchanged(const qstring &datadirstr)
{
    /* disable ok button until check result comes in */
    ui->buttonbox->button(qdialogbuttonbox::ok)->setenabled(false);
    checkpath(datadirstr);
}

void intro::on_ellipsisbutton_clicked()
{
    qstring dir = qdir::tonativeseparators(qfiledialog::getexistingdirectory(0, "choose data directory", ui->datadirectory->text()));
    if(!dir.isempty())
        ui->datadirectory->settext(dir);
}

void intro::on_datadirdefault_clicked()
{
    setdatadirectory(getdefaultdatadirectory());
}

void intro::on_datadircustom_clicked()
{
    ui->datadirectory->setenabled(true);
    ui->ellipsisbutton->setenabled(true);
}

void intro::startthread()
{
    thread = new qthread(this);
    freespacechecker *executor = new freespacechecker(this);
    executor->movetothread(thread);

    connect(executor, signal(reply(int,qstring,quint64)), this, slot(setstatus(int,qstring,quint64)));
    connect(this, signal(requestcheck()), executor, slot(check()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, signal(stopthread()), executor, slot(deletelater()));
    connect(this, signal(stopthread()), thread, slot(quit()));

    thread->start();
}

void intro::checkpath(const qstring &datadir)
{
    mutex.lock();
    pathtocheck = datadir;
    if(!signalled)
    {
        signalled = true;
        emit requestcheck();
    }
    mutex.unlock();
}

qstring intro::getpathtocheck()
{
    qstring retval;
    mutex.lock();
    retval = pathtocheck;
    signalled = false; /* new request can be queued now */
    mutex.unlock();
    return retval;
}
