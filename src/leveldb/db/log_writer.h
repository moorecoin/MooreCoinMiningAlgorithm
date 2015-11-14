// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_log_writer_h_
#define storage_leveldb_db_log_writer_h_

#include <stdint.h>
#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class writablefile;

namespace log {

class writer {
 public:
  // create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this writer is in use.
  explicit writer(writablefile* dest);
  ~writer();

  status addrecord(const slice& slice);

 private:
  writablefile* dest_;
  int block_offset_;       // current offset in block

  // crc32c values for all supported record types.  these are
  // pre-computed to reduce the overhead of computing the crc of the
  // record type stored in the header.
  uint32_t type_crc_[kmaxrecordtype + 1];

  status emitphysicalrecord(recordtype type, const char* ptr, size_t length);

  // no copying allowed
  writer(const writer&);
  void operator=(const writer&);
};

}  // namespace log
}  // namespace leveldb

#endif  // storage_leveldb_db_log_writer_h_
