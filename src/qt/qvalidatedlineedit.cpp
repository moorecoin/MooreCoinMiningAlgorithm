// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "qvalidatedlineedit.h"

#include "moorecoinaddressvalidator.h"
#include "guiconstants.h"

qvalidatedlineedit::qvalidatedlineedit(qwidget *parent) :
    qlineedit(parent),
    valid(true),
    checkvalidator(0)
{
    connect(this, signal(textchanged(qstring)), this, slot(markvalid()));
}

void qvalidatedlineedit::setvalid(bool valid)
{
    if(valid == this->valid)
    {
        return;
    }

    if(valid)
    {
        setstylesheet("");
    }
    else
    {
        setstylesheet(style_invalid);
    }
    this->valid = valid;
}

void qvalidatedlineedit::focusinevent(qfocusevent *evt)
{
    // clear invalid flag on focus
    setvalid(true);

    qlineedit::focusinevent(evt);
}

void qvalidatedlineedit::focusoutevent(qfocusevent *evt)
{
    checkvalidity();

    qlineedit::focusoutevent(evt);
}

void qvalidatedlineedit::markvalid()
{
    // as long as a user is typing ensure we display state as valid
    setvalid(true);
}

void qvalidatedlineedit::clear()
{
    setvalid(true);
    qlineedit::clear();
}

void qvalidatedlineedit::setenabled(bool enabled)
{
    if (!enabled)
    {
        // a disabled qvalidatedlineedit should be marked valid
        setvalid(true);
    }
    else
    {
        // recheck validity when qvalidatedlineedit gets enabled
        checkvalidity();
    }

    qlineedit::setenabled(enabled);
}

void qvalidatedlineedit::checkvalidity()
{
    if (text().isempty())
    {
        setvalid(true);
    }
    else if (hasacceptableinput())
    {
        setvalid(true);

        // check contents on focus out
        if (checkvalidator)
        {
            qstring address = text();
            int pos = 0;
            if (checkvalidator->validate(address, pos) == qvalidator::acceptable)
                setvalid(true);
            else
                setvalid(false);
        }
    }
    else
        setvalid(false);
}

void qvalidatedlineedit::setcheckvalidator(const qvalidator *v)
{
    checkvalidator = v;
}
