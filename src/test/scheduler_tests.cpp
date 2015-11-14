// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "random.h"
#include "scheduler.h"

#include "test/test_moorecoin.h"

#include <boost/bind.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/thread.hpp>
#include <boost/test/unit_test.hpp>

boost_auto_test_suite(scheduler_tests)

static void microtask(cscheduler& s, boost::mutex& mutex, int& counter, int delta, boost::chrono::system_clock::time_point rescheduletime)
{
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        counter += delta;
    }
    boost::chrono::system_clock::time_point notime = boost::chrono::system_clock::time_point::min();
    if (rescheduletime != notime) {
        cscheduler::function f = boost::bind(&microtask, boost::ref(s), boost::ref(mutex), boost::ref(counter), -delta + 1, notime);
        s.schedule(f, rescheduletime);
    }
}

static void microsleep(uint64_t n)
{
#if defined(have_working_boost_sleep_for)
    boost::this_thread::sleep_for(boost::chrono::microseconds(n));
#elif defined(have_working_boost_sleep)
    boost::this_thread::sleep(boost::posix_time::microseconds(n));
#else
    //should never get here
    #error missing boost sleep implementation
#endif
}

boost_auto_test_case(manythreads)
{
    seed_insecure_rand(false);

    // stress test: hundreds of microsecond-scheduled tasks,
    // serviced by 10 threads.
    //
    // so... ten shared counters, which if all the tasks execute
    // properly will sum to the number of tasks done.
    // each task adds or subtracts from one of the counters a
    // random amount, and then schedules another task 0-1000
    // microseconds in the future to subtract or add from
    // the counter -random_amount+1, so in the end the shared
    // counters should sum to the number of initial tasks performed.
    cscheduler microtasks;

    boost::mutex countermutex[10];
    int counter[10] = { 0 };
    boost::random::mt19937 rng(insecure_rand());
    boost::random::uniform_int_distribution<> zerotonine(0, 9);
    boost::random::uniform_int_distribution<> randommsec(-11, 1000);
    boost::random::uniform_int_distribution<> randomdelta(-1000, 1000);

    boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
    boost::chrono::system_clock::time_point now = start;
    boost::chrono::system_clock::time_point first, last;
    size_t ntasks = microtasks.getqueueinfo(first, last);
    boost_check(ntasks == 0);

    for (int i = 0; i < 100; i++) {
        boost::chrono::system_clock::time_point t = now + boost::chrono::microseconds(randommsec(rng));
        boost::chrono::system_clock::time_point treschedule = now + boost::chrono::microseconds(500 + randommsec(rng));
        int whichcounter = zerotonine(rng);
        cscheduler::function f = boost::bind(&microtask, boost::ref(microtasks),
                                             boost::ref(countermutex[whichcounter]), boost::ref(counter[whichcounter]),
                                             randomdelta(rng), treschedule);
        microtasks.schedule(f, t);
    }
    ntasks = microtasks.getqueueinfo(first, last);
    boost_check(ntasks == 100);
    boost_check(first < last);
    boost_check(last > now);

    // as soon as these are created they will start running and servicing the queue
    boost::thread_group microthreads;
    for (int i = 0; i < 5; i++)
        microthreads.create_thread(boost::bind(&cscheduler::servicequeue, &microtasks));

    microsleep(600);
    now = boost::chrono::system_clock::now();

    // more threads and more tasks:
    for (int i = 0; i < 5; i++)
        microthreads.create_thread(boost::bind(&cscheduler::servicequeue, &microtasks));
    for (int i = 0; i < 100; i++) {
        boost::chrono::system_clock::time_point t = now + boost::chrono::microseconds(randommsec(rng));
        boost::chrono::system_clock::time_point treschedule = now + boost::chrono::microseconds(500 + randommsec(rng));
        int whichcounter = zerotonine(rng);
        cscheduler::function f = boost::bind(&microtask, boost::ref(microtasks),
                                             boost::ref(countermutex[whichcounter]), boost::ref(counter[whichcounter]),
                                             randomdelta(rng), treschedule);
        microtasks.schedule(f, t);
    }

    // drain the task queue then exit threads
    microtasks.stop(true);
    microthreads.join_all(); // ... wait until all the threads are done

    int countersum = 0;
    for (int i = 0; i < 10; i++) {
        boost_check(counter[i] != 0);
        countersum += counter[i];
    }
    boost_check_equal(countersum, 200);
}

boost_auto_test_suite_end()
