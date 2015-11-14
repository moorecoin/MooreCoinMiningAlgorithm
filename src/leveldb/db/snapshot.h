// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_snapshot_h_
#define storage_leveldb_db_snapshot_h_

#include "leveldb/db.h"

namespace leveldb {

class snapshotlist;

// snapshots are kept in a doubly-linked list in the db.
// each snapshotimpl corresponds to a particular sequence number.
class snapshotimpl : public snapshot {
 public:
  sequencenumber number_;  // const after creation

 private:
  friend class snapshotlist;

  // snapshotimpl is kept in a doubly-linked circular list
  snapshotimpl* prev_;
  snapshotimpl* next_;

  snapshotlist* list_;                 // just for sanity checks
};

class snapshotlist {
 public:
  snapshotlist() {
    list_.prev_ = &list_;
    list_.next_ = &list_;
  }

  bool empty() const { return list_.next_ == &list_; }
  snapshotimpl* oldest() const { assert(!empty()); return list_.next_; }
  snapshotimpl* newest() const { assert(!empty()); return list_.prev_; }

  const snapshotimpl* new(sequencenumber seq) {
    snapshotimpl* s = new snapshotimpl;
    s->number_ = seq;
    s->list_ = this;
    s->next_ = &list_;
    s->prev_ = list_.prev_;
    s->prev_->next_ = s;
    s->next_->prev_ = s;
    return s;
  }

  void delete(const snapshotimpl* s) {
    assert(s->list_ == this);
    s->prev_->next_ = s->next_;
    s->next_->prev_ = s->prev_;
    delete s;
  }

 private:
  // dummy head of doubly-linked list of snapshots
  snapshotimpl list_;
};

}  // namespace leveldb

#endif  // storage_leveldb_db_snapshot_h_
