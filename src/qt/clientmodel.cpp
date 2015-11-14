// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "clientmodel.h"

#include "guiconstants.h"
#include "peertablemodel.h"

#include "alert.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "ui_interface.h"
#include "util.h"

#include <stdint.h>

#include <qdebug>
#include <qtimer>

static const int64_t nclientstartuptime = gettime();

clientmodel::clientmodel(optionsmodel *optionsmodel, qobject *parent) :
    qobject(parent),
    optionsmodel(optionsmodel),
    peertablemodel(0),
    cachednumblocks(0),
    cachedblockdate(qdatetime()),
    cachedreindexing(0),
    cachedimporting(0),
    polltimer(0)
{
    peertablemodel = new peertablemodel(this);
    polltimer = new qtimer(this);
    connect(polltimer, signal(timeout()), this, slot(updatetimer()));
    polltimer->start(model_update_delay);

    subscribetocoresignals();
}

clientmodel::~clientmodel()
{
    unsubscribefromcoresignals();
}

int clientmodel::getnumconnections(unsigned int flags) const
{
    lock(cs_vnodes);
    if (flags == connections_all) // shortcut if we want total
        return vnodes.size();

    int nnum = 0;
    boost_foreach(cnode* pnode, vnodes)
    if (flags & (pnode->finbound ? connections_in : connections_out))
        nnum++;

    return nnum;
}

int clientmodel::getnumblocks() const
{
    lock(cs_main);
    return chainactive.height();
}

quint64 clientmodel::gettotalbytesrecv() const
{
    return cnode::gettotalbytesrecv();
}

quint64 clientmodel::gettotalbytessent() const
{
    return cnode::gettotalbytessent();
}

qdatetime clientmodel::getlastblockdate() const
{
    lock(cs_main);

    if (chainactive.tip())
        return qdatetime::fromtime_t(chainactive.tip()->getblocktime());

    return qdatetime::fromtime_t(params().genesisblock().getblocktime()); // genesis block's time of current network
}

double clientmodel::getverificationprogress() const
{
    lock(cs_main);
    return checkpoints::guessverificationprogress(params().checkpoints(), chainactive.tip());
}

void clientmodel::updatetimer()
{
    // get required lock upfront. this avoids the gui from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    try_lock(cs_main, lockmain);
    if (!lockmain)
        return;

    // some quantities (such as number of blocks) change so fast that we don't want to be notified for each change.
    // periodically check and update with a timer.
    int newnumblocks = getnumblocks();
    qdatetime newblockdate = getlastblockdate();

    // check for changed number of blocks we have, number of blocks peers claim to have, reindexing state and importing state
    if (cachednumblocks != newnumblocks ||
        cachedblockdate != newblockdate ||
        cachedreindexing != freindex ||
        cachedimporting != fimporting)
    {
        cachednumblocks = newnumblocks;
        cachedblockdate = newblockdate;
        cachedreindexing = freindex;
        cachedimporting = fimporting;

        emit numblockschanged(newnumblocks, newblockdate);
    }

    emit byteschanged(gettotalbytesrecv(), gettotalbytessent());
}

void clientmodel::updatenumconnections(int numconnections)
{
    emit numconnectionschanged(numconnections);
}

void clientmodel::updatealert(const qstring &hash, int status)
{
    // show error message notification for new alert
    if(status == ct_new)
    {
        uint256 hash_256;
        hash_256.sethex(hash.tostdstring());
        calert alert = calert::getalertbyhash(hash_256);
        if(!alert.isnull())
        {
            emit message(tr("network alert"), qstring::fromstdstring(alert.strstatusbar), cclientuiinterface::icon_error);
        }
    }

    emit alertschanged(getstatusbarwarnings());
}

bool clientmodel::ininitialblockdownload() const
{
    return isinitialblockdownload();
}

enum blocksource clientmodel::getblocksource() const
{
    if (freindex)
        return block_source_reindex;
    else if (fimporting)
        return block_source_disk;
    else if (getnumconnections() > 0)
        return block_source_network;

    return block_source_none;
}

qstring clientmodel::getstatusbarwarnings() const
{
    return qstring::fromstdstring(getwarnings("statusbar"));
}

optionsmodel *clientmodel::getoptionsmodel()
{
    return optionsmodel;
}

peertablemodel *clientmodel::getpeertablemodel()
{
    return peertablemodel;
}

qstring clientmodel::formatfullversion() const
{
    return qstring::fromstdstring(formatfullversion());
}

qstring clientmodel::formatbuilddate() const
{
    return qstring::fromstdstring(client_date);
}

bool clientmodel::isreleaseversion() const
{
    return client_version_is_release;
}

qstring clientmodel::clientname() const
{
    return qstring::fromstdstring(client_name);
}

qstring clientmodel::formatclientstartuptime() const
{
    return qdatetime::fromtime_t(nclientstartuptime).tostring();
}

// handlers for core signals
static void showprogress(clientmodel *clientmodel, const std::string &title, int nprogress)
{
    // emits signal "showprogress"
    qmetaobject::invokemethod(clientmodel, "showprogress", qt::queuedconnection,
                              q_arg(qstring, qstring::fromstdstring(title)),
                              q_arg(int, nprogress));
}

static void notifynumconnectionschanged(clientmodel *clientmodel, int newnumconnections)
{
    // too noisy: qdebug() << "notifynumconnectionschanged: " + qstring::number(newnumconnections);
    qmetaobject::invokemethod(clientmodel, "updatenumconnections", qt::queuedconnection,
                              q_arg(int, newnumconnections));
}

static void notifyalertchanged(clientmodel *clientmodel, const uint256 &hash, changetype status)
{
    qdebug() << "notifyalertchanged: " + qstring::fromstdstring(hash.gethex()) + " status=" + qstring::number(status);
    qmetaobject::invokemethod(clientmodel, "updatealert", qt::queuedconnection,
                              q_arg(qstring, qstring::fromstdstring(hash.gethex())),
                              q_arg(int, status));
}

void clientmodel::subscribetocoresignals()
{
    // connect signals to client
    uiinterface.showprogress.connect(boost::bind(showprogress, this, _1, _2));
    uiinterface.notifynumconnectionschanged.connect(boost::bind(notifynumconnectionschanged, this, _1));
    uiinterface.notifyalertchanged.connect(boost::bind(notifyalertchanged, this, _1, _2));
}

void clientmodel::unsubscribefromcoresignals()
{
    // disconnect signals from client
    uiinterface.showprogress.disconnect(boost::bind(showprogress, this, _1, _2));
    uiinterface.notifynumconnectionschanged.disconnect(boost::bind(notifynumconnectionschanged, this, _1));
    uiinterface.notifyalertchanged.disconnect(boost::bind(notifyalertchanged, this, _1, _2));
}
