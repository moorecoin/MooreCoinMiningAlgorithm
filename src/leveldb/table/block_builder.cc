// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// blockbuilder generates blocks where keys are prefix-compressed:
//
// when we store a key, we drop the prefix shared with the previous
// string.  this helps reduce the space requirement significantly.
// furthermore, once every k keys, we do not apply the prefix
// compression and store the entire key.  we call this a "restart
// point".  the tail end of the block stores the offsets of all of the
// restart points, and can be used to do a binary search when looking
// for a particular key.  values are stored as-is (without compression)
// immediately following the corresponding key.
//
// an entry for a particular key-value pair has the form:
//     shared_bytes: varint32
//     unshared_bytes: varint32
//     value_length: varint32
//     key_delta: char[unshared_bytes]
//     value: char[value_length]
// shared_bytes == 0 for restart points.
//
// the trailer of the block has the form:
//     restarts: uint32[num_restarts]
//     num_restarts: uint32
// restarts[i] contains the offset within the block of the ith restart point.

#include "table/block_builder.h"

#include <algorithm>
#include <assert.h>
#include "leveldb/comparator.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"

namespace leveldb {

blockbuilder::blockbuilder(const options* options)
    : options_(options),
      restarts_(),
      counter_(0),
      finished_(false) {
  assert(options->block_restart_interval >= 1);
  restarts_.push_back(0);       // first restart point is at offset 0
}

void blockbuilder::reset() {
  buffer_.clear();
  restarts_.clear();
  restarts_.push_back(0);       // first restart point is at offset 0
  counter_ = 0;
  finished_ = false;
  last_key_.clear();
}

size_t blockbuilder::currentsizeestimate() const {
  return (buffer_.size() +                        // raw data buffer
          restarts_.size() * sizeof(uint32_t) +   // restart array
          sizeof(uint32_t));                      // restart array length
}

slice blockbuilder::finish() {
  // append restart array
  for (size_t i = 0; i < restarts_.size(); i++) {
    putfixed32(&buffer_, restarts_[i]);
  }
  putfixed32(&buffer_, restarts_.size());
  finished_ = true;
  return slice(buffer_);
}

void blockbuilder::add(const slice& key, const slice& value) {
  slice last_key_piece(last_key_);
  assert(!finished_);
  assert(counter_ <= options_->block_restart_interval);
  assert(buffer_.empty() // no values yet?
         || options_->comparator->compare(key, last_key_piece) > 0);
  size_t shared = 0;
  if (counter_ < options_->block_restart_interval) {
    // see how much sharing to do with previous string
    const size_t min_length = std::min(last_key_piece.size(), key.size());
    while ((shared < min_length) && (last_key_piece[shared] == key[shared])) {
      shared++;
    }
  } else {
    // restart compression
    restarts_.push_back(buffer_.size());
    counter_ = 0;
  }
  const size_t non_shared = key.size() - shared;

  // add "<shared><non_shared><value_size>" to buffer_
  putvarint32(&buffer_, shared);
  putvarint32(&buffer_, non_shared);
  putvarint32(&buffer_, value.size());

  // add string delta to buffer_ followed by value
  buffer_.append(key.data() + shared, non_shared);
  buffer_.append(value.data(), value.size());

  // update state
  last_key_.resize(shared);
  last_key_.append(key.data() + shared, non_shared);
  assert(slice(last_key_) == key);
  counter_++;
}

}  // namespace leveldb
