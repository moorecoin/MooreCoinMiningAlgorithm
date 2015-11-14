// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_version_edit_h_
#define storage_leveldb_db_version_edit_h_

#include <set>
#include <utility>
#include <vector>
#include "db/dbformat.h"

namespace leveldb {

class versionset;

struct filemetadata {
  int refs;
  int allowed_seeks;          // seeks allowed until compaction
  uint64_t number;
  uint64_t file_size;         // file size in bytes
  internalkey smallest;       // smallest internal key served by table
  internalkey largest;        // largest internal key served by table

  filemetadata() : refs(0), allowed_seeks(1 << 30), file_size(0) { }
};

class versionedit {
 public:
  versionedit() { clear(); }
  ~versionedit() { }

  void clear();

  void setcomparatorname(const slice& name) {
    has_comparator_ = true;
    comparator_ = name.tostring();
  }
  void setlognumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  void setprevlognumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void setnextfile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void setlastsequence(sequencenumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }
  void setcompactpointer(int level, const internalkey& key) {
    compact_pointers_.push_back(std::make_pair(level, key));
  }

  // add the specified file at the specified number.
  // requires: this version has not been saved (see versionset::saveto)
  // requires: "smallest" and "largest" are smallest and largest keys in file
  void addfile(int level, uint64_t file,
               uint64_t file_size,
               const internalkey& smallest,
               const internalkey& largest) {
    filemetadata f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
    new_files_.push_back(std::make_pair(level, f));
  }

  // delete the specified "file" from the specified "level".
  void deletefile(int level, uint64_t file) {
    deleted_files_.insert(std::make_pair(level, file));
  }

  void encodeto(std::string* dst) const;
  status decodefrom(const slice& src);

  std::string debugstring() const;

 private:
  friend class versionset;

  typedef std::set< std::pair<int, uint64_t> > deletedfileset;

  std::string comparator_;
  uint64_t log_number_;
  uint64_t prev_log_number_;
  uint64_t next_file_number_;
  sequencenumber last_sequence_;
  bool has_comparator_;
  bool has_log_number_;
  bool has_prev_log_number_;
  bool has_next_file_number_;
  bool has_last_sequence_;

  std::vector< std::pair<int, internalkey> > compact_pointers_;
  deletedfileset deleted_files_;
  std::vector< std::pair<int, filemetadata> > new_files_;
};

}  // namespace leveldb

#endif  // storage_leveldb_db_version_edit_h_
