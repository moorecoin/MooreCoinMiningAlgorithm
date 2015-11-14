// copyright (c) 2013 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

// test for issue 178: a manual compaction causes deleted data to reappear.
#include <iostream>
#include <sstream>
#include <cstdlib>

#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "util/testharness.h"

namespace {

const int knumkeys = 1100000;

std::string key1(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "my_key_%d", i);
  return buf;
}

std::string key2(int i) {
  return key1(i) + "_xxx";
}

class issue178 { };

test(issue178, test) {
  // get rid of any state from an old run.
  std::string dbpath = leveldb::test::tmpdir() + "/leveldb_cbug_test";
  destroydb(dbpath, leveldb::options());

  // open database.  disable compression since it affects the creation
  // of layers and the code below is trying to test against a very
  // specific scenario.
  leveldb::db* db;
  leveldb::options db_options;
  db_options.create_if_missing = true;
  db_options.compression = leveldb::knocompression;
  assert_ok(leveldb::db::open(db_options, dbpath, &db));

  // create first key range
  leveldb::writebatch batch;
  for (size_t i = 0; i < knumkeys; i++) {
    batch.put(key1(i), "value for range 1 key");
  }
  assert_ok(db->write(leveldb::writeoptions(), &batch));

  // create second key range
  batch.clear();
  for (size_t i = 0; i < knumkeys; i++) {
    batch.put(key2(i), "value for range 2 key");
  }
  assert_ok(db->write(leveldb::writeoptions(), &batch));

  // delete second key range
  batch.clear();
  for (size_t i = 0; i < knumkeys; i++) {
    batch.delete(key2(i));
  }
  assert_ok(db->write(leveldb::writeoptions(), &batch));

  // compact database
  std::string start_key = key1(0);
  std::string end_key = key1(knumkeys - 1);
  leveldb::slice least(start_key.data(), start_key.size());
  leveldb::slice greatest(end_key.data(), end_key.size());

  // commenting out the line below causes the example to work correctly
  db->compactrange(&least, &greatest);

  // count the keys
  leveldb::iterator* iter = db->newiterator(leveldb::readoptions());
  size_t num_keys = 0;
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    num_keys++;
  }
  delete iter;
  assert_eq(knumkeys, num_keys) << "bad number of keys";

  // close database
  delete db;
  destroydb(dbpath, leveldb::options());
}

}  // anonymous namespace

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
