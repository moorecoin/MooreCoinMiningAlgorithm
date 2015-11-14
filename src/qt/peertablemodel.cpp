// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "peertablemodel.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"

#include "net.h"
#include "sync.h"

#include <qdebug>
#include <qlist>
#include <qtimer>

bool nodelessthan::operator()(const cnodecombinedstats &left, const cnodecombinedstats &right) const
{
    const cnodestats *pleft = &(left.nodestats);
    const cnodestats *pright = &(right.nodestats);

    if (order == qt::descendingorder)
        std::swap(pleft, pright);

    switch(column)
    {
    case peertablemodel::address:
        return pleft->addrname.compare(pright->addrname) < 0;
    case peertablemodel::subversion:
        return pleft->cleansubver.compare(pright->cleansubver) < 0;
    case peertablemodel::ping:
        return pleft->dpingtime < pright->dpingtime;
    }

    return false;
}

// private implementation
class peertablepriv
{
public:
    /** local cache of peer information */
    qlist<cnodecombinedstats> cachednodestats;
    /** column to sort nodes by */
    int sortcolumn;
    /** order (ascending or descending) to sort nodes by */
    qt::sortorder sortorder;
    /** index of rows by node id */
    std::map<nodeid, int> mapnoderows;

    /** pull a full list of peers from vnodes into our cache */
    void refreshpeers()
    {
        {
            try_lock(cs_vnodes, locknodes);
            if (!locknodes)
            {
                // skip the refresh if we can't immediately get the lock
                return;
            }
            cachednodestats.clear();
#if qt_version >= 0x040700
            cachednodestats.reserve(vnodes.size());
#endif
            foreach (cnode* pnode, vnodes)
            {
                cnodecombinedstats stats;
                stats.nodestatestats.nmisbehavior = 0;
                stats.nodestatestats.nsyncheight = -1;
                stats.nodestatestats.ncommonheight = -1;
                stats.fnodestatestatsavailable = false;
                pnode->copystats(stats.nodestats);
                cachednodestats.append(stats);
            }
        }

        // try to retrieve the cnodestatestats for each node.
        {
            try_lock(cs_main, lockmain);
            if (lockmain)
            {
                boost_foreach(cnodecombinedstats &stats, cachednodestats)
                    stats.fnodestatestatsavailable = getnodestatestats(stats.nodestats.nodeid, stats.nodestatestats);
            }
        }

        if (sortcolumn >= 0)
            // sort cachenodestats (use stable sort to prevent rows jumping around unneceesarily)
            qstablesort(cachednodestats.begin(), cachednodestats.end(), nodelessthan(sortcolumn, sortorder));

        // build index map
        mapnoderows.clear();
        int row = 0;
        foreach (const cnodecombinedstats& stats, cachednodestats)
            mapnoderows.insert(std::pair<nodeid, int>(stats.nodestats.nodeid, row++));
    }

    int size()
    {
        return cachednodestats.size();
    }

    cnodecombinedstats *index(int idx)
    {
        if(idx >= 0 && idx < cachednodestats.size()) {
            return &cachednodestats[idx];
        } else {
            return 0;
        }
    }
};

peertablemodel::peertablemodel(clientmodel *parent) :
    qabstracttablemodel(parent),
    clientmodel(parent),
    timer(0)
{
    columns << tr("node/service") << tr("user agent") << tr("ping time");
    priv = new peertablepriv();
    // default to unsorted
    priv->sortcolumn = -1;

    // set up timer for auto refresh
    timer = new qtimer();
    connect(timer, signal(timeout()), slot(refresh()));
    timer->setinterval(model_update_delay);

    // load initial data
    refresh();
}

void peertablemodel::startautorefresh()
{
    timer->start();
}

void peertablemodel::stopautorefresh()
{
    timer->stop();
}

int peertablemodel::rowcount(const qmodelindex &parent) const
{
    q_unused(parent);
    return priv->size();
}

int peertablemodel::columncount(const qmodelindex &parent) const
{
    q_unused(parent);
    return columns.length();;
}

qvariant peertablemodel::data(const qmodelindex &index, int role) const
{
    if(!index.isvalid())
        return qvariant();

    cnodecombinedstats *rec = static_cast<cnodecombinedstats*>(index.internalpointer());

    if (role == qt::displayrole) {
        switch(index.column())
        {
        case address:
            return qstring::fromstdstring(rec->nodestats.addrname);
        case subversion:
            return qstring::fromstdstring(rec->nodestats.cleansubver);
        case ping:
            return guiutil::formatpingtime(rec->nodestats.dpingtime);
        }
    } else if (role == qt::textalignmentrole) {
        if (index.column() == ping)
            return (int)(qt::alignright | qt::alignvcenter);
    }

    return qvariant();
}

qvariant peertablemodel::headerdata(int section, qt::orientation orientation, int role) const
{
    if(orientation == qt::horizontal)
    {
        if(role == qt::displayrole && section < columns.size())
        {
            return columns[section];
        }
    }
    return qvariant();
}

qt::itemflags peertablemodel::flags(const qmodelindex &index) const
{
    if(!index.isvalid())
        return 0;

    qt::itemflags retval = qt::itemisselectable | qt::itemisenabled;
    return retval;
}

qmodelindex peertablemodel::index(int row, int column, const qmodelindex &parent) const
{
    q_unused(parent);
    cnodecombinedstats *data = priv->index(row);

    if (data)
    {
        return createindex(row, column, data);
    }
    else
    {
        return qmodelindex();
    }
}

const cnodecombinedstats *peertablemodel::getnodestats(int idx)
{
    return priv->index(idx);
}

void peertablemodel::refresh()
{
    emit layoutabouttobechanged();
    priv->refreshpeers();
    emit layoutchanged();
}

int peertablemodel::getrowbynodeid(nodeid nodeid)
{
    std::map<nodeid, int>::iterator it = priv->mapnoderows.find(nodeid);
    if (it == priv->mapnoderows.end())
        return -1;

    return it->second;
}

void peertablemodel::sort(int column, qt::sortorder order)
{
    priv->sortcolumn = column;
    priv->sortorder = order;
    refresh();
}
