// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"

namespace leveldb {

// see doc/table_format.txt for an explanation of the filter block format.

// generate new filter every 2kb of data
static const size_t kfilterbaselg = 11;
static const size_t kfilterbase = 1 << kfilterbaselg;

filterblockbuilder::filterblockbuilder(const filterpolicy* policy)
    : policy_(policy) {
}

void filterblockbuilder::startblock(uint64_t block_offset) {
  uint64_t filter_index = (block_offset / kfilterbase);
  assert(filter_index >= filter_offsets_.size());
  while (filter_index > filter_offsets_.size()) {
    generatefilter();
  }
}

void filterblockbuilder::addkey(const slice& key) {
  slice k = key;
  start_.push_back(keys_.size());
  keys_.append(k.data(), k.size());
}

slice filterblockbuilder::finish() {
  if (!start_.empty()) {
    generatefilter();
  }

  // append array of per-filter offsets
  const uint32_t array_offset = result_.size();
  for (size_t i = 0; i < filter_offsets_.size(); i++) {
    putfixed32(&result_, filter_offsets_[i]);
  }

  putfixed32(&result_, array_offset);
  result_.push_back(kfilterbaselg);  // save encoding parameter in result
  return slice(result_);
}

void filterblockbuilder::generatefilter() {
  const size_t num_keys = start_.size();
  if (num_keys == 0) {
    // fast path if there are no keys for this filter
    filter_offsets_.push_back(result_.size());
    return;
  }

  // make list of keys from flattened key structure
  start_.push_back(keys_.size());  // simplify length computation
  tmp_keys_.resize(num_keys);
  for (size_t i = 0; i < num_keys; i++) {
    const char* base = keys_.data() + start_[i];
    size_t length = start_[i+1] - start_[i];
    tmp_keys_[i] = slice(base, length);
  }

  // generate filter for current set of keys and append to result_.
  filter_offsets_.push_back(result_.size());
  policy_->createfilter(&tmp_keys_[0], num_keys, &result_);

  tmp_keys_.clear();
  keys_.clear();
  start_.clear();
}

filterblockreader::filterblockreader(const filterpolicy* policy,
                                     const slice& contents)
    : policy_(policy),
      data_(null),
      offset_(null),
      num_(0),
      base_lg_(0) {
  size_t n = contents.size();
  if (n < 5) return;  // 1 byte for base_lg_ and 4 for start of offset array
  base_lg_ = contents[n-1];
  uint32_t last_word = decodefixed32(contents.data() + n - 5);
  if (last_word > n - 5) return;
  data_ = contents.data();
  offset_ = data_ + last_word;
  num_ = (n - 5 - last_word) / 4;
}

bool filterblockreader::keymaymatch(uint64_t block_offset, const slice& key) {
  uint64_t index = block_offset >> base_lg_;
  if (index < num_) {
    uint32_t start = decodefixed32(offset_ + index*4);
    uint32_t limit = decodefixed32(offset_ + index*4 + 4);
    if (start <= limit && limit <= (offset_ - data_)) {
      slice filter = slice(data_ + start, limit - start);
      return policy_->keymaymatch(key, filter);
    } else if (start == limit) {
      // empty filters do not match any keys
      return false;
    }
  }
  return true;  // errors are treated as potential matches
}

}
