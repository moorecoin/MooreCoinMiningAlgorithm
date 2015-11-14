// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/table_cache.h"

#include "db/filename.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/coding.h"

namespace leveldb {

struct tableandfile {
  randomaccessfile* file;
  table* table;
};

static void deleteentry(const slice& key, void* value) {
  tableandfile* tf = reinterpret_cast<tableandfile*>(value);
  delete tf->table;
  delete tf->file;
  delete tf;
}

static void unrefentry(void* arg1, void* arg2) {
  cache* cache = reinterpret_cast<cache*>(arg1);
  cache::handle* h = reinterpret_cast<cache::handle*>(arg2);
  cache->release(h);
}

tablecache::tablecache(const std::string& dbname,
                       const options* options,
                       int entries)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      cache_(newlrucache(entries)) {
}

tablecache::~tablecache() {
  delete cache_;
}

status tablecache::findtable(uint64_t file_number, uint64_t file_size,
                             cache::handle** handle) {
  status s;
  char buf[sizeof(file_number)];
  encodefixed64(buf, file_number);
  slice key(buf, sizeof(buf));
  *handle = cache_->lookup(key);
  if (*handle == null) {
    std::string fname = tablefilename(dbname_, file_number);
    randomaccessfile* file = null;
    table* table = null;
    s = env_->newrandomaccessfile(fname, &file);
    if (!s.ok()) {
      std::string old_fname = ssttablefilename(dbname_, file_number);
      if (env_->newrandomaccessfile(old_fname, &file).ok()) {
        s = status::ok();
      }
    }
    if (s.ok()) {
      s = table::open(*options_, file, file_size, &table);
    }

    if (!s.ok()) {
      assert(table == null);
      delete file;
      // we do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else {
      tableandfile* tf = new tableandfile;
      tf->file = file;
      tf->table = table;
      *handle = cache_->insert(key, tf, 1, &deleteentry);
    }
  }
  return s;
}

iterator* tablecache::newiterator(const readoptions& options,
                                  uint64_t file_number,
                                  uint64_t file_size,
                                  table** tableptr) {
  if (tableptr != null) {
    *tableptr = null;
  }

  cache::handle* handle = null;
  status s = findtable(file_number, file_size, &handle);
  if (!s.ok()) {
    return newerroriterator(s);
  }

  table* table = reinterpret_cast<tableandfile*>(cache_->value(handle))->table;
  iterator* result = table->newiterator(options);
  result->registercleanup(&unrefentry, cache_, handle);
  if (tableptr != null) {
    *tableptr = table;
  }
  return result;
}

status tablecache::get(const readoptions& options,
                       uint64_t file_number,
                       uint64_t file_size,
                       const slice& k,
                       void* arg,
                       void (*saver)(void*, const slice&, const slice&)) {
  cache::handle* handle = null;
  status s = findtable(file_number, file_size, &handle);
  if (s.ok()) {
    table* t = reinterpret_cast<tableandfile*>(cache_->value(handle))->table;
    s = t->internalget(options, k, arg, saver);
    cache_->release(handle);
  }
  return s;
}

void tablecache::evict(uint64_t file_number) {
  char buf[sizeof(file_number)];
  encodefixed64(buf, file_number);
  cache_->erase(slice(buf, sizeof(buf)));
}

}  // namespace leveldb
