// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "helpers/memenv/memenv.h"

#include "db/db_impl.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "util/testharness.h"
#include <string>
#include <vector>

namespace leveldb {

class memenvtest {
 public:
  env* env_;

  memenvtest()
      : env_(newmemenv(env::default())) {
  }
  ~memenvtest() {
    delete env_;
  }
};

test(memenvtest, basics) {
  uint64_t file_size;
  writablefile* writable_file;
  std::vector<std::string> children;

  assert_ok(env_->createdir("/dir"));

  // check that the directory is empty.
  assert_true(!env_->fileexists("/dir/non_existent"));
  assert_true(!env_->getfilesize("/dir/non_existent", &file_size).ok());
  assert_ok(env_->getchildren("/dir", &children));
  assert_eq(0, children.size());

  // create a file.
  assert_ok(env_->newwritablefile("/dir/f", &writable_file));
  delete writable_file;

  // check that the file exists.
  assert_true(env_->fileexists("/dir/f"));
  assert_ok(env_->getfilesize("/dir/f", &file_size));
  assert_eq(0, file_size);
  assert_ok(env_->getchildren("/dir", &children));
  assert_eq(1, children.size());
  assert_eq("f", children[0]);

  // write to the file.
  assert_ok(env_->newwritablefile("/dir/f", &writable_file));
  assert_ok(writable_file->append("abc"));
  delete writable_file;

  // check for expected size.
  assert_ok(env_->getfilesize("/dir/f", &file_size));
  assert_eq(3, file_size);

  // check that renaming works.
  assert_true(!env_->renamefile("/dir/non_existent", "/dir/g").ok());
  assert_ok(env_->renamefile("/dir/f", "/dir/g"));
  assert_true(!env_->fileexists("/dir/f"));
  assert_true(env_->fileexists("/dir/g"));
  assert_ok(env_->getfilesize("/dir/g", &file_size));
  assert_eq(3, file_size);

  // check that opening non-existent file fails.
  sequentialfile* seq_file;
  randomaccessfile* rand_file;
  assert_true(!env_->newsequentialfile("/dir/non_existent", &seq_file).ok());
  assert_true(!seq_file);
  assert_true(!env_->newrandomaccessfile("/dir/non_existent", &rand_file).ok());
  assert_true(!rand_file);

  // check that deleting works.
  assert_true(!env_->deletefile("/dir/non_existent").ok());
  assert_ok(env_->deletefile("/dir/g"));
  assert_true(!env_->fileexists("/dir/g"));
  assert_ok(env_->getchildren("/dir", &children));
  assert_eq(0, children.size());
  assert_ok(env_->deletedir("/dir"));
}

test(memenvtest, readwrite) {
  writablefile* writable_file;
  sequentialfile* seq_file;
  randomaccessfile* rand_file;
  slice result;
  char scratch[100];

  assert_ok(env_->createdir("/dir"));

  assert_ok(env_->newwritablefile("/dir/f", &writable_file));
  assert_ok(writable_file->append("hello "));
  assert_ok(writable_file->append("world"));
  delete writable_file;

  // read sequentially.
  assert_ok(env_->newsequentialfile("/dir/f", &seq_file));
  assert_ok(seq_file->read(5, &result, scratch)); // read "hello".
  assert_eq(0, result.compare("hello"));
  assert_ok(seq_file->skip(1));
  assert_ok(seq_file->read(1000, &result, scratch)); // read "world".
  assert_eq(0, result.compare("world"));
  assert_ok(seq_file->read(1000, &result, scratch)); // try reading past eof.
  assert_eq(0, result.size());
  assert_ok(seq_file->skip(100)); // try to skip past end of file.
  assert_ok(seq_file->read(1000, &result, scratch));
  assert_eq(0, result.size());
  delete seq_file;

  // random reads.
  assert_ok(env_->newrandomaccessfile("/dir/f", &rand_file));
  assert_ok(rand_file->read(6, 5, &result, scratch)); // read "world".
  assert_eq(0, result.compare("world"));
  assert_ok(rand_file->read(0, 5, &result, scratch)); // read "hello".
  assert_eq(0, result.compare("hello"));
  assert_ok(rand_file->read(10, 100, &result, scratch)); // read "d".
  assert_eq(0, result.compare("d"));

  // too high offset.
  assert_true(!rand_file->read(1000, 5, &result, scratch).ok());
  delete rand_file;
}

test(memenvtest, locks) {
  filelock* lock;

  // these are no-ops, but we test they return success.
  assert_ok(env_->lockfile("some file", &lock));
  assert_ok(env_->unlockfile(lock));
}

test(memenvtest, misc) {
  std::string test_dir;
  assert_ok(env_->gettestdirectory(&test_dir));
  assert_true(!test_dir.empty());

  writablefile* writable_file;
  assert_ok(env_->newwritablefile("/a/b", &writable_file));

  // these are no-ops, but we test they return success.
  assert_ok(writable_file->sync());
  assert_ok(writable_file->flush());
  assert_ok(writable_file->close());
  delete writable_file;
}

test(memenvtest, largewrite) {
  const size_t kwritesize = 300 * 1024;
  char* scratch = new char[kwritesize * 2];

  std::string write_data;
  for (size_t i = 0; i < kwritesize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  writablefile* writable_file;
  assert_ok(env_->newwritablefile("/dir/f", &writable_file));
  assert_ok(writable_file->append("foo"));
  assert_ok(writable_file->append(write_data));
  delete writable_file;

  sequentialfile* seq_file;
  slice result;
  assert_ok(env_->newsequentialfile("/dir/f", &seq_file));
  assert_ok(seq_file->read(3, &result, scratch)); // read "foo".
  assert_eq(0, result.compare("foo"));

  size_t read = 0;
  std::string read_data;
  while (read < kwritesize) {
    assert_ok(seq_file->read(kwritesize - read, &result, scratch));
    read_data.append(result.data(), result.size());
    read += result.size();
  }
  assert_true(write_data == read_data);
  delete seq_file;
  delete [] scratch;
}

test(memenvtest, dbtest) {
  options options;
  options.create_if_missing = true;
  options.env = env_;
  db* db;

  const slice keys[] = {slice("aaa"), slice("bbb"), slice("ccc")};
  const slice vals[] = {slice("foo"), slice("bar"), slice("baz")};

  assert_ok(db::open(options, "/dir/db", &db));
  for (size_t i = 0; i < 3; ++i) {
    assert_ok(db->put(writeoptions(), keys[i], vals[i]));
  }

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    assert_ok(db->get(readoptions(), keys[i], &res));
    assert_true(res == vals[i]);
  }

  iterator* iterator = db->newiterator(readoptions());
  iterator->seektofirst();
  for (size_t i = 0; i < 3; ++i) {
    assert_true(iterator->valid());
    assert_true(keys[i] == iterator->key());
    assert_true(vals[i] == iterator->value());
    iterator->next();
  }
  assert_true(!iterator->valid());
  delete iterator;

  dbimpl* dbi = reinterpret_cast<dbimpl*>(db);
  assert_ok(dbi->test_compactmemtable());

  for (size_t i = 0; i < 3; ++i) {
    std::string res;
    assert_ok(db->get(readoptions(), keys[i], &res));
    assert_true(res == vals[i]);
  }

  delete db;
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
