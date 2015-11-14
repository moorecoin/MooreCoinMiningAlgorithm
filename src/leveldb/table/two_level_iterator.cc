// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/two_level_iterator.h"

#include "leveldb/table.h"
#include "table/block.h"
#include "table/format.h"
#include "table/iterator_wrapper.h"

namespace leveldb {

namespace {

typedef iterator* (*blockfunction)(void*, const readoptions&, const slice&);

class twoleveliterator: public iterator {
 public:
  twoleveliterator(
    iterator* index_iter,
    blockfunction block_function,
    void* arg,
    const readoptions& options);

  virtual ~twoleveliterator();

  virtual void seek(const slice& target);
  virtual void seektofirst();
  virtual void seektolast();
  virtual void next();
  virtual void prev();

  virtual bool valid() const {
    return data_iter_.valid();
  }
  virtual slice key() const {
    assert(valid());
    return data_iter_.key();
  }
  virtual slice value() const {
    assert(valid());
    return data_iter_.value();
  }
  virtual status status() const {
    // it'd be nice if status() returned a const status& instead of a status
    if (!index_iter_.status().ok()) {
      return index_iter_.status();
    } else if (data_iter_.iter() != null && !data_iter_.status().ok()) {
      return data_iter_.status();
    } else {
      return status_;
    }
  }

 private:
  void saveerror(const status& s) {
    if (status_.ok() && !s.ok()) status_ = s;
  }
  void skipemptydatablocksforward();
  void skipemptydatablocksbackward();
  void setdataiterator(iterator* data_iter);
  void initdatablock();

  blockfunction block_function_;
  void* arg_;
  const readoptions options_;
  status status_;
  iteratorwrapper index_iter_;
  iteratorwrapper data_iter_; // may be null
  // if data_iter_ is non-null, then "data_block_handle_" holds the
  // "index_value" passed to block_function_ to create the data_iter_.
  std::string data_block_handle_;
};

twoleveliterator::twoleveliterator(
    iterator* index_iter,
    blockfunction block_function,
    void* arg,
    const readoptions& options)
    : block_function_(block_function),
      arg_(arg),
      options_(options),
      index_iter_(index_iter),
      data_iter_(null) {
}

twoleveliterator::~twoleveliterator() {
}

void twoleveliterator::seek(const slice& target) {
  index_iter_.seek(target);
  initdatablock();
  if (data_iter_.iter() != null) data_iter_.seek(target);
  skipemptydatablocksforward();
}

void twoleveliterator::seektofirst() {
  index_iter_.seektofirst();
  initdatablock();
  if (data_iter_.iter() != null) data_iter_.seektofirst();
  skipemptydatablocksforward();
}

void twoleveliterator::seektolast() {
  index_iter_.seektolast();
  initdatablock();
  if (data_iter_.iter() != null) data_iter_.seektolast();
  skipemptydatablocksbackward();
}

void twoleveliterator::next() {
  assert(valid());
  data_iter_.next();
  skipemptydatablocksforward();
}

void twoleveliterator::prev() {
  assert(valid());
  data_iter_.prev();
  skipemptydatablocksbackward();
}


void twoleveliterator::skipemptydatablocksforward() {
  while (data_iter_.iter() == null || !data_iter_.valid()) {
    // move to next block
    if (!index_iter_.valid()) {
      setdataiterator(null);
      return;
    }
    index_iter_.next();
    initdatablock();
    if (data_iter_.iter() != null) data_iter_.seektofirst();
  }
}

void twoleveliterator::skipemptydatablocksbackward() {
  while (data_iter_.iter() == null || !data_iter_.valid()) {
    // move to next block
    if (!index_iter_.valid()) {
      setdataiterator(null);
      return;
    }
    index_iter_.prev();
    initdatablock();
    if (data_iter_.iter() != null) data_iter_.seektolast();
  }
}

void twoleveliterator::setdataiterator(iterator* data_iter) {
  if (data_iter_.iter() != null) saveerror(data_iter_.status());
  data_iter_.set(data_iter);
}

void twoleveliterator::initdatablock() {
  if (!index_iter_.valid()) {
    setdataiterator(null);
  } else {
    slice handle = index_iter_.value();
    if (data_iter_.iter() != null && handle.compare(data_block_handle_) == 0) {
      // data_iter_ is already constructed with this iterator, so
      // no need to change anything
    } else {
      iterator* iter = (*block_function_)(arg_, options_, handle);
      data_block_handle_.assign(handle.data(), handle.size());
      setdataiterator(iter);
    }
  }
}

}  // namespace

iterator* newtwoleveliterator(
    iterator* index_iter,
    blockfunction block_function,
    void* arg,
    const readoptions& options) {
  return new twoleveliterator(index_iter, block_function, arg, options);
}

}  // namespace leveldb
