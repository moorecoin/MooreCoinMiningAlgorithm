// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_table_merger_h_
#define storage_leveldb_table_merger_h_

namespace leveldb {

class comparator;
class iterator;

// return an iterator that provided the union of the data in
// children[0,n-1].  takes ownership of the child iterators and
// will delete them when the result iterator is deleted.
//
// the result does no duplicate suppression.  i.e., if a particular
// key is present in k child iterators, it will be yielded k times.
//
// requires: n >= 0
extern iterator* newmergingiterator(
    const comparator* comparator, iterator** children, int n);

}  // namespace leveldb

#endif  // storage_leveldb_table_merger_h_
