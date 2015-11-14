// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_builder_h_
#define storage_leveldb_db_builder_h_

#include "leveldb/status.h"

namespace leveldb {

struct options;
struct filemetadata;

class env;
class iterator;
class tablecache;
class versionedit;

// build a table file from the contents of *iter.  the generated file
// will be named according to meta->number.  on success, the rest of
// *meta will be filled with metadata about the generated table.
// if no data is present in *iter, meta->file_size will be set to
// zero, and no table file will be produced.
extern status buildtable(const std::string& dbname,
                         env* env,
                         const options& options,
                         tablecache* table_cache,
                         iterator* iter,
                         filemetadata* meta);

}  // namespace leveldb

#endif  // storage_leveldb_db_builder_h_
