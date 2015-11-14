// copyright (c) 2014 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_include_dumpfile_h_
#define storage_leveldb_include_dumpfile_h_

#include <string>
#include "leveldb/env.h"
#include "leveldb/status.h"

namespace leveldb {

// dump the contents of the file named by fname in text format to
// *dst.  makes a sequence of dst->append() calls; each call is passed
// the newline-terminated text corresponding to a single item found
// in the file.
//
// returns a non-ok result if fname does not name a leveldb storage
// file, or if the file cannot be read.
status dumpfile(env* env, const std::string& fname, writablefile* dst);

}  // namespace leveldb

#endif  // storage_leveldb_include_dumpfile_h_
