// copyright (c) 2015 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_scheduler_h
#define moorecoin_scheduler_h

//
// note:
// boost::thread / boost::function / boost::chrono should be ported to
// std::thread / std::function / std::chrono when we support c++11.
//
#include <boost/function.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>
#include <map>

//
// simple class for background tasks that should be run
// periodically or once "after a while"
//
// usage:
//
// cscheduler* s = new cscheduler();
// s->schedulefromnow(dosomething, 11); // assuming a: void dosomething() { }
// s->schedulefromnow(boost::bind(class::func, this, argument), 3);
// boost::thread* t = new boost::thread(boost::bind(cscheduler::servicequeue, s));
//
// ... then at program shutdown, clean up the thread running servicequeue:
// t->interrupt();
// t->join();
// delete t;
// delete s; // must be done after thread is interrupted/joined.
//

class cscheduler
{
public:
    cscheduler();
    ~cscheduler();

    typedef boost::function<void(void)> function;

    // call func at/after time t
    void schedule(function f, boost::chrono::system_clock::time_point t);

    // convenience method: call f once deltaseconds from now
    void schedulefromnow(function f, int64_t deltaseconds);

    // another convenience method: call f approximately
    // every deltaseconds forever, starting deltaseconds from now.
    // to be more precise: every time f is finished, it
    // is rescheduled to run deltaseconds later. if you
    // need more accurate scheduling, don't use this method.
    void scheduleevery(function f, int64_t deltaseconds);

    // to keep things as simple as possible, there is no unschedule.

    // services the queue 'forever'. should be run in a thread,
    // and interrupted using boost::interrupt_thread
    void servicequeue();

    // tell any threads running servicequeue to stop as soon as they're
    // done servicing whatever task they're currently servicing (drain=false)
    // or when there is no work left to be done (drain=true)
    void stop(bool drain=false);

    // returns number of tasks waiting to be serviced,
    // and first and last task times
    size_t getqueueinfo(boost::chrono::system_clock::time_point &first,
                        boost::chrono::system_clock::time_point &last) const;

private:
    std::multimap<boost::chrono::system_clock::time_point, function> taskqueue;
    boost::condition_variable newtaskscheduled;
    mutable boost::mutex newtaskmutex;
    int nthreadsservicingqueue;
    bool stoprequested;
    bool stopwhenempty;
    bool shouldstop() { return stoprequested || (stopwhenempty && taskqueue.empty()); }
};

#endif
