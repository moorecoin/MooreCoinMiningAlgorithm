// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "transactionfilterproxy.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"

#include <cstdlib>

#include <qdatetime>

// earliest date that can be represented (far in the past)
const qdatetime transactionfilterproxy::min_date = qdatetime::fromtime_t(0);
// last date that can be represented (far in the future)
const qdatetime transactionfilterproxy::max_date = qdatetime::fromtime_t(0xffffffff);

transactionfilterproxy::transactionfilterproxy(qobject *parent) :
    qsortfilterproxymodel(parent),
    datefrom(min_date),
    dateto(max_date),
    addrprefix(),
    typefilter(all_types),
    watchonlyfilter(watchonlyfilter_all),
    minamount(0),
    limitrows(-1),
    showinactive(true)
{
}

bool transactionfilterproxy::filteracceptsrow(int sourcerow, const qmodelindex &sourceparent) const
{
    qmodelindex index = sourcemodel()->index(sourcerow, 0, sourceparent);

    int type = index.data(transactiontablemodel::typerole).toint();
    qdatetime datetime = index.data(transactiontablemodel::daterole).todatetime();
    bool involveswatchaddress = index.data(transactiontablemodel::watchonlyrole).tobool();
    qstring address = index.data(transactiontablemodel::addressrole).tostring();
    qstring label = index.data(transactiontablemodel::labelrole).tostring();
    qint64 amount = llabs(index.data(transactiontablemodel::amountrole).tolonglong());
    int status = index.data(transactiontablemodel::statusrole).toint();

    if(!showinactive && status == transactionstatus::conflicted)
        return false;
    if(!(type(type) & typefilter))
        return false;
    if (involveswatchaddress && watchonlyfilter == watchonlyfilter_no)
        return false;
    if (!involveswatchaddress && watchonlyfilter == watchonlyfilter_yes)
        return false;
    if(datetime < datefrom || datetime > dateto)
        return false;
    if (!address.contains(addrprefix, qt::caseinsensitive) && !label.contains(addrprefix, qt::caseinsensitive))
        return false;
    if(amount < minamount)
        return false;

    return true;
}

void transactionfilterproxy::setdaterange(const qdatetime &from, const qdatetime &to)
{
    this->datefrom = from;
    this->dateto = to;
    invalidatefilter();
}

void transactionfilterproxy::setaddressprefix(const qstring &addrprefix)
{
    this->addrprefix = addrprefix;
    invalidatefilter();
}

void transactionfilterproxy::settypefilter(quint32 modes)
{
    this->typefilter = modes;
    invalidatefilter();
}

void transactionfilterproxy::setminamount(const camount& minimum)
{
    this->minamount = minimum;
    invalidatefilter();
}

void transactionfilterproxy::setwatchonlyfilter(watchonlyfilter filter)
{
    this->watchonlyfilter = filter;
    invalidatefilter();
}

void transactionfilterproxy::setlimit(int limit)
{
    this->limitrows = limit;
}

void transactionfilterproxy::setshowinactive(bool showinactive)
{
    this->showinactive = showinactive;
    invalidatefilter();
}

int transactionfilterproxy::rowcount(const qmodelindex &parent) const
{
    if(limitrows != -1)
    {
        return std::min(qsortfilterproxymodel::rowcount(parent), limitrows);
    }
    else
    {
        return qsortfilterproxymodel::rowcount(parent);
    }
}
