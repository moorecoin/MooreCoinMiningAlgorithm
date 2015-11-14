// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "timedata.h"

#include "netbase.h"
#include "sync.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/foreach.hpp>

using namespace std;

static ccriticalsection cs_ntimeoffset;
static int64_t ntimeoffset = 0;

/**
 * "never go to sea with two chronometers; take one or three."
 * our three time sources are:
 *  - system clock
 *  - median of other nodes clocks
 *  - the user (asking the user to fix the system clock if the first two disagree)
 */
int64_t gettimeoffset()
{
    lock(cs_ntimeoffset);
    return ntimeoffset;
}

int64_t getadjustedtime()
{
    return gettime() + gettimeoffset();
}

static int64_t abs64(int64_t n)
{
    return (n >= 0 ? n : -n);
}

void addtimedata(const cnetaddr& ip, int64_t noffsetsample)
{
    lock(cs_ntimeoffset);
    // ignore duplicates
    static set<cnetaddr> setknown;
    if (!setknown.insert(ip).second)
        return;

    // add data
    static cmedianfilter<int64_t> vtimeoffsets(200,0);
    vtimeoffsets.input(noffsetsample);
    logprintf("added time data, samples %d, offset %+d (%+d minutes)\n", vtimeoffsets.size(), noffsetsample, noffsetsample/60);

    // there is a known issue here (see issue #4521):
    //
    // - the structure vtimeoffsets contains up to 200 elements, after which
    // any new element added to it will not increase its size, replacing the
    // oldest element.
    //
    // - the condition to update ntimeoffset includes checking whether the
    // number of elements in vtimeoffsets is odd, which will never happen after
    // there are 200 elements.
    //
    // but in this case the 'bug' is protective against some attacks, and may
    // actually explain why we've never seen attacks which manipulate the
    // clock offset.
    //
    // so we should hold off on fixing this and clean it up as part of
    // a timing cleanup that strengthens it in a number of other ways.
    //
    if (vtimeoffsets.size() >= 5 && vtimeoffsets.size() % 2 == 1)
    {
        int64_t nmedian = vtimeoffsets.median();
        std::vector<int64_t> vsorted = vtimeoffsets.sorted();
        // only let other nodes change our time by so much
        if (abs64(nmedian) < 70 * 60)
        {
            ntimeoffset = nmedian;
        }
        else
        {
            ntimeoffset = 0;

            static bool fdone;
            if (!fdone)
            {
                // if nobody has a time different than ours but within 5 minutes of ours, give a warning
                bool fmatch = false;
                boost_foreach(int64_t noffset, vsorted)
                    if (noffset != 0 && abs64(noffset) < 5 * 60)
                        fmatch = true;

                if (!fmatch)
                {
                    fdone = true;
                    string strmessage = _("warning: please check that your computer's date and time are correct! if your clock is wrong moorecoin core will not work properly.");
                    strmiscwarning = strmessage;
                    logprintf("*** %s\n", strmessage);
                    uiinterface.threadsafemessagebox(strmessage, "", cclientuiinterface::msg_warning);
                }
            }
        }
        if (fdebug) {
            boost_foreach(int64_t n, vsorted)
                logprintf("%+d  ", n);
            logprintf("|  ");
        }
        logprintf("ntimeoffset = %+d  (%+d minutes)\n", ntimeoffset, ntimeoffset/60);
    }
}
