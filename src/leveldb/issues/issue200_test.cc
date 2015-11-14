// copyright (c) 2013 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

// test for issue 200: when iterator switches direction from backward
// to forward, the current key can be yielded unexpectedly if a new
// mutation has been added just before the current key.

#include "leveldb/db.h"
#include "util/testharness.h"

namespace leveldb {

class issue200 { };

test(issue200, test) {
  // get rid of any state from an old run.
  std::string dbpath = test::tmpdir() + "/leveldb_issue200_test";
  destroydb(dbpath, options());

  db *db;
  options options;
  options.create_if_missing = true;
  assert_ok(db::open(options, dbpath, &db));

  writeoptions write_options;
  assert_ok(db->put(write_options, "1", "b"));
  assert_ok(db->put(write_options, "2", "c"));
  assert_ok(db->put(write_options, "3", "d"));
  assert_ok(db->put(write_options, "4", "e"));
  assert_ok(db->put(write_options, "5", "f"));

  readoptions read_options;
  iterator *iter = db->newiterator(read_options);

  // add an element that should not be reflected in the iterator.
  assert_ok(db->put(write_options, "25", "cd"));

  iter->seek("5");
  assert_eq(iter->key().tostring(), "5");
  iter->prev();
  assert_eq(iter->key().tostring(), "4");
  iter->prev();
  assert_eq(iter->key().tostring(), "3");
  iter->next();
  assert_eq(iter->key().tostring(), "4");
  iter->next();
  assert_eq(iter->key().tostring(), "5");

  delete iter;
  delete db;
  destroydb(dbpath, options);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
