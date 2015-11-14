// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_util_arena_h_
#define storage_leveldb_util_arena_h_

#include <vector>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

namespace leveldb {

class arena {
 public:
  arena();
  ~arena();

  // return a pointer to a newly allocated memory block of "bytes" bytes.
  char* allocate(size_t bytes);

  // allocate memory with the normal alignment guarantees provided by malloc
  char* allocatealigned(size_t bytes);

  // returns an estimate of the total memory usage of data allocated
  // by the arena (including space allocated but not yet used for user
  // allocations).
  size_t memoryusage() const {
    return blocks_memory_ + blocks_.capacity() * sizeof(char*);
  }

 private:
  char* allocatefallback(size_t bytes);
  char* allocatenewblock(size_t block_bytes);

  // allocation state
  char* alloc_ptr_;
  size_t alloc_bytes_remaining_;

  // array of new[] allocated memory blocks
  std::vector<char*> blocks_;

  // bytes of memory in blocks allocated so far
  size_t blocks_memory_;

  // no copying allowed
  arena(const arena&);
  void operator=(const arena&);
};

inline char* arena::allocate(size_t bytes) {
  // the semantics of what to return are a bit messy if we allow
  // 0-byte allocations, so we disallow them here (we don't need
  // them for our internal use).
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return allocatefallback(bytes);
}

}  // namespace leveldb

#endif  // storage_leveldb_util_arena_h_
