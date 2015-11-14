// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "port/port_posix.h"

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include "util/logging.h"

namespace leveldb {
namespace port {

static void pthreadcall(const char* label, int result) {
  if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    abort();
  }
}

mutex::mutex() { pthreadcall("init mutex", pthread_mutex_init(&mu_, null)); }

mutex::~mutex() { pthreadcall("destroy mutex", pthread_mutex_destroy(&mu_)); }

void mutex::lock() { pthreadcall("lock", pthread_mutex_lock(&mu_)); }

void mutex::unlock() { pthreadcall("unlock", pthread_mutex_unlock(&mu_)); }

condvar::condvar(mutex* mu)
    : mu_(mu) {
    pthreadcall("init cv", pthread_cond_init(&cv_, null));
}

condvar::~condvar() { pthreadcall("destroy cv", pthread_cond_destroy(&cv_)); }

void condvar::wait() {
  pthreadcall("wait", pthread_cond_wait(&cv_, &mu_->mu_));
}

void condvar::signal() {
  pthreadcall("signal", pthread_cond_signal(&cv_));
}

void condvar::signalall() {
  pthreadcall("broadcast", pthread_cond_broadcast(&cv_));
}

void initonce(oncetype* once, void (*initializer)()) {
  pthreadcall("once", pthread_once(once, initializer));
}

}  // namespace port
}  // namespace leveldb
