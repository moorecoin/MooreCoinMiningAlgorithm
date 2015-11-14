// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <ctype.h>
#include <stdio.h>
#include "db/filename.h"
#include "db/dbformat.h"
#include "leveldb/env.h"
#include "util/logging.h"

namespace leveldb {

// a utility routine: write "data" to the named file and sync() it.
extern status writestringtofilesync(env* env, const slice& data,
                                    const std::string& fname);

static std::string makefilename(const std::string& name, uint64_t number,
                                const char* suffix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "/%06llu.%s",
           static_cast<unsigned long long>(number),
           suffix);
  return name + buf;
}

std::string logfilename(const std::string& name, uint64_t number) {
  assert(number > 0);
  return makefilename(name, number, "log");
}

std::string tablefilename(const std::string& name, uint64_t number) {
  assert(number > 0);
  return makefilename(name, number, "ldb");
}

std::string ssttablefilename(const std::string& name, uint64_t number) {
  assert(number > 0);
  return makefilename(name, number, "sst");
}

std::string descriptorfilename(const std::string& dbname, uint64_t number) {
  assert(number > 0);
  char buf[100];
  snprintf(buf, sizeof(buf), "/manifest-%06llu",
           static_cast<unsigned long long>(number));
  return dbname + buf;
}

std::string currentfilename(const std::string& dbname) {
  return dbname + "/current";
}

std::string lockfilename(const std::string& dbname) {
  return dbname + "/lock";
}

std::string tempfilename(const std::string& dbname, uint64_t number) {
  assert(number > 0);
  return makefilename(dbname, number, "dbtmp");
}

std::string infologfilename(const std::string& dbname) {
  return dbname + "/log";
}

// return the name of the old info log file for "dbname".
std::string oldinfologfilename(const std::string& dbname) {
  return dbname + "/log.old";
}


// owned filenames have the form:
//    dbname/current
//    dbname/lock
//    dbname/log
//    dbname/log.old
//    dbname/manifest-[0-9]+
//    dbname/[0-9]+.(log|sst|ldb)
bool parsefilename(const std::string& fname,
                   uint64_t* number,
                   filetype* type) {
  slice rest(fname);
  if (rest == "current") {
    *number = 0;
    *type = kcurrentfile;
  } else if (rest == "lock") {
    *number = 0;
    *type = kdblockfile;
  } else if (rest == "log" || rest == "log.old") {
    *number = 0;
    *type = kinfologfile;
  } else if (rest.starts_with("manifest-")) {
    rest.remove_prefix(strlen("manifest-"));
    uint64_t num;
    if (!consumedecimalnumber(&rest, &num)) {
      return false;
    }
    if (!rest.empty()) {
      return false;
    }
    *type = kdescriptorfile;
    *number = num;
  } else {
    // avoid strtoull() to keep filename format independent of the
    // current locale
    uint64_t num;
    if (!consumedecimalnumber(&rest, &num)) {
      return false;
    }
    slice suffix = rest;
    if (suffix == slice(".log")) {
      *type = klogfile;
    } else if (suffix == slice(".sst") || suffix == slice(".ldb")) {
      *type = ktablefile;
    } else if (suffix == slice(".dbtmp")) {
      *type = ktempfile;
    } else {
      return false;
    }
    *number = num;
  }
  return true;
}

status setcurrentfile(env* env, const std::string& dbname,
                      uint64_t descriptor_number) {
  // remove leading "dbname/" and add newline to manifest file name
  std::string manifest = descriptorfilename(dbname, descriptor_number);
  slice contents = manifest;
  assert(contents.starts_with(dbname + "/"));
  contents.remove_prefix(dbname.size() + 1);
  std::string tmp = tempfilename(dbname, descriptor_number);
  status s = writestringtofilesync(env, contents.tostring() + "\n", tmp);
  if (s.ok()) {
    s = env->renamefile(tmp, currentfilename(dbname));
  }
  if (!s.ok()) {
    env->deletefile(tmp);
  }
  return s;
}

}  // namespace leveldb
