// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_table_two_level_iterator_h_
#define storage_leveldb_table_two_level_iterator_h_

#include "leveldb/iterator.h"

namespace leveldb {

struct readoptions;

// return a new two level iterator.  a two-level iterator contains an
// index iterator whose values point to a sequence of blocks where
// each block is itself a sequence of key,value pairs.  the returned
// two-level iterator yields the concatenation of all key/value pairs
// in the sequence of blocks.  takes ownership of "index_iter" and
// will delete it when no longer needed.
//
// uses a supplied function to convert an index_iter value into
// an iterator over the contents of the corresponding block.
extern iterator* newtwoleveliterator(
    iterator* index_iter,
    iterator* (*block_function)(
        void* arg,
        const readoptions& options,
        const slice& index_value),
    void* arg,
    const readoptions& options);

}  // namespace leveldb

#endif  // storage_leveldb_table_two_level_iterator_h_
