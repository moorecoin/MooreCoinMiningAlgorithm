// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a cache is an interface that maps keys to values.  it has internal
// synchronization and may be safely accessed concurrently from
// multiple threads.  it may automatically evict entries to make room
// for new entries.  values have a specified charge against the cache
// capacity.  for example, a cache where the values are variable
// length strings, may use the length of the string as the charge for
// the string.
//
// a builtin cache implementation with a least-recently-used eviction
// policy is provided.  clients may use their own implementations if
// they want something more sophisticated (like scan-resistance, a
// custom eviction policy, variable cache sizing, etc.)

#ifndef storage_leveldb_include_cache_h_
#define storage_leveldb_include_cache_h_

#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb {

class cache;

// create a new cache with a fixed size capacity.  this implementation
// of cache uses a least-recently-used eviction policy.
extern cache* newlrucache(size_t capacity);

class cache {
 public:
  cache() { }

  // destroys all existing entries by calling the "deleter"
  // function that was passed to the constructor.
  virtual ~cache();

  // opaque handle to an entry stored in the cache.
  struct handle { };

  // insert a mapping from key->value into the cache and assign it
  // the specified charge against the total cache capacity.
  //
  // returns a handle that corresponds to the mapping.  the caller
  // must call this->release(handle) when the returned mapping is no
  // longer needed.
  //
  // when the inserted entry is no longer needed, the key and
  // value will be passed to "deleter".
  virtual handle* insert(const slice& key, void* value, size_t charge,
                         void (*deleter)(const slice& key, void* value)) = 0;

  // if the cache has no mapping for "key", returns null.
  //
  // else return a handle that corresponds to the mapping.  the caller
  // must call this->release(handle) when the returned mapping is no
  // longer needed.
  virtual handle* lookup(const slice& key) = 0;

  // release a mapping returned by a previous lookup().
  // requires: handle must not have been released yet.
  // requires: handle must have been returned by a method on *this.
  virtual void release(handle* handle) = 0;

  // return the value encapsulated in a handle returned by a
  // successful lookup().
  // requires: handle must not have been released yet.
  // requires: handle must have been returned by a method on *this.
  virtual void* value(handle* handle) = 0;

  // if the cache contains entry for key, erase it.  note that the
  // underlying entry will be kept around until all existing handles
  // to it have been released.
  virtual void erase(const slice& key) = 0;

  // return a new numeric id.  may be used by multiple clients who are
  // sharing the same cache to partition the key space.  typically the
  // client will allocate a new id at startup and prepend the id to
  // its cache keys.
  virtual uint64_t newid() = 0;

 private:
  void lru_remove(handle* e);
  void lru_append(handle* e);
  void unref(handle* e);

  struct rep;
  rep* rep_;

  // no copying allowed
  cache(const cache&);
  void operator=(const cache&);
};

}  // namespace leveldb

#endif  // storage_leveldb_include_cache_h_
