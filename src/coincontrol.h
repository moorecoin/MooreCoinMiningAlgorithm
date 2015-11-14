// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_coincontrol_h
#define moorecoin_coincontrol_h

#include "primitives/transaction.h"

/** coin control features. */
class ccoincontrol
{
public:
    ctxdestination destchange;

    ccoincontrol()
    {
        setnull();
    }

    void setnull()
    {
        destchange = cnodestination();
        setselected.clear();
    }

    bool hasselected() const
    {
        return (setselected.size() > 0);
    }

    bool isselected(const uint256& hash, unsigned int n) const
    {
        coutpoint outpt(hash, n);
        return (setselected.count(outpt) > 0);
    }

    void select(const coutpoint& output)
    {
        setselected.insert(output);
    }

    void unselect(const coutpoint& output)
    {
        setselected.erase(output);
    }

    void unselectall()
    {
        setselected.clear();
    }

    void listselected(std::vector<coutpoint>& voutpoints)
    {
        voutpoints.assign(setselected.begin(), setselected.end());
    }

private:
    std::set<coutpoint> setselected;
};

#endif // moorecoin_coincontrol_h
