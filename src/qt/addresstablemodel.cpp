// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "addresstablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"

#include "base58.h"
#include "wallet/wallet.h"

#include <boost/foreach.hpp>

#include <qfont>
#include <qdebug>

const qstring addresstablemodel::send = "s";
const qstring addresstablemodel::receive = "r";

struct addresstableentry
{
    enum type {
        sending,
        receiving,
        hidden /* qsortfilterproxymodel will filter these out */
    };

    type type;
    qstring label;
    qstring address;

    addresstableentry() {}
    addresstableentry(type type, const qstring &label, const qstring &address):
        type(type), label(label), address(address) {}
};

struct addresstableentrylessthan
{
    bool operator()(const addresstableentry &a, const addresstableentry &b) const
    {
        return a.address < b.address;
    }
    bool operator()(const addresstableentry &a, const qstring &b) const
    {
        return a.address < b;
    }
    bool operator()(const qstring &a, const addresstableentry &b) const
    {
        return a < b.address;
    }
};

/* determine address type from address purpose */
static addresstableentry::type translatetransactiontype(const qstring &strpurpose, bool ismine)
{
    addresstableentry::type addresstype = addresstableentry::hidden;
    // "refund" addresses aren't shown, and change addresses aren't in mapaddressbook at all.
    if (strpurpose == "send")
        addresstype = addresstableentry::sending;
    else if (strpurpose == "receive")
        addresstype = addresstableentry::receiving;
    else if (strpurpose == "unknown" || strpurpose == "") // if purpose not set, guess
        addresstype = (ismine ? addresstableentry::receiving : addresstableentry::sending);
    return addresstype;
}

// private implementation
class addresstablepriv
{
public:
    cwallet *wallet;
    qlist<addresstableentry> cachedaddresstable;
    addresstablemodel *parent;

    addresstablepriv(cwallet *wallet, addresstablemodel *parent):
        wallet(wallet), parent(parent) {}

    void refreshaddresstable()
    {
        cachedaddresstable.clear();
        {
            lock(wallet->cs_wallet);
            boost_foreach(const pairtype(ctxdestination, caddressbookdata)& item, wallet->mapaddressbook)
            {
                const cmoorecoinaddress& address = item.first;
                bool fmine = ismine(*wallet, address.get());
                addresstableentry::type addresstype = translatetransactiontype(
                        qstring::fromstdstring(item.second.purpose), fmine);
                const std::string& strname = item.second.name;
                cachedaddresstable.append(addresstableentry(addresstype,
                                  qstring::fromstdstring(strname),
                                  qstring::fromstdstring(address.tostring())));
            }
        }
        // qlowerbound() and qupperbound() require our cachedaddresstable list to be sorted in asc order
        // even though the map is already sorted this re-sorting step is needed because the originating map
        // is sorted by binary address, not by base58() address.
        qsort(cachedaddresstable.begin(), cachedaddresstable.end(), addresstableentrylessthan());
    }

    void updateentry(const qstring &address, const qstring &label, bool ismine, const qstring &purpose, int status)
    {
        // find address / label in model
        qlist<addresstableentry>::iterator lower = qlowerbound(
            cachedaddresstable.begin(), cachedaddresstable.end(), address, addresstableentrylessthan());
        qlist<addresstableentry>::iterator upper = qupperbound(
            cachedaddresstable.begin(), cachedaddresstable.end(), address, addresstableentrylessthan());
        int lowerindex = (lower - cachedaddresstable.begin());
        int upperindex = (upper - cachedaddresstable.begin());
        bool inmodel = (lower != upper);
        addresstableentry::type newentrytype = translatetransactiontype(purpose, ismine);

        switch(status)
        {
        case ct_new:
            if(inmodel)
            {
                qwarning() << "addresstablepriv::updateentry: warning: got ct_new, but entry is already in model";
                break;
            }
            parent->begininsertrows(qmodelindex(), lowerindex, lowerindex);
            cachedaddresstable.insert(lowerindex, addresstableentry(newentrytype, label, address));
            parent->endinsertrows();
            break;
        case ct_updated:
            if(!inmodel)
            {
                qwarning() << "addresstablepriv::updateentry: warning: got ct_updated, but entry is not in model";
                break;
            }
            lower->type = newentrytype;
            lower->label = label;
            parent->emitdatachanged(lowerindex);
            break;
        case ct_deleted:
            if(!inmodel)
            {
                qwarning() << "addresstablepriv::updateentry: warning: got ct_deleted, but entry is not in model";
                break;
            }
            parent->beginremoverows(qmodelindex(), lowerindex, upperindex-1);
            cachedaddresstable.erase(lower, upper);
            parent->endremoverows();
            break;
        }
    }

    int size()
    {
        return cachedaddresstable.size();
    }

    addresstableentry *index(int idx)
    {
        if(idx >= 0 && idx < cachedaddresstable.size())
        {
            return &cachedaddresstable[idx];
        }
        else
        {
            return 0;
        }
    }
};

addresstablemodel::addresstablemodel(cwallet *wallet, walletmodel *parent) :
    qabstracttablemodel(parent),walletmodel(parent),wallet(wallet),priv(0)
{
    columns << tr("label") << tr("address");
    priv = new addresstablepriv(wallet, this);
    priv->refreshaddresstable();
}

addresstablemodel::~addresstablemodel()
{
    delete priv;
}

int addresstablemodel::rowcount(const qmodelindex &parent) const
{
    q_unused(parent);
    return priv->size();
}

int addresstablemodel::columncount(const qmodelindex &parent) const
{
    q_unused(parent);
    return columns.length();
}

qvariant addresstablemodel::data(const qmodelindex &index, int role) const
{
    if(!index.isvalid())
        return qvariant();

    addresstableentry *rec = static_cast<addresstableentry*>(index.internalpointer());

    if(role == qt::displayrole || role == qt::editrole)
    {
        switch(index.column())
        {
        case label:
            if(rec->label.isempty() && role == qt::displayrole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->label;
            }
        case address:
            return rec->address;
        }
    }
    else if (role == qt::fontrole)
    {
        qfont font;
        if(index.column() == address)
        {
            font = guiutil::moorecoinaddressfont();
        }
        return font;
    }
    else if (role == typerole)
    {
        switch(rec->type)
        {
        case addresstableentry::sending:
            return send;
        case addresstableentry::receiving:
            return receive;
        default: break;
        }
    }
    return qvariant();
}

bool addresstablemodel::setdata(const qmodelindex &index, const qvariant &value, int role)
{
    if(!index.isvalid())
        return false;
    addresstableentry *rec = static_cast<addresstableentry*>(index.internalpointer());
    std::string strpurpose = (rec->type == addresstableentry::sending ? "send" : "receive");
    editstatus = ok;

    if(role == qt::editrole)
    {
        lock(wallet->cs_wallet); /* for setaddressbook / deladdressbook */
        ctxdestination curaddress = cmoorecoinaddress(rec->address.tostdstring()).get();
        if(index.column() == label)
        {
            // do nothing, if old label == new label
            if(rec->label == value.tostring())
            {
                editstatus = no_changes;
                return false;
            }
            wallet->setaddressbook(curaddress, value.tostring().tostdstring(), strpurpose);
        } else if(index.column() == address) {
            ctxdestination newaddress = cmoorecoinaddress(value.tostring().tostdstring()).get();
            // refuse to set invalid address, set error status and return false
            if(boost::get<cnodestination>(&newaddress))
            {
                editstatus = invalid_address;
                return false;
            }
            // do nothing, if old address == new address
            else if(newaddress == curaddress)
            {
                editstatus = no_changes;
                return false;
            }
            // check for duplicate addresses to prevent accidental deletion of addresses, if you try
            // to paste an existing address over another address (with a different label)
            else if(wallet->mapaddressbook.count(newaddress))
            {
                editstatus = duplicate_address;
                return false;
            }
            // double-check that we're not overwriting a receiving address
            else if(rec->type == addresstableentry::sending)
            {
                // remove old entry
                wallet->deladdressbook(curaddress);
                // add new entry with new address
                wallet->setaddressbook(newaddress, rec->label.tostdstring(), strpurpose);
            }
        }
        return true;
    }
    return false;
}

qvariant addresstablemodel::headerdata(int section, qt::orientation orientation, int role) const
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

qt::itemflags addresstablemodel::flags(const qmodelindex &index) const
{
    if(!index.isvalid())
        return 0;
    addresstableentry *rec = static_cast<addresstableentry*>(index.internalpointer());

    qt::itemflags retval = qt::itemisselectable | qt::itemisenabled;
    // can edit address and label for sending addresses,
    // and only label for receiving addresses.
    if(rec->type == addresstableentry::sending ||
      (rec->type == addresstableentry::receiving && index.column()==label))
    {
        retval |= qt::itemiseditable;
    }
    return retval;
}

qmodelindex addresstablemodel::index(int row, int column, const qmodelindex &parent) const
{
    q_unused(parent);
    addresstableentry *data = priv->index(row);
    if(data)
    {
        return createindex(row, column, priv->index(row));
    }
    else
    {
        return qmodelindex();
    }
}

void addresstablemodel::updateentry(const qstring &address,
        const qstring &label, bool ismine, const qstring &purpose, int status)
{
    // update address book model from moorecoin core
    priv->updateentry(address, label, ismine, purpose, status);
}

qstring addresstablemodel::addrow(const qstring &type, const qstring &label, const qstring &address)
{
    std::string strlabel = label.tostdstring();
    std::string straddress = address.tostdstring();

    editstatus = ok;

    if(type == send)
    {
        if(!walletmodel->validateaddress(address))
        {
            editstatus = invalid_address;
            return qstring();
        }
        // check for duplicate addresses
        {
            lock(wallet->cs_wallet);
            if(wallet->mapaddressbook.count(cmoorecoinaddress(straddress).get()))
            {
                editstatus = duplicate_address;
                return qstring();
            }
        }
    }
    else if(type == receive)
    {
        // generate a new address to associate with given label
        cpubkey newkey;
        if(!wallet->getkeyfrompool(newkey))
        {
            walletmodel::unlockcontext ctx(walletmodel->requestunlock());
            if(!ctx.isvalid())
            {
                // unlock wallet failed or was cancelled
                editstatus = wallet_unlock_failure;
                return qstring();
            }
            if(!wallet->getkeyfrompool(newkey))
            {
                editstatus = key_generation_failure;
                return qstring();
            }
        }
        straddress = cmoorecoinaddress(newkey.getid()).tostring();
    }
    else
    {
        return qstring();
    }

    // add entry
    {
        lock(wallet->cs_wallet);
        wallet->setaddressbook(cmoorecoinaddress(straddress).get(), strlabel,
                               (type == send ? "send" : "receive"));
    }
    return qstring::fromstdstring(straddress);
}

bool addresstablemodel::removerows(int row, int count, const qmodelindex &parent)
{
    q_unused(parent);
    addresstableentry *rec = priv->index(row);
    if(count != 1 || !rec || rec->type == addresstableentry::receiving)
    {
        // can only remove one row at a time, and cannot remove rows not in model.
        // also refuse to remove receiving addresses.
        return false;
    }
    {
        lock(wallet->cs_wallet);
        wallet->deladdressbook(cmoorecoinaddress(rec->address.tostdstring()).get());
    }
    return true;
}

/* look up label for address in address book, if not found return empty string.
 */
qstring addresstablemodel::labelforaddress(const qstring &address) const
{
    {
        lock(wallet->cs_wallet);
        cmoorecoinaddress address_parsed(address.tostdstring());
        std::map<ctxdestination, caddressbookdata>::iterator mi = wallet->mapaddressbook.find(address_parsed.get());
        if (mi != wallet->mapaddressbook.end())
        {
            return qstring::fromstdstring(mi->second.name);
        }
    }
    return qstring();
}

int addresstablemodel::lookupaddress(const qstring &address) const
{
    qmodelindexlist lst = match(index(0, address, qmodelindex()),
                                qt::editrole, address, 1, qt::matchexactly);
    if(lst.isempty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

void addresstablemodel::emitdatachanged(int idx)
{
    emit datachanged(index(idx, 0, qmodelindex()), index(idx, columns.length()-1, qmodelindex()));
}
