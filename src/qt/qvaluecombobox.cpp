// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "qvaluecombobox.h"

qvaluecombobox::qvaluecombobox(qwidget *parent) :
        qcombobox(parent), role(qt::userrole)
{
    connect(this, signal(currentindexchanged(int)), this, slot(handleselectionchanged(int)));
}

qvariant qvaluecombobox::value() const
{
    return itemdata(currentindex(), role);
}

void qvaluecombobox::setvalue(const qvariant &value)
{
    setcurrentindex(finddata(value, role));
}

void qvaluecombobox::setrole(int role)
{
    this->role = role;
}

void qvaluecombobox::handleselectionchanged(int idx)
{
    emit valuechanged();
}
