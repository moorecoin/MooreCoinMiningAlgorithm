// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/arena.h"

#include "util/random.h"
#include "util/testharness.h"

namespace leveldb {

class arenatest { };

test(arenatest, empty) {
  arena arena;
}

test(arenatest, simple) {
  std::vector<std::pair<size_t, char*> > allocated;
  arena arena;
  const int n = 100000;
  size_t bytes = 0;
  random rnd(301);
  for (int i = 0; i < n; i++) {
    size_t s;
    if (i % (n / 10) == 0) {
      s = i;
    } else {
      s = rnd.onein(4000) ? rnd.uniform(6000) :
          (rnd.onein(10) ? rnd.uniform(100) : rnd.uniform(20));
    }
    if (s == 0) {
      // our arena disallows size 0 allocations.
      s = 1;
    }
    char* r;
    if (rnd.onein(10)) {
      r = arena.allocatealigned(s);
    } else {
      r = arena.allocate(s);
    }

    for (size_t b = 0; b < s; b++) {
      // fill the "i"th allocation with a known bit pattern
      r[b] = i % 256;
    }
    bytes += s;
    allocated.push_back(std::make_pair(s, r));
    assert_ge(arena.memoryusage(), bytes);
    if (i > n/10) {
      assert_le(arena.memoryusage(), bytes * 1.10);
    }
  }
  for (size_t i = 0; i < allocated.size(); i++) {
    size_t num_bytes = allocated[i].first;
    const char* p = allocated[i].second;
    for (size_t b = 0; b < num_bytes; b++) {
      // check the "i"th allocation for the known bit pattern
      assert_eq(int(p[b]) & 0xff, i % 256);
    }
  }
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
