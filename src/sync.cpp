// copyright (c) 2011-2012 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "sync.h"

#include "util.h"
#include "utilstrencodings.h"

#include <stdio.h>

#include <boost/foreach.hpp>
#include <boost/thread.hpp>

#ifdef debug_lockcontention
void printlockcontention(const char* pszname, const char* pszfile, int nline)
{
    logprintf("lockcontention: %s\n", pszname);
    logprintf("locker: %s:%d\n", pszfile, nline);
}
#endif /* debug_lockcontention */

#ifdef debug_lockorder
//
// early deadlock detection.
// problem being solved:
//    thread 1 locks  a, then b, then c
//    thread 2 locks  d, then c, then a
//     --> may result in deadlock between the two threads, depending on when they run.
// solution implemented here:
// keep track of pairs of locks: (a before b), (a before c), etc.
// complain if any thread tries to lock in a different order.
//

struct clocklocation {
    clocklocation(const char* pszname, const char* pszfile, int nline)
    {
        mutexname = pszname;
        sourcefile = pszfile;
        sourceline = nline;
    }

    std::string tostring() const
    {
        return mutexname + "  " + sourcefile + ":" + itostr(sourceline);
    }

    std::string mutexname() const { return mutexname; }

private:
    std::string mutexname;
    std::string sourcefile;
    int sourceline;
};

typedef std::vector<std::pair<void*, clocklocation> > lockstack;

static boost::mutex dd_mutex;
static std::map<std::pair<void*, void*>, lockstack> lockorders;
static boost::thread_specific_ptr<lockstack> lockstack;


static void potential_deadlock_detected(const std::pair<void*, void*>& mismatch, const lockstack& s1, const lockstack& s2)
{
    logprintf("potential deadlock detected\n");
    logprintf("previous lock order was:\n");
    boost_foreach (const pairtype(void*, clocklocation) & i, s2) {
        if (i.first == mismatch.first)
            logprintf(" (1)");
        if (i.first == mismatch.second)
            logprintf(" (2)");
        logprintf(" %s\n", i.second.tostring());
    }
    logprintf("current lock order is:\n");
    boost_foreach (const pairtype(void*, clocklocation) & i, s1) {
        if (i.first == mismatch.first)
            logprintf(" (1)");
        if (i.first == mismatch.second)
            logprintf(" (2)");
        logprintf(" %s\n", i.second.tostring());
    }
}

static void push_lock(void* c, const clocklocation& locklocation, bool ftry)
{
    if (lockstack.get() == null)
        lockstack.reset(new lockstack);

    dd_mutex.lock();

    (*lockstack).push_back(std::make_pair(c, locklocation));

    if (!ftry) {
        boost_foreach (const pairtype(void*, clocklocation) & i, (*lockstack)) {
            if (i.first == c)
                break;

            std::pair<void*, void*> p1 = std::make_pair(i.first, c);
            if (lockorders.count(p1))
                continue;
            lockorders[p1] = (*lockstack);

            std::pair<void*, void*> p2 = std::make_pair(c, i.first);
            if (lockorders.count(p2)) {
                potential_deadlock_detected(p1, lockorders[p2], lockorders[p1]);
                break;
            }
        }
    }
    dd_mutex.unlock();
}

static void pop_lock()
{
    dd_mutex.lock();
    (*lockstack).pop_back();
    dd_mutex.unlock();
}

void entercritical(const char* pszname, const char* pszfile, int nline, void* cs, bool ftry)
{
    push_lock(cs, clocklocation(pszname, pszfile, nline), ftry);
}

void leavecritical()
{
    pop_lock();
}

std::string locksheld()
{
    std::string result;
    boost_foreach (const pairtype(void*, clocklocation) & i, *lockstack)
        result += i.second.tostring() + std::string("\n");
    return result;
}

void assertlockheldinternal(const char* pszname, const char* pszfile, int nline, void* cs)
{
    boost_foreach (const pairtype(void*, clocklocation) & i, *lockstack)
        if (i.first == cs)
            return;
    fprintf(stderr, "assertion failed: lock %s not held in %s:%i; locks held:\n%s", pszname, pszfile, nline, locksheld().c_str());
    abort();
}

#endif /* debug_lockorder */
