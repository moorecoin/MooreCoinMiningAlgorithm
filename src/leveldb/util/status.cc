// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include "port/port.h"
#include "leveldb/status.h"

namespace leveldb {

const char* status::copystate(const char* state) {
  uint32_t size;
  memcpy(&size, state, sizeof(size));
  char* result = new char[size + 5];
  memcpy(result, state, size + 5);
  return result;
}

status::status(code code, const slice& msg, const slice& msg2) {
  assert(code != kok);
  const uint32_t len1 = msg.size();
  const uint32_t len2 = msg2.size();
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
  char* result = new char[size + 5];
  memcpy(result, &size, sizeof(size));
  result[4] = static_cast<char>(code);
  memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string status::tostring() const {
  if (state_ == null) {
    return "ok";
  } else {
    char tmp[30];
    const char* type;
    switch (code()) {
      case kok:
        type = "ok";
        break;
      case knotfound:
        type = "notfound: ";
        break;
      case kcorruption:
        type = "corruption: ";
        break;
      case knotsupported:
        type = "not implemented: ";
        break;
      case kinvalidargument:
        type = "invalid argument: ";
        break;
      case kioerror:
        type = "io error: ";
        break;
      default:
        snprintf(tmp, sizeof(tmp), "unknown code(%d): ",
                 static_cast<int>(code()));
        type = tmp;
        break;
    }
    std::string result(type);
    uint32_t length;
    memcpy(&length, state_, sizeof(length));
    result.append(state_ + 5, length);
    return result;
  }
}

}  // namespace leveldb
