// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_edit.h"

#include "db/version_set.h"
#include "util/coding.h"

namespace leveldb {

// tag numbers for serialized versionedit.  these numbers are written to
// disk and should not be changed.
enum tag {
  kcomparator           = 1,
  klognumber            = 2,
  knextfilenumber       = 3,
  klastsequence         = 4,
  kcompactpointer       = 5,
  kdeletedfile          = 6,
  knewfile              = 7,
  // 8 was used for large value refs
  kprevlognumber        = 9
};

void versionedit::clear() {
  comparator_.clear();
  log_number_ = 0;
  prev_log_number_ = 0;
  last_sequence_ = 0;
  next_file_number_ = 0;
  has_comparator_ = false;
  has_log_number_ = false;
  has_prev_log_number_ = false;
  has_next_file_number_ = false;
  has_last_sequence_ = false;
  deleted_files_.clear();
  new_files_.clear();
}

void versionedit::encodeto(std::string* dst) const {
  if (has_comparator_) {
    putvarint32(dst, kcomparator);
    putlengthprefixedslice(dst, comparator_);
  }
  if (has_log_number_) {
    putvarint32(dst, klognumber);
    putvarint64(dst, log_number_);
  }
  if (has_prev_log_number_) {
    putvarint32(dst, kprevlognumber);
    putvarint64(dst, prev_log_number_);
  }
  if (has_next_file_number_) {
    putvarint32(dst, knextfilenumber);
    putvarint64(dst, next_file_number_);
  }
  if (has_last_sequence_) {
    putvarint32(dst, klastsequence);
    putvarint64(dst, last_sequence_);
  }

  for (size_t i = 0; i < compact_pointers_.size(); i++) {
    putvarint32(dst, kcompactpointer);
    putvarint32(dst, compact_pointers_[i].first);  // level
    putlengthprefixedslice(dst, compact_pointers_[i].second.encode());
  }

  for (deletedfileset::const_iterator iter = deleted_files_.begin();
       iter != deleted_files_.end();
       ++iter) {
    putvarint32(dst, kdeletedfile);
    putvarint32(dst, iter->first);   // level
    putvarint64(dst, iter->second);  // file number
  }

  for (size_t i = 0; i < new_files_.size(); i++) {
    const filemetadata& f = new_files_[i].second;
    putvarint32(dst, knewfile);
    putvarint32(dst, new_files_[i].first);  // level
    putvarint64(dst, f.number);
    putvarint64(dst, f.file_size);
    putlengthprefixedslice(dst, f.smallest.encode());
    putlengthprefixedslice(dst, f.largest.encode());
  }
}

static bool getinternalkey(slice* input, internalkey* dst) {
  slice str;
  if (getlengthprefixedslice(input, &str)) {
    dst->decodefrom(str);
    return true;
  } else {
    return false;
  }
}

static bool getlevel(slice* input, int* level) {
  uint32_t v;
  if (getvarint32(input, &v) &&
      v < config::knumlevels) {
    *level = v;
    return true;
  } else {
    return false;
  }
}

status versionedit::decodefrom(const slice& src) {
  clear();
  slice input = src;
  const char* msg = null;
  uint32_t tag;

  // temporary storage for parsing
  int level;
  uint64_t number;
  filemetadata f;
  slice str;
  internalkey key;

  while (msg == null && getvarint32(&input, &tag)) {
    switch (tag) {
      case kcomparator:
        if (getlengthprefixedslice(&input, &str)) {
          comparator_ = str.tostring();
          has_comparator_ = true;
        } else {
          msg = "comparator name";
        }
        break;

      case klognumber:
        if (getvarint64(&input, &log_number_)) {
          has_log_number_ = true;
        } else {
          msg = "log number";
        }
        break;

      case kprevlognumber:
        if (getvarint64(&input, &prev_log_number_)) {
          has_prev_log_number_ = true;
        } else {
          msg = "previous log number";
        }
        break;

      case knextfilenumber:
        if (getvarint64(&input, &next_file_number_)) {
          has_next_file_number_ = true;
        } else {
          msg = "next file number";
        }
        break;

      case klastsequence:
        if (getvarint64(&input, &last_sequence_)) {
          has_last_sequence_ = true;
        } else {
          msg = "last sequence number";
        }
        break;

      case kcompactpointer:
        if (getlevel(&input, &level) &&
            getinternalkey(&input, &key)) {
          compact_pointers_.push_back(std::make_pair(level, key));
        } else {
          msg = "compaction pointer";
        }
        break;

      case kdeletedfile:
        if (getlevel(&input, &level) &&
            getvarint64(&input, &number)) {
          deleted_files_.insert(std::make_pair(level, number));
        } else {
          msg = "deleted file";
        }
        break;

      case knewfile:
        if (getlevel(&input, &level) &&
            getvarint64(&input, &f.number) &&
            getvarint64(&input, &f.file_size) &&
            getinternalkey(&input, &f.smallest) &&
            getinternalkey(&input, &f.largest)) {
          new_files_.push_back(std::make_pair(level, f));
        } else {
          msg = "new-file entry";
        }
        break;

      default:
        msg = "unknown tag";
        break;
    }
  }

  if (msg == null && !input.empty()) {
    msg = "invalid tag";
  }

  status result;
  if (msg != null) {
    result = status::corruption("versionedit", msg);
  }
  return result;
}

std::string versionedit::debugstring() const {
  std::string r;
  r.append("versionedit {");
  if (has_comparator_) {
    r.append("\n  comparator: ");
    r.append(comparator_);
  }
  if (has_log_number_) {
    r.append("\n  lognumber: ");
    appendnumberto(&r, log_number_);
  }
  if (has_prev_log_number_) {
    r.append("\n  prevlognumber: ");
    appendnumberto(&r, prev_log_number_);
  }
  if (has_next_file_number_) {
    r.append("\n  nextfile: ");
    appendnumberto(&r, next_file_number_);
  }
  if (has_last_sequence_) {
    r.append("\n  lastseq: ");
    appendnumberto(&r, last_sequence_);
  }
  for (size_t i = 0; i < compact_pointers_.size(); i++) {
    r.append("\n  compactpointer: ");
    appendnumberto(&r, compact_pointers_[i].first);
    r.append(" ");
    r.append(compact_pointers_[i].second.debugstring());
  }
  for (deletedfileset::const_iterator iter = deleted_files_.begin();
       iter != deleted_files_.end();
       ++iter) {
    r.append("\n  deletefile: ");
    appendnumberto(&r, iter->first);
    r.append(" ");
    appendnumberto(&r, iter->second);
  }
  for (size_t i = 0; i < new_files_.size(); i++) {
    const filemetadata& f = new_files_[i].second;
    r.append("\n  addfile: ");
    appendnumberto(&r, new_files_[i].first);
    r.append(" ");
    appendnumberto(&r, f.number);
    r.append(" ");
    appendnumberto(&r, f.file_size);
    r.append(" ");
    r.append(f.smallest.debugstring());
    r.append(" .. ");
    r.append(f.largest.debugstring());
  }
  r.append("\n}\n");
  return r;
}

}  // namespace leveldb
