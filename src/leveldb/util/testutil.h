// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_util_testutil_h_
#define storage_leveldb_util_testutil_h_

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "util/random.h"

namespace leveldb {
namespace test {

// store in *dst a random string of length "len" and return a slice that
// references the generated data.
extern slice randomstring(random* rnd, int len, std::string* dst);

// return a random key with the specified length that may contain interesting
// characters (e.g. \x00, \xff, etc.).
extern std::string randomkey(random* rnd, int len);

// store in *dst a string of length "len" that will compress to
// "n*compressed_fraction" bytes and return a slice that references
// the generated data.
extern slice compressiblestring(random* rnd, double compressed_fraction,
                                size_t len, std::string* dst);

// a wrapper that allows injection of errors.
class errorenv : public envwrapper {
 public:
  bool writable_file_error_;
  int num_writable_file_errors_;

  errorenv() : envwrapper(env::default()),
               writable_file_error_(false),
               num_writable_file_errors_(0) { }

  virtual status newwritablefile(const std::string& fname,
                                 writablefile** result) {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = null;
      return status::ioerror(fname, "fake error");
    }
    return target()->newwritablefile(fname, result);
  }
};

}  // namespace test
}  // namespace leveldb

#endif  // storage_leveldb_util_testutil_h_
