// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_utiltime_h
#define moorecoin_utiltime_h

#include <stdint.h>
#include <string>

int64_t gettime();
int64_t gettimemillis();
int64_t gettimemicros();
void setmocktime(int64_t nmocktimein);
void millisleep(int64_t n);

std::string datetimestrformat(const char* pszformat, int64_t ntime);

#endif // moorecoin_utiltime_h
