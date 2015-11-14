// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a filter block is stored near the end of a table file.  it contains
// filters (e.g., bloom filters) for all data blocks in the table combined
// into a single filter block.

#ifndef storage_leveldb_table_filter_block_h_
#define storage_leveldb_table_filter_block_h_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "leveldb/slice.h"
#include "util/hash.h"

namespace leveldb {

class filterpolicy;

// a filterblockbuilder is used to construct all of the filters for a
// particular table.  it generates a single string which is stored as
// a special block in the table.
//
// the sequence of calls to filterblockbuilder must match the regexp:
//      (startblock addkey*)* finish
class filterblockbuilder {
 public:
  explicit filterblockbuilder(const filterpolicy*);

  void startblock(uint64_t block_offset);
  void addkey(const slice& key);
  slice finish();

 private:
  void generatefilter();

  const filterpolicy* policy_;
  std::string keys_;              // flattened key contents
  std::vector<size_t> start_;     // starting index in keys_ of each key
  std::string result_;            // filter data computed so far
  std::vector<slice> tmp_keys_;   // policy_->createfilter() argument
  std::vector<uint32_t> filter_offsets_;

  // no copying allowed
  filterblockbuilder(const filterblockbuilder&);
  void operator=(const filterblockbuilder&);
};

class filterblockreader {
 public:
 // requires: "contents" and *policy must stay live while *this is live.
  filterblockreader(const filterpolicy* policy, const slice& contents);
  bool keymaymatch(uint64_t block_offset, const slice& key);

 private:
  const filterpolicy* policy_;
  const char* data_;    // pointer to filter data (at block-start)
  const char* offset_;  // pointer to beginning of offset array (at block-end)
  size_t num_;          // number of entries in offset array
  size_t base_lg_;      // encoding parameter (see kfilterbaselg in .cc file)
};

}

#endif  // storage_leveldb_table_filter_block_h_
