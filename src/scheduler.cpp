// copyright (c) 2015 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "scheduler.h"

#include <assert.h>
#include <boost/bind.hpp>
#include <utility>

cscheduler::cscheduler() : nthreadsservicingqueue(0), stoprequested(false), stopwhenempty(false)
{
}

cscheduler::~cscheduler()
{
    assert(nthreadsservicingqueue == 0);
}


#if boost_version < 105000
static boost::system_time toposixtime(const boost::chrono::system_clock::time_point& t)
{
    return boost::posix_time::from_time_t(boost::chrono::system_clock::to_time_t(t));
}
#endif

void cscheduler::servicequeue()
{
    boost::unique_lock<boost::mutex> lock(newtaskmutex);
    ++nthreadsservicingqueue;

    // newtaskmutex is locked throughout this loop except
    // when the thread is waiting or when the user's function
    // is called.
    while (!shouldstop()) {
        try {
            while (!shouldstop() && taskqueue.empty()) {
                // wait until there is something to do.
                newtaskscheduled.wait(lock);
            }

            // wait until either there is a new task, or until
            // the time of the first item on the queue:

// wait_until needs boost 1.50 or later; older versions have timed_wait:
#if boost_version < 105000
            while (!shouldstop() && !taskqueue.empty() &&
                   newtaskscheduled.timed_wait(lock, toposixtime(taskqueue.begin()->first))) {
                // keep waiting until timeout
            }
#else
            // some boost versions have a conflicting overload of wait_until that returns void.
            // explicitly use a template here to avoid hitting that overload.
            while (!shouldstop() && !taskqueue.empty() &&
                   newtaskscheduled.wait_until<>(lock, taskqueue.begin()->first) != boost::cv_status::timeout) {
                // keep waiting until timeout
            }
#endif
            // if there are multiple threads, the queue can empty while we're waiting (another
            // thread may service the task we were waiting on).
            if (shouldstop() || taskqueue.empty())
                continue;

            function f = taskqueue.begin()->second;
            taskqueue.erase(taskqueue.begin());

            // unlock before calling f, so it can reschedule itself or another task
            // without deadlocking:
            lock.unlock();
            f();
            lock.lock();
        } catch (...) {
            --nthreadsservicingqueue;
            throw;
        }
    }
    --nthreadsservicingqueue;
}

void cscheduler::stop(bool drain)
{
    {
        boost::unique_lock<boost::mutex> lock(newtaskmutex);
        if (drain)
            stopwhenempty = true;
        else
            stoprequested = true;
    }
    newtaskscheduled.notify_all();
}

void cscheduler::schedule(cscheduler::function f, boost::chrono::system_clock::time_point t)
{
    {
        boost::unique_lock<boost::mutex> lock(newtaskmutex);
        taskqueue.insert(std::make_pair(t, f));
    }
    newtaskscheduled.notify_one();
}

void cscheduler::schedulefromnow(cscheduler::function f, int64_t deltaseconds)
{
    schedule(f, boost::chrono::system_clock::now() + boost::chrono::seconds(deltaseconds));
}

static void repeat(cscheduler* s, cscheduler::function f, int64_t deltaseconds)
{
    f();
    s->schedulefromnow(boost::bind(&repeat, s, f, deltaseconds), deltaseconds);
}

void cscheduler::scheduleevery(cscheduler::function f, int64_t deltaseconds)
{
    schedulefromnow(boost::bind(&repeat, this, f, deltaseconds), deltaseconds);
}

size_t cscheduler::getqueueinfo(boost::chrono::system_clock::time_point &first,
                             boost::chrono::system_clock::time_point &last) const
{
    boost::unique_lock<boost::mutex> lock(newtaskmutex);
    size_t result = taskqueue.size();
    if (!taskqueue.empty()) {
        first = taskqueue.begin()->first;
        last = taskqueue.rbegin()->first;
    }
    return result;
}
