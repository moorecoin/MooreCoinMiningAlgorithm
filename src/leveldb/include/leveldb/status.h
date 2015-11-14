// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a status encapsulates the result of an operation.  it may indicate success,
// or it may indicate an error with an associated error message.
//
// multiple threads can invoke const methods on a status without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same status must use
// external synchronization.

#ifndef storage_leveldb_include_status_h_
#define storage_leveldb_include_status_h_

#include <string>
#include "leveldb/slice.h"

namespace leveldb {

class status {
 public:
  // create a success status.
  status() : state_(null) { }
  ~status() { delete[] state_; }

  // copy the specified status.
  status(const status& s);
  void operator=(const status& s);

  // return a success status.
  static status ok() { return status(); }

  // return error status of an appropriate type.
  static status notfound(const slice& msg, const slice& msg2 = slice()) {
    return status(knotfound, msg, msg2);
  }
  static status corruption(const slice& msg, const slice& msg2 = slice()) {
    return status(kcorruption, msg, msg2);
  }
  static status notsupported(const slice& msg, const slice& msg2 = slice()) {
    return status(knotsupported, msg, msg2);
  }
  static status invalidargument(const slice& msg, const slice& msg2 = slice()) {
    return status(kinvalidargument, msg, msg2);
  }
  static status ioerror(const slice& msg, const slice& msg2 = slice()) {
    return status(kioerror, msg, msg2);
  }

  // returns true iff the status indicates success.
  bool ok() const { return (state_ == null); }

  // returns true iff the status indicates a notfound error.
  bool isnotfound() const { return code() == knotfound; }

  // returns true iff the status indicates a corruption error.
  bool iscorruption() const { return code() == kcorruption; }

  // returns true iff the status indicates an ioerror.
  bool isioerror() const { return code() == kioerror; }

  // return a string representation of this status suitable for printing.
  // returns the string "ok" for success.
  std::string tostring() const;

 private:
  // ok status has a null state_.  otherwise, state_ is a new[] array
  // of the following form:
  //    state_[0..3] == length of message
  //    state_[4]    == code
  //    state_[5..]  == message
  const char* state_;

  enum code {
    kok = 0,
    knotfound = 1,
    kcorruption = 2,
    knotsupported = 3,
    kinvalidargument = 4,
    kioerror = 5
  };

  code code() const {
    return (state_ == null) ? kok : static_cast<code>(state_[4]);
  }

  status(code code, const slice& msg, const slice& msg2);
  static const char* copystate(const char* s);
};

inline status::status(const status& s) {
  state_ = (s.state_ == null) ? null : copystate(s.state_);
}
inline void status::operator=(const status& s) {
  // the following condition catches both aliasing (when this == &s),
  // and the common case where both s and *this are ok.
  if (state_ != s.state_) {
    delete[] state_;
    state_ = (s.state_ == null) ? null : copystate(s.state_);
  }
}

}  // namespace leveldb

#endif  // storage_leveldb_include_status_h_
