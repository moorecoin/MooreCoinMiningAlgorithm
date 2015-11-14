// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// writebatch::rep_ :=
//    sequence: fixed64
//    count: fixed32
//    data: record[count]
// record :=
//    ktypevalue varstring varstring         |
//    ktypedeletion varstring
// varstring :=
//    len: varint32
//    data: uint8[len]

#include "leveldb/write_batch.h"

#include "leveldb/db.h"
#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "util/coding.h"

namespace leveldb {

// writebatch header has an 8-byte sequence number followed by a 4-byte count.
static const size_t kheader = 12;

writebatch::writebatch() {
  clear();
}

writebatch::~writebatch() { }

writebatch::handler::~handler() { }

void writebatch::clear() {
  rep_.clear();
  rep_.resize(kheader);
}

status writebatch::iterate(handler* handler) const {
  slice input(rep_);
  if (input.size() < kheader) {
    return status::corruption("malformed writebatch (too small)");
  }

  input.remove_prefix(kheader);
  slice key, value;
  int found = 0;
  while (!input.empty()) {
    found++;
    char tag = input[0];
    input.remove_prefix(1);
    switch (tag) {
      case ktypevalue:
        if (getlengthprefixedslice(&input, &key) &&
            getlengthprefixedslice(&input, &value)) {
          handler->put(key, value);
        } else {
          return status::corruption("bad writebatch put");
        }
        break;
      case ktypedeletion:
        if (getlengthprefixedslice(&input, &key)) {
          handler->delete(key);
        } else {
          return status::corruption("bad writebatch delete");
        }
        break;
      default:
        return status::corruption("unknown writebatch tag");
    }
  }
  if (found != writebatchinternal::count(this)) {
    return status::corruption("writebatch has wrong count");
  } else {
    return status::ok();
  }
}

int writebatchinternal::count(const writebatch* b) {
  return decodefixed32(b->rep_.data() + 8);
}

void writebatchinternal::setcount(writebatch* b, int n) {
  encodefixed32(&b->rep_[8], n);
}

sequencenumber writebatchinternal::sequence(const writebatch* b) {
  return sequencenumber(decodefixed64(b->rep_.data()));
}

void writebatchinternal::setsequence(writebatch* b, sequencenumber seq) {
  encodefixed64(&b->rep_[0], seq);
}

void writebatch::put(const slice& key, const slice& value) {
  writebatchinternal::setcount(this, writebatchinternal::count(this) + 1);
  rep_.push_back(static_cast<char>(ktypevalue));
  putlengthprefixedslice(&rep_, key);
  putlengthprefixedslice(&rep_, value);
}

void writebatch::delete(const slice& key) {
  writebatchinternal::setcount(this, writebatchinternal::count(this) + 1);
  rep_.push_back(static_cast<char>(ktypedeletion));
  putlengthprefixedslice(&rep_, key);
}

namespace {
class memtableinserter : public writebatch::handler {
 public:
  sequencenumber sequence_;
  memtable* mem_;

  virtual void put(const slice& key, const slice& value) {
    mem_->add(sequence_, ktypevalue, key, value);
    sequence_++;
  }
  virtual void delete(const slice& key) {
    mem_->add(sequence_, ktypedeletion, key, slice());
    sequence_++;
  }
};
}  // namespace

status writebatchinternal::insertinto(const writebatch* b,
                                      memtable* memtable) {
  memtableinserter inserter;
  inserter.sequence_ = writebatchinternal::sequence(b);
  inserter.mem_ = memtable;
  return b->iterate(&inserter);
}

void writebatchinternal::setcontents(writebatch* b, const slice& contents) {
  assert(contents.size() >= kheader);
  b->rep_.assign(contents.data(), contents.size());
}

void writebatchinternal::append(writebatch* dst, const writebatch* src) {
  setcount(dst, count(dst) + count(src));
  assert(src->rep_.size() >= kheader);
  dst->rep_.append(src->rep_.data() + kheader, src->rep_.size() - kheader);
}

}  // namespace leveldb
