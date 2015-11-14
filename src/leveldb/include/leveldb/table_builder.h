// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// tablebuilder provides the interface used to build a table
// (an immutable and sorted map from keys to values).
//
// multiple threads can invoke const methods on a tablebuilder without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same tablebuilder must use
// external synchronization.

#ifndef storage_leveldb_include_table_builder_h_
#define storage_leveldb_include_table_builder_h_

#include <stdint.h>
#include "leveldb/options.h"
#include "leveldb/status.h"

namespace leveldb {

class blockbuilder;
class blockhandle;
class writablefile;

class tablebuilder {
 public:
  // create a builder that will store the contents of the table it is
  // building in *file.  does not close the file.  it is up to the
  // caller to close the file after calling finish().
  tablebuilder(const options& options, writablefile* file);

  // requires: either finish() or abandon() has been called.
  ~tablebuilder();

  // change the options used by this builder.  note: only some of the
  // option fields can be changed after construction.  if a field is
  // not allowed to change dynamically and its value in the structure
  // passed to the constructor is different from its value in the
  // structure passed to this method, this method will return an error
  // without changing any fields.
  status changeoptions(const options& options);

  // add key,value to the table being constructed.
  // requires: key is after any previously added key according to comparator.
  // requires: finish(), abandon() have not been called
  void add(const slice& key, const slice& value);

  // advanced operation: flush any buffered key/value pairs to file.
  // can be used to ensure that two adjacent entries never live in
  // the same data block.  most clients should not need to use this method.
  // requires: finish(), abandon() have not been called
  void flush();

  // return non-ok iff some error has been detected.
  status status() const;

  // finish building the table.  stops using the file passed to the
  // constructor after this function returns.
  // requires: finish(), abandon() have not been called
  status finish();

  // indicate that the contents of this builder should be abandoned.  stops
  // using the file passed to the constructor after this function returns.
  // if the caller is not going to call finish(), it must call abandon()
  // before destroying this builder.
  // requires: finish(), abandon() have not been called
  void abandon();

  // number of calls to add() so far.
  uint64_t numentries() const;

  // size of the file generated so far.  if invoked after a successful
  // finish() call, returns the size of the final generated file.
  uint64_t filesize() const;

 private:
  bool ok() const { return status().ok(); }
  void writeblock(blockbuilder* block, blockhandle* handle);
  void writerawblock(const slice& data, compressiontype, blockhandle* handle);

  struct rep;
  rep* rep_;

  // no copying allowed
  tablebuilder(const tablebuilder&);
  void operator=(const tablebuilder&);
};

}  // namespace leveldb

#endif  // storage_leveldb_include_table_builder_h_
