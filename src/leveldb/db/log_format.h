// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// log format information shared by reader and writer.
// see ../doc/log_format.txt for more detail.

#ifndef storage_leveldb_db_log_format_h_
#define storage_leveldb_db_log_format_h_

namespace leveldb {
namespace log {

enum recordtype {
  // zero is reserved for preallocated files
  kzerotype = 0,

  kfulltype = 1,

  // for fragments
  kfirsttype = 2,
  kmiddletype = 3,
  klasttype = 4
};
static const int kmaxrecordtype = klasttype;

static const int kblocksize = 32768;

// header is checksum (4 bytes), length (2 bytes), type (1 byte).
static const int kheadersize = 4 + 2 + 1;

}  // namespace log
}  // namespace leveldb

#endif  // storage_leveldb_db_log_format_h_
