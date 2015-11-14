// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/env.h"

#include "port/port.h"
#include "util/testharness.h"

namespace leveldb {

static const int kdelaymicros = 100000;

class envposixtest {
 private:
  port::mutex mu_;
  std::string events_;

 public:
  env* env_;
  envposixtest() : env_(env::default()) { }
};

static void setbool(void* ptr) {
  reinterpret_cast<port::atomicpointer*>(ptr)->nobarrier_store(ptr);
}

test(envposixtest, runimmediately) {
  port::atomicpointer called (null);
  env_->schedule(&setbool, &called);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(called.nobarrier_load() != null);
}

test(envposixtest, runmany) {
  port::atomicpointer last_id (null);

  struct cb {
    port::atomicpointer* last_id_ptr;   // pointer to shared slot
    uintptr_t id;             // order# for the execution of this callback

    cb(port::atomicpointer* p, int i) : last_id_ptr(p), id(i) { }

    static void run(void* v) {
      cb* cb = reinterpret_cast<cb*>(v);
      void* cur = cb->last_id_ptr->nobarrier_load();
      assert_eq(cb->id-1, reinterpret_cast<uintptr_t>(cur));
      cb->last_id_ptr->release_store(reinterpret_cast<void*>(cb->id));
    }
  };

  // schedule in different order than start time
  cb cb1(&last_id, 1);
  cb cb2(&last_id, 2);
  cb cb3(&last_id, 3);
  cb cb4(&last_id, 4);
  env_->schedule(&cb::run, &cb1);
  env_->schedule(&cb::run, &cb2);
  env_->schedule(&cb::run, &cb3);
  env_->schedule(&cb::run, &cb4);

  env::default()->sleepformicroseconds(kdelaymicros);
  void* cur = last_id.acquire_load();
  assert_eq(4, reinterpret_cast<uintptr_t>(cur));
}

struct state {
  port::mutex mu;
  int val;
  int num_running;
};

static void threadbody(void* arg) {
  state* s = reinterpret_cast<state*>(arg);
  s->mu.lock();
  s->val += 1;
  s->num_running -= 1;
  s->mu.unlock();
}

test(envposixtest, startthread) {
  state state;
  state.val = 0;
  state.num_running = 3;
  for (int i = 0; i < 3; i++) {
    env_->startthread(&threadbody, &state);
  }
  while (true) {
    state.mu.lock();
    int num = state.num_running;
    state.mu.unlock();
    if (num == 0) {
      break;
    }
    env::default()->sleepformicroseconds(kdelaymicros);
  }
  assert_eq(state.val, 3);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
