// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_eccryptoverify_h
#define moorecoin_eccryptoverify_h

#include <vector>
#include <cstdlib>

class uint256;

namespace eccrypto {

bool check(const unsigned char *vch);
bool checksignatureelement(const unsigned char *vch, int len, bool half);

} // eccrypto namespace

#endif // moorecoin_eccryptoverify_h
