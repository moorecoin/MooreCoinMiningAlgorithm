// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// file names used by db code

#ifndef storage_leveldb_db_filename_h_
#define storage_leveldb_db_filename_h_

#include <stdint.h>
#include <string>
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"

namespace leveldb {

class env;

enum filetype {
  klogfile,
  kdblockfile,
  ktablefile,
  kdescriptorfile,
  kcurrentfile,
  ktempfile,
  kinfologfile  // either the current one, or an old one
};

// return the name of the log file with the specified number
// in the db named by "dbname".  the result will be prefixed with
// "dbname".
extern std::string logfilename(const std::string& dbname, uint64_t number);

// return the name of the sstable with the specified number
// in the db named by "dbname".  the result will be prefixed with
// "dbname".
extern std::string tablefilename(const std::string& dbname, uint64_t number);

// return the legacy file name for an sstable with the specified number
// in the db named by "dbname". the result will be prefixed with
// "dbname".
extern std::string ssttablefilename(const std::string& dbname, uint64_t number);

// return the name of the descriptor file for the db named by
// "dbname" and the specified incarnation number.  the result will be
// prefixed with "dbname".
extern std::string descriptorfilename(const std::string& dbname,
                                      uint64_t number);

// return the name of the current file.  this file contains the name
// of the current manifest file.  the result will be prefixed with
// "dbname".
extern std::string currentfilename(const std::string& dbname);

// return the name of the lock file for the db named by
// "dbname".  the result will be prefixed with "dbname".
extern std::string lockfilename(const std::string& dbname);

// return the name of a temporary file owned by the db named "dbname".
// the result will be prefixed with "dbname".
extern std::string tempfilename(const std::string& dbname, uint64_t number);

// return the name of the info log file for "dbname".
extern std::string infologfilename(const std::string& dbname);

// return the name of the old info log file for "dbname".
extern std::string oldinfologfilename(const std::string& dbname);

// if filename is a leveldb file, store the type of the file in *type.
// the number encoded in the filename is stored in *number.  if the
// filename was successfully parsed, returns true.  else return false.
extern bool parsefilename(const std::string& filename,
                          uint64_t* number,
                          filetype* type);

// make the current file point to the descriptor file with the
// specified number.
extern status setcurrentfile(env* env, const std::string& dbname,
                             uint64_t descriptor_number);


}  // namespace leveldb

#endif  // storage_leveldb_db_filename_h_
