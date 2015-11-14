// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_sync_h
#define moorecoin_sync_h

#include "threadsafety.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>


////////////////////////////////////////////////
//                                            //
// the simple definiton, excluding debug code //
//                                            //
////////////////////////////////////////////////

/*
ccriticalsection mutex;
    boost::recursive_mutex mutex;

lock(mutex);
    boost::unique_lock<boost::recursive_mutex> criticalblock(mutex);

lock2(mutex1, mutex2);
    boost::unique_lock<boost::recursive_mutex> criticalblock1(mutex1);
    boost::unique_lock<boost::recursive_mutex> criticalblock2(mutex2);

try_lock(mutex, name);
    boost::unique_lock<boost::recursive_mutex> name(mutex, boost::try_to_lock_t);

enter_critical_section(mutex); // no raii
    mutex.lock();

leave_critical_section(mutex); // no raii
    mutex.unlock();
 */

///////////////////////////////
//                           //
// the actual implementation //
//                           //
///////////////////////////////

/**
 * template mixin that adds -wthread-safety locking
 * annotations to a subset of the mutex api.
 */
template <typename parent>
class lockable annotatedmixin : public parent
{
public:
    void lock() exclusive_lock_function()
    {
        parent::lock();
    }

    void unlock() unlock_function()
    {
        parent::unlock();
    }

    bool try_lock() exclusive_trylock_function(true)
    {
        return parent::try_lock();
    }
};

/**
 * wrapped boost mutex: supports recursive locking, but no waiting
 * todo: we should move away from using the recursive lock by default.
 */
typedef annotatedmixin<boost::recursive_mutex> ccriticalsection;

/** wrapped boost mutex: supports waiting but not recursive locking */
typedef annotatedmixin<boost::mutex> cwaitablecriticalsection;

/** just a typedef for boost::condition_variable, can be wrapped later if desired */
typedef boost::condition_variable cconditionvariable;

#ifdef debug_lockorder
void entercritical(const char* pszname, const char* pszfile, int nline, void* cs, bool ftry = false);
void leavecritical();
std::string locksheld();
void assertlockheldinternal(const char* pszname, const char* pszfile, int nline, void* cs);
#else
void static inline entercritical(const char* pszname, const char* pszfile, int nline, void* cs, bool ftry = false) {}
void static inline leavecritical() {}
void static inline assertlockheldinternal(const char* pszname, const char* pszfile, int nline, void* cs) {}
#endif
#define assertlockheld(cs) assertlockheldinternal(#cs, __file__, __line__, &cs)

#ifdef debug_lockcontention
void printlockcontention(const char* pszname, const char* pszfile, int nline);
#endif

/** wrapper around boost::unique_lock<mutex> */
template <typename mutex>
class cmutexlock
{
private:
    boost::unique_lock<mutex> lock;

    void enter(const char* pszname, const char* pszfile, int nline)
    {
        entercritical(pszname, pszfile, nline, (void*)(lock.mutex()));
#ifdef debug_lockcontention
        if (!lock.try_lock()) {
            printlockcontention(pszname, pszfile, nline);
#endif
            lock.lock();
#ifdef debug_lockcontention
        }
#endif
    }

    bool tryenter(const char* pszname, const char* pszfile, int nline)
    {
        entercritical(pszname, pszfile, nline, (void*)(lock.mutex()), true);
        lock.try_lock();
        if (!lock.owns_lock())
            leavecritical();
        return lock.owns_lock();
    }

public:
    cmutexlock(mutex& mutexin, const char* pszname, const char* pszfile, int nline, bool ftry = false) : lock(mutexin, boost::defer_lock)
    {
        if (ftry)
            tryenter(pszname, pszfile, nline);
        else
            enter(pszname, pszfile, nline);
    }

    cmutexlock(mutex* pmutexin, const char* pszname, const char* pszfile, int nline, bool ftry = false)
    {
        if (!pmutexin) return;

        lock = boost::unique_lock<mutex>(*pmutexin, boost::defer_lock);
        if (ftry)
            tryenter(pszname, pszfile, nline);
        else
            enter(pszname, pszfile, nline);
    }

    ~cmutexlock()
    {
        if (lock.owns_lock())
            leavecritical();
    }

    operator bool()
    {
        return lock.owns_lock();
    }
};

typedef cmutexlock<ccriticalsection> ccriticalblock;

#define lock(cs) ccriticalblock criticalblock(cs, #cs, __file__, __line__)
#define lock2(cs1, cs2) ccriticalblock criticalblock1(cs1, #cs1, __file__, __line__), criticalblock2(cs2, #cs2, __file__, __line__)
#define try_lock(cs, name) ccriticalblock name(cs, #cs, __file__, __line__, true)

#define enter_critical_section(cs)                            \
    {                                                         \
        entercritical(#cs, __file__, __line__, (void*)(&cs)); \
        (cs).lock();                                          \
    }

#define leave_critical_section(cs) \
    {                              \
        (cs).unlock();             \
        leavecritical();           \
    }

class csemaphore
{
private:
    boost::condition_variable condition;
    boost::mutex mutex;
    int value;

public:
    csemaphore(int init) : value(init) {}

    void wait()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        while (value < 1) {
            condition.wait(lock);
        }
        value--;
    }

    bool try_wait()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        if (value < 1)
            return false;
        value--;
        return true;
    }

    void post()
    {
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            value++;
        }
        condition.notify_one();
    }
};

/** raii-style semaphore lock */
class csemaphoregrant
{
private:
    csemaphore* sem;
    bool fhavegrant;

public:
    void acquire()
    {
        if (fhavegrant)
            return;
        sem->wait();
        fhavegrant = true;
    }

    void release()
    {
        if (!fhavegrant)
            return;
        sem->post();
        fhavegrant = false;
    }

    bool tryacquire()
    {
        if (!fhavegrant && sem->try_wait())
            fhavegrant = true;
        return fhavegrant;
    }

    void moveto(csemaphoregrant& grant)
    {
        grant.release();
        grant.sem = sem;
        grant.fhavegrant = fhavegrant;
        sem = null;
        fhavegrant = false;
    }

    csemaphoregrant() : sem(null), fhavegrant(false) {}

    csemaphoregrant(csemaphore& sema, bool ftry = false) : sem(&sema), fhavegrant(false)
    {
        if (ftry)
            tryacquire();
        else
            acquire();
    }

    ~csemaphoregrant()
    {
        release();
    }

    operator bool()
    {
        return fhavegrant;
    }
};

#endif // moorecoin_sync_h
