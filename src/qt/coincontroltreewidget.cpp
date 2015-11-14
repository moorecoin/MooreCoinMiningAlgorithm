// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "coincontroltreewidget.h"
#include "coincontroldialog.h"

coincontroltreewidget::coincontroltreewidget(qwidget *parent) :
    qtreewidget(parent)
{

}

void coincontroltreewidget::keypressevent(qkeyevent *event)
{
    if (event->key() == qt::key_space) // press spacebar -> select checkbox
    {
        event->ignore();
        int column_checkbox = 0;
        if(this->currentitem())
            this->currentitem()->setcheckstate(column_checkbox, ((this->currentitem()->checkstate(column_checkbox) == qt::checked) ? qt::unchecked : qt::checked));
    }
    else if (event->key() == qt::key_escape) // press esc -> close dialog
    {
        event->ignore();
        coincontroldialog *coincontroldialog = (coincontroldialog*)this->parentwidget();
        coincontroldialog->done(qdialog::accepted);
    }
    else
    {
        this->qtreewidget::keypressevent(event);
    }
}
