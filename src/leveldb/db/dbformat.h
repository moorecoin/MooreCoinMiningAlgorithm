// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_dbformat_h_
#define storage_leveldb_db_dbformat_h_

#include <stdio.h>
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

// grouping of constants.  we may want to make some of these
// parameters set via options.
namespace config {
static const int knumlevels = 7;

// level-0 compaction is started when we hit this many files.
static const int kl0_compactiontrigger = 4;

// soft limit on number of level-0 files.  we slow down writes at this point.
static const int kl0_slowdownwritestrigger = 8;

// maximum number of level-0 files.  we stop writes at this point.
static const int kl0_stopwritestrigger = 12;

// maximum level to which a new compacted memtable is pushed if it
// does not create overlap.  we try to push to level 2 to avoid the
// relatively expensive level 0=>1 compactions and to avoid some
// expensive manifest file operations.  we do not push all the way to
// the largest level since that can generate a lot of wasted disk
// space if the same key space is being repeatedly overwritten.
static const int kmaxmemcompactlevel = 2;

// approximate gap in bytes between samples of data read during iteration.
static const int kreadbytesperiod = 1048576;

}  // namespace config

class internalkey;

// value types encoded as the last component of internal keys.
// do not change these enum values: they are embedded in the on-disk
// data structures.
enum valuetype {
  ktypedeletion = 0x0,
  ktypevalue = 0x1
};
// kvaluetypeforseek defines the valuetype that should be passed when
// constructing a parsedinternalkey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// valuetype, not the lowest).
static const valuetype kvaluetypeforseek = ktypevalue;

typedef uint64_t sequencenumber;

// we leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
static const sequencenumber kmaxsequencenumber =
    ((0x1ull << 56) - 1);

struct parsedinternalkey {
  slice user_key;
  sequencenumber sequence;
  valuetype type;

  parsedinternalkey() { }  // intentionally left uninitialized (for speed)
  parsedinternalkey(const slice& u, const sequencenumber& seq, valuetype t)
      : user_key(u), sequence(seq), type(t) { }
  std::string debugstring() const;
};

// return the length of the encoding of "key".
inline size_t internalkeyencodinglength(const parsedinternalkey& key) {
  return key.user_key.size() + 8;
}

// append the serialization of "key" to *result.
extern void appendinternalkey(std::string* result,
                              const parsedinternalkey& key);

// attempt to parse an internal key from "internal_key".  on success,
// stores the parsed data in "*result", and returns true.
//
// on error, returns false, leaves "*result" in an undefined state.
extern bool parseinternalkey(const slice& internal_key,
                             parsedinternalkey* result);

// returns the user key portion of an internal key.
inline slice extractuserkey(const slice& internal_key) {
  assert(internal_key.size() >= 8);
  return slice(internal_key.data(), internal_key.size() - 8);
}

inline valuetype extractvaluetype(const slice& internal_key) {
  assert(internal_key.size() >= 8);
  const size_t n = internal_key.size();
  uint64_t num = decodefixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  return static_cast<valuetype>(c);
}

// a comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class internalkeycomparator : public comparator {
 private:
  const comparator* user_comparator_;
 public:
  explicit internalkeycomparator(const comparator* c) : user_comparator_(c) { }
  virtual const char* name() const;
  virtual int compare(const slice& a, const slice& b) const;
  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const;
  virtual void findshortsuccessor(std::string* key) const;

  const comparator* user_comparator() const { return user_comparator_; }

  int compare(const internalkey& a, const internalkey& b) const;
};

// filter policy wrapper that converts from internal keys to user keys
class internalfilterpolicy : public filterpolicy {
 private:
  const filterpolicy* const user_policy_;
 public:
  explicit internalfilterpolicy(const filterpolicy* p) : user_policy_(p) { }
  virtual const char* name() const;
  virtual void createfilter(const slice* keys, int n, std::string* dst) const;
  virtual bool keymaymatch(const slice& key, const slice& filter) const;
};

// modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an internalkeycomparator.
class internalkey {
 private:
  std::string rep_;
 public:
  internalkey() { }   // leave rep_ as empty to indicate it is invalid
  internalkey(const slice& user_key, sequencenumber s, valuetype t) {
    appendinternalkey(&rep_, parsedinternalkey(user_key, s, t));
  }

  void decodefrom(const slice& s) { rep_.assign(s.data(), s.size()); }
  slice encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  slice user_key() const { return extractuserkey(rep_); }

  void setfrom(const parsedinternalkey& p) {
    rep_.clear();
    appendinternalkey(&rep_, p);
  }

  void clear() { rep_.clear(); }

  std::string debugstring() const;
};

inline int internalkeycomparator::compare(
    const internalkey& a, const internalkey& b) const {
  return compare(a.encode(), b.encode());
}

inline bool parseinternalkey(const slice& internal_key,
                             parsedinternalkey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = decodefixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<valuetype>(c);
  result->user_key = slice(internal_key.data(), n - 8);
  return (c <= static_cast<unsigned char>(ktypevalue));
}

// a helper class useful for dbimpl::get()
class lookupkey {
 public:
  // initialize *this for looking up user_key at a snapshot with
  // the specified sequence number.
  lookupkey(const slice& user_key, sequencenumber sequence);

  ~lookupkey();

  // return a key suitable for lookup in a memtable.
  slice memtable_key() const { return slice(start_, end_ - start_); }

  // return an internal key (suitable for passing to an internal iterator)
  slice internal_key() const { return slice(kstart_, end_ - kstart_); }

  // return the user key
  slice user_key() const { return slice(kstart_, end_ - kstart_ - 8); }

 private:
  // we construct a char array of the form:
  //    klength  varint32               <-- start_
  //    userkey  char[klength]          <-- kstart_
  //    tag      uint64
  //                                    <-- end_
  // the array is a suitable memtable key.
  // the suffix starting with "userkey" can be used as an internalkey.
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];      // avoid allocation for short keys

  // no copying allowed
  lookupkey(const lookupkey&);
  void operator=(const lookupkey&);
};

inline lookupkey::~lookupkey() {
  if (start_ != space_) delete[] start_;
}

}  // namespace leveldb

#endif  // storage_leveldb_db_dbformat_h_
