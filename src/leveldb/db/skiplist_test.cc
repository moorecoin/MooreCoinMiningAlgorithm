// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/skiplist.h"
#include <set>
#include "leveldb/env.h"
#include "util/arena.h"
#include "util/hash.h"
#include "util/random.h"
#include "util/testharness.h"

namespace leveldb {

typedef uint64_t key;

struct comparator {
  int operator()(const key& a, const key& b) const {
    if (a < b) {
      return -1;
    } else if (a > b) {
      return +1;
    } else {
      return 0;
    }
  }
};

class skiptest { };

test(skiptest, empty) {
  arena arena;
  comparator cmp;
  skiplist<key, comparator> list(cmp, &arena);
  assert_true(!list.contains(10));

  skiplist<key, comparator>::iterator iter(&list);
  assert_true(!iter.valid());
  iter.seektofirst();
  assert_true(!iter.valid());
  iter.seek(100);
  assert_true(!iter.valid());
  iter.seektolast();
  assert_true(!iter.valid());
}

test(skiptest, insertandlookup) {
  const int n = 2000;
  const int r = 5000;
  random rnd(1000);
  std::set<key> keys;
  arena arena;
  comparator cmp;
  skiplist<key, comparator> list(cmp, &arena);
  for (int i = 0; i < n; i++) {
    key key = rnd.next() % r;
    if (keys.insert(key).second) {
      list.insert(key);
    }
  }

  for (int i = 0; i < r; i++) {
    if (list.contains(i)) {
      assert_eq(keys.count(i), 1);
    } else {
      assert_eq(keys.count(i), 0);
    }
  }

  // simple iterator tests
  {
    skiplist<key, comparator>::iterator iter(&list);
    assert_true(!iter.valid());

    iter.seek(0);
    assert_true(iter.valid());
    assert_eq(*(keys.begin()), iter.key());

    iter.seektofirst();
    assert_true(iter.valid());
    assert_eq(*(keys.begin()), iter.key());

    iter.seektolast();
    assert_true(iter.valid());
    assert_eq(*(keys.rbegin()), iter.key());
  }

  // forward iteration test
  for (int i = 0; i < r; i++) {
    skiplist<key, comparator>::iterator iter(&list);
    iter.seek(i);

    // compare against model iterator
    std::set<key>::iterator model_iter = keys.lower_bound(i);
    for (int j = 0; j < 3; j++) {
      if (model_iter == keys.end()) {
        assert_true(!iter.valid());
        break;
      } else {
        assert_true(iter.valid());
        assert_eq(*model_iter, iter.key());
        ++model_iter;
        iter.next();
      }
    }
  }

  // backward iteration test
  {
    skiplist<key, comparator>::iterator iter(&list);
    iter.seektolast();

    // compare against model iterator
    for (std::set<key>::reverse_iterator model_iter = keys.rbegin();
         model_iter != keys.rend();
         ++model_iter) {
      assert_true(iter.valid());
      assert_eq(*model_iter, iter.key());
      iter.prev();
    }
    assert_true(!iter.valid());
  }
}

// we want to make sure that with a single writer and multiple
// concurrent readers (with no synchronization other than when a
// reader's iterator is created), the reader always observes all the
// data that was present in the skip list when the iterator was
// constructor.  because insertions are happening concurrently, we may
// also observe new values that were inserted since the iterator was
// constructed, but we should never miss any values that were present
// at iterator construction time.
//
// we generate multi-part keys:
//     <key,gen,hash>
// where:
//     key is in range [0..k-1]
//     gen is a generation number for key
//     hash is hash(key,gen)
//
// the insertion code picks a random key, sets gen to be 1 + the last
// generation number inserted for that key, and sets hash to hash(key,gen).
//
// at the beginning of a read, we snapshot the last inserted
// generation number for each key.  we then iterate, including random
// calls to next() and seek().  for every key we encounter, we
// check that it is either expected given the initial snapshot or has
// been concurrently added since the iterator started.
class concurrenttest {
 private:
  static const uint32_t k = 4;

  static uint64_t key(key key) { return (key >> 40); }
  static uint64_t gen(key key) { return (key >> 8) & 0xffffffffu; }
  static uint64_t hash(key key) { return key & 0xff; }

  static uint64_t hashnumbers(uint64_t k, uint64_t g) {
    uint64_t data[2] = { k, g };
    return hash(reinterpret_cast<char*>(data), sizeof(data), 0);
  }

  static key makekey(uint64_t k, uint64_t g) {
    assert(sizeof(key) == sizeof(uint64_t));
    assert(k <= k);  // we sometimes pass k to seek to the end of the skiplist
    assert(g <= 0xffffffffu);
    return ((k << 40) | (g << 8) | (hashnumbers(k, g) & 0xff));
  }

  static bool isvalidkey(key k) {
    return hash(k) == (hashnumbers(key(k), gen(k)) & 0xff);
  }

  static key randomtarget(random* rnd) {
    switch (rnd->next() % 10) {
      case 0:
        // seek to beginning
        return makekey(0, 0);
      case 1:
        // seek to end
        return makekey(k, 0);
      default:
        // seek to middle
        return makekey(rnd->next() % k, 0);
    }
  }

  // per-key generation
  struct state {
    port::atomicpointer generation[k];
    void set(int k, intptr_t v) {
      generation[k].release_store(reinterpret_cast<void*>(v));
    }
    intptr_t get(int k) {
      return reinterpret_cast<intptr_t>(generation[k].acquire_load());
    }

    state() {
      for (int k = 0; k < k; k++) {
        set(k, 0);
      }
    }
  };

  // current state of the test
  state current_;

  arena arena_;

  // skiplist is not protected by mu_.  we just use a single writer
  // thread to modify it.
  skiplist<key, comparator> list_;

 public:
  concurrenttest() : list_(comparator(), &arena_) { }

  // requires: external synchronization
  void writestep(random* rnd) {
    const uint32_t k = rnd->next() % k;
    const intptr_t g = current_.get(k) + 1;
    const key key = makekey(k, g);
    list_.insert(key);
    current_.set(k, g);
  }

  void readstep(random* rnd) {
    // remember the initial committed state of the skiplist.
    state initial_state;
    for (int k = 0; k < k; k++) {
      initial_state.set(k, current_.get(k));
    }

    key pos = randomtarget(rnd);
    skiplist<key, comparator>::iterator iter(&list_);
    iter.seek(pos);
    while (true) {
      key current;
      if (!iter.valid()) {
        current = makekey(k, 0);
      } else {
        current = iter.key();
        assert_true(isvalidkey(current)) << current;
      }
      assert_le(pos, current) << "should not go backwards";

      // verify that everything in [pos,current) was not present in
      // initial_state.
      while (pos < current) {
        assert_lt(key(pos), k) << pos;

        // note that generation 0 is never inserted, so it is ok if
        // <*,0,*> is missing.
        assert_true((gen(pos) == 0) ||
                    (gen(pos) > initial_state.get(key(pos)))
                    ) << "key: " << key(pos)
                      << "; gen: " << gen(pos)
                      << "; initgen: "
                      << initial_state.get(key(pos));

        // advance to next key in the valid key space
        if (key(pos) < key(current)) {
          pos = makekey(key(pos) + 1, 0);
        } else {
          pos = makekey(key(pos), gen(pos) + 1);
        }
      }

      if (!iter.valid()) {
        break;
      }

      if (rnd->next() % 2) {
        iter.next();
        pos = makekey(key(pos), gen(pos) + 1);
      } else {
        key new_target = randomtarget(rnd);
        if (new_target > pos) {
          pos = new_target;
          iter.seek(new_target);
        }
      }
    }
  }
};
const uint32_t concurrenttest::k;

// simple test that does single-threaded testing of the concurrenttest
// scaffolding.
test(skiptest, concurrentwithoutthreads) {
  concurrenttest test;
  random rnd(test::randomseed());
  for (int i = 0; i < 10000; i++) {
    test.readstep(&rnd);
    test.writestep(&rnd);
  }
}

class teststate {
 public:
  concurrenttest t_;
  int seed_;
  port::atomicpointer quit_flag_;

  enum readerstate {
    starting,
    running,
    done
  };

  explicit teststate(int s)
      : seed_(s),
        quit_flag_(null),
        state_(starting),
        state_cv_(&mu_) {}

  void wait(readerstate s) {
    mu_.lock();
    while (state_ != s) {
      state_cv_.wait();
    }
    mu_.unlock();
  }

  void change(readerstate s) {
    mu_.lock();
    state_ = s;
    state_cv_.signal();
    mu_.unlock();
  }

 private:
  port::mutex mu_;
  readerstate state_;
  port::condvar state_cv_;
};

static void concurrentreader(void* arg) {
  teststate* state = reinterpret_cast<teststate*>(arg);
  random rnd(state->seed_);
  int64_t reads = 0;
  state->change(teststate::running);
  while (!state->quit_flag_.acquire_load()) {
    state->t_.readstep(&rnd);
    ++reads;
  }
  state->change(teststate::done);
}

static void runconcurrent(int run) {
  const int seed = test::randomseed() + (run * 100);
  random rnd(seed);
  const int n = 1000;
  const int ksize = 1000;
  for (int i = 0; i < n; i++) {
    if ((i % 100) == 0) {
      fprintf(stderr, "run %d of %d\n", i, n);
    }
    teststate state(seed + 1);
    env::default()->schedule(concurrentreader, &state);
    state.wait(teststate::running);
    for (int i = 0; i < ksize; i++) {
      state.t_.writestep(&rnd);
    }
    state.quit_flag_.release_store(&state);  // any non-null arg will do
    state.wait(teststate::done);
  }
}

test(skiptest, concurrent1) { runconcurrent(1); }
test(skiptest, concurrent2) { runconcurrent(2); }
test(skiptest, concurrent3) { runconcurrent(3); }
test(skiptest, concurrent4) { runconcurrent(4); }
test(skiptest, concurrent5) { runconcurrent(5); }

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
