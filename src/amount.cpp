// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"

#include "tinyformat.h"

cfeerate::cfeerate(const camount& nfeepaid, size_t nsize)
{
    if (nsize > 0)
        nsatoshisperk = nfeepaid*1000/nsize;
    else
        nsatoshisperk = 0;
}

camount cfeerate::getfee(size_t nsize) const
{
    camount nfee = nsatoshisperk*nsize / 1000;

    if (nfee == 0 && nsatoshisperk > 0)
        nfee = nsatoshisperk;

    return nfee;
}

std::string cfeerate::tostring() const
{
    return strprintf("%d.%08d btc/kb", nsatoshisperk / coin, nsatoshisperk % coin);
}
