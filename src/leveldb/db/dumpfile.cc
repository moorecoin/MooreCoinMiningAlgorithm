// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/version_edit.h"
#include "db/write_batch_internal.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "leveldb/table.h"
#include "leveldb/write_batch.h"
#include "util/logging.h"

namespace leveldb {

namespace {

bool guesstype(const std::string& fname, filetype* type) {
  size_t pos = fname.rfind('/');
  std::string basename;
  if (pos == std::string::npos) {
    basename = fname;
  } else {
    basename = std::string(fname.data() + pos + 1, fname.size() - pos - 1);
  }
  uint64_t ignored;
  return parsefilename(basename, &ignored, type);
}

// notified when log reader encounters corruption.
class corruptionreporter : public log::reader::reporter {
 public:
  writablefile* dst_;
  virtual void corruption(size_t bytes, const status& status) {
    std::string r = "corruption: ";
    appendnumberto(&r, bytes);
    r += " bytes; ";
    r += status.tostring();
    r.push_back('\n');
    dst_->append(r);
  }
};

// print contents of a log file. (*func)() is called on every record.
status printlogcontents(env* env, const std::string& fname,
                        void (*func)(uint64_t, slice, writablefile*),
                        writablefile* dst) {
  sequentialfile* file;
  status s = env->newsequentialfile(fname, &file);
  if (!s.ok()) {
    return s;
  }
  corruptionreporter reporter;
  reporter.dst_ = dst;
  log::reader reader(file, &reporter, true, 0);
  slice record;
  std::string scratch;
  while (reader.readrecord(&record, &scratch)) {
    (*func)(reader.lastrecordoffset(), record, dst);
  }
  delete file;
  return status::ok();
}

// called on every item found in a writebatch.
class writebatchitemprinter : public writebatch::handler {
 public:
  writablefile* dst_;
  virtual void put(const slice& key, const slice& value) {
    std::string r = "  put '";
    appendescapedstringto(&r, key);
    r += "' '";
    appendescapedstringto(&r, value);
    r += "'\n";
    dst_->append(r);
  }
  virtual void delete(const slice& key) {
    std::string r = "  del '";
    appendescapedstringto(&r, key);
    r += "'\n";
    dst_->append(r);
  }
};


// called on every log record (each one of which is a writebatch)
// found in a klogfile.
static void writebatchprinter(uint64_t pos, slice record, writablefile* dst) {
  std::string r = "--- offset ";
  appendnumberto(&r, pos);
  r += "; ";
  if (record.size() < 12) {
    r += "log record length ";
    appendnumberto(&r, record.size());
    r += " is too small\n";
    dst->append(r);
    return;
  }
  writebatch batch;
  writebatchinternal::setcontents(&batch, record);
  r += "sequence ";
  appendnumberto(&r, writebatchinternal::sequence(&batch));
  r.push_back('\n');
  dst->append(r);
  writebatchitemprinter batch_item_printer;
  batch_item_printer.dst_ = dst;
  status s = batch.iterate(&batch_item_printer);
  if (!s.ok()) {
    dst->append("  error: " + s.tostring() + "\n");
  }
}

status dumplog(env* env, const std::string& fname, writablefile* dst) {
  return printlogcontents(env, fname, writebatchprinter, dst);
}

// called on every log record (each one of which is a writebatch)
// found in a kdescriptorfile.
static void versioneditprinter(uint64_t pos, slice record, writablefile* dst) {
  std::string r = "--- offset ";
  appendnumberto(&r, pos);
  r += "; ";
  versionedit edit;
  status s = edit.decodefrom(record);
  if (!s.ok()) {
    r += s.tostring();
    r.push_back('\n');
  } else {
    r += edit.debugstring();
  }
  dst->append(r);
}

status dumpdescriptor(env* env, const std::string& fname, writablefile* dst) {
  return printlogcontents(env, fname, versioneditprinter, dst);
}

status dumptable(env* env, const std::string& fname, writablefile* dst) {
  uint64_t file_size;
  randomaccessfile* file = null;
  table* table = null;
  status s = env->getfilesize(fname, &file_size);
  if (s.ok()) {
    s = env->newrandomaccessfile(fname, &file);
  }
  if (s.ok()) {
    // we use the default comparator, which may or may not match the
    // comparator used in this database. however this should not cause
    // problems since we only use table operations that do not require
    // any comparisons.  in particular, we do not call seek or prev.
    s = table::open(options(), file, file_size, &table);
  }
  if (!s.ok()) {
    delete table;
    delete file;
    return s;
  }

  readoptions ro;
  ro.fill_cache = false;
  iterator* iter = table->newiterator(ro);
  std::string r;
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    r.clear();
    parsedinternalkey key;
    if (!parseinternalkey(iter->key(), &key)) {
      r = "badkey '";
      appendescapedstringto(&r, iter->key());
      r += "' => '";
      appendescapedstringto(&r, iter->value());
      r += "'\n";
      dst->append(r);
    } else {
      r = "'";
      appendescapedstringto(&r, key.user_key);
      r += "' @ ";
      appendnumberto(&r, key.sequence);
      r += " : ";
      if (key.type == ktypedeletion) {
        r += "del";
      } else if (key.type == ktypevalue) {
        r += "val";
      } else {
        appendnumberto(&r, key.type);
      }
      r += " => '";
      appendescapedstringto(&r, iter->value());
      r += "'\n";
      dst->append(r);
    }
  }
  s = iter->status();
  if (!s.ok()) {
    dst->append("iterator error: " + s.tostring() + "\n");
  }

  delete iter;
  delete table;
  delete file;
  return status::ok();
}

}  // namespace

status dumpfile(env* env, const std::string& fname, writablefile* dst) {
  filetype ftype;
  if (!guesstype(fname, &ftype)) {
    return status::invalidargument(fname + ": unknown file type");
  }
  switch (ftype) {
    case klogfile:         return dumplog(env, fname, dst);
    case kdescriptorfile:  return dumpdescriptor(env, fname, dst);
    case ktablefile:       return dumptable(env, fname, dst);
    default:
      break;
  }
  return status::invalidargument(fname + ": not a dump-able file type");
}

}  // namespace leveldb
