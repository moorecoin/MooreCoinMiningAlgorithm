// leveldb copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// see port_example.h for documentation for the following types/functions.

// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//  * redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * neither the name of the university of california, berkeley nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
// 
// this software is provided by the regents and contributors ``as is'' and any
// express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are
// disclaimed. in no event shall the regents and contributors be liable for any
// direct, indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused and
// on any theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use of this
// software, even if advised of the possibility of such damage.
//

#include "port/port_win.h"

#include <windows.h>
#include <cassert>

namespace leveldb {
namespace port {

mutex::mutex() :
    cs_(null) {
  assert(!cs_);
  cs_ = static_cast<void *>(new critical_section());
  ::initializecriticalsection(static_cast<critical_section *>(cs_));
  assert(cs_);
}

mutex::~mutex() {
  assert(cs_);
  ::deletecriticalsection(static_cast<critical_section *>(cs_));
  delete static_cast<critical_section *>(cs_);
  cs_ = null;
  assert(!cs_);
}

void mutex::lock() {
  assert(cs_);
  ::entercriticalsection(static_cast<critical_section *>(cs_));
}

void mutex::unlock() {
  assert(cs_);
  ::leavecriticalsection(static_cast<critical_section *>(cs_));
}

void mutex::assertheld() {
  assert(cs_);
  assert(1);
}

condvar::condvar(mutex* mu) :
    waiting_(0), 
    mu_(mu), 
    sem1_(::createsemaphore(null, 0, 10000, null)), 
    sem2_(::createsemaphore(null, 0, 10000, null)) {
  assert(mu_);
}

condvar::~condvar() {
  ::closehandle(sem1_);
  ::closehandle(sem2_);
}

void condvar::wait() {
  mu_->assertheld();

  wait_mtx_.lock();
  ++waiting_;
  wait_mtx_.unlock();

  mu_->unlock();

  // initiate handshake
  ::waitforsingleobject(sem1_, infinite);
  ::releasesemaphore(sem2_, 1, null);
  mu_->lock();
}

void condvar::signal() {
  wait_mtx_.lock();
  if (waiting_ > 0) {
    --waiting_;

    // finalize handshake
    ::releasesemaphore(sem1_, 1, null);
    ::waitforsingleobject(sem2_, infinite);
  }
  wait_mtx_.unlock();
}

void condvar::signalall() {
  wait_mtx_.lock();
  ::releasesemaphore(sem1_, waiting_, null);
  while(waiting_ > 0) {
    --waiting_;
    ::waitforsingleobject(sem2_, infinite);
  }
  wait_mtx_.unlock();
}

atomicpointer::atomicpointer(void* v) {
  release_store(v);
}

void initonce(oncetype* once, void (*initializer)()) {
  once->initonce(initializer);
}

void* atomicpointer::acquire_load() const {
  void * p = null;
  interlockedexchangepointer(&p, rep_);
  return p;
}

void atomicpointer::release_store(void* v) {
  interlockedexchangepointer(&rep_, v);
}

void* atomicpointer::nobarrier_load() const {
  return rep_;
}

void atomicpointer::nobarrier_store(void* v) {
  rep_ = v;
}

}
}
