// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// thread-safe (provides internal synchronization)

#ifndef storage_leveldb_db_table_cache_h_
#define storage_leveldb_db_table_cache_h_

#include <string>
#include <stdint.h>
#include "db/dbformat.h"
#include "leveldb/cache.h"
#include "leveldb/table.h"
#include "port/port.h"

namespace leveldb {

class env;

class tablecache {
 public:
  tablecache(const std::string& dbname, const options* options, int entries);
  ~tablecache();

  // return an iterator for the specified file number (the corresponding
  // file length must be exactly "file_size" bytes).  if "tableptr" is
  // non-null, also sets "*tableptr" to point to the table object
  // underlying the returned iterator, or null if no table object underlies
  // the returned iterator.  the returned "*tableptr" object is owned by
  // the cache and should not be deleted, and is valid for as long as the
  // returned iterator is live.
  iterator* newiterator(const readoptions& options,
                        uint64_t file_number,
                        uint64_t file_size,
                        table** tableptr = null);

  // if a seek to internal key "k" in specified file finds an entry,
  // call (*handle_result)(arg, found_key, found_value).
  status get(const readoptions& options,
             uint64_t file_number,
             uint64_t file_size,
             const slice& k,
             void* arg,
             void (*handle_result)(void*, const slice&, const slice&));

  // evict any entry for the specified file number
  void evict(uint64_t file_number);

 private:
  env* const env_;
  const std::string dbname_;
  const options* options_;
  cache* cache_;

  status findtable(uint64_t file_number, uint64_t file_size, cache::handle**);
};

}  // namespace leveldb

#endif  // storage_leveldb_db_table_cache_h_
