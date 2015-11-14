// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/db.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "leveldb/write_batch.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/log_format.h"
#include "db/version_set.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

static const int kvaluesize = 1000;

class corruptiontest {
 public:
  test::errorenv env_;
  std::string dbname_;
  cache* tiny_cache_;
  options options_;
  db* db_;

  corruptiontest() {
    tiny_cache_ = newlrucache(100);
    options_.env = &env_;
    options_.block_cache = tiny_cache_;
    dbname_ = test::tmpdir() + "/db_test";
    destroydb(dbname_, options_);

    db_ = null;
    options_.create_if_missing = true;
    reopen();
    options_.create_if_missing = false;
  }

  ~corruptiontest() {
     delete db_;
     destroydb(dbname_, options());
     delete tiny_cache_;
  }

  status tryreopen() {
    delete db_;
    db_ = null;
    return db::open(options_, dbname_, &db_);
  }

  void reopen() {
    assert_ok(tryreopen());
  }

  void repairdb() {
    delete db_;
    db_ = null;
    assert_ok(::leveldb::repairdb(dbname_, options_));
  }

  void build(int n) {
    std::string key_space, value_space;
    writebatch batch;
    for (int i = 0; i < n; i++) {
      //if ((i % 100) == 0) fprintf(stderr, "@ %d of %d\n", i, n);
      slice key = key(i, &key_space);
      batch.clear();
      batch.put(key, value(i, &value_space));
      writeoptions options;
      // corrupt() doesn't work without this sync on windows; stat reports 0 for
      // the file size.
      if (i == n - 1) {
        options.sync = true;
      }
      assert_ok(db_->write(options, &batch));
    }
  }

  void check(int min_expected, int max_expected) {
    int next_expected = 0;
    int missed = 0;
    int bad_keys = 0;
    int bad_values = 0;
    int correct = 0;
    std::string value_space;
    iterator* iter = db_->newiterator(readoptions());
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      uint64_t key;
      slice in(iter->key());
      if (in == "" || in == "~") {
        // ignore boundary keys.
        continue;
      }
      if (!consumedecimalnumber(&in, &key) ||
          !in.empty() ||
          key < next_expected) {
        bad_keys++;
        continue;
      }
      missed += (key - next_expected);
      next_expected = key + 1;
      if (iter->value() != value(key, &value_space)) {
        bad_values++;
      } else {
        correct++;
      }
    }
    delete iter;

    fprintf(stderr,
            "expected=%d..%d; got=%d; bad_keys=%d; bad_values=%d; missed=%d\n",
            min_expected, max_expected, correct, bad_keys, bad_values, missed);
    assert_le(min_expected, correct);
    assert_ge(max_expected, correct);
  }

  void corrupt(filetype filetype, int offset, int bytes_to_corrupt) {
    // pick file to corrupt
    std::vector<std::string> filenames;
    assert_ok(env_.getchildren(dbname_, &filenames));
    uint64_t number;
    filetype type;
    std::string fname;
    int picked_number = -1;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type) &&
          type == filetype &&
          int(number) > picked_number) {  // pick latest file
        fname = dbname_ + "/" + filenames[i];
        picked_number = number;
      }
    }
    assert_true(!fname.empty()) << filetype;

    struct stat sbuf;
    if (stat(fname.c_str(), &sbuf) != 0) {
      const char* msg = strerror(errno);
      assert_true(false) << fname << ": " << msg;
    }

    if (offset < 0) {
      // relative to end of file; make it absolute
      if (-offset > sbuf.st_size) {
        offset = 0;
      } else {
        offset = sbuf.st_size + offset;
      }
    }
    if (offset > sbuf.st_size) {
      offset = sbuf.st_size;
    }
    if (offset + bytes_to_corrupt > sbuf.st_size) {
      bytes_to_corrupt = sbuf.st_size - offset;
    }

    // do it
    std::string contents;
    status s = readfiletostring(env::default(), fname, &contents);
    assert_true(s.ok()) << s.tostring();
    for (int i = 0; i < bytes_to_corrupt; i++) {
      contents[i + offset] ^= 0x80;
    }
    s = writestringtofile(env::default(), contents, fname);
    assert_true(s.ok()) << s.tostring();
  }

  int property(const std::string& name) {
    std::string property;
    int result;
    if (db_->getproperty(name, &property) &&
        sscanf(property.c_str(), "%d", &result) == 1) {
      return result;
    } else {
      return -1;
    }
  }

  // return the ith key
  slice key(int i, std::string* storage) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%016d", i);
    storage->assign(buf, strlen(buf));
    return slice(*storage);
  }

  // return the value to associate with the specified key
  slice value(int k, std::string* storage) {
    random r(k);
    return test::randomstring(&r, kvaluesize, storage);
  }
};

test(corruptiontest, recovery) {
  build(100);
  check(100, 100);
  corrupt(klogfile, 19, 1);      // writebatch tag for first record
  corrupt(klogfile, log::kblocksize + 1000, 1);  // somewhere in second block
  reopen();

  // the 64 records in the first two log blocks are completely lost.
  check(36, 36);
}

test(corruptiontest, recoverwriteerror) {
  env_.writable_file_error_ = true;
  status s = tryreopen();
  assert_true(!s.ok());
}

test(corruptiontest, newfileerrorduringwrite) {
  // do enough writing to force minor compaction
  env_.writable_file_error_ = true;
  const int num = 3 + (options().write_buffer_size / kvaluesize);
  std::string value_storage;
  status s;
  for (int i = 0; s.ok() && i < num; i++) {
    writebatch batch;
    batch.put("a", value(100, &value_storage));
    s = db_->write(writeoptions(), &batch);
  }
  assert_true(!s.ok());
  assert_ge(env_.num_writable_file_errors_, 1);
  env_.writable_file_error_ = false;
  reopen();
}

test(corruptiontest, tablefile) {
  build(100);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_compactmemtable();
  dbi->test_compactrange(0, null, null);
  dbi->test_compactrange(1, null, null);

  corrupt(ktablefile, 100, 1);
  check(90, 99);
}

test(corruptiontest, tablefilerepair) {
  options_.block_size = 2 * kvaluesize;  // limit scope of corruption
  options_.paranoid_checks = true;
  reopen();
  build(100);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_compactmemtable();
  dbi->test_compactrange(0, null, null);
  dbi->test_compactrange(1, null, null);

  corrupt(ktablefile, 100, 1);
  repairdb();
  reopen();
  check(95, 99);
}

test(corruptiontest, tablefileindexdata) {
  build(10000);  // enough to build multiple tables
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_compactmemtable();

  corrupt(ktablefile, -2000, 500);
  reopen();
  check(5000, 9999);
}

test(corruptiontest, missingdescriptor) {
  build(1000);
  repairdb();
  reopen();
  check(1000, 1000);
}

test(corruptiontest, sequencenumberrecovery) {
  assert_ok(db_->put(writeoptions(), "foo", "v1"));
  assert_ok(db_->put(writeoptions(), "foo", "v2"));
  assert_ok(db_->put(writeoptions(), "foo", "v3"));
  assert_ok(db_->put(writeoptions(), "foo", "v4"));
  assert_ok(db_->put(writeoptions(), "foo", "v5"));
  repairdb();
  reopen();
  std::string v;
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("v5", v);
  // write something.  if sequence number was not recovered properly,
  // it will be hidden by an earlier write.
  assert_ok(db_->put(writeoptions(), "foo", "v6"));
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("v6", v);
  reopen();
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("v6", v);
}

test(corruptiontest, corrupteddescriptor) {
  assert_ok(db_->put(writeoptions(), "foo", "hello"));
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_compactmemtable();
  dbi->test_compactrange(0, null, null);

  corrupt(kdescriptorfile, 0, 1000);
  status s = tryreopen();
  assert_true(!s.ok());

  repairdb();
  reopen();
  std::string v;
  assert_ok(db_->get(readoptions(), "foo", &v));
  assert_eq("hello", v);
}

test(corruptiontest, compactioninputerror) {
  build(10);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_compactmemtable();
  const int last = config::kmaxmemcompactlevel;
  assert_eq(1, property("leveldb.num-files-at-level" + numbertostring(last)));

  corrupt(ktablefile, 100, 1);
  check(5, 9);

  // force compactions by writing lots of values
  build(10000);
  check(10000, 10000);
}

test(corruptiontest, compactioninputerrorparanoid) {
  options_.paranoid_checks = true;
  options_.write_buffer_size = 512 << 10;
  reopen();
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);

  // make multiple inputs so we need to compact.
  for (int i = 0; i < 2; i++) {
    build(10);
    dbi->test_compactmemtable();
    corrupt(ktablefile, 100, 1);
    env_.sleepformicroseconds(100000);
  }
  dbi->compactrange(null, null);

  // write must fail because of corrupted table
  std::string tmp1, tmp2;
  status s = db_->put(writeoptions(), key(5, &tmp1), value(5, &tmp2));
  assert_true(!s.ok()) << "write did not fail in corrupted paranoid db";
}

test(corruptiontest, unrelatedkeys) {
  build(10);
  dbimpl* dbi = reinterpret_cast<dbimpl*>(db_);
  dbi->test_compactmemtable();
  corrupt(ktablefile, 100, 1);

  std::string tmp1, tmp2;
  assert_ok(db_->put(writeoptions(), key(1000, &tmp1), value(1000, &tmp2)));
  std::string v;
  assert_ok(db_->get(readoptions(), key(1000, &tmp1), &v));
  assert_eq(value(1000, &tmp2).tostring(), v);
  dbi->test_compactmemtable();
  assert_ok(db_->get(readoptions(), key(1000, &tmp1), &v));
  assert_eq(value(1000, &tmp2).tostring(), v);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
