// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/log_writer.h"

#include <stdint.h>
#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {
namespace log {

writer::writer(writablefile* dest)
    : dest_(dest),
      block_offset_(0) {
  for (int i = 0; i <= kmaxrecordtype; i++) {
    char t = static_cast<char>(i);
    type_crc_[i] = crc32c::value(&t, 1);
  }
}

writer::~writer() {
}

status writer::addrecord(const slice& slice) {
  const char* ptr = slice.data();
  size_t left = slice.size();

  // fragment the record if necessary and emit it.  note that if slice
  // is empty, we still want to iterate once to emit a single
  // zero-length record
  status s;
  bool begin = true;
  do {
    const int leftover = kblocksize - block_offset_;
    assert(leftover >= 0);
    if (leftover < kheadersize) {
      // switch to a new block
      if (leftover > 0) {
        // fill the trailer (literal below relies on kheadersize being 7)
        assert(kheadersize == 7);
        dest_->append(slice("\x00\x00\x00\x00\x00\x00", leftover));
      }
      block_offset_ = 0;
    }

    // invariant: we never leave < kheadersize bytes in a block.
    assert(kblocksize - block_offset_ - kheadersize >= 0);

    const size_t avail = kblocksize - block_offset_ - kheadersize;
    const size_t fragment_length = (left < avail) ? left : avail;

    recordtype type;
    const bool end = (left == fragment_length);
    if (begin && end) {
      type = kfulltype;
    } else if (begin) {
      type = kfirsttype;
    } else if (end) {
      type = klasttype;
    } else {
      type = kmiddletype;
    }

    s = emitphysicalrecord(type, ptr, fragment_length);
    ptr += fragment_length;
    left -= fragment_length;
    begin = false;
  } while (s.ok() && left > 0);
  return s;
}

status writer::emitphysicalrecord(recordtype t, const char* ptr, size_t n) {
  assert(n <= 0xffff);  // must fit in two bytes
  assert(block_offset_ + kheadersize + n <= kblocksize);

  // format the header
  char buf[kheadersize];
  buf[4] = static_cast<char>(n & 0xff);
  buf[5] = static_cast<char>(n >> 8);
  buf[6] = static_cast<char>(t);

  // compute the crc of the record type and the payload.
  uint32_t crc = crc32c::extend(type_crc_[t], ptr, n);
  crc = crc32c::mask(crc);                 // adjust for storage
  encodefixed32(buf, crc);

  // write the header and the payload
  status s = dest_->append(slice(buf, kheadersize));
  if (s.ok()) {
    s = dest_->append(slice(ptr, n));
    if (s.ok()) {
      s = dest_->flush();
    }
  }
  block_offset_ += kheadersize + n;
  return s;
}

}  // namespace log
}  // namespace leveldb
