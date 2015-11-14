// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_util_random_h_
#define storage_leveldb_util_random_h_

#include <stdint.h>

namespace leveldb {

// a very simple random number generator.  not especially good at
// generating truly random bits, but good enough for our needs in this
// package.
class random {
 private:
  uint32_t seed_;
 public:
  explicit random(uint32_t s) : seed_(s & 0x7fffffffu) {
    // avoid bad seeds.
    if (seed_ == 0 || seed_ == 2147483647l) {
      seed_ = 1;
    }
  }
  uint32_t next() {
    static const uint32_t m = 2147483647l;   // 2^31-1
    static const uint64_t a = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
    // we are computing
    //       seed_ = (seed_ * a) % m,    where m = 2^31-1
    //
    // seed_ must not be zero or m, or else all subsequent computed values
    // will be zero or m respectively.  for all other values, seed_ will end
    // up cycling through every number in [1,m-1]
    uint64_t product = seed_ * a;

    // compute (product % m) using the fact that ((x << 31) % m) == x.
    seed_ = static_cast<uint32_t>((product >> 31) + (product & m));
    // the first reduction may overflow by 1 bit, so we may need to
    // repeat.  mod == m is not possible; using > allows the faster
    // sign-bit-based test.
    if (seed_ > m) {
      seed_ -= m;
    }
    return seed_;
  }
  // returns a uniformly distributed value in the range [0..n-1]
  // requires: n > 0
  uint32_t uniform(int n) { return next() % n; }

  // randomly returns true ~"1/n" of the time, and false otherwise.
  // requires: n > 0
  bool onein(int n) { return (next() % n) == 0; }

  // skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  the effect is to pick a number in the
  // range [0,2^max_log-1] with exponential bias towards smaller numbers.
  uint32_t skewed(int max_log) {
    return uniform(1 << uniform(max_log + 1));
  }
};

}  // namespace leveldb

#endif  // storage_leveldb_util_random_h_
