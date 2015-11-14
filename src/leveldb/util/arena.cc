// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/arena.h"
#include <assert.h>

namespace leveldb {

static const int kblocksize = 4096;

arena::arena() {
  blocks_memory_ = 0;
  alloc_ptr_ = null;  // first allocation will allocate a block
  alloc_bytes_remaining_ = 0;
}

arena::~arena() {
  for (size_t i = 0; i < blocks_.size(); i++) {
    delete[] blocks_[i];
  }
}

char* arena::allocatefallback(size_t bytes) {
  if (bytes > kblocksize / 4) {
    // object is more than a quarter of our block size.  allocate it separately
    // to avoid wasting too much space in leftover bytes.
    char* result = allocatenewblock(bytes);
    return result;
  }

  // we waste the remaining space in the current block.
  alloc_ptr_ = allocatenewblock(kblocksize);
  alloc_bytes_remaining_ = kblocksize;

  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

char* arena::allocatealigned(size_t bytes) {
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  assert((align & (align-1)) == 0);   // pointer size should be a power of 2
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align-1);
  size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // allocatefallback always returned aligned memory
    result = allocatefallback(bytes);
  }
  assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);
  return result;
}

char* arena::allocatenewblock(size_t block_bytes) {
  char* result = new char[block_bytes];
  blocks_memory_ += block_bytes;
  blocks_.push_back(result);
  return result;
}

}  // namespace leveldb
