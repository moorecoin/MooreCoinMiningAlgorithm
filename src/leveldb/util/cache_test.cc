// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/cache.h"

#include <vector>
#include "util/coding.h"
#include "util/testharness.h"

namespace leveldb {

// conversions between numeric keys/values and the types expected by cache.
static std::string encodekey(int k) {
  std::string result;
  putfixed32(&result, k);
  return result;
}
static int decodekey(const slice& k) {
  assert(k.size() == 4);
  return decodefixed32(k.data());
}
static void* encodevalue(uintptr_t v) { return reinterpret_cast<void*>(v); }
static int decodevalue(void* v) { return reinterpret_cast<uintptr_t>(v); }

class cachetest {
 public:
  static cachetest* current_;

  static void deleter(const slice& key, void* v) {
    current_->deleted_keys_.push_back(decodekey(key));
    current_->deleted_values_.push_back(decodevalue(v));
  }

  static const int kcachesize = 1000;
  std::vector<int> deleted_keys_;
  std::vector<int> deleted_values_;
  cache* cache_;

  cachetest() : cache_(newlrucache(kcachesize)) {
    current_ = this;
  }

  ~cachetest() {
    delete cache_;
  }

  int lookup(int key) {
    cache::handle* handle = cache_->lookup(encodekey(key));
    const int r = (handle == null) ? -1 : decodevalue(cache_->value(handle));
    if (handle != null) {
      cache_->release(handle);
    }
    return r;
  }

  void insert(int key, int value, int charge = 1) {
    cache_->release(cache_->insert(encodekey(key), encodevalue(value), charge,
                                   &cachetest::deleter));
  }

  void erase(int key) {
    cache_->erase(encodekey(key));
  }
};
cachetest* cachetest::current_;

test(cachetest, hitandmiss) {
  assert_eq(-1, lookup(100));

  insert(100, 101);
  assert_eq(101, lookup(100));
  assert_eq(-1,  lookup(200));
  assert_eq(-1,  lookup(300));

  insert(200, 201);
  assert_eq(101, lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(-1,  lookup(300));

  insert(100, 102);
  assert_eq(102, lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(-1,  lookup(300));

  assert_eq(1, deleted_keys_.size());
  assert_eq(100, deleted_keys_[0]);
  assert_eq(101, deleted_values_[0]);
}

test(cachetest, erase) {
  erase(200);
  assert_eq(0, deleted_keys_.size());

  insert(100, 101);
  insert(200, 201);
  erase(100);
  assert_eq(-1,  lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(1, deleted_keys_.size());
  assert_eq(100, deleted_keys_[0]);
  assert_eq(101, deleted_values_[0]);

  erase(100);
  assert_eq(-1,  lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(1, deleted_keys_.size());
}

test(cachetest, entriesarepinned) {
  insert(100, 101);
  cache::handle* h1 = cache_->lookup(encodekey(100));
  assert_eq(101, decodevalue(cache_->value(h1)));

  insert(100, 102);
  cache::handle* h2 = cache_->lookup(encodekey(100));
  assert_eq(102, decodevalue(cache_->value(h2)));
  assert_eq(0, deleted_keys_.size());

  cache_->release(h1);
  assert_eq(1, deleted_keys_.size());
  assert_eq(100, deleted_keys_[0]);
  assert_eq(101, deleted_values_[0]);

  erase(100);
  assert_eq(-1, lookup(100));
  assert_eq(1, deleted_keys_.size());

  cache_->release(h2);
  assert_eq(2, deleted_keys_.size());
  assert_eq(100, deleted_keys_[1]);
  assert_eq(102, deleted_values_[1]);
}

test(cachetest, evictionpolicy) {
  insert(100, 101);
  insert(200, 201);

  // frequently used entry must be kept around
  for (int i = 0; i < kcachesize + 100; i++) {
    insert(1000+i, 2000+i);
    assert_eq(2000+i, lookup(1000+i));
    assert_eq(101, lookup(100));
  }
  assert_eq(101, lookup(100));
  assert_eq(-1, lookup(200));
}

test(cachetest, heavyentries) {
  // add a bunch of light and heavy entries and then count the combined
  // size of items still in the cache, which must be approximately the
  // same as the total capacity.
  const int klight = 1;
  const int kheavy = 10;
  int added = 0;
  int index = 0;
  while (added < 2*kcachesize) {
    const int weight = (index & 1) ? klight : kheavy;
    insert(index, 1000+index, weight);
    added += weight;
    index++;
  }

  int cached_weight = 0;
  for (int i = 0; i < index; i++) {
    const int weight = (i & 1 ? klight : kheavy);
    int r = lookup(i);
    if (r >= 0) {
      cached_weight += weight;
      assert_eq(1000+i, r);
    }
  }
  assert_le(cached_weight, kcachesize + kcachesize/10);
}

test(cachetest, newid) {
  uint64_t a = cache_->newid();
  uint64_t b = cache_->newid();
  assert_ne(a, b);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
