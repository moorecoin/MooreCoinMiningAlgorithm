/* copyright (c) 2011 the leveldb authors. all rights reserved.
   use of this source code is governed by a bsd-style license that can be
   found in the license file. see the authors file for names of contributors. */

#include "leveldb/c.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

const char* phase = "";
static char dbname[200];

static void startphase(const char* name) {
  fprintf(stderr, "=== test %s\n", name);
  phase = name;
}

static const char* gettempdir(void) {
    const char* ret = getenv("test_tmpdir");
    if (ret == null || ret[0] == '\0')
        ret = "/tmp";
    return ret;
}

#define checknoerror(err)                                               \
  if ((err) != null) {                                                  \
    fprintf(stderr, "%s:%d: %s: %s\n", __file__, __line__, phase, (err)); \
    abort();                                                            \
  }

#define checkcondition(cond)                                            \
  if (!(cond)) {                                                        \
    fprintf(stderr, "%s:%d: %s: %s\n", __file__, __line__, phase, #cond); \
    abort();                                                            \
  }

static void checkequal(const char* expected, const char* v, size_t n) {
  if (expected == null && v == null) {
    // ok
  } else if (expected != null && v != null && n == strlen(expected) &&
             memcmp(expected, v, n) == 0) {
    // ok
    return;
  } else {
    fprintf(stderr, "%s: expected '%s', got '%s'\n",
            phase,
            (expected ? expected : "(null)"),
            (v ? v : "(null"));
    abort();
  }
}

static void free(char** ptr) {
  if (*ptr) {
    free(*ptr);
    *ptr = null;
  }
}

static void checkget(
    leveldb_t* db,
    const leveldb_readoptions_t* options,
    const char* key,
    const char* expected) {
  char* err = null;
  size_t val_len;
  char* val;
  val = leveldb_get(db, options, key, strlen(key), &val_len, &err);
  checknoerror(err);
  checkequal(expected, val, val_len);
  free(&val);
}

static void checkiter(leveldb_iterator_t* iter,
                      const char* key, const char* val) {
  size_t len;
  const char* str;
  str = leveldb_iter_key(iter, &len);
  checkequal(key, str, len);
  str = leveldb_iter_value(iter, &len);
  checkequal(val, str, len);
}

// callback from leveldb_writebatch_iterate()
static void checkput(void* ptr,
                     const char* k, size_t klen,
                     const char* v, size_t vlen) {
  int* state = (int*) ptr;
  checkcondition(*state < 2);
  switch (*state) {
    case 0:
      checkequal("bar", k, klen);
      checkequal("b", v, vlen);
      break;
    case 1:
      checkequal("box", k, klen);
      checkequal("c", v, vlen);
      break;
  }
  (*state)++;
}

// callback from leveldb_writebatch_iterate()
static void checkdel(void* ptr, const char* k, size_t klen) {
  int* state = (int*) ptr;
  checkcondition(*state == 2);
  checkequal("bar", k, klen);
  (*state)++;
}

static void cmpdestroy(void* arg) { }

static int cmpcompare(void* arg, const char* a, size_t alen,
                      const char* b, size_t blen) {
  int n = (alen < blen) ? alen : blen;
  int r = memcmp(a, b, n);
  if (r == 0) {
    if (alen < blen) r = -1;
    else if (alen > blen) r = +1;
  }
  return r;
}

static const char* cmpname(void* arg) {
  return "foo";
}

// custom filter policy
static unsigned char fake_filter_result = 1;
static void filterdestroy(void* arg) { }
static const char* filtername(void* arg) {
  return "testfilter";
}
static char* filtercreate(
    void* arg,
    const char* const* key_array, const size_t* key_length_array,
    int num_keys,
    size_t* filter_length) {
  *filter_length = 4;
  char* result = malloc(4);
  memcpy(result, "fake", 4);
  return result;
}
unsigned char filterkeymatch(
    void* arg,
    const char* key, size_t length,
    const char* filter, size_t filter_length) {
  checkcondition(filter_length == 4);
  checkcondition(memcmp(filter, "fake", 4) == 0);
  return fake_filter_result;
}

int main(int argc, char** argv) {
  leveldb_t* db;
  leveldb_comparator_t* cmp;
  leveldb_cache_t* cache;
  leveldb_env_t* env;
  leveldb_options_t* options;
  leveldb_readoptions_t* roptions;
  leveldb_writeoptions_t* woptions;
  char* err = null;
  int run = -1;

  checkcondition(leveldb_major_version() >= 1);
  checkcondition(leveldb_minor_version() >= 1);

  snprintf(dbname, sizeof(dbname),
           "%s/leveldb_c_test-%d",
           gettempdir(),
           ((int) geteuid()));

  startphase("create_objects");
  cmp = leveldb_comparator_create(null, cmpdestroy, cmpcompare, cmpname);
  env = leveldb_create_default_env();
  cache = leveldb_cache_create_lru(100000);

  options = leveldb_options_create();
  leveldb_options_set_comparator(options, cmp);
  leveldb_options_set_error_if_exists(options, 1);
  leveldb_options_set_cache(options, cache);
  leveldb_options_set_env(options, env);
  leveldb_options_set_info_log(options, null);
  leveldb_options_set_write_buffer_size(options, 100000);
  leveldb_options_set_paranoid_checks(options, 1);
  leveldb_options_set_max_open_files(options, 10);
  leveldb_options_set_block_size(options, 1024);
  leveldb_options_set_block_restart_interval(options, 8);
  leveldb_options_set_compression(options, leveldb_no_compression);

  roptions = leveldb_readoptions_create();
  leveldb_readoptions_set_verify_checksums(roptions, 1);
  leveldb_readoptions_set_fill_cache(roptions, 0);

  woptions = leveldb_writeoptions_create();
  leveldb_writeoptions_set_sync(woptions, 1);

  startphase("destroy");
  leveldb_destroy_db(options, dbname, &err);
  free(&err);

  startphase("open_error");
  db = leveldb_open(options, dbname, &err);
  checkcondition(err != null);
  free(&err);

  startphase("leveldb_free");
  db = leveldb_open(options, dbname, &err);
  checkcondition(err != null);
  leveldb_free(err);
  err = null;

  startphase("open");
  leveldb_options_set_create_if_missing(options, 1);
  db = leveldb_open(options, dbname, &err);
  checknoerror(err);
  checkget(db, roptions, "foo", null);

  startphase("put");
  leveldb_put(db, woptions, "foo", 3, "hello", 5, &err);
  checknoerror(err);
  checkget(db, roptions, "foo", "hello");

  startphase("compactall");
  leveldb_compact_range(db, null, 0, null, 0);
  checkget(db, roptions, "foo", "hello");

  startphase("compactrange");
  leveldb_compact_range(db, "a", 1, "z", 1);
  checkget(db, roptions, "foo", "hello");

  startphase("writebatch");
  {
    leveldb_writebatch_t* wb = leveldb_writebatch_create();
    leveldb_writebatch_put(wb, "foo", 3, "a", 1);
    leveldb_writebatch_clear(wb);
    leveldb_writebatch_put(wb, "bar", 3, "b", 1);
    leveldb_writebatch_put(wb, "box", 3, "c", 1);
    leveldb_writebatch_delete(wb, "bar", 3);
    leveldb_write(db, woptions, wb, &err);
    checknoerror(err);
    checkget(db, roptions, "foo", "hello");
    checkget(db, roptions, "bar", null);
    checkget(db, roptions, "box", "c");
    int pos = 0;
    leveldb_writebatch_iterate(wb, &pos, checkput, checkdel);
    checkcondition(pos == 3);
    leveldb_writebatch_destroy(wb);
  }

  startphase("iter");
  {
    leveldb_iterator_t* iter = leveldb_create_iterator(db, roptions);
    checkcondition(!leveldb_iter_valid(iter));
    leveldb_iter_seek_to_first(iter);
    checkcondition(leveldb_iter_valid(iter));
    checkiter(iter, "box", "c");
    leveldb_iter_next(iter);
    checkiter(iter, "foo", "hello");
    leveldb_iter_prev(iter);
    checkiter(iter, "box", "c");
    leveldb_iter_prev(iter);
    checkcondition(!leveldb_iter_valid(iter));
    leveldb_iter_seek_to_last(iter);
    checkiter(iter, "foo", "hello");
    leveldb_iter_seek(iter, "b", 1);
    checkiter(iter, "box", "c");
    leveldb_iter_get_error(iter, &err);
    checknoerror(err);
    leveldb_iter_destroy(iter);
  }

  startphase("approximate_sizes");
  {
    int i;
    int n = 20000;
    char keybuf[100];
    char valbuf[100];
    uint64_t sizes[2];
    const char* start[2] = { "a", "k00000000000000010000" };
    size_t start_len[2] = { 1, 21 };
    const char* limit[2] = { "k00000000000000010000", "z" };
    size_t limit_len[2] = { 21, 1 };
    leveldb_writeoptions_set_sync(woptions, 0);
    for (i = 0; i < n; i++) {
      snprintf(keybuf, sizeof(keybuf), "k%020d", i);
      snprintf(valbuf, sizeof(valbuf), "v%020d", i);
      leveldb_put(db, woptions, keybuf, strlen(keybuf), valbuf, strlen(valbuf),
                  &err);
      checknoerror(err);
    }
    leveldb_approximate_sizes(db, 2, start, start_len, limit, limit_len, sizes);
    checkcondition(sizes[0] > 0);
    checkcondition(sizes[1] > 0);
  }

  startphase("property");
  {
    char* prop = leveldb_property_value(db, "nosuchprop");
    checkcondition(prop == null);
    prop = leveldb_property_value(db, "leveldb.stats");
    checkcondition(prop != null);
    free(&prop);
  }

  startphase("snapshot");
  {
    const leveldb_snapshot_t* snap;
    snap = leveldb_create_snapshot(db);
    leveldb_delete(db, woptions, "foo", 3, &err);
    checknoerror(err);
    leveldb_readoptions_set_snapshot(roptions, snap);
    checkget(db, roptions, "foo", "hello");
    leveldb_readoptions_set_snapshot(roptions, null);
    checkget(db, roptions, "foo", null);
    leveldb_release_snapshot(db, snap);
  }

  startphase("repair");
  {
    leveldb_close(db);
    leveldb_options_set_create_if_missing(options, 0);
    leveldb_options_set_error_if_exists(options, 0);
    leveldb_repair_db(options, dbname, &err);
    checknoerror(err);
    db = leveldb_open(options, dbname, &err);
    checknoerror(err);
    checkget(db, roptions, "foo", null);
    checkget(db, roptions, "bar", null);
    checkget(db, roptions, "box", "c");
    leveldb_options_set_create_if_missing(options, 1);
    leveldb_options_set_error_if_exists(options, 1);
  }

  startphase("filter");
  for (run = 0; run < 2; run++) {
    // first run uses custom filter, second run uses bloom filter
    checknoerror(err);
    leveldb_filterpolicy_t* policy;
    if (run == 0) {
      policy = leveldb_filterpolicy_create(
          null, filterdestroy, filtercreate, filterkeymatch, filtername);
    } else {
      policy = leveldb_filterpolicy_create_bloom(10);
    }

    // create new database
    leveldb_close(db);
    leveldb_destroy_db(options, dbname, &err);
    leveldb_options_set_filter_policy(options, policy);
    db = leveldb_open(options, dbname, &err);
    checknoerror(err);
    leveldb_put(db, woptions, "foo", 3, "foovalue", 8, &err);
    checknoerror(err);
    leveldb_put(db, woptions, "bar", 3, "barvalue", 8, &err);
    checknoerror(err);
    leveldb_compact_range(db, null, 0, null, 0);

    fake_filter_result = 1;
    checkget(db, roptions, "foo", "foovalue");
    checkget(db, roptions, "bar", "barvalue");
    if (phase == 0) {
      // must not find value when custom filter returns false
      fake_filter_result = 0;
      checkget(db, roptions, "foo", null);
      checkget(db, roptions, "bar", null);
      fake_filter_result = 1;

      checkget(db, roptions, "foo", "foovalue");
      checkget(db, roptions, "bar", "barvalue");
    }
    leveldb_options_set_filter_policy(options, null);
    leveldb_filterpolicy_destroy(policy);
  }

  startphase("cleanup");
  leveldb_close(db);
  leveldb_options_destroy(options);
  leveldb_readoptions_destroy(roptions);
  leveldb_writeoptions_destroy(woptions);
  leveldb_cache_destroy(cache);
  leveldb_comparator_destroy(cmp);
  leveldb_env_destroy(env);

  fprintf(stderr, "pass\n");
  return 0;
}
