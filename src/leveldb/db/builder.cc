// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/builder.h"

#include "db/filename.h"
#include "db/dbformat.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"

namespace leveldb {

status buildtable(const std::string& dbname,
                  env* env,
                  const options& options,
                  tablecache* table_cache,
                  iterator* iter,
                  filemetadata* meta) {
  status s;
  meta->file_size = 0;
  iter->seektofirst();

  std::string fname = tablefilename(dbname, meta->number);
  if (iter->valid()) {
    writablefile* file;
    s = env->newwritablefile(fname, &file);
    if (!s.ok()) {
      return s;
    }

    tablebuilder* builder = new tablebuilder(options, file);
    meta->smallest.decodefrom(iter->key());
    for (; iter->valid(); iter->next()) {
      slice key = iter->key();
      meta->largest.decodefrom(key);
      builder->add(key, iter->value());
    }

    // finish and check for builder errors
    if (s.ok()) {
      s = builder->finish();
      if (s.ok()) {
        meta->file_size = builder->filesize();
        assert(meta->file_size > 0);
      }
    } else {
      builder->abandon();
    }
    delete builder;

    // finish and check for file errors
    if (s.ok()) {
      s = file->sync();
    }
    if (s.ok()) {
      s = file->close();
    }
    delete file;
    file = null;

    if (s.ok()) {
      // verify that the table is usable
      iterator* it = table_cache->newiterator(readoptions(),
                                              meta->number,
                                              meta->file_size);
      s = it->status();
      delete it;
    }
  }

  // check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  if (s.ok() && meta->file_size > 0) {
    // keep it
  } else {
    env->deletefile(fname);
  }
  return s;
}

}  // namespace leveldb
