// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_table_block_h_
#define storage_leveldb_table_block_h_

#include <stddef.h>
#include <stdint.h>
#include "leveldb/iterator.h"

namespace leveldb {

struct blockcontents;
class comparator;

class block {
 public:
  // initialize the block with the specified contents.
  explicit block(const blockcontents& contents);

  ~block();

  size_t size() const { return size_; }
  iterator* newiterator(const comparator* comparator);

 private:
  uint32_t numrestarts() const;

  const char* data_;
  size_t size_;
  uint32_t restart_offset_;     // offset in data_ of restart array
  bool owned_;                  // block owns data_[]

  // no copying allowed
  block(const block&);
  void operator=(const block&);

  class iter;
};

}  // namespace leveldb

#endif  // storage_leveldb_table_block_h_
