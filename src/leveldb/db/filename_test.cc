// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/filename.h"

#include "db/dbformat.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/testharness.h"

namespace leveldb {

class filenametest { };

test(filenametest, parse) {
  slice db;
  filetype type;
  uint64_t number;

  // successful parses
  static struct {
    const char* fname;
    uint64_t number;
    filetype type;
  } cases[] = {
    { "100.log",            100,   klogfile },
    { "0.log",              0,     klogfile },
    { "0.sst",              0,     ktablefile },
    { "0.ldb",              0,     ktablefile },
    { "current",            0,     kcurrentfile },
    { "lock",               0,     kdblockfile },
    { "manifest-2",         2,     kdescriptorfile },
    { "manifest-7",         7,     kdescriptorfile },
    { "log",                0,     kinfologfile },
    { "log.old",            0,     kinfologfile },
    { "18446744073709551615.log", 18446744073709551615ull, klogfile },
  };
  for (int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    std::string f = cases[i].fname;
    assert_true(parsefilename(f, &number, &type)) << f;
    assert_eq(cases[i].type, type) << f;
    assert_eq(cases[i].number, number) << f;
  }

  // errors
  static const char* errors[] = {
    "",
    "foo",
    "foo-dx-100.log",
    ".log",
    "",
    "manifest",
    "curren",
    "currentx",
    "manifes",
    "manifest",
    "manifest-",
    "xmanifest-3",
    "manifest-3x",
    "loc",
    "lockx",
    "lo",
    "logx",
    "18446744073709551616.log",
    "184467440737095516150.log",
    "100",
    "100.",
    "100.lop"
  };
  for (int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
    std::string f = errors[i];
    assert_true(!parsefilename(f, &number, &type)) << f;
  }
}

test(filenametest, construction) {
  uint64_t number;
  filetype type;
  std::string fname;

  fname = currentfilename("foo");
  assert_eq("foo/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(0, number);
  assert_eq(kcurrentfile, type);

  fname = lockfilename("foo");
  assert_eq("foo/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(0, number);
  assert_eq(kdblockfile, type);

  fname = logfilename("foo", 192);
  assert_eq("foo/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(192, number);
  assert_eq(klogfile, type);

  fname = tablefilename("bar", 200);
  assert_eq("bar/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(200, number);
  assert_eq(ktablefile, type);

  fname = descriptorfilename("bar", 100);
  assert_eq("bar/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(100, number);
  assert_eq(kdescriptorfile, type);

  fname = tempfilename("tmp", 999);
  assert_eq("tmp/", std::string(fname.data(), 4));
  assert_true(parsefilename(fname.c_str() + 4, &number, &type));
  assert_eq(999, number);
  assert_eq(ktempfile, type);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
