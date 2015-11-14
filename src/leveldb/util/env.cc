// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/env.h"

namespace leveldb {

env::~env() {
}

sequentialfile::~sequentialfile() {
}

randomaccessfile::~randomaccessfile() {
}

writablefile::~writablefile() {
}

logger::~logger() {
}

filelock::~filelock() {
}

void log(logger* info_log, const char* format, ...) {
  if (info_log != null) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(format, ap);
    va_end(ap);
  }
}

static status dowritestringtofile(env* env, const slice& data,
                                  const std::string& fname,
                                  bool should_sync) {
  writablefile* file;
  status s = env->newwritablefile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  s = file->append(data);
  if (s.ok() && should_sync) {
    s = file->sync();
  }
  if (s.ok()) {
    s = file->close();
  }
  delete file;  // will auto-close if we did not close above
  if (!s.ok()) {
    env->deletefile(fname);
  }
  return s;
}

status writestringtofile(env* env, const slice& data,
                         const std::string& fname) {
  return dowritestringtofile(env, data, fname, false);
}

status writestringtofilesync(env* env, const slice& data,
                             const std::string& fname) {
  return dowritestringtofile(env, data, fname, true);
}

status readfiletostring(env* env, const std::string& fname, std::string* data) {
  data->clear();
  sequentialfile* file;
  status s = env->newsequentialfile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  static const int kbuffersize = 8192;
  char* space = new char[kbuffersize];
  while (true) {
    slice fragment;
    s = file->read(kbuffersize, &fragment, space);
    if (!s.ok()) {
      break;
    }
    data->append(fragment.data(), fragment.size());
    if (fragment.empty()) {
      break;
    }
  }
  delete[] space;
  delete file;
  return s;
}

envwrapper::~envwrapper() {
}

}  // namespace leveldb
