// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_util_mutexlock_h_
#define storage_leveldb_util_mutexlock_h_

#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

// helper class that locks a mutex on construction and unlocks the mutex when
// the destructor of the mutexlock object is invoked.
//
// typical usage:
//
//   void myclass::mymethod() {
//     mutexlock l(&mu_);       // mu_ is an instance variable
//     ... some complex code, possibly with multiple return paths ...
//   }

class scoped_lockable mutexlock {
 public:
  explicit mutexlock(port::mutex *mu) exclusive_lock_function(mu)
      : mu_(mu)  {
    this->mu_->lock();
  }
  ~mutexlock() unlock_function() { this->mu_->unlock(); }

 private:
  port::mutex *const mu_;
  // no copying allowed
  mutexlock(const mutexlock&);
  void operator=(const mutexlock&);
};

}  // namespace leveldb


#endif  // storage_leveldb_util_mutexlock_h_
