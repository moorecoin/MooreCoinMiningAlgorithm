// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// an env is an interface used by the leveldb implementation to access
// operating system functionality like the filesystem etc.  callers
// may wish to provide a custom env object when opening a database to
// get fine gain control; e.g., to rate limit file system operations.
//
// all env implementations are safe for concurrent access from
// multiple threads without any external synchronization.

#ifndef storage_leveldb_include_env_h_
#define storage_leveldb_include_env_h_

#include <string>
#include <vector>
#include <stdarg.h>
#include <stdint.h>
#include "leveldb/status.h"

namespace leveldb {

class filelock;
class logger;
class randomaccessfile;
class sequentialfile;
class slice;
class writablefile;

class env {
 public:
  env() { }
  virtual ~env();

  // return a default environment suitable for the current operating
  // system.  sophisticated users may wish to provide their own env
  // implementation instead of relying on this default environment.
  //
  // the result of default() belongs to leveldb and must never be deleted.
  static env* default();

  // create a brand new sequentially-readable file with the specified name.
  // on success, stores a pointer to the new file in *result and returns ok.
  // on failure stores null in *result and returns non-ok.  if the file does
  // not exist, returns a non-ok status.
  //
  // the returned file will only be accessed by one thread at a time.
  virtual status newsequentialfile(const std::string& fname,
                                   sequentialfile** result) = 0;

  // create a brand new random access read-only file with the
  // specified name.  on success, stores a pointer to the new file in
  // *result and returns ok.  on failure stores null in *result and
  // returns non-ok.  if the file does not exist, returns a non-ok
  // status.
  //
  // the returned file may be concurrently accessed by multiple threads.
  virtual status newrandomaccessfile(const std::string& fname,
                                     randomaccessfile** result) = 0;

  // create an object that writes to a new file with the specified
  // name.  deletes any existing file with the same name and creates a
  // new file.  on success, stores a pointer to the new file in
  // *result and returns ok.  on failure stores null in *result and
  // returns non-ok.
  //
  // the returned file will only be accessed by one thread at a time.
  virtual status newwritablefile(const std::string& fname,
                                 writablefile** result) = 0;

  // returns true iff the named file exists.
  virtual bool fileexists(const std::string& fname) = 0;

  // store in *result the names of the children of the specified directory.
  // the names are relative to "dir".
  // original contents of *results are dropped.
  virtual status getchildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;

  // delete the named file.
  virtual status deletefile(const std::string& fname) = 0;

  // create the specified directory.
  virtual status createdir(const std::string& dirname) = 0;

  // delete the specified directory.
  virtual status deletedir(const std::string& dirname) = 0;

  // store the size of fname in *file_size.
  virtual status getfilesize(const std::string& fname, uint64_t* file_size) = 0;

  // rename file src to target.
  virtual status renamefile(const std::string& src,
                            const std::string& target) = 0;

  // lock the specified file.  used to prevent concurrent access to
  // the same db by multiple processes.  on failure, stores null in
  // *lock and returns non-ok.
  //
  // on success, stores a pointer to the object that represents the
  // acquired lock in *lock and returns ok.  the caller should call
  // unlockfile(*lock) to release the lock.  if the process exits,
  // the lock will be automatically released.
  //
  // if somebody else already holds the lock, finishes immediately
  // with a failure.  i.e., this call does not wait for existing locks
  // to go away.
  //
  // may create the named file if it does not already exist.
  virtual status lockfile(const std::string& fname, filelock** lock) = 0;

  // release the lock acquired by a previous successful call to lockfile.
  // requires: lock was returned by a successful lockfile() call
  // requires: lock has not already been unlocked.
  virtual status unlockfile(filelock* lock) = 0;

  // arrange to run "(*function)(arg)" once in a background thread.
  //
  // "function" may run in an unspecified thread.  multiple functions
  // added to the same env may run concurrently in different threads.
  // i.e., the caller may not assume that background work items are
  // serialized.
  virtual void schedule(
      void (*function)(void* arg),
      void* arg) = 0;

  // start a new thread, invoking "function(arg)" within the new thread.
  // when "function(arg)" returns, the thread will be destroyed.
  virtual void startthread(void (*function)(void* arg), void* arg) = 0;

  // *path is set to a temporary directory that can be used for testing. it may
  // or many not have just been created. the directory may or may not differ
  // between runs of the same process, but subsequent calls will return the
  // same directory.
  virtual status gettestdirectory(std::string* path) = 0;

  // create and return a log file for storing informational messages.
  virtual status newlogger(const std::string& fname, logger** result) = 0;

  // returns the number of micro-seconds since some fixed point in time. only
  // useful for computing deltas of time.
  virtual uint64_t nowmicros() = 0;

  // sleep/delay the thread for the prescribed number of micro-seconds.
  virtual void sleepformicroseconds(int micros) = 0;

 private:
  // no copying allowed
  env(const env&);
  void operator=(const env&);
};

// a file abstraction for reading sequentially through a file
class sequentialfile {
 public:
  sequentialfile() { }
  virtual ~sequentialfile();

  // read up to "n" bytes from the file.  "scratch[0..n-1]" may be
  // written by this routine.  sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully read).
  // may set "*result" to point at data in "scratch[0..n-1]", so
  // "scratch[0..n-1]" must be live when "*result" is used.
  // if an error was encountered, returns a non-ok status.
  //
  // requires: external synchronization
  virtual status read(size_t n, slice* result, char* scratch) = 0;

  // skip "n" bytes from the file. this is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // if end of file is reached, skipping will stop at the end of the
  // file, and skip will return ok.
  //
  // requires: external synchronization
  virtual status skip(uint64_t n) = 0;

 private:
  // no copying allowed
  sequentialfile(const sequentialfile&);
  void operator=(const sequentialfile&);
};

// a file abstraction for randomly reading the contents of a file.
class randomaccessfile {
 public:
  randomaccessfile() { }
  virtual ~randomaccessfile();

  // read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine.  sets "*result"
  // to the data that was read (including if fewer than "n" bytes were
  // successfully read).  may set "*result" to point at data in
  // "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
  // "*result" is used.  if an error was encountered, returns a non-ok
  // status.
  //
  // safe for concurrent use by multiple threads.
  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const = 0;

 private:
  // no copying allowed
  randomaccessfile(const randomaccessfile&);
  void operator=(const randomaccessfile&);
};

// a file abstraction for sequential writing.  the implementation
// must provide buffering since callers may append small fragments
// at a time to the file.
class writablefile {
 public:
  writablefile() { }
  virtual ~writablefile();

  virtual status append(const slice& data) = 0;
  virtual status close() = 0;
  virtual status flush() = 0;
  virtual status sync() = 0;

 private:
  // no copying allowed
  writablefile(const writablefile&);
  void operator=(const writablefile&);
};

// an interface for writing log messages.
class logger {
 public:
  logger() { }
  virtual ~logger();

  // write an entry to the log file with the specified format.
  virtual void logv(const char* format, va_list ap) = 0;

 private:
  // no copying allowed
  logger(const logger&);
  void operator=(const logger&);
};


// identifies a locked file.
class filelock {
 public:
  filelock() { }
  virtual ~filelock();
 private:
  // no copying allowed
  filelock(const filelock&);
  void operator=(const filelock&);
};

// log the specified data to *info_log if info_log is non-null.
extern void log(logger* info_log, const char* format, ...)
#   if defined(__gnuc__) || defined(__clang__)
    __attribute__((__format__ (__printf__, 2, 3)))
#   endif
    ;

// a utility routine: write "data" to the named file.
extern status writestringtofile(env* env, const slice& data,
                                const std::string& fname);

// a utility routine: read contents of named file into *data
extern status readfiletostring(env* env, const std::string& fname,
                               std::string* data);

// an implementation of env that forwards all calls to another env.
// may be useful to clients who wish to override just part of the
// functionality of another env.
class envwrapper : public env {
 public:
  // initialize an envwrapper that delegates all calls to *t
  explicit envwrapper(env* t) : target_(t) { }
  virtual ~envwrapper();

  // return the target to which this env forwards all calls
  env* target() const { return target_; }

  // the following text is boilerplate that forwards all methods to target()
  status newsequentialfile(const std::string& f, sequentialfile** r) {
    return target_->newsequentialfile(f, r);
  }
  status newrandomaccessfile(const std::string& f, randomaccessfile** r) {
    return target_->newrandomaccessfile(f, r);
  }
  status newwritablefile(const std::string& f, writablefile** r) {
    return target_->newwritablefile(f, r);
  }
  bool fileexists(const std::string& f) { return target_->fileexists(f); }
  status getchildren(const std::string& dir, std::vector<std::string>* r) {
    return target_->getchildren(dir, r);
  }
  status deletefile(const std::string& f) { return target_->deletefile(f); }
  status createdir(const std::string& d) { return target_->createdir(d); }
  status deletedir(const std::string& d) { return target_->deletedir(d); }
  status getfilesize(const std::string& f, uint64_t* s) {
    return target_->getfilesize(f, s);
  }
  status renamefile(const std::string& s, const std::string& t) {
    return target_->renamefile(s, t);
  }
  status lockfile(const std::string& f, filelock** l) {
    return target_->lockfile(f, l);
  }
  status unlockfile(filelock* l) { return target_->unlockfile(l); }
  void schedule(void (*f)(void*), void* a) {
    return target_->schedule(f, a);
  }
  void startthread(void (*f)(void*), void* a) {
    return target_->startthread(f, a);
  }
  virtual status gettestdirectory(std::string* path) {
    return target_->gettestdirectory(path);
  }
  virtual status newlogger(const std::string& fname, logger** result) {
    return target_->newlogger(fname, result);
  }
  uint64_t nowmicros() {
    return target_->nowmicros();
  }
  void sleepformicroseconds(int micros) {
    target_->sleepformicroseconds(micros);
  }
 private:
  env* target_;
};

}  // namespace leveldb

#endif  // storage_leveldb_include_env_h_
