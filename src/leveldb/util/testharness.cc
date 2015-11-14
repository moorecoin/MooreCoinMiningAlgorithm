// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/testharness.h"

#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace leveldb {
namespace test {

namespace {
struct test {
  const char* base;
  const char* name;
  void (*func)();
};
std::vector<test>* tests;
}

bool registertest(const char* base, const char* name, void (*func)()) {
  if (tests == null) {
    tests = new std::vector<test>;
  }
  test t;
  t.base = base;
  t.name = name;
  t.func = func;
  tests->push_back(t);
  return true;
}

int runalltests() {
  const char* matcher = getenv("leveldb_tests");

  int num = 0;
  if (tests != null) {
    for (size_t i = 0; i < tests->size(); i++) {
      const test& t = (*tests)[i];
      if (matcher != null) {
        std::string name = t.base;
        name.push_back('.');
        name.append(t.name);
        if (strstr(name.c_str(), matcher) == null) {
          continue;
        }
      }
      fprintf(stderr, "==== test %s.%s\n", t.base, t.name);
      (*t.func)();
      ++num;
    }
  }
  fprintf(stderr, "==== passed %d tests\n", num);
  return 0;
}

std::string tmpdir() {
  std::string dir;
  status s = env::default()->gettestdirectory(&dir);
  assert_true(s.ok()) << s.tostring();
  return dir;
}

int randomseed() {
  const char* env = getenv("test_random_seed");
  int result = (env != null ? atoi(env) : 301);
  if (result <= 0) {
    result = 301;
  }
  return result;
}

}  // namespace test
}  // namespace leveldb
