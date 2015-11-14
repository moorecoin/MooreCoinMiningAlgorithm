// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "utilmoneystr.h"

#include "primitives/transaction.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

using namespace std;

std::string formatmoney(const camount& n)
{
    // note: not using straight sprintf here because we do not want
    // localized number formatting.
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs/coin;
    int64_t remainder = n_abs%coin;
    string str = strprintf("%d.%08d", quotient, remainder);

    // right-trim excess zeros before the decimal point:
    int ntrim = 0;
    for (int i = str.size()-1; (str[i] == '0' && isdigit(str[i-2])); --i)
        ++ntrim;
    if (ntrim)
        str.erase(str.size()-ntrim, ntrim);

    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    return str;
}


bool parsemoney(const string& str, camount& nret)
{
    return parsemoney(str.c_str(), nret);
}

bool parsemoney(const char* pszin, camount& nret)
{
    string strwhole;
    int64_t nunits = 0;
    const char* p = pszin;
    while (isspace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == '.')
        {
            p++;
            int64_t nmult = cent*10;
            while (isdigit(*p) && (nmult > 0))
            {
                nunits += nmult * (*p++ - '0');
                nmult /= 10;
            }
            break;
        }
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;
        strwhole.insert(strwhole.end(), *p);
    }
    for (; *p; p++)
        if (!isspace(*p))
            return false;
    if (strwhole.size() > 10) // guard against 63 bit overflow
        return false;
    if (nunits < 0 || nunits > coin)
        return false;
    int64_t nwhole = atoi64(strwhole);
    camount nvalue = nwhole*coin + nunits;

    nret = nvalue;
    return true;
}
