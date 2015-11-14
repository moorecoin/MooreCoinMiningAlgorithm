// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/db_iter.h"

#include "db/filename.h"
#include "db/db_impl.h"
#include "db/dbformat.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/random.h"

namespace leveldb {

#if 0
static void dumpinternaliter(iterator* iter) {
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    parsedinternalkey k;
    if (!parseinternalkey(iter->key(), &k)) {
      fprintf(stderr, "corrupt '%s'\n", escapestring(iter->key()).c_str());
    } else {
      fprintf(stderr, "@ '%s'\n", k.debugstring().c_str());
    }
  }
}
#endif

namespace {

// memtables and sstables that make the db representation contain
// (userkey,seq,type) => uservalue entries.  dbiter
// combines multiple entries for the same userkey found in the db
// representation into a single entry while accounting for sequence
// numbers, deletion markers, overwrites, etc.
class dbiter: public iterator {
 public:
  // which direction is the iterator currently moving?
  // (1) when moving forward, the internal iterator is positioned at
  //     the exact entry that yields this->key(), this->value()
  // (2) when moving backwards, the internal iterator is positioned
  //     just before all entries whose user key == this->key().
  enum direction {
    kforward,
    kreverse
  };

  dbiter(dbimpl* db, const comparator* cmp, iterator* iter, sequencenumber s,
         uint32_t seed)
      : db_(db),
        user_comparator_(cmp),
        iter_(iter),
        sequence_(s),
        direction_(kforward),
        valid_(false),
        rnd_(seed),
        bytes_counter_(randomperiod()) {
  }
  virtual ~dbiter() {
    delete iter_;
  }
  virtual bool valid() const { return valid_; }
  virtual slice key() const {
    assert(valid_);
    return (direction_ == kforward) ? extractuserkey(iter_->key()) : saved_key_;
  }
  virtual slice value() const {
    assert(valid_);
    return (direction_ == kforward) ? iter_->value() : saved_value_;
  }
  virtual status status() const {
    if (status_.ok()) {
      return iter_->status();
    } else {
      return status_;
    }
  }

  virtual void next();
  virtual void prev();
  virtual void seek(const slice& target);
  virtual void seektofirst();
  virtual void seektolast();

 private:
  void findnextuserentry(bool skipping, std::string* skip);
  void findprevuserentry();
  bool parsekey(parsedinternalkey* key);

  inline void savekey(const slice& k, std::string* dst) {
    dst->assign(k.data(), k.size());
  }

  inline void clearsavedvalue() {
    if (saved_value_.capacity() > 1048576) {
      std::string empty;
      swap(empty, saved_value_);
    } else {
      saved_value_.clear();
    }
  }

  // pick next gap with average value of config::kreadbytesperiod.
  ssize_t randomperiod() {
    return rnd_.uniform(2*config::kreadbytesperiod);
  }

  dbimpl* db_;
  const comparator* const user_comparator_;
  iterator* const iter_;
  sequencenumber const sequence_;

  status status_;
  std::string saved_key_;     // == current key when direction_==kreverse
  std::string saved_value_;   // == current raw value when direction_==kreverse
  direction direction_;
  bool valid_;

  random rnd_;
  ssize_t bytes_counter_;

  // no copying allowed
  dbiter(const dbiter&);
  void operator=(const dbiter&);
};

inline bool dbiter::parsekey(parsedinternalkey* ikey) {
  slice k = iter_->key();
  ssize_t n = k.size() + iter_->value().size();
  bytes_counter_ -= n;
  while (bytes_counter_ < 0) {
    bytes_counter_ += randomperiod();
    db_->recordreadsample(k);
  }
  if (!parseinternalkey(k, ikey)) {
    status_ = status::corruption("corrupted internal key in dbiter");
    return false;
  } else {
    return true;
  }
}

void dbiter::next() {
  assert(valid_);

  if (direction_ == kreverse) {  // switch directions?
    direction_ = kforward;
    // iter_ is pointing just before the entries for this->key(),
    // so advance into the range of entries for this->key() and then
    // use the normal skipping code below.
    if (!iter_->valid()) {
      iter_->seektofirst();
    } else {
      iter_->next();
    }
    if (!iter_->valid()) {
      valid_ = false;
      saved_key_.clear();
      return;
    }
    // saved_key_ already contains the key to skip past.
  } else {
    // store in saved_key_ the current key so we skip it below.
    savekey(extractuserkey(iter_->key()), &saved_key_);
  }

  findnextuserentry(true, &saved_key_);
}

void dbiter::findnextuserentry(bool skipping, std::string* skip) {
  // loop until we hit an acceptable entry to yield
  assert(iter_->valid());
  assert(direction_ == kforward);
  do {
    parsedinternalkey ikey;
    if (parsekey(&ikey) && ikey.sequence <= sequence_) {
      switch (ikey.type) {
        case ktypedeletion:
          // arrange to skip all upcoming entries for this key since
          // they are hidden by this deletion.
          savekey(ikey.user_key, skip);
          skipping = true;
          break;
        case ktypevalue:
          if (skipping &&
              user_comparator_->compare(ikey.user_key, *skip) <= 0) {
            // entry hidden
          } else {
            valid_ = true;
            saved_key_.clear();
            return;
          }
          break;
      }
    }
    iter_->next();
  } while (iter_->valid());
  saved_key_.clear();
  valid_ = false;
}

void dbiter::prev() {
  assert(valid_);

  if (direction_ == kforward) {  // switch directions?
    // iter_ is pointing at the current entry.  scan backwards until
    // the key changes so we can use the normal reverse scanning code.
    assert(iter_->valid());  // otherwise valid_ would have been false
    savekey(extractuserkey(iter_->key()), &saved_key_);
    while (true) {
      iter_->prev();
      if (!iter_->valid()) {
        valid_ = false;
        saved_key_.clear();
        clearsavedvalue();
        return;
      }
      if (user_comparator_->compare(extractuserkey(iter_->key()),
                                    saved_key_) < 0) {
        break;
      }
    }
    direction_ = kreverse;
  }

  findprevuserentry();
}

void dbiter::findprevuserentry() {
  assert(direction_ == kreverse);

  valuetype value_type = ktypedeletion;
  if (iter_->valid()) {
    do {
      parsedinternalkey ikey;
      if (parsekey(&ikey) && ikey.sequence <= sequence_) {
        if ((value_type != ktypedeletion) &&
            user_comparator_->compare(ikey.user_key, saved_key_) < 0) {
          // we encountered a non-deleted value in entries for previous keys,
          break;
        }
        value_type = ikey.type;
        if (value_type == ktypedeletion) {
          saved_key_.clear();
          clearsavedvalue();
        } else {
          slice raw_value = iter_->value();
          if (saved_value_.capacity() > raw_value.size() + 1048576) {
            std::string empty;
            swap(empty, saved_value_);
          }
          savekey(extractuserkey(iter_->key()), &saved_key_);
          saved_value_.assign(raw_value.data(), raw_value.size());
        }
      }
      iter_->prev();
    } while (iter_->valid());
  }

  if (value_type == ktypedeletion) {
    // end
    valid_ = false;
    saved_key_.clear();
    clearsavedvalue();
    direction_ = kforward;
  } else {
    valid_ = true;
  }
}

void dbiter::seek(const slice& target) {
  direction_ = kforward;
  clearsavedvalue();
  saved_key_.clear();
  appendinternalkey(
      &saved_key_, parsedinternalkey(target, sequence_, kvaluetypeforseek));
  iter_->seek(saved_key_);
  if (iter_->valid()) {
    findnextuserentry(false, &saved_key_ /* temporary storage */);
  } else {
    valid_ = false;
  }
}

void dbiter::seektofirst() {
  direction_ = kforward;
  clearsavedvalue();
  iter_->seektofirst();
  if (iter_->valid()) {
    findnextuserentry(false, &saved_key_ /* temporary storage */);
  } else {
    valid_ = false;
  }
}

void dbiter::seektolast() {
  direction_ = kreverse;
  clearsavedvalue();
  iter_->seektolast();
  findprevuserentry();
}

}  // anonymous namespace

iterator* newdbiterator(
    dbimpl* db,
    const comparator* user_key_comparator,
    iterator* internal_iter,
    sequencenumber sequence,
    uint32_t seed) {
  return new dbiter(db, user_key_comparator, internal_iter, sequence, seed);
}

}  // namespace leveldb
