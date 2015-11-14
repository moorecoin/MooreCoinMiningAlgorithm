// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// writebatch holds a collection of updates to apply atomically to a db.
//
// the updates are applied in the order in which they are added
// to the writebatch.  for example, the value of "key" will be "v3"
// after the following batch is written:
//
//    batch.put("key", "v1");
//    batch.delete("key");
//    batch.put("key", "v2");
//    batch.put("key", "v3");
//
// multiple threads can invoke const methods on a writebatch without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same writebatch must use
// external synchronization.

#ifndef storage_leveldb_include_write_batch_h_
#define storage_leveldb_include_write_batch_h_

#include <string>
#include "leveldb/status.h"

namespace leveldb {

class slice;

class writebatch {
 public:
  writebatch();
  ~writebatch();

  // store the mapping "key->value" in the database.
  void put(const slice& key, const slice& value);

  // if the database contains a mapping for "key", erase it.  else do nothing.
  void delete(const slice& key);

  // clear all updates buffered in this batch.
  void clear();

  // support for iterating over the contents of a batch.
  class handler {
   public:
    virtual ~handler();
    virtual void put(const slice& key, const slice& value) = 0;
    virtual void delete(const slice& key) = 0;
  };
  status iterate(handler* handler) const;

 private:
  friend class writebatchinternal;

  std::string rep_;  // see comment in write_batch.cc for the format of rep_

  // intentionally copyable
};

}  // namespace leveldb

#endif  // storage_leveldb_include_write_batch_h_
