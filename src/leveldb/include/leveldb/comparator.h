// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_include_comparator_h_
#define storage_leveldb_include_comparator_h_

#include <string>

namespace leveldb {

class slice;

// a comparator object provides a total order across slices that are
// used as keys in an sstable or a database.  a comparator implementation
// must be thread-safe since leveldb may invoke its methods concurrently
// from multiple threads.
class comparator {
 public:
  virtual ~comparator();

  // three-way comparison.  returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  virtual int compare(const slice& a, const slice& b) const = 0;

  // the name of the comparator.  used to check for comparator
  // mismatches (i.e., a db created with one comparator is
  // accessed using a different comparator.
  //
  // the client of this package should switch to a new name whenever
  // the comparator implementation changes in a way that will cause
  // the relative ordering of any two keys to change.
  //
  // names starting with "leveldb." are reserved and should not be used
  // by any clients of this package.
  virtual const char* name() const = 0;

  // advanced functions: these are used to reduce the space requirements
  // for internal data structures like index blocks.

  // if *start < limit, changes *start to a short string in [start,limit).
  // simple comparator implementations may return with *start unchanged,
  // i.e., an implementation of this method that does nothing is correct.
  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const = 0;

  // changes *key to a short string >= *key.
  // simple comparator implementations may return with *key unchanged,
  // i.e., an implementation of this method that does nothing is correct.
  virtual void findshortsuccessor(std::string* key) const = 0;
};

// return a builtin comparator that uses lexicographic byte-wise
// ordering.  the result remains the property of this module and
// must not be deleted.
extern const comparator* bytewisecomparator();

}  // namespace leveldb

#endif  // storage_leveldb_include_comparator_h_
