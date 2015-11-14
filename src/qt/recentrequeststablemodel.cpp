// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "recentrequeststablemodel.h"

#include "moorecoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"

#include "clientversion.h"
#include "streams.h"

#include <boost/foreach.hpp>

recentrequeststablemodel::recentrequeststablemodel(cwallet *wallet, walletmodel *parent) :
    walletmodel(parent)
{
    q_unused(wallet);
    nreceiverequestsmaxid = 0;

    // load entries from wallet
    std::vector<std::string> vreceiverequests;
    parent->loadreceiverequests(vreceiverequests);
    boost_foreach(const std::string& request, vreceiverequests)
        addnewrequest(request);

    /* these columns must match the indices in the columnindex enumeration */
    columns << tr("date") << tr("label") << tr("message") << getamounttitle();

    connect(walletmodel->getoptionsmodel(), signal(displayunitchanged(int)), this, slot(updatedisplayunit()));
}

recentrequeststablemodel::~recentrequeststablemodel()
{
    /* intentionally left empty */
}

int recentrequeststablemodel::rowcount(const qmodelindex &parent) const
{
    q_unused(parent);

    return list.length();
}

int recentrequeststablemodel::columncount(const qmodelindex &parent) const
{
    q_unused(parent);

    return columns.length();
}

qvariant recentrequeststablemodel::data(const qmodelindex &index, int role) const
{
    if(!index.isvalid() || index.row() >= list.length())
        return qvariant();

    const recentrequestentry *rec = &list[index.row()];

    if(role == qt::displayrole || role == qt::editrole)
    {
        switch(index.column())
        {
        case date:
            return guiutil::datetimestr(rec->date);
        case label:
            if(rec->recipient.label.isempty() && role == qt::displayrole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->recipient.label;
            }
        case message:
            if(rec->recipient.message.isempty() && role == qt::displayrole)
            {
                return tr("(no message)");
            }
            else
            {
                return rec->recipient.message;
            }
        case amount:
            if (rec->recipient.amount == 0 && role == qt::displayrole)
                return tr("(no amount)");
            else if (role == qt::editrole)
                return moorecoinunits::format(walletmodel->getoptionsmodel()->getdisplayunit(), rec->recipient.amount, false, moorecoinunits::separatornever);
            else
                return moorecoinunits::format(walletmodel->getoptionsmodel()->getdisplayunit(), rec->recipient.amount);
        }
    }
    else if (role == qt::textalignmentrole)
    {
        if (index.column() == amount)
            return (int)(qt::alignright|qt::alignvcenter);
    }
    return qvariant();
}

bool recentrequeststablemodel::setdata(const qmodelindex &index, const qvariant &value, int role)
{
    return true;
}

qvariant recentrequeststablemodel::headerdata(int section, qt::orientation orientation, int role) const
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

/** updates the column title to "amount (displayunit)" and emits headerdatachanged() signal for table headers to react. */
void recentrequeststablemodel::updateamountcolumntitle()
{
    columns[amount] = getamounttitle();
    emit headerdatachanged(qt::horizontal,amount,amount);
}

/** gets title for amount column including current display unit if optionsmodel reference available. */
qstring recentrequeststablemodel::getamounttitle()
{
    qstring amounttitle = tr("amount");
    if (this->walletmodel->getoptionsmodel() != null)
    {
        amounttitle += " ("+moorecoinunits::name(this->walletmodel->getoptionsmodel()->getdisplayunit()) + ")";
    }
    return amounttitle;
}

qmodelindex recentrequeststablemodel::index(int row, int column, const qmodelindex &parent) const
{
    q_unused(parent);

    return createindex(row, column);
}

bool recentrequeststablemodel::removerows(int row, int count, const qmodelindex &parent)
{
    q_unused(parent);

    if(count > 0 && row >= 0 && (row+count) <= list.size())
    {
        const recentrequestentry *rec;
        for (int i = 0; i < count; ++i)
        {
            rec = &list[row+i];
            if (!walletmodel->savereceiverequest(rec->recipient.address.tostdstring(), rec->id, ""))
                return false;
        }

        beginremoverows(parent, row, row + count - 1);
        list.erase(list.begin() + row, list.begin() + row + count);
        endremoverows();
        return true;
    } else {
        return false;
    }
}

qt::itemflags recentrequeststablemodel::flags(const qmodelindex &index) const
{
    return qt::itemisselectable | qt::itemisenabled;
}

// called when adding a request from the gui
void recentrequeststablemodel::addnewrequest(const sendcoinsrecipient &recipient)
{
    recentrequestentry newentry;
    newentry.id = ++nreceiverequestsmaxid;
    newentry.date = qdatetime::currentdatetime();
    newentry.recipient = recipient;

    cdatastream ss(ser_disk, client_version);
    ss << newentry;

    if (!walletmodel->savereceiverequest(recipient.address.tostdstring(), newentry.id, ss.str()))
        return;

    addnewrequest(newentry);
}

// called from ctor when loading from wallet
void recentrequeststablemodel::addnewrequest(const std::string &recipient)
{
    std::vector<char> data(recipient.begin(), recipient.end());
    cdatastream ss(data, ser_disk, client_version);

    recentrequestentry entry;
    ss >> entry;

    if (entry.id == 0) // should not happen
        return;

    if (entry.id > nreceiverequestsmaxid)
        nreceiverequestsmaxid = entry.id;

    addnewrequest(entry);
}

// actually add to table in gui
void recentrequeststablemodel::addnewrequest(recentrequestentry &recipient)
{
    begininsertrows(qmodelindex(), 0, 0);
    list.prepend(recipient);
    endinsertrows();
}

void recentrequeststablemodel::sort(int column, qt::sortorder order)
{
    qsort(list.begin(), list.end(), recentrequestentrylessthan(column, order));
    emit datachanged(index(0, 0, qmodelindex()), index(list.size() - 1, number_of_columns - 1, qmodelindex()));
}

void recentrequeststablemodel::updatedisplayunit()
{
    updateamountcolumntitle();
}

bool recentrequestentrylessthan::operator()(recentrequestentry &left, recentrequestentry &right) const
{
    recentrequestentry *pleft = &left;
    recentrequestentry *pright = &right;
    if (order == qt::descendingorder)
        std::swap(pleft, pright);

    switch(column)
    {
    case recentrequeststablemodel::date:
        return pleft->date.totime_t() < pright->date.totime_t();
    case recentrequeststablemodel::label:
        return pleft->recipient.label < pright->recipient.label;
    case recentrequeststablemodel::message:
        return pleft->recipient.message < pright->recipient.message;
    case recentrequeststablemodel::amount:
        return pleft->recipient.amount < pright->recipient.amount;
    default:
        return pleft->id < pright->id;
    }
}
