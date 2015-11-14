// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

/**
 * money parsing/formatting utilities.
 */
#ifndef moorecoin_utilmoneystr_h
#define moorecoin_utilmoneystr_h

#include <stdint.h>
#include <string>

#include "amount.h"

std::string formatmoney(const camount& n);
bool parsemoney(const std::string& str, camount& nret);
bool parsemoney(const char* pszin, camount& nret);

#endif // moorecoin_utilmoneystr_h
