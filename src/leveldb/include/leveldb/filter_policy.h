// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a database can be configured with a custom filterpolicy object.
// this object is responsible for creating a small filter from a set
// of keys.  these filters are stored in leveldb and are consulted
// automatically by leveldb to decide whether or not to read some
// information from disk. in many cases, a filter can cut down the
// number of disk seeks form a handful to a single disk seek per
// db::get() call.
//
// most people will want to use the builtin bloom filter support (see
// newbloomfilterpolicy() below).

#ifndef storage_leveldb_include_filter_policy_h_
#define storage_leveldb_include_filter_policy_h_

#include <string>

namespace leveldb {

class slice;

class filterpolicy {
 public:
  virtual ~filterpolicy();

  // return the name of this policy.  note that if the filter encoding
  // changes in an incompatible way, the name returned by this method
  // must be changed.  otherwise, old incompatible filters may be
  // passed to methods of this type.
  virtual const char* name() const = 0;

  // keys[0,n-1] contains a list of keys (potentially with duplicates)
  // that are ordered according to the user supplied comparator.
  // append a filter that summarizes keys[0,n-1] to *dst.
  //
  // warning: do not change the initial contents of *dst.  instead,
  // append the newly constructed filter to *dst.
  virtual void createfilter(const slice* keys, int n, std::string* dst)
      const = 0;

  // "filter" contains the data appended by a preceding call to
  // createfilter() on this class.  this method must return true if
  // the key was in the list of keys passed to createfilter().
  // this method may return true or false if the key was not on the
  // list, but it should aim to return false with a high probability.
  virtual bool keymaymatch(const slice& key, const slice& filter) const = 0;
};

// return a new filter policy that uses a bloom filter with approximately
// the specified number of bits per key.  a good value for bits_per_key
// is 10, which yields a filter with ~ 1% false positive rate.
//
// callers must delete the result after any database that is using the
// result has been closed.
//
// note: if you are using a custom comparator that ignores some parts
// of the keys being compared, you must not use newbloomfilterpolicy()
// and must provide your own filterpolicy that also ignores the
// corresponding parts of the keys.  for example, if the comparator
// ignores trailing spaces, it would be incorrect to use a
// filterpolicy (like newbloomfilterpolicy) that does not ignore
// trailing spaces in keys.
extern const filterpolicy* newbloomfilterpolicy(int bits_per_key);

}

#endif  // storage_leveldb_include_filter_policy_h_
