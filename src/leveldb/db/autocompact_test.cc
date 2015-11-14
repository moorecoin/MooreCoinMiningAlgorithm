// copyright (c) 2013 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/db.h"
#include "db/db_impl.h"
#include "leveldb/cache.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

class autocompacttest {
 public:
  std::string dbname_;
  cache* tiny_cache_;
  options options_;
  db* db_;

  autocompacttest() {
    dbname_ = test::tmpdir() + "/autocompact_test";
    tiny_cache_ = newlrucache(100);
    options_.block_cache = tiny_cache_;
    destroydb(dbname_, options_);
    options_.create_if_missing = true;
    options_.compression = knocompression;
    assert_ok(db::open(options_, dbname_, &db_));
  }

  ~autocompacttest() {
    delete db_;
    destroydb(dbname_, options());
    delete tiny_cache_;
  }

  std::string key(int i) {
    char buf[100];
    snprintf(buf, sizeof(buf), "key%06d", i);
    return std::string(buf);
  }

  uint64_t size(const slice& start, const slice& limit) {
    range r(start, limit);
    uint64_t size;
    db_->getapproximatesizes(&r, 1, &size);
    return size;
  }

  void doreads(int n);
};

static const int kvaluesize = 200 * 1024;
static const int ktotalsize = 100 * 1024 * 1024;
static const int kcount = ktotalsize / kvaluesize;

// read through the first n keys repeatedly and check that they get
// compacted (verified by checking the size of the key space).
void autocompacttest::doreads(int n) {
  std::string value(kvaluesize, 'x');
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);

  // fill database
  for (int i = 0; i < kcount; i++) {
    assert_ok(db_->put(writeoptions(), key(i), value));
  }
  assert_ok(dbi->test_compactmemtable());

  // delete everything
  for (int i = 0; i < kcount; i++) {
    assert_ok(db_->delete(writeoptions(), key(i)));
  }
  assert_ok(dbi->test_compactmemtable());

  // get initial measurement of the space we will be reading.
  const int64_t initial_size = size(key(0), key(n));
  const int64_t initial_other_size = size(key(n), key(kcount));

  // read until size drops significantly.
  std::string limit_key = key(n);
  for (int read = 0; true; read++) {
    assert_lt(read, 100) << "taking too long to compact";
    iterator* iter = db_->newiterator(readoptions());
    for (iter->seektofirst();
         iter->valid() && iter->key().tostring() < limit_key;
         iter->next()) {
      // drop data
    }
    delete iter;
    // wait a little bit to allow any triggered compactions to complete.
    env::default()->sleepformicroseconds(1000000);
    uint64_t size = size(key(0), key(n));
    fprintf(stderr, "iter %3d => %7.3f mb [other %7.3f mb]\n",
            read+1, size/1048576.0, size(key(n), key(kcount))/1048576.0);
    if (size <= initial_size/10) {
      break;
    }
  }

  // verify that the size of the key space not touched by the reads
  // is pretty much unchanged.
  const int64_t final_other_size = size(key(n), key(kcount));
  assert_le(final_other_size, initial_other_size + 1048576);
  assert_ge(final_other_size, initial_other_size/5 - 1048576);
}

test(autocompacttest, readall) {
  doreads(kcount);
}

test(autocompacttest, readhalf) {
  doreads(kcount/2);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
