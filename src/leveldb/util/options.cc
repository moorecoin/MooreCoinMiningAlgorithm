// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/options.h"

#include "leveldb/comparator.h"
#include "leveldb/env.h"

namespace leveldb {

options::options()
    : comparator(bytewisecomparator()),
      create_if_missing(false),
      error_if_exists(false),
      paranoid_checks(false),
      env(env::default()),
      info_log(null),
      write_buffer_size(4<<20),
      max_open_files(1000),
      block_cache(null),
      block_size(4096),
      block_restart_interval(16),
      compression(ksnappycompression),
      filter_policy(null) {
}


}  // namespace leveldb
