// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_db_iter_h_
#define storage_leveldb_db_db_iter_h_

#include <stdint.h>
#include "leveldb/db.h"
#include "db/dbformat.h"

namespace leveldb {

class dbimpl;

// return a new iterator that converts internal keys (yielded by
// "*internal_iter") that were live at the specified "sequence" number
// into appropriate user keys.
extern iterator* newdbiterator(
    dbimpl* db,
    const comparator* user_key_comparator,
    iterator* internal_iter,
    sequencenumber sequence,
    uint32_t seed);

}  // namespace leveldb

#endif  // storage_leveldb_db_db_iter_h_
