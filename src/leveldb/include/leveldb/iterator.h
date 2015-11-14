// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// an iterator yields a sequence of key/value pairs from a source.
// the following class defines the interface.  multiple implementations
// are provided by this library.  in particular, iterators are provided
// to access the contents of a table or a db.
//
// multiple threads can invoke const methods on an iterator without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same iterator must use
// external synchronization.

#ifndef storage_leveldb_include_iterator_h_
#define storage_leveldb_include_iterator_h_

#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class iterator {
 public:
  iterator();
  virtual ~iterator();

  // an iterator is either positioned at a key/value pair, or
  // not valid.  this method returns true iff the iterator is valid.
  virtual bool valid() const = 0;

  // position at the first key in the source.  the iterator is valid()
  // after this call iff the source is not empty.
  virtual void seektofirst() = 0;

  // position at the last key in the source.  the iterator is
  // valid() after this call iff the source is not empty.
  virtual void seektolast() = 0;

  // position at the first key in the source that at or past target
  // the iterator is valid() after this call iff the source contains
  // an entry that comes at or past target.
  virtual void seek(const slice& target) = 0;

  // moves to the next entry in the source.  after this call, valid() is
  // true iff the iterator was not positioned at the last entry in the source.
  // requires: valid()
  virtual void next() = 0;

  // moves to the previous entry in the source.  after this call, valid() is
  // true iff the iterator was not positioned at the first entry in source.
  // requires: valid()
  virtual void prev() = 0;

  // return the key for the current entry.  the underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // requires: valid()
  virtual slice key() const = 0;

  // return the value for the current entry.  the underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // requires: valid()
  virtual slice value() const = 0;

  // if an error has occurred, return it.  else return an ok status.
  virtual status status() const = 0;

  // clients are allowed to register function/arg1/arg2 triples that
  // will be invoked when this iterator is destroyed.
  //
  // note that unlike all of the preceding methods, this method is
  // not abstract and therefore clients should not override it.
  typedef void (*cleanupfunction)(void* arg1, void* arg2);
  void registercleanup(cleanupfunction function, void* arg1, void* arg2);

 private:
  struct cleanup {
    cleanupfunction function;
    void* arg1;
    void* arg2;
    cleanup* next;
  };
  cleanup cleanup_;

  // no copying allowed
  iterator(const iterator&);
  void operator=(const iterator&);
};

// return an empty iterator (yields nothing).
extern iterator* newemptyiterator();

// return an empty iterator with the specified status.
extern iterator* newerroriterator(const status& status);

}  // namespace leveldb

#endif  // storage_leveldb_include_iterator_h_
