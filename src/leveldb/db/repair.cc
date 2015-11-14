// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// we recover the contents of the descriptor from the other files we find.
// (1) any log files are first converted to tables
// (2) we scan every table to compute
//     (a) smallest/largest for the table
//     (b) largest sequence number in the table
// (3) we generate descriptor contents:
//      - log number is set to zero
//      - next-file-number is set to 1 + largest file number we found
//      - last-sequence-number is set to largest sequence# found across
//        all tables (see 2c)
//      - compaction pointers are cleared
//      - every table file is added at level 0
//
// possible optimization 1:
//   (a) compute total size and use to pick appropriate max-level m
//   (b) sort tables by largest sequence# in the table
//   (c) for each table: if it overlaps earlier table, place in level-0,
//       else place in level-m.
// possible optimization 2:
//   store per-table metadata (smallest, largest, largest-seq#, ...)
//   in the table's meta section to speed up scantable.

#include "db/builder.h"
#include "db/db_impl.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "db/write_batch_internal.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"

namespace leveldb {

namespace {

class repairer {
 public:
  repairer(const std::string& dbname, const options& options)
      : dbname_(dbname),
        env_(options.env),
        icmp_(options.comparator),
        ipolicy_(options.filter_policy),
        options_(sanitizeoptions(dbname, &icmp_, &ipolicy_, options)),
        owns_info_log_(options_.info_log != options.info_log),
        owns_cache_(options_.block_cache != options.block_cache),
        next_file_number_(1) {
    // tablecache can be small since we expect each table to be opened once.
    table_cache_ = new tablecache(dbname_, &options_, 10);
  }

  ~repairer() {
    delete table_cache_;
    if (owns_info_log_) {
      delete options_.info_log;
    }
    if (owns_cache_) {
      delete options_.block_cache;
    }
  }

  status run() {
    status status = findfiles();
    if (status.ok()) {
      convertlogfilestotables();
      extractmetadata();
      status = writedescriptor();
    }
    if (status.ok()) {
      unsigned long long bytes = 0;
      for (size_t i = 0; i < tables_.size(); i++) {
        bytes += tables_[i].meta.file_size;
      }
      log(options_.info_log,
          "**** repaired leveldb %s; "
          "recovered %d files; %llu bytes. "
          "some data may have been lost. "
          "****",
          dbname_.c_str(),
          static_cast<int>(tables_.size()),
          bytes);
    }
    return status;
  }

 private:
  struct tableinfo {
    filemetadata meta;
    sequencenumber max_sequence;
  };

  std::string const dbname_;
  env* const env_;
  internalkeycomparator const icmp_;
  internalfilterpolicy const ipolicy_;
  options const options_;
  bool owns_info_log_;
  bool owns_cache_;
  tablecache* table_cache_;
  versionedit edit_;

  std::vector<std::string> manifests_;
  std::vector<uint64_t> table_numbers_;
  std::vector<uint64_t> logs_;
  std::vector<tableinfo> tables_;
  uint64_t next_file_number_;

  status findfiles() {
    std::vector<std::string> filenames;
    status status = env_->getchildren(dbname_, &filenames);
    if (!status.ok()) {
      return status;
    }
    if (filenames.empty()) {
      return status::ioerror(dbname_, "repair found no files");
    }

    uint64_t number;
    filetype type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type)) {
        if (type == kdescriptorfile) {
          manifests_.push_back(filenames[i]);
        } else {
          if (number + 1 > next_file_number_) {
            next_file_number_ = number + 1;
          }
          if (type == klogfile) {
            logs_.push_back(number);
          } else if (type == ktablefile) {
            table_numbers_.push_back(number);
          } else {
            // ignore other files
          }
        }
      }
    }
    return status;
  }

  void convertlogfilestotables() {
    for (size_t i = 0; i < logs_.size(); i++) {
      std::string logname = logfilename(dbname_, logs_[i]);
      status status = convertlogtotable(logs_[i]);
      if (!status.ok()) {
        log(options_.info_log, "log #%llu: ignoring conversion error: %s",
            (unsigned long long) logs_[i],
            status.tostring().c_str());
      }
      archivefile(logname);
    }
  }

  status convertlogtotable(uint64_t log) {
    struct logreporter : public log::reader::reporter {
      env* env;
      logger* info_log;
      uint64_t lognum;
      virtual void corruption(size_t bytes, const status& s) {
        // we print error messages for corruption, but continue repairing.
        log(info_log, "log #%llu: dropping %d bytes; %s",
            (unsigned long long) lognum,
            static_cast<int>(bytes),
            s.tostring().c_str());
      }
    };

    // open the log file
    std::string logname = logfilename(dbname_, log);
    sequentialfile* lfile;
    status status = env_->newsequentialfile(logname, &lfile);
    if (!status.ok()) {
      return status;
    }

    // create the log reader.
    logreporter reporter;
    reporter.env = env_;
    reporter.info_log = options_.info_log;
    reporter.lognum = log;
    // we intentionally make log::reader do checksumming so that
    // corruptions cause entire commits to be skipped instead of
    // propagating bad information (like overly large sequence
    // numbers).
    log::reader reader(lfile, &reporter, false/*do not checksum*/,
                       0/*initial_offset*/);

    // read all the records and add to a memtable
    std::string scratch;
    slice record;
    writebatch batch;
    memtable* mem = new memtable(icmp_);
    mem->ref();
    int counter = 0;
    while (reader.readrecord(&record, &scratch)) {
      if (record.size() < 12) {
        reporter.corruption(
            record.size(), status::corruption("log record too small"));
        continue;
      }
      writebatchinternal::setcontents(&batch, record);
      status = writebatchinternal::insertinto(&batch, mem);
      if (status.ok()) {
        counter += writebatchinternal::count(&batch);
      } else {
        log(options_.info_log, "log #%llu: ignoring %s",
            (unsigned long long) log,
            status.tostring().c_str());
        status = status::ok();  // keep going with rest of file
      }
    }
    delete lfile;

    // do not record a version edit for this conversion to a table
    // since extractmetadata() will also generate edits.
    filemetadata meta;
    meta.number = next_file_number_++;
    iterator* iter = mem->newiterator();
    status = buildtable(dbname_, env_, options_, table_cache_, iter, &meta);
    delete iter;
    mem->unref();
    mem = null;
    if (status.ok()) {
      if (meta.file_size > 0) {
        table_numbers_.push_back(meta.number);
      }
    }
    log(options_.info_log, "log #%llu: %d ops saved to table #%llu %s",
        (unsigned long long) log,
        counter,
        (unsigned long long) meta.number,
        status.tostring().c_str());
    return status;
  }

  void extractmetadata() {
    for (size_t i = 0; i < table_numbers_.size(); i++) {
      scantable(table_numbers_[i]);
    }
  }

  iterator* newtableiterator(const filemetadata& meta) {
    // same as compaction iterators: if paranoid_checks are on, turn
    // on checksum verification.
    readoptions r;
    r.verify_checksums = options_.paranoid_checks;
    return table_cache_->newiterator(r, meta.number, meta.file_size);
  }

  void scantable(uint64_t number) {
    tableinfo t;
    t.meta.number = number;
    std::string fname = tablefilename(dbname_, number);
    status status = env_->getfilesize(fname, &t.meta.file_size);
    if (!status.ok()) {
      // try alternate file name.
      fname = ssttablefilename(dbname_, number);
      status s2 = env_->getfilesize(fname, &t.meta.file_size);
      if (s2.ok()) {
        status = status::ok();
      }
    }
    if (!status.ok()) {
      archivefile(tablefilename(dbname_, number));
      archivefile(ssttablefilename(dbname_, number));
      log(options_.info_log, "table #%llu: dropped: %s",
          (unsigned long long) t.meta.number,
          status.tostring().c_str());
      return;
    }

    // extract metadata by scanning through table.
    int counter = 0;
    iterator* iter = newtableiterator(t.meta);
    bool empty = true;
    parsedinternalkey parsed;
    t.max_sequence = 0;
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      slice key = iter->key();
      if (!parseinternalkey(key, &parsed)) {
        log(options_.info_log, "table #%llu: unparsable key %s",
            (unsigned long long) t.meta.number,
            escapestring(key).c_str());
        continue;
      }

      counter++;
      if (empty) {
        empty = false;
        t.meta.smallest.decodefrom(key);
      }
      t.meta.largest.decodefrom(key);
      if (parsed.sequence > t.max_sequence) {
        t.max_sequence = parsed.sequence;
      }
    }
    if (!iter->status().ok()) {
      status = iter->status();
    }
    delete iter;
    log(options_.info_log, "table #%llu: %d entries %s",
        (unsigned long long) t.meta.number,
        counter,
        status.tostring().c_str());

    if (status.ok()) {
      tables_.push_back(t);
    } else {
      repairtable(fname, t);  // repairtable archives input file.
    }
  }

  void repairtable(const std::string& src, tableinfo t) {
    // we will copy src contents to a new table and then rename the
    // new table over the source.

    // create builder.
    std::string copy = tablefilename(dbname_, next_file_number_++);
    writablefile* file;
    status s = env_->newwritablefile(copy, &file);
    if (!s.ok()) {
      return;
    }
    tablebuilder* builder = new tablebuilder(options_, file);

    // copy data.
    iterator* iter = newtableiterator(t.meta);
    int counter = 0;
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      builder->add(iter->key(), iter->value());
      counter++;
    }
    delete iter;

    archivefile(src);
    if (counter == 0) {
      builder->abandon();  // nothing to save
    } else {
      s = builder->finish();
      if (s.ok()) {
        t.meta.file_size = builder->filesize();
      }
    }
    delete builder;
    builder = null;

    if (s.ok()) {
      s = file->close();
    }
    delete file;
    file = null;

    if (counter > 0 && s.ok()) {
      std::string orig = tablefilename(dbname_, t.meta.number);
      s = env_->renamefile(copy, orig);
      if (s.ok()) {
        log(options_.info_log, "table #%llu: %d entries repaired",
            (unsigned long long) t.meta.number, counter);
        tables_.push_back(t);
      }
    }
    if (!s.ok()) {
      env_->deletefile(copy);
    }
  }

  status writedescriptor() {
    std::string tmp = tempfilename(dbname_, 1);
    writablefile* file;
    status status = env_->newwritablefile(tmp, &file);
    if (!status.ok()) {
      return status;
    }

    sequencenumber max_sequence = 0;
    for (size_t i = 0; i < tables_.size(); i++) {
      if (max_sequence < tables_[i].max_sequence) {
        max_sequence = tables_[i].max_sequence;
      }
    }

    edit_.setcomparatorname(icmp_.user_comparator()->name());
    edit_.setlognumber(0);
    edit_.setnextfile(next_file_number_);
    edit_.setlastsequence(max_sequence);

    for (size_t i = 0; i < tables_.size(); i++) {
      // todo(opt): separate out into multiple levels
      const tableinfo& t = tables_[i];
      edit_.addfile(0, t.meta.number, t.meta.file_size,
                    t.meta.smallest, t.meta.largest);
    }

    //fprintf(stderr, "newdescriptor:\n%s\n", edit_.debugstring().c_str());
    {
      log::writer log(file);
      std::string record;
      edit_.encodeto(&record);
      status = log.addrecord(record);
    }
    if (status.ok()) {
      status = file->close();
    }
    delete file;
    file = null;

    if (!status.ok()) {
      env_->deletefile(tmp);
    } else {
      // discard older manifests
      for (size_t i = 0; i < manifests_.size(); i++) {
        archivefile(dbname_ + "/" + manifests_[i]);
      }

      // install new manifest
      status = env_->renamefile(tmp, descriptorfilename(dbname_, 1));
      if (status.ok()) {
        status = setcurrentfile(env_, dbname_, 1);
      } else {
        env_->deletefile(tmp);
      }
    }
    return status;
  }

  void archivefile(const std::string& fname) {
    // move into another directory.  e.g., for
    //    dir/foo
    // rename to
    //    dir/lost/foo
    const char* slash = strrchr(fname.c_str(), '/');
    std::string new_dir;
    if (slash != null) {
      new_dir.assign(fname.data(), slash - fname.data());
    }
    new_dir.append("/lost");
    env_->createdir(new_dir);  // ignore error
    std::string new_file = new_dir;
    new_file.append("/");
    new_file.append((slash == null) ? fname.c_str() : slash + 1);
    status s = env_->renamefile(fname, new_file);
    log(options_.info_log, "archiving %s: %s\n",
        fname.c_str(), s.tostring().c_str());
  }
};
}  // namespace

status repairdb(const std::string& dbname, const options& options) {
  repairer repairer(dbname, options);
  return repairer.run();
}

}  // namespace leveldb
