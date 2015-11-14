// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "moorecoinaddressvalidator.h"

#include "base58.h"

/* base58 characters are:
     "123456789abcdefghjklmnpqrstuvwxyzabcdefghijkmnopqrstuvwxyz"

  this is:
  - all numbers except for '0'
  - all upper-case letters except for 'i' and 'o'
  - all lower-case letters except for 'l'
*/

moorecoinaddressentryvalidator::moorecoinaddressentryvalidator(qobject *parent) :
    qvalidator(parent)
{
}

qvalidator::state moorecoinaddressentryvalidator::validate(qstring &input, int &pos) const
{
    q_unused(pos);

    // empty address is "intermediate" input
    if (input.isempty())
        return qvalidator::intermediate;

    // correction
    for (int idx = 0; idx < input.size();)
    {
        bool removechar = false;
        qchar ch = input.at(idx);
        // corrections made are very conservative on purpose, to avoid
        // users unexpectedly getting away with typos that would normally
        // be detected, and thus sending to the wrong address.
        switch(ch.unicode())
        {
        // qt categorizes these as "other_format" not "separator_space"
        case 0x200b: // zero width space
        case 0xfeff: // zero width no-break space
            removechar = true;
            break;
        default:
            break;
        }

        // remove whitespace
        if (ch.isspace())
            removechar = true;

        // to next character
        if (removechar)
            input.remove(idx, 1);
        else
            ++idx;
    }

    // validation
    qvalidator::state state = qvalidator::acceptable;
    for (int idx = 0; idx < input.size(); ++idx)
    {
        int ch = input.at(idx).unicode();

        if (((ch >= '0' && ch<='9') ||
            (ch >= 'a' && ch<='z') ||
            (ch >= 'a' && ch<='z')) &&
            ch != 'l' && ch != 'i' && ch != '0' && ch != 'o')
        {
            // alphanumeric and not a 'forbidden' character
        }
        else
        {
            state = qvalidator::invalid;
        }
    }

    return state;
}

moorecoinaddresscheckvalidator::moorecoinaddresscheckvalidator(qobject *parent) :
    qvalidator(parent)
{
}

qvalidator::state moorecoinaddresscheckvalidator::validate(qstring &input, int &pos) const
{
    q_unused(pos);
    // validate the passed moorecoin address
    cmoorecoinaddress addr(input.tostdstring());
    if (addr.isvalid())
        return qvalidator::acceptable;

    return qvalidator::invalid;
}
