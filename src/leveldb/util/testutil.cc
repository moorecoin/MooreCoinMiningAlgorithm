// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/testutil.h"

#include "util/random.h"

namespace leveldb {
namespace test {

slice randomstring(random* rnd, int len, std::string* dst) {
  dst->resize(len);
  for (int i = 0; i < len; i++) {
    (*dst)[i] = static_cast<char>(' ' + rnd->uniform(95));   // ' ' .. '~'
  }
  return slice(*dst);
}

std::string randomkey(random* rnd, int len) {
  // make sure to generate a wide variety of characters so we
  // test the boundary conditions for short-key optimizations.
  static const char ktestchars[] = {
    '\0', '\1', 'a', 'b', 'c', 'd', 'e', '\xfd', '\xfe', '\xff'
  };
  std::string result;
  for (int i = 0; i < len; i++) {
    result += ktestchars[rnd->uniform(sizeof(ktestchars))];
  }
  return result;
}


extern slice compressiblestring(random* rnd, double compressed_fraction,
                                size_t len, std::string* dst) {
  int raw = static_cast<int>(len * compressed_fraction);
  if (raw < 1) raw = 1;
  std::string raw_data;
  randomstring(rnd, raw, &raw_data);

  // duplicate the random data until we have filled "len" bytes
  dst->clear();
  while (dst->size() < len) {
    dst->append(raw_data);
  }
  dst->resize(len);
  return slice(*dst);
}

}  // namespace test
}  // namespace leveldb
