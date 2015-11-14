// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/logging.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "leveldb/env.h"
#include "leveldb/slice.h"

namespace leveldb {

void appendnumberto(std::string* str, uint64_t num) {
  char buf[30];
  snprintf(buf, sizeof(buf), "%llu", (unsigned long long) num);
  str->append(buf);
}

void appendescapedstringto(std::string* str, const slice& value) {
  for (size_t i = 0; i < value.size(); i++) {
    char c = value[i];
    if (c >= ' ' && c <= '~') {
      str->push_back(c);
    } else {
      char buf[10];
      snprintf(buf, sizeof(buf), "\\x%02x",
               static_cast<unsigned int>(c) & 0xff);
      str->append(buf);
    }
  }
}

std::string numbertostring(uint64_t num) {
  std::string r;
  appendnumberto(&r, num);
  return r;
}

std::string escapestring(const slice& value) {
  std::string r;
  appendescapedstringto(&r, value);
  return r;
}

bool consumedecimalnumber(slice* in, uint64_t* val) {
  uint64_t v = 0;
  int digits = 0;
  while (!in->empty()) {
    char c = (*in)[0];
    if (c >= '0' && c <= '9') {
      ++digits;
      const int delta = (c - '0');
      static const uint64_t kmaxuint64 = ~static_cast<uint64_t>(0);
      if (v > kmaxuint64/10 ||
          (v == kmaxuint64/10 && delta > kmaxuint64%10)) {
        // overflow
        return false;
      }
      v = (v * 10) + delta;
      in->remove_prefix(1);
    } else {
      break;
    }
  }
  *val = v;
  return (digits > 0);
}

}  // namespace leveldb
