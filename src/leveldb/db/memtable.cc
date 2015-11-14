// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/memtable.h"
#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "util/coding.h"

namespace leveldb {

static slice getlengthprefixedslice(const char* data) {
  uint32_t len;
  const char* p = data;
  p = getvarint32ptr(p, p + 5, &len);  // +5: we assume "p" is not corrupted
  return slice(p, len);
}

memtable::memtable(const internalkeycomparator& cmp)
    : comparator_(cmp),
      refs_(0),
      table_(comparator_, &arena_) {
}

memtable::~memtable() {
  assert(refs_ == 0);
}

size_t memtable::approximatememoryusage() { return arena_.memoryusage(); }

int memtable::keycomparator::operator()(const char* aptr, const char* bptr)
    const {
  // internal keys are encoded as length-prefixed strings.
  slice a = getlengthprefixedslice(aptr);
  slice b = getlengthprefixedslice(bptr);
  return comparator.compare(a, b);
}

// encode a suitable internal key target for "target" and return it.
// uses *scratch as scratch space, and the returned pointer will point
// into this scratch space.
static const char* encodekey(std::string* scratch, const slice& target) {
  scratch->clear();
  putvarint32(scratch, target.size());
  scratch->append(target.data(), target.size());
  return scratch->data();
}

class memtableiterator: public iterator {
 public:
  explicit memtableiterator(memtable::table* table) : iter_(table) { }

  virtual bool valid() const { return iter_.valid(); }
  virtual void seek(const slice& k) { iter_.seek(encodekey(&tmp_, k)); }
  virtual void seektofirst() { iter_.seektofirst(); }
  virtual void seektolast() { iter_.seektolast(); }
  virtual void next() { iter_.next(); }
  virtual void prev() { iter_.prev(); }
  virtual slice key() const { return getlengthprefixedslice(iter_.key()); }
  virtual slice value() const {
    slice key_slice = getlengthprefixedslice(iter_.key());
    return getlengthprefixedslice(key_slice.data() + key_slice.size());
  }

  virtual status status() const { return status::ok(); }

 private:
  memtable::table::iterator iter_;
  std::string tmp_;       // for passing to encodekey

  // no copying allowed
  memtableiterator(const memtableiterator&);
  void operator=(const memtableiterator&);
};

iterator* memtable::newiterator() {
  return new memtableiterator(&table_);
}

void memtable::add(sequencenumber s, valuetype type,
                   const slice& key,
                   const slice& value) {
  // format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  size_t val_size = value.size();
  size_t internal_key_size = key_size + 8;
  const size_t encoded_len =
      varintlength(internal_key_size) + internal_key_size +
      varintlength(val_size) + val_size;
  char* buf = arena_.allocate(encoded_len);
  char* p = encodevarint32(buf, internal_key_size);
  memcpy(p, key.data(), key_size);
  p += key_size;
  encodefixed64(p, (s << 8) | type);
  p += 8;
  p = encodevarint32(p, val_size);
  memcpy(p, value.data(), val_size);
  assert((p + val_size) - buf == encoded_len);
  table_.insert(buf);
}

bool memtable::get(const lookupkey& key, std::string* value, status* s) {
  slice memkey = key.memtable_key();
  table::iterator iter(&table_);
  iter.seek(memkey.data());
  if (iter.valid()) {
    // entry format is:
    //    klength  varint32
    //    userkey  char[klength]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // check that it belongs to same user key.  we do not check the
    // sequence number since the seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter.key();
    uint32_t key_length;
    const char* key_ptr = getvarint32ptr(entry, entry+5, &key_length);
    if (comparator_.comparator.user_comparator()->compare(
            slice(key_ptr, key_length - 8),
            key.user_key()) == 0) {
      // correct user key
      const uint64_t tag = decodefixed64(key_ptr + key_length - 8);
      switch (static_cast<valuetype>(tag & 0xff)) {
        case ktypevalue: {
          slice v = getlengthprefixedslice(key_ptr + key_length);
          value->assign(v.data(), v.size());
          return true;
        }
        case ktypedeletion:
          *s = status::notfound(slice());
          return true;
      }
    }
  }
  return false;
}

}  // namespace leveldb
