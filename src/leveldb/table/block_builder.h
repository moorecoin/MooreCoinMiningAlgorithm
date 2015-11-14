// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_table_block_builder_h_
#define storage_leveldb_table_block_builder_h_

#include <vector>

#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb {

struct options;

class blockbuilder {
 public:
  explicit blockbuilder(const options* options);

  // reset the contents as if the blockbuilder was just constructed.
  void reset();

  // requires: finish() has not been called since the last call to reset().
  // requires: key is larger than any previously added key
  void add(const slice& key, const slice& value);

  // finish building the block and return a slice that refers to the
  // block contents.  the returned slice will remain valid for the
  // lifetime of this builder or until reset() is called.
  slice finish();

  // returns an estimate of the current (uncompressed) size of the block
  // we are building.
  size_t currentsizeestimate() const;

  // return true iff no entries have been added since the last reset()
  bool empty() const {
    return buffer_.empty();
  }

 private:
  const options*        options_;
  std::string           buffer_;      // destination buffer
  std::vector<uint32_t> restarts_;    // restart points
  int                   counter_;     // number of entries emitted since restart
  bool                  finished_;    // has finish() been called?
  std::string           last_key_;

  // no copying allowed
  blockbuilder(const blockbuilder&);
  void operator=(const blockbuilder&);
};

}  // namespace leveldb

#endif  // storage_leveldb_table_block_builder_h_
