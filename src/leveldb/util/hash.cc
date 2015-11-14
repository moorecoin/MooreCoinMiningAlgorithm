// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <string.h>
#include "util/coding.h"
#include "util/hash.h"

// the fallthrough_intended macro can be used to annotate implicit fall-through
// between switch labels. the real definition should be provided externally.
// this one is a fallback version for unsupported compilers.
#ifndef fallthrough_intended
#define fallthrough_intended do { } while (0)
#endif

namespace leveldb {

uint32_t hash(const char* data, size_t n, uint32_t seed) {
  // similar to murmur hash
  const uint32_t m = 0xc6a4a793;
  const uint32_t r = 24;
  const char* limit = data + n;
  uint32_t h = seed ^ (n * m);

  // pick up four bytes at a time
  while (data + 4 <= limit) {
    uint32_t w = decodefixed32(data);
    data += 4;
    h += w;
    h *= m;
    h ^= (h >> 16);
  }

  // pick up remaining bytes
  switch (limit - data) {
    case 3:
      h += static_cast<unsigned char>(data[2]) << 16;
      fallthrough_intended;
    case 2:
      h += static_cast<unsigned char>(data[1]) << 8;
      fallthrough_intended;
    case 1:
      h += static_cast<unsigned char>(data[0]);
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}


}  // namespace leveldb
