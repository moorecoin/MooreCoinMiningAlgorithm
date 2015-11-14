// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_checkqueue_h
#define moorecoin_checkqueue_h

#include <algorithm>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>

template <typename t>
class ccheckqueuecontrol;

/** 
 * queue for verifications that have to be performed.
  * the verifications are represented by a type t, which must provide an
  * operator(), returning a bool.
  *
  * one thread (the master) is assumed to push batches of verifications
  * onto the queue, where they are processed by n-1 worker threads. when
  * the master is done adding work, it temporarily joins the worker pool
  * as an n'th worker, until all jobs are done.
  */
template <typename t>
class ccheckqueue
{
private:
    //! mutex to protect the inner state
    boost::mutex mutex;

    //! worker threads block on this when out of work
    boost::condition_variable condworker;

    //! master thread blocks on this when out of work
    boost::condition_variable condmaster;

    //! the queue of elements to be processed.
    //! as the order of booleans doesn't matter, it is used as a lifo (stack)
    std::vector<t> queue;

    //! the number of workers (including the master) that are idle.
    int nidle;

    //! the total number of workers (including the master).
    int ntotal;

    //! the temporary evaluation result.
    bool fallok;

    /**
     * number of verifications that haven't completed yet.
     * this includes elements that are no longer queued, but still in the
     * worker's own batches.
     */
    unsigned int ntodo;

    //! whether we're shutting down.
    bool fquit;

    //! the maximum number of elements to be processed in one batch
    unsigned int nbatchsize;

    /** internal function that does bulk of the verification work. */
    bool loop(bool fmaster = false)
    {
        boost::condition_variable& cond = fmaster ? condmaster : condworker;
        std::vector<t> vchecks;
        vchecks.reserve(nbatchsize);
        unsigned int nnow = 0;
        bool fok = true;
        do {
            {
                boost::unique_lock<boost::mutex> lock(mutex);
                // first do the clean-up of the previous loop run (allowing us to do it in the same critsect)
                if (nnow) {
                    fallok &= fok;
                    ntodo -= nnow;
                    if (ntodo == 0 && !fmaster)
                        // we processed the last element; inform the master it can exit and return the result
                        condmaster.notify_one();
                } else {
                    // first iteration
                    ntotal++;
                }
                // logically, the do loop starts here
                while (queue.empty()) {
                    if ((fmaster || fquit) && ntodo == 0) {
                        ntotal--;
                        bool fret = fallok;
                        // reset the status for new work later
                        if (fmaster)
                            fallok = true;
                        // return the current status
                        return fret;
                    }
                    nidle++;
                    cond.wait(lock); // wait
                    nidle--;
                }
                // decide how many work units to process now.
                // * do not try to do everything at once, but aim for increasingly smaller batches so
                //   all workers finish approximately simultaneously.
                // * try to account for idle jobs which will instantly start helping.
                // * don't do batches smaller than 1 (duh), or larger than nbatchsize.
                nnow = std::max(1u, std::min(nbatchsize, (unsigned int)queue.size() / (ntotal + nidle + 1)));
                vchecks.resize(nnow);
                for (unsigned int i = 0; i < nnow; i++) {
                    // we want the lock on the mutex to be as short as possible, so swap jobs from the global
                    // queue to the local batch vector instead of copying.
                    vchecks[i].swap(queue.back());
                    queue.pop_back();
                }
                // check whether we need to do work at all
                fok = fallok;
            }
            // execute work
            boost_foreach (t& check, vchecks)
                if (fok)
                    fok = check();
            vchecks.clear();
        } while (true);
    }

public:
    //! create a new check queue
    ccheckqueue(unsigned int nbatchsizein) : nidle(0), ntotal(0), fallok(true), ntodo(0), fquit(false), nbatchsize(nbatchsizein) {}

    //! worker thread
    void thread()
    {
        loop();
    }

    //! wait until execution finishes, and return whether all evaluations were successful.
    bool wait()
    {
        return loop(true);
    }

    //! add a batch of checks to the queue
    void add(std::vector<t>& vchecks)
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        boost_foreach (t& check, vchecks) {
            queue.push_back(t());
            check.swap(queue.back());
        }
        ntodo += vchecks.size();
        if (vchecks.size() == 1)
            condworker.notify_one();
        else if (vchecks.size() > 1)
            condworker.notify_all();
    }

    ~ccheckqueue()
    {
    }

    bool isidle()
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        return (ntotal == nidle && ntodo == 0 && fallok == true);
    }

};

/** 
 * raii-style controller object for a ccheckqueue that guarantees the passed
 * queue is finished before continuing.
 */
template <typename t>
class ccheckqueuecontrol
{
private:
    ccheckqueue<t>* pqueue;
    bool fdone;

public:
    ccheckqueuecontrol(ccheckqueue<t>* pqueuein) : pqueue(pqueuein), fdone(false)
    {
        // passed queue is supposed to be unused, or null
        if (pqueue != null) {
            bool isidle = pqueue->isidle();
            assert(isidle);
        }
    }

    bool wait()
    {
        if (pqueue == null)
            return true;
        bool fret = pqueue->wait();
        fdone = true;
        return fret;
    }

    void add(std::vector<t>& vchecks)
    {
        if (pqueue != null)
            pqueue->add(vchecks);
    }

    ~ccheckqueuecontrol()
    {
        if (!fdone)
            wait();
    }
};

#endif // moorecoin_checkqueue_h
