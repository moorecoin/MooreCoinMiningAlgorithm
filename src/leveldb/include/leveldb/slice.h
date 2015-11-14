// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// slice is a simple structure containing a pointer into some external
// storage and a size.  the user of a slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// multiple threads can invoke const methods on a slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same slice must use
// external synchronization.

#ifndef storage_leveldb_include_slice_h_
#define storage_leveldb_include_slice_h_

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <string>

namespace leveldb {

class slice {
 public:
  // create an empty slice.
  slice() : data_(""), size_(0) { }

  // create a slice that refers to d[0,n-1].
  slice(const char* d, size_t n) : data_(d), size_(n) { }

  // create a slice that refers to the contents of "s"
  slice(const std::string& s) : data_(s.data()), size_(s.size()) { }

  // create a slice that refers to s[0,strlen(s)-1]
  slice(const char* s) : data_(s), size_(strlen(s)) { }

  // return a pointer to the beginning of the referenced data
  const char* data() const { return data_; }

  // return the length (in bytes) of the referenced data
  size_t size() const { return size_; }

  // return true iff the length of the referenced data is zero
  bool empty() const { return size_ == 0; }

  // return the ith byte in the referenced data.
  // requires: n < size()
  char operator[](size_t n) const {
    assert(n < size());
    return data_[n];
  }

  // change this slice to refer to an empty array
  void clear() { data_ = ""; size_ = 0; }

  // drop the first "n" bytes from this slice.
  void remove_prefix(size_t n) {
    assert(n <= size());
    data_ += n;
    size_ -= n;
  }

  // return a string that contains the copy of the referenced data.
  std::string tostring() const { return std::string(data_, size_); }

  // three-way comparison.  returns value:
  //   <  0 iff "*this" <  "b",
  //   == 0 iff "*this" == "b",
  //   >  0 iff "*this" >  "b"
  int compare(const slice& b) const;

  // return true iff "x" is a prefix of "*this"
  bool starts_with(const slice& x) const {
    return ((size_ >= x.size_) &&
            (memcmp(data_, x.data_, x.size_) == 0));
  }

 private:
  const char* data_;
  size_t size_;

  // intentionally copyable
};

inline bool operator==(const slice& x, const slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const slice& x, const slice& y) {
  return !(x == y);
}

inline int slice::compare(const slice& b) const {
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}

}  // namespace leveldb


#endif  // storage_leveldb_include_slice_h_
