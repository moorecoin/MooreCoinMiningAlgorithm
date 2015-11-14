// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <algorithm>
#include <stdint.h>
#include "leveldb/comparator.h"
#include "leveldb/slice.h"
#include "port/port.h"
#include "util/logging.h"

namespace leveldb {

comparator::~comparator() { }

namespace {
class bytewisecomparatorimpl : public comparator {
 public:
  bytewisecomparatorimpl() { }

  virtual const char* name() const {
    return "leveldb.bytewisecomparator";
  }

  virtual int compare(const slice& a, const slice& b) const {
    return a.compare(b);
  }

  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const {
    // find length of common prefix
    size_t min_length = std::min(start->size(), limit.size());
    size_t diff_index = 0;
    while ((diff_index < min_length) &&
           ((*start)[diff_index] == limit[diff_index])) {
      diff_index++;
    }

    if (diff_index >= min_length) {
      // do not shorten if one string is a prefix of the other
    } else {
      uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
      if (diff_byte < static_cast<uint8_t>(0xff) &&
          diff_byte + 1 < static_cast<uint8_t>(limit[diff_index])) {
        (*start)[diff_index]++;
        start->resize(diff_index + 1);
        assert(compare(*start, limit) < 0);
      }
    }
  }

  virtual void findshortsuccessor(std::string* key) const {
    // find first character that can be incremented
    size_t n = key->size();
    for (size_t i = 0; i < n; i++) {
      const uint8_t byte = (*key)[i];
      if (byte != static_cast<uint8_t>(0xff)) {
        (*key)[i] = byte + 1;
        key->resize(i+1);
        return;
      }
    }
    // *key is a run of 0xffs.  leave it alone.
  }
};
}  // namespace

static port::oncetype once = leveldb_once_init;
static const comparator* bytewise;

static void initmodule() {
  bytewise = new bytewisecomparatorimpl;
}

const comparator* bytewisecomparator() {
  port::initonce(&once, initmodule);
  return bytewise;
}

}  // namespace leveldb
