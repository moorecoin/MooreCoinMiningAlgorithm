// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_helpers_memenv_memenv_h_
#define storage_leveldb_helpers_memenv_memenv_h_

namespace leveldb {

class env;

// returns a new environment that stores its data in memory and delegates
// all non-file-storage tasks to base_env. the caller must delete the result
// when it is no longer needed.
// *base_env must remain live while the result is in use.
env* newmemenv(env* base_env);

}  // namespace leveldb

#endif  // storage_leveldb_helpers_memenv_memenv_h_
