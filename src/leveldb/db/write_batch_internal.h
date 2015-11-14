// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_write_batch_internal_h_
#define storage_leveldb_db_write_batch_internal_h_

#include "leveldb/write_batch.h"

namespace leveldb {

class memtable;

// writebatchinternal provides static methods for manipulating a
// writebatch that we don't want in the public writebatch interface.
class writebatchinternal {
 public:
  // return the number of entries in the batch.
  static int count(const writebatch* batch);

  // set the count for the number of entries in the batch.
  static void setcount(writebatch* batch, int n);

  // return the sequence number for the start of this batch.
  static sequencenumber sequence(const writebatch* batch);

  // store the specified number as the sequence number for the start of
  // this batch.
  static void setsequence(writebatch* batch, sequencenumber seq);

  static slice contents(const writebatch* batch) {
    return slice(batch->rep_);
  }

  static size_t bytesize(const writebatch* batch) {
    return batch->rep_.size();
  }

  static void setcontents(writebatch* batch, const slice& contents);

  static status insertinto(const writebatch* batch, memtable* memtable);

  static void append(writebatch* dst, const writebatch* src);
};

}  // namespace leveldb


#endif  // storage_leveldb_db_write_batch_internal_h_
