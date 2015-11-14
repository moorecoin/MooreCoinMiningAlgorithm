// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "utiltime.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

using namespace std;

static int64_t nmocktime = 0;  //! for unit testing

int64_t gettime()
{
    if (nmocktime) return nmocktime;

    return time(null);
}

void setmocktime(int64_t nmocktimein)
{
    nmocktime = nmocktimein;
}

int64_t gettimemillis()
{
    return (boost::posix_time::microsec_clock::universal_time() -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
}

int64_t gettimemicros()
{
    return (boost::posix_time::microsec_clock::universal_time() -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

void millisleep(int64_t n)
{

/**
 * boost's sleep_for was uninterruptable when backed by nanosleep from 1.50
 * until fixed in 1.52. use the deprecated sleep method for the broken case.
 * see: https://svn.boost.org/trac/boost/ticket/7238
 */
#if defined(have_working_boost_sleep_for)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#elif defined(have_working_boost_sleep)
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
#else
//should never get here
#error missing boost sleep implementation
#endif
}

std::string datetimestrformat(const char* pszformat, int64_t ntime)
{
    // std::locale takes ownership of the pointer
    std::locale loc(std::locale::classic(), new boost::posix_time::time_facet(pszformat));
    std::stringstream ss;
    ss.imbue(loc);
    ss << boost::posix_time::from_time_t(ntime);
    return ss.str();
}
