// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "leveldb/cache.h"
#include "port/port.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {

cache::~cache() {
}

namespace {

// lru cache implementation

// an entry is a variable length heap-allocated structure.  entries
// are kept in a circular doubly linked list ordered by access time.
struct lruhandle {
  void* value;
  void (*deleter)(const slice&, void* value);
  lruhandle* next_hash;
  lruhandle* next;
  lruhandle* prev;
  size_t charge;      // todo(opt): only allow uint32_t?
  size_t key_length;
  uint32_t refs;
  uint32_t hash;      // hash of key(); used for fast sharding and comparisons
  char key_data[1];   // beginning of key

  slice key() const {
    // for cheaper lookups, we allow a temporary handle object
    // to store a pointer to a key in "value".
    if (next == this) {
      return *(reinterpret_cast<slice*>(value));
    } else {
      return slice(key_data, key_length);
    }
  }
};

// we provide our own simple hash table since it removes a whole bunch
// of porting hacks and is also faster than some of the built-in hash
// table implementations in some of the compiler/runtime combinations
// we have tested.  e.g., readrandom speeds up by ~5% over the g++
// 4.4.3's builtin hashtable.
class handletable {
 public:
  handletable() : length_(0), elems_(0), list_(null) { resize(); }
  ~handletable() { delete[] list_; }

  lruhandle* lookup(const slice& key, uint32_t hash) {
    return *findpointer(key, hash);
  }

  lruhandle* insert(lruhandle* h) {
    lruhandle** ptr = findpointer(h->key(), h->hash);
    lruhandle* old = *ptr;
    h->next_hash = (old == null ? null : old->next_hash);
    *ptr = h;
    if (old == null) {
      ++elems_;
      if (elems_ > length_) {
        // since each cache entry is fairly large, we aim for a small
        // average linked list length (<= 1).
        resize();
      }
    }
    return old;
  }

  lruhandle* remove(const slice& key, uint32_t hash) {
    lruhandle** ptr = findpointer(key, hash);
    lruhandle* result = *ptr;
    if (result != null) {
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
  // the table consists of an array of buckets where each bucket is
  // a linked list of cache entries that hash into the bucket.
  uint32_t length_;
  uint32_t elems_;
  lruhandle** list_;

  // return a pointer to slot that points to a cache entry that
  // matches key/hash.  if there is no such cache entry, return a
  // pointer to the trailing slot in the corresponding linked list.
  lruhandle** findpointer(const slice& key, uint32_t hash) {
    lruhandle** ptr = &list_[hash & (length_ - 1)];
    while (*ptr != null &&
           ((*ptr)->hash != hash || key != (*ptr)->key())) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    lruhandle** new_list = new lruhandle*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      lruhandle* h = list_[i];
      while (h != null) {
        lruhandle* next = h->next_hash;
        uint32_t hash = h->hash;
        lruhandle** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};

// a single shard of sharded cache.
class lrucache {
 public:
  lrucache();
  ~lrucache();

  // separate from constructor so caller can easily make an array of lrucache
  void setcapacity(size_t capacity) { capacity_ = capacity; }

  // like cache methods, but with an extra "hash" parameter.
  cache::handle* insert(const slice& key, uint32_t hash,
                        void* value, size_t charge,
                        void (*deleter)(const slice& key, void* value));
  cache::handle* lookup(const slice& key, uint32_t hash);
  void release(cache::handle* handle);
  void erase(const slice& key, uint32_t hash);

 private:
  void lru_remove(lruhandle* e);
  void lru_append(lruhandle* e);
  void unref(lruhandle* e);

  // initialized before use.
  size_t capacity_;

  // mutex_ protects the following state.
  port::mutex mutex_;
  size_t usage_;

  // dummy head of lru list.
  // lru.prev is newest entry, lru.next is oldest entry.
  lruhandle lru_;

  handletable table_;
};

lrucache::lrucache()
    : usage_(0) {
  // make empty circular linked list
  lru_.next = &lru_;
  lru_.prev = &lru_;
}

lrucache::~lrucache() {
  for (lruhandle* e = lru_.next; e != &lru_; ) {
    lruhandle* next = e->next;
    assert(e->refs == 1);  // error if caller has an unreleased handle
    unref(e);
    e = next;
  }
}

void lrucache::unref(lruhandle* e) {
  assert(e->refs > 0);
  e->refs--;
  if (e->refs <= 0) {
    usage_ -= e->charge;
    (*e->deleter)(e->key(), e->value);
    free(e);
  }
}

void lrucache::lru_remove(lruhandle* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

void lrucache::lru_append(lruhandle* e) {
  // make "e" newest entry by inserting just before lru_
  e->next = &lru_;
  e->prev = lru_.prev;
  e->prev->next = e;
  e->next->prev = e;
}

cache::handle* lrucache::lookup(const slice& key, uint32_t hash) {
  mutexlock l(&mutex_);
  lruhandle* e = table_.lookup(key, hash);
  if (e != null) {
    e->refs++;
    lru_remove(e);
    lru_append(e);
  }
  return reinterpret_cast<cache::handle*>(e);
}

void lrucache::release(cache::handle* handle) {
  mutexlock l(&mutex_);
  unref(reinterpret_cast<lruhandle*>(handle));
}

cache::handle* lrucache::insert(
    const slice& key, uint32_t hash, void* value, size_t charge,
    void (*deleter)(const slice& key, void* value)) {
  mutexlock l(&mutex_);

  lruhandle* e = reinterpret_cast<lruhandle*>(
      malloc(sizeof(lruhandle)-1 + key.size()));
  e->value = value;
  e->deleter = deleter;
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->refs = 2;  // one from lrucache, one for the returned handle
  memcpy(e->key_data, key.data(), key.size());
  lru_append(e);
  usage_ += charge;

  lruhandle* old = table_.insert(e);
  if (old != null) {
    lru_remove(old);
    unref(old);
  }

  while (usage_ > capacity_ && lru_.next != &lru_) {
    lruhandle* old = lru_.next;
    lru_remove(old);
    table_.remove(old->key(), old->hash);
    unref(old);
  }

  return reinterpret_cast<cache::handle*>(e);
}

void lrucache::erase(const slice& key, uint32_t hash) {
  mutexlock l(&mutex_);
  lruhandle* e = table_.remove(key, hash);
  if (e != null) {
    lru_remove(e);
    unref(e);
  }
}

static const int knumshardbits = 4;
static const int knumshards = 1 << knumshardbits;

class shardedlrucache : public cache {
 private:
  lrucache shard_[knumshards];
  port::mutex id_mutex_;
  uint64_t last_id_;

  static inline uint32_t hashslice(const slice& s) {
    return hash(s.data(), s.size(), 0);
  }

  static uint32_t shard(uint32_t hash) {
    return hash >> (32 - knumshardbits);
  }

 public:
  explicit shardedlrucache(size_t capacity)
      : last_id_(0) {
    const size_t per_shard = (capacity + (knumshards - 1)) / knumshards;
    for (int s = 0; s < knumshards; s++) {
      shard_[s].setcapacity(per_shard);
    }
  }
  virtual ~shardedlrucache() { }
  virtual handle* insert(const slice& key, void* value, size_t charge,
                         void (*deleter)(const slice& key, void* value)) {
    const uint32_t hash = hashslice(key);
    return shard_[shard(hash)].insert(key, hash, value, charge, deleter);
  }
  virtual handle* lookup(const slice& key) {
    const uint32_t hash = hashslice(key);
    return shard_[shard(hash)].lookup(key, hash);
  }
  virtual void release(handle* handle) {
    lruhandle* h = reinterpret_cast<lruhandle*>(handle);
    shard_[shard(h->hash)].release(handle);
  }
  virtual void erase(const slice& key) {
    const uint32_t hash = hashslice(key);
    shard_[shard(hash)].erase(key, hash);
  }
  virtual void* value(handle* handle) {
    return reinterpret_cast<lruhandle*>(handle)->value;
  }
  virtual uint64_t newid() {
    mutexlock l(&id_mutex_);
    return ++(last_id_);
  }
};

}  // end anonymous namespace

cache* newlrucache(size_t capacity) {
  return new shardedlrucache(capacity);
}

}  // namespace leveldb
