// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_include_db_h_
#define storage_leveldb_include_db_h_

#include <stdint.h>
#include <stdio.h>
#include "leveldb/iterator.h"
#include "leveldb/options.h"

namespace leveldb {

// update makefile if you change these
static const int kmajorversion = 1;
static const int kminorversion = 18;

struct options;
struct readoptions;
struct writeoptions;
class writebatch;

// abstract handle to particular state of a db.
// a snapshot is an immutable object and can therefore be safely
// accessed from multiple threads without any external synchronization.
class snapshot {
 protected:
  virtual ~snapshot();
};

// a range of keys
struct range {
  slice start;          // included in the range
  slice limit;          // not included in the range

  range() { }
  range(const slice& s, const slice& l) : start(s), limit(l) { }
};

// a db is a persistent ordered map from keys to values.
// a db is safe for concurrent access from multiple threads without
// any external synchronization.
class db {
 public:
  // open the database with the specified "name".
  // stores a pointer to a heap-allocated database in *dbptr and returns
  // ok on success.
  // stores null in *dbptr and returns a non-ok status on error.
  // caller should delete *dbptr when it is no longer needed.
  static status open(const options& options,
                     const std::string& name,
                     db** dbptr);

  db() { }
  virtual ~db();

  // set the database entry for "key" to "value".  returns ok on success,
  // and a non-ok status on error.
  // note: consider setting options.sync = true.
  virtual status put(const writeoptions& options,
                     const slice& key,
                     const slice& value) = 0;

  // remove the database entry (if any) for "key".  returns ok on
  // success, and a non-ok status on error.  it is not an error if "key"
  // did not exist in the database.
  // note: consider setting options.sync = true.
  virtual status delete(const writeoptions& options, const slice& key) = 0;

  // apply the specified updates to the database.
  // returns ok on success, non-ok on failure.
  // note: consider setting options.sync = true.
  virtual status write(const writeoptions& options, writebatch* updates) = 0;

  // if the database contains an entry for "key" store the
  // corresponding value in *value and return ok.
  //
  // if there is no entry for "key" leave *value unchanged and return
  // a status for which status::isnotfound() returns true.
  //
  // may return some other status on an error.
  virtual status get(const readoptions& options,
                     const slice& key, std::string* value) = 0;

  // return a heap-allocated iterator over the contents of the database.
  // the result of newiterator() is initially invalid (caller must
  // call one of the seek methods on the iterator before using it).
  //
  // caller should delete the iterator when it is no longer needed.
  // the returned iterator should be deleted before this db is deleted.
  virtual iterator* newiterator(const readoptions& options) = 0;

  // return a handle to the current db state.  iterators created with
  // this handle will all observe a stable snapshot of the current db
  // state.  the caller must call releasesnapshot(result) when the
  // snapshot is no longer needed.
  virtual const snapshot* getsnapshot() = 0;

  // release a previously acquired snapshot.  the caller must not
  // use "snapshot" after this call.
  virtual void releasesnapshot(const snapshot* snapshot) = 0;

  // db implementations can export properties about their state
  // via this method.  if "property" is a valid property understood by this
  // db implementation, fills "*value" with its current value and returns
  // true.  otherwise returns false.
  //
  //
  // valid property names include:
  //
  //  "leveldb.num-files-at-level<n>" - return the number of files at level <n>,
  //     where <n> is an ascii representation of a level number (e.g. "0").
  //  "leveldb.stats" - returns a multi-line string that describes statistics
  //     about the internal operation of the db.
  //  "leveldb.sstables" - returns a multi-line string that describes all
  //     of the sstables that make up the db contents.
  virtual bool getproperty(const slice& property, std::string* value) = 0;

  // for each i in [0,n-1], store in "sizes[i]", the approximate
  // file system space used by keys in "[range[i].start .. range[i].limit)".
  //
  // note that the returned sizes measure file system space usage, so
  // if the user data compresses by a factor of ten, the returned
  // sizes will be one-tenth the size of the corresponding user data size.
  //
  // the results may not include the sizes of recently written data.
  virtual void getapproximatesizes(const range* range, int n,
                                   uint64_t* sizes) = 0;

  // compact the underlying storage for the key range [*begin,*end].
  // in particular, deleted and overwritten versions are discarded,
  // and the data is rearranged to reduce the cost of operations
  // needed to access the data.  this operation should typically only
  // be invoked by users who understand the underlying implementation.
  //
  // begin==null is treated as a key before all keys in the database.
  // end==null is treated as a key after all keys in the database.
  // therefore the following call will compact the entire database:
  //    db->compactrange(null, null);
  virtual void compactrange(const slice* begin, const slice* end) = 0;

 private:
  // no copying allowed
  db(const db&);
  void operator=(const db&);
};

// destroy the contents of the specified database.
// be very careful using this method.
status destroydb(const std::string& name, const options& options);

// if a db cannot be opened, you may attempt to call this method to
// resurrect as much of the contents of the database as possible.
// some data may be lost, so be careful when calling this function
// on a database that contains important information.
status repairdb(const std::string& dbname, const options& options);

}  // namespace leveldb

#endif  // storage_leveldb_include_db_h_
