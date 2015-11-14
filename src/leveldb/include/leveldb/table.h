// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_include_table_h_
#define storage_leveldb_include_table_h_

#include <stdint.h>
#include "leveldb/iterator.h"

namespace leveldb {

class block;
class blockhandle;
class footer;
struct options;
class randomaccessfile;
struct readoptions;
class tablecache;

// a table is a sorted map from strings to strings.  tables are
// immutable and persistent.  a table may be safely accessed from
// multiple threads without external synchronization.
class table {
 public:
  // attempt to open the table that is stored in bytes [0..file_size)
  // of "file", and read the metadata entries necessary to allow
  // retrieving data from the table.
  //
  // if successful, returns ok and sets "*table" to the newly opened
  // table.  the client should delete "*table" when no longer needed.
  // if there was an error while initializing the table, sets "*table"
  // to null and returns a non-ok status.  does not take ownership of
  // "*source", but the client must ensure that "source" remains live
  // for the duration of the returned table's lifetime.
  //
  // *file must remain live while this table is in use.
  static status open(const options& options,
                     randomaccessfile* file,
                     uint64_t file_size,
                     table** table);

  ~table();

  // returns a new iterator over the table contents.
  // the result of newiterator() is initially invalid (caller must
  // call one of the seek methods on the iterator before using it).
  iterator* newiterator(const readoptions&) const;

  // given a key, return an approximate byte offset in the file where
  // the data for that key begins (or would begin if the key were
  // present in the file).  the returned value is in terms of file
  // bytes, and so includes effects like compression of the underlying data.
  // e.g., the approximate offset of the last key in the table will
  // be close to the file length.
  uint64_t approximateoffsetof(const slice& key) const;

 private:
  struct rep;
  rep* rep_;

  explicit table(rep* rep) { rep_ = rep; }
  static iterator* blockreader(void*, const readoptions&, const slice&);

  // calls (*handle_result)(arg, ...) with the entry found after a call
  // to seek(key).  may not make such a call if filter policy says
  // that key is not present.
  friend class tablecache;
  status internalget(
      const readoptions&, const slice& key,
      void* arg,
      void (*handle_result)(void* arg, const slice& k, const slice& v));


  void readmeta(const footer& footer);
  void readfilter(const slice& filter_handle_value);

  // no copying allowed
  table(const table&);
  void operator=(const table&);
};

}  // namespace leveldb

#endif  // storage_leveldb_include_table_h_
