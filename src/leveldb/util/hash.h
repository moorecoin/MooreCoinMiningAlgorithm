// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// simple hash function used for internal data structures

#ifndef storage_leveldb_util_hash_h_
#define storage_leveldb_util_hash_h_

#include <stddef.h>
#include <stdint.h>

namespace leveldb {

extern uint32_t hash(const char* data, size_t n, uint32_t seed);

}

#endif  // storage_leveldb_util_hash_h_
