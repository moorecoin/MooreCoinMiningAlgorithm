// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/c.h"

#include <stdlib.h>
#include <unistd.h>
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "leveldb/write_batch.h"

using leveldb::cache;
using leveldb::comparator;
using leveldb::compressiontype;
using leveldb::db;
using leveldb::env;
using leveldb::filelock;
using leveldb::filterpolicy;
using leveldb::iterator;
using leveldb::kmajorversion;
using leveldb::kminorversion;
using leveldb::logger;
using leveldb::newbloomfilterpolicy;
using leveldb::newlrucache;
using leveldb::options;
using leveldb::randomaccessfile;
using leveldb::range;
using leveldb::readoptions;
using leveldb::sequentialfile;
using leveldb::slice;
using leveldb::snapshot;
using leveldb::status;
using leveldb::writablefile;
using leveldb::writebatch;
using leveldb::writeoptions;

extern "c" {

struct leveldb_t              { db*               rep; };
struct leveldb_iterator_t     { iterator*         rep; };
struct leveldb_writebatch_t   { writebatch        rep; };
struct leveldb_snapshot_t     { const snapshot*   rep; };
struct leveldb_readoptions_t  { readoptions       rep; };
struct leveldb_writeoptions_t { writeoptions      rep; };
struct leveldb_options_t      { options           rep; };
struct leveldb_cache_t        { cache*            rep; };
struct leveldb_seqfile_t      { sequentialfile*   rep; };
struct leveldb_randomfile_t   { randomaccessfile* rep; };
struct leveldb_writablefile_t { writablefile*     rep; };
struct leveldb_logger_t       { logger*           rep; };
struct leveldb_filelock_t     { filelock*         rep; };

struct leveldb_comparator_t : public comparator {
  void* state_;
  void (*destructor_)(void*);
  int (*compare_)(
      void*,
      const char* a, size_t alen,
      const char* b, size_t blen);
  const char* (*name_)(void*);

  virtual ~leveldb_comparator_t() {
    (*destructor_)(state_);
  }

  virtual int compare(const slice& a, const slice& b) const {
    return (*compare_)(state_, a.data(), a.size(), b.data(), b.size());
  }

  virtual const char* name() const {
    return (*name_)(state_);
  }

  // no-ops since the c binding does not support key shortening methods.
  virtual void findshortestseparator(std::string*, const slice&) const { }
  virtual void findshortsuccessor(std::string* key) const { }
};

struct leveldb_filterpolicy_t : public filterpolicy {
  void* state_;
  void (*destructor_)(void*);
  const char* (*name_)(void*);
  char* (*create_)(
      void*,
      const char* const* key_array, const size_t* key_length_array,
      int num_keys,
      size_t* filter_length);
  unsigned char (*key_match_)(
      void*,
      const char* key, size_t length,
      const char* filter, size_t filter_length);

  virtual ~leveldb_filterpolicy_t() {
    (*destructor_)(state_);
  }

  virtual const char* name() const {
    return (*name_)(state_);
  }

  virtual void createfilter(const slice* keys, int n, std::string* dst) const {
    std::vector<const char*> key_pointers(n);
    std::vector<size_t> key_sizes(n);
    for (int i = 0; i < n; i++) {
      key_pointers[i] = keys[i].data();
      key_sizes[i] = keys[i].size();
    }
    size_t len;
    char* filter = (*create_)(state_, &key_pointers[0], &key_sizes[0], n, &len);
    dst->append(filter, len);
    free(filter);
  }

  virtual bool keymaymatch(const slice& key, const slice& filter) const {
    return (*key_match_)(state_, key.data(), key.size(),
                         filter.data(), filter.size());
  }
};

struct leveldb_env_t {
  env* rep;
  bool is_default;
};

static bool saveerror(char** errptr, const status& s) {
  assert(errptr != null);
  if (s.ok()) {
    return false;
  } else if (*errptr == null) {
    *errptr = strdup(s.tostring().c_str());
  } else {
    // todo(sanjay): merge with existing error?
    free(*errptr);
    *errptr = strdup(s.tostring().c_str());
  }
  return true;
}

static char* copystring(const std::string& str) {
  char* result = reinterpret_cast<char*>(malloc(sizeof(char) * str.size()));
  memcpy(result, str.data(), sizeof(char) * str.size());
  return result;
}

leveldb_t* leveldb_open(
    const leveldb_options_t* options,
    const char* name,
    char** errptr) {
  db* db;
  if (saveerror(errptr, db::open(options->rep, std::string(name), &db))) {
    return null;
  }
  leveldb_t* result = new leveldb_t;
  result->rep = db;
  return result;
}

void leveldb_close(leveldb_t* db) {
  delete db->rep;
  delete db;
}

void leveldb_put(
    leveldb_t* db,
    const leveldb_writeoptions_t* options,
    const char* key, size_t keylen,
    const char* val, size_t vallen,
    char** errptr) {
  saveerror(errptr,
            db->rep->put(options->rep, slice(key, keylen), slice(val, vallen)));
}

void leveldb_delete(
    leveldb_t* db,
    const leveldb_writeoptions_t* options,
    const char* key, size_t keylen,
    char** errptr) {
  saveerror(errptr, db->rep->delete(options->rep, slice(key, keylen)));
}


void leveldb_write(
    leveldb_t* db,
    const leveldb_writeoptions_t* options,
    leveldb_writebatch_t* batch,
    char** errptr) {
  saveerror(errptr, db->rep->write(options->rep, &batch->rep));
}

char* leveldb_get(
    leveldb_t* db,
    const leveldb_readoptions_t* options,
    const char* key, size_t keylen,
    size_t* vallen,
    char** errptr) {
  char* result = null;
  std::string tmp;
  status s = db->rep->get(options->rep, slice(key, keylen), &tmp);
  if (s.ok()) {
    *vallen = tmp.size();
    result = copystring(tmp);
  } else {
    *vallen = 0;
    if (!s.isnotfound()) {
      saveerror(errptr, s);
    }
  }
  return result;
}

leveldb_iterator_t* leveldb_create_iterator(
    leveldb_t* db,
    const leveldb_readoptions_t* options) {
  leveldb_iterator_t* result = new leveldb_iterator_t;
  result->rep = db->rep->newiterator(options->rep);
  return result;
}

const leveldb_snapshot_t* leveldb_create_snapshot(
    leveldb_t* db) {
  leveldb_snapshot_t* result = new leveldb_snapshot_t;
  result->rep = db->rep->getsnapshot();
  return result;
}

void leveldb_release_snapshot(
    leveldb_t* db,
    const leveldb_snapshot_t* snapshot) {
  db->rep->releasesnapshot(snapshot->rep);
  delete snapshot;
}

char* leveldb_property_value(
    leveldb_t* db,
    const char* propname) {
  std::string tmp;
  if (db->rep->getproperty(slice(propname), &tmp)) {
    // we use strdup() since we expect human readable output.
    return strdup(tmp.c_str());
  } else {
    return null;
  }
}

void leveldb_approximate_sizes(
    leveldb_t* db,
    int num_ranges,
    const char* const* range_start_key, const size_t* range_start_key_len,
    const char* const* range_limit_key, const size_t* range_limit_key_len,
    uint64_t* sizes) {
  range* ranges = new range[num_ranges];
  for (int i = 0; i < num_ranges; i++) {
    ranges[i].start = slice(range_start_key[i], range_start_key_len[i]);
    ranges[i].limit = slice(range_limit_key[i], range_limit_key_len[i]);
  }
  db->rep->getapproximatesizes(ranges, num_ranges, sizes);
  delete[] ranges;
}

void leveldb_compact_range(
    leveldb_t* db,
    const char* start_key, size_t start_key_len,
    const char* limit_key, size_t limit_key_len) {
  slice a, b;
  db->rep->compactrange(
      // pass null slice if corresponding "const char*" is null
      (start_key ? (a = slice(start_key, start_key_len), &a) : null),
      (limit_key ? (b = slice(limit_key, limit_key_len), &b) : null));
}

void leveldb_destroy_db(
    const leveldb_options_t* options,
    const char* name,
    char** errptr) {
  saveerror(errptr, destroydb(name, options->rep));
}

void leveldb_repair_db(
    const leveldb_options_t* options,
    const char* name,
    char** errptr) {
  saveerror(errptr, repairdb(name, options->rep));
}

void leveldb_iter_destroy(leveldb_iterator_t* iter) {
  delete iter->rep;
  delete iter;
}

unsigned char leveldb_iter_valid(const leveldb_iterator_t* iter) {
  return iter->rep->valid();
}

void leveldb_iter_seek_to_first(leveldb_iterator_t* iter) {
  iter->rep->seektofirst();
}

void leveldb_iter_seek_to_last(leveldb_iterator_t* iter) {
  iter->rep->seektolast();
}

void leveldb_iter_seek(leveldb_iterator_t* iter, const char* k, size_t klen) {
  iter->rep->seek(slice(k, klen));
}

void leveldb_iter_next(leveldb_iterator_t* iter) {
  iter->rep->next();
}

void leveldb_iter_prev(leveldb_iterator_t* iter) {
  iter->rep->prev();
}

const char* leveldb_iter_key(const leveldb_iterator_t* iter, size_t* klen) {
  slice s = iter->rep->key();
  *klen = s.size();
  return s.data();
}

const char* leveldb_iter_value(const leveldb_iterator_t* iter, size_t* vlen) {
  slice s = iter->rep->value();
  *vlen = s.size();
  return s.data();
}

void leveldb_iter_get_error(const leveldb_iterator_t* iter, char** errptr) {
  saveerror(errptr, iter->rep->status());
}

leveldb_writebatch_t* leveldb_writebatch_create() {
  return new leveldb_writebatch_t;
}

void leveldb_writebatch_destroy(leveldb_writebatch_t* b) {
  delete b;
}

void leveldb_writebatch_clear(leveldb_writebatch_t* b) {
  b->rep.clear();
}

void leveldb_writebatch_put(
    leveldb_writebatch_t* b,
    const char* key, size_t klen,
    const char* val, size_t vlen) {
  b->rep.put(slice(key, klen), slice(val, vlen));
}

void leveldb_writebatch_delete(
    leveldb_writebatch_t* b,
    const char* key, size_t klen) {
  b->rep.delete(slice(key, klen));
}

void leveldb_writebatch_iterate(
    leveldb_writebatch_t* b,
    void* state,
    void (*put)(void*, const char* k, size_t klen, const char* v, size_t vlen),
    void (*deleted)(void*, const char* k, size_t klen)) {
  class h : public writebatch::handler {
   public:
    void* state_;
    void (*put_)(void*, const char* k, size_t klen, const char* v, size_t vlen);
    void (*deleted_)(void*, const char* k, size_t klen);
    virtual void put(const slice& key, const slice& value) {
      (*put_)(state_, key.data(), key.size(), value.data(), value.size());
    }
    virtual void delete(const slice& key) {
      (*deleted_)(state_, key.data(), key.size());
    }
  };
  h handler;
  handler.state_ = state;
  handler.put_ = put;
  handler.deleted_ = deleted;
  b->rep.iterate(&handler);
}

leveldb_options_t* leveldb_options_create() {
  return new leveldb_options_t;
}

void leveldb_options_destroy(leveldb_options_t* options) {
  delete options;
}

void leveldb_options_set_comparator(
    leveldb_options_t* opt,
    leveldb_comparator_t* cmp) {
  opt->rep.comparator = cmp;
}

void leveldb_options_set_filter_policy(
    leveldb_options_t* opt,
    leveldb_filterpolicy_t* policy) {
  opt->rep.filter_policy = policy;
}

void leveldb_options_set_create_if_missing(
    leveldb_options_t* opt, unsigned char v) {
  opt->rep.create_if_missing = v;
}

void leveldb_options_set_error_if_exists(
    leveldb_options_t* opt, unsigned char v) {
  opt->rep.error_if_exists = v;
}

void leveldb_options_set_paranoid_checks(
    leveldb_options_t* opt, unsigned char v) {
  opt->rep.paranoid_checks = v;
}

void leveldb_options_set_env(leveldb_options_t* opt, leveldb_env_t* env) {
  opt->rep.env = (env ? env->rep : null);
}

void leveldb_options_set_info_log(leveldb_options_t* opt, leveldb_logger_t* l) {
  opt->rep.info_log = (l ? l->rep : null);
}

void leveldb_options_set_write_buffer_size(leveldb_options_t* opt, size_t s) {
  opt->rep.write_buffer_size = s;
}

void leveldb_options_set_max_open_files(leveldb_options_t* opt, int n) {
  opt->rep.max_open_files = n;
}

void leveldb_options_set_cache(leveldb_options_t* opt, leveldb_cache_t* c) {
  opt->rep.block_cache = c->rep;
}

void leveldb_options_set_block_size(leveldb_options_t* opt, size_t s) {
  opt->rep.block_size = s;
}

void leveldb_options_set_block_restart_interval(leveldb_options_t* opt, int n) {
  opt->rep.block_restart_interval = n;
}

void leveldb_options_set_compression(leveldb_options_t* opt, int t) {
  opt->rep.compression = static_cast<compressiontype>(t);
}

leveldb_comparator_t* leveldb_comparator_create(
    void* state,
    void (*destructor)(void*),
    int (*compare)(
        void*,
        const char* a, size_t alen,
        const char* b, size_t blen),
    const char* (*name)(void*)) {
  leveldb_comparator_t* result = new leveldb_comparator_t;
  result->state_ = state;
  result->destructor_ = destructor;
  result->compare_ = compare;
  result->name_ = name;
  return result;
}

void leveldb_comparator_destroy(leveldb_comparator_t* cmp) {
  delete cmp;
}

leveldb_filterpolicy_t* leveldb_filterpolicy_create(
    void* state,
    void (*destructor)(void*),
    char* (*create_filter)(
        void*,
        const char* const* key_array, const size_t* key_length_array,
        int num_keys,
        size_t* filter_length),
    unsigned char (*key_may_match)(
        void*,
        const char* key, size_t length,
        const char* filter, size_t filter_length),
    const char* (*name)(void*)) {
  leveldb_filterpolicy_t* result = new leveldb_filterpolicy_t;
  result->state_ = state;
  result->destructor_ = destructor;
  result->create_ = create_filter;
  result->key_match_ = key_may_match;
  result->name_ = name;
  return result;
}

void leveldb_filterpolicy_destroy(leveldb_filterpolicy_t* filter) {
  delete filter;
}

leveldb_filterpolicy_t* leveldb_filterpolicy_create_bloom(int bits_per_key) {
  // make a leveldb_filterpolicy_t, but override all of its methods so
  // they delegate to a newbloomfilterpolicy() instead of user
  // supplied c functions.
  struct wrapper : public leveldb_filterpolicy_t {
    const filterpolicy* rep_;
    ~wrapper() { delete rep_; }
    const char* name() const { return rep_->name(); }
    void createfilter(const slice* keys, int n, std::string* dst) const {
      return rep_->createfilter(keys, n, dst);
    }
    bool keymaymatch(const slice& key, const slice& filter) const {
      return rep_->keymaymatch(key, filter);
    }
    static void donothing(void*) { }
  };
  wrapper* wrapper = new wrapper;
  wrapper->rep_ = newbloomfilterpolicy(bits_per_key);
  wrapper->state_ = null;
  wrapper->destructor_ = &wrapper::donothing;
  return wrapper;
}

leveldb_readoptions_t* leveldb_readoptions_create() {
  return new leveldb_readoptions_t;
}

void leveldb_readoptions_destroy(leveldb_readoptions_t* opt) {
  delete opt;
}

void leveldb_readoptions_set_verify_checksums(
    leveldb_readoptions_t* opt,
    unsigned char v) {
  opt->rep.verify_checksums = v;
}

void leveldb_readoptions_set_fill_cache(
    leveldb_readoptions_t* opt, unsigned char v) {
  opt->rep.fill_cache = v;
}

void leveldb_readoptions_set_snapshot(
    leveldb_readoptions_t* opt,
    const leveldb_snapshot_t* snap) {
  opt->rep.snapshot = (snap ? snap->rep : null);
}

leveldb_writeoptions_t* leveldb_writeoptions_create() {
  return new leveldb_writeoptions_t;
}

void leveldb_writeoptions_destroy(leveldb_writeoptions_t* opt) {
  delete opt;
}

void leveldb_writeoptions_set_sync(
    leveldb_writeoptions_t* opt, unsigned char v) {
  opt->rep.sync = v;
}

leveldb_cache_t* leveldb_cache_create_lru(size_t capacity) {
  leveldb_cache_t* c = new leveldb_cache_t;
  c->rep = newlrucache(capacity);
  return c;
}

void leveldb_cache_destroy(leveldb_cache_t* cache) {
  delete cache->rep;
  delete cache;
}

leveldb_env_t* leveldb_create_default_env() {
  leveldb_env_t* result = new leveldb_env_t;
  result->rep = env::default();
  result->is_default = true;
  return result;
}

void leveldb_env_destroy(leveldb_env_t* env) {
  if (!env->is_default) delete env->rep;
  delete env;
}

void leveldb_free(void* ptr) {
  free(ptr);
}

int leveldb_major_version() {
  return kmajorversion;
}

int leveldb_minor_version() {
  return kminorversion;
}

}  // end extern "c"
