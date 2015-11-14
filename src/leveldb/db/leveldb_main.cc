// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include "leveldb/dumpfile.h"
#include "leveldb/env.h"
#include "leveldb/status.h"

namespace leveldb {
namespace {

class stdoutprinter : public writablefile {
 public:
  virtual status append(const slice& data) {
    fwrite(data.data(), 1, data.size(), stdout);
    return status::ok();
  }
  virtual status close() { return status::ok(); }
  virtual status flush() { return status::ok(); }
  virtual status sync() { return status::ok(); }
};

bool handledumpcommand(env* env, char** files, int num) {
  stdoutprinter printer;
  bool ok = true;
  for (int i = 0; i < num; i++) {
    status s = dumpfile(env, files[i], &printer);
    if (!s.ok()) {
      fprintf(stderr, "%s\n", s.tostring().c_str());
      ok = false;
    }
  }
  return ok;
}

}  // namespace
}  // namespace leveldb

static void usage() {
  fprintf(
      stderr,
      "usage: leveldbutil command...\n"
      "   dump files...         -- dump contents of specified files\n"
      );
}

int main(int argc, char** argv) {
  leveldb::env* env = leveldb::env::default();
  bool ok = true;
  if (argc < 2) {
    usage();
    ok = false;
  } else {
    std::string command = argv[1];
    if (command == "dump") {
      ok = leveldb::handledumpcommand(env, argv+2, argc-2);
    } else {
      usage();
      ok = false;
    }
  }
  return (ok ? 0 : 1);
}
