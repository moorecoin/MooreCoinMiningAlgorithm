// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_qt_moorecoinaddressvalidator_h
#define moorecoin_qt_moorecoinaddressvalidator_h

#include <qvalidator>

/** base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class moorecoinaddressentryvalidator : public qvalidator
{
    q_object

public:
    explicit moorecoinaddressentryvalidator(qobject *parent);

    state validate(qstring &input, int &pos) const;
};

/** moorecoin address widget validator, checks for a valid moorecoin address.
 */
class moorecoinaddresscheckvalidator : public qvalidator
{
    q_object

public:
    explicit moorecoinaddresscheckvalidator(qobject *parent);

    state validate(qstring &input, int &pos) const;
};

#endif // moorecoin_qt_moorecoinaddressvalidator_h
