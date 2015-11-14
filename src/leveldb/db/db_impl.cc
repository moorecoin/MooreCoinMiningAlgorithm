// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/db_impl.h"

#include <algorithm>
#include <set>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "db/builder.h"
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/status.h"
#include "leveldb/table.h"
#include "leveldb/table_builder.h"
#include "port/port.h"
#include "table/block.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"
#include "util/mutexlock.h"

namespace leveldb {

const int knumnontablecachefiles = 10;

// information kept for every waiting writer
struct dbimpl::writer {
  status status;
  writebatch* batch;
  bool sync;
  bool done;
  port::condvar cv;

  explicit writer(port::mutex* mu) : cv(mu) { }
};

struct dbimpl::compactionstate {
  compaction* const compaction;

  // sequence numbers < smallest_snapshot are not significant since we
  // will never have to service a snapshot below smallest_snapshot.
  // therefore if we have seen a sequence number s <= smallest_snapshot,
  // we can drop all entries for the same key with sequence numbers < s.
  sequencenumber smallest_snapshot;

  // files produced by compaction
  struct output {
    uint64_t number;
    uint64_t file_size;
    internalkey smallest, largest;
  };
  std::vector<output> outputs;

  // state kept for output being generated
  writablefile* outfile;
  tablebuilder* builder;

  uint64_t total_bytes;

  output* current_output() { return &outputs[outputs.size()-1]; }

  explicit compactionstate(compaction* c)
      : compaction(c),
        outfile(null),
        builder(null),
        total_bytes(0) {
  }
};

// fix user-supplied options to be reasonable
template <class t,class v>
static void cliptorange(t* ptr, v minvalue, v maxvalue) {
  if (static_cast<v>(*ptr) > maxvalue) *ptr = maxvalue;
  if (static_cast<v>(*ptr) < minvalue) *ptr = minvalue;
}
options sanitizeoptions(const std::string& dbname,
                        const internalkeycomparator* icmp,
                        const internalfilterpolicy* ipolicy,
                        const options& src) {
  options result = src;
  result.comparator = icmp;
  result.filter_policy = (src.filter_policy != null) ? ipolicy : null;
  cliptorange(&result.max_open_files,    64 + knumnontablecachefiles, 50000);
  cliptorange(&result.write_buffer_size, 64<<10,                      1<<30);
  cliptorange(&result.block_size,        1<<10,                       4<<20);
  if (result.info_log == null) {
    // open a log file in the same directory as the db
    src.env->createdir(dbname);  // in case it does not exist
    src.env->renamefile(infologfilename(dbname), oldinfologfilename(dbname));
    status s = src.env->newlogger(infologfilename(dbname), &result.info_log);
    if (!s.ok()) {
      // no place suitable for logging
      result.info_log = null;
    }
  }
  if (result.block_cache == null) {
    result.block_cache = newlrucache(8 << 20);
  }
  return result;
}

dbimpl::dbimpl(const options& raw_options, const std::string& dbname)
    : env_(raw_options.env),
      internal_comparator_(raw_options.comparator),
      internal_filter_policy_(raw_options.filter_policy),
      options_(sanitizeoptions(dbname, &internal_comparator_,
                               &internal_filter_policy_, raw_options)),
      owns_info_log_(options_.info_log != raw_options.info_log),
      owns_cache_(options_.block_cache != raw_options.block_cache),
      dbname_(dbname),
      db_lock_(null),
      shutting_down_(null),
      bg_cv_(&mutex_),
      mem_(new memtable(internal_comparator_)),
      imm_(null),
      logfile_(null),
      logfile_number_(0),
      log_(null),
      seed_(0),
      tmp_batch_(new writebatch),
      bg_compaction_scheduled_(false),
      manual_compaction_(null) {
  mem_->ref();
  has_imm_.release_store(null);

  // reserve ten files or so for other uses and give the rest to tablecache.
  const int table_cache_size = options_.max_open_files - knumnontablecachefiles;
  table_cache_ = new tablecache(dbname_, &options_, table_cache_size);

  versions_ = new versionset(dbname_, &options_, table_cache_,
                             &internal_comparator_);
}

dbimpl::~dbimpl() {
  // wait for background work to finish
  mutex_.lock();
  shutting_down_.release_store(this);  // any non-null value is ok
  while (bg_compaction_scheduled_) {
    bg_cv_.wait();
  }
  mutex_.unlock();

  if (db_lock_ != null) {
    env_->unlockfile(db_lock_);
  }

  delete versions_;
  if (mem_ != null) mem_->unref();
  if (imm_ != null) imm_->unref();
  delete tmp_batch_;
  delete log_;
  delete logfile_;
  delete table_cache_;

  if (owns_info_log_) {
    delete options_.info_log;
  }
  if (owns_cache_) {
    delete options_.block_cache;
  }
}

status dbimpl::newdb() {
  versionedit new_db;
  new_db.setcomparatorname(user_comparator()->name());
  new_db.setlognumber(0);
  new_db.setnextfile(2);
  new_db.setlastsequence(0);

  const std::string manifest = descriptorfilename(dbname_, 1);
  writablefile* file;
  status s = env_->newwritablefile(manifest, &file);
  if (!s.ok()) {
    return s;
  }
  {
    log::writer log(file);
    std::string record;
    new_db.encodeto(&record);
    s = log.addrecord(record);
    if (s.ok()) {
      s = file->close();
    }
  }
  delete file;
  if (s.ok()) {
    // make "current" file that points to the new manifest file.
    s = setcurrentfile(env_, dbname_, 1);
  } else {
    env_->deletefile(manifest);
  }
  return s;
}

void dbimpl::maybeignoreerror(status* s) const {
  if (s->ok() || options_.paranoid_checks) {
    // no change needed
  } else {
    log(options_.info_log, "ignoring error %s", s->tostring().c_str());
    *s = status::ok();
  }
}

void dbimpl::deleteobsoletefiles() {
  if (!bg_error_.ok()) {
    // after a background error, we don't know whether a new version may
    // or may not have been committed, so we cannot safely garbage collect.
    return;
  }

  // make a set of all of the live files
  std::set<uint64_t> live = pending_outputs_;
  versions_->addlivefiles(&live);

  std::vector<std::string> filenames;
  env_->getchildren(dbname_, &filenames); // ignoring errors on purpose
  uint64_t number;
  filetype type;
  for (size_t i = 0; i < filenames.size(); i++) {
    if (parsefilename(filenames[i], &number, &type)) {
      bool keep = true;
      switch (type) {
        case klogfile:
          keep = ((number >= versions_->lognumber()) ||
                  (number == versions_->prevlognumber()));
          break;
        case kdescriptorfile:
          // keep my manifest file, and any newer incarnations'
          // (in case there is a race that allows other incarnations)
          keep = (number >= versions_->manifestfilenumber());
          break;
        case ktablefile:
          keep = (live.find(number) != live.end());
          break;
        case ktempfile:
          // any temp files that are currently being written to must
          // be recorded in pending_outputs_, which is inserted into "live"
          keep = (live.find(number) != live.end());
          break;
        case kcurrentfile:
        case kdblockfile:
        case kinfologfile:
          keep = true;
          break;
      }

      if (!keep) {
        if (type == ktablefile) {
          table_cache_->evict(number);
        }
        log(options_.info_log, "delete type=%d #%lld\n",
            int(type),
            static_cast<unsigned long long>(number));
        env_->deletefile(dbname_ + "/" + filenames[i]);
      }
    }
  }
}

status dbimpl::recover(versionedit* edit) {
  mutex_.assertheld();

  // ignore error from createdir since the creation of the db is
  // committed only when the descriptor is created, and this directory
  // may already exist from a previous failed creation attempt.
  env_->createdir(dbname_);
  assert(db_lock_ == null);
  status s = env_->lockfile(lockfilename(dbname_), &db_lock_);
  if (!s.ok()) {
    return s;
  }

  if (!env_->fileexists(currentfilename(dbname_))) {
    if (options_.create_if_missing) {
      s = newdb();
      if (!s.ok()) {
        return s;
      }
    } else {
      return status::invalidargument(
          dbname_, "does not exist (create_if_missing is false)");
    }
  } else {
    if (options_.error_if_exists) {
      return status::invalidargument(
          dbname_, "exists (error_if_exists is true)");
    }
  }

  s = versions_->recover();
  if (s.ok()) {
    sequencenumber max_sequence(0);

    // recover from all newer log files than the ones named in the
    // descriptor (new log files may have been added by the previous
    // incarnation without registering them in the descriptor).
    //
    // note that prevlognumber() is no longer used, but we pay
    // attention to it in case we are recovering a database
    // produced by an older version of leveldb.
    const uint64_t min_log = versions_->lognumber();
    const uint64_t prev_log = versions_->prevlognumber();
    std::vector<std::string> filenames;
    s = env_->getchildren(dbname_, &filenames);
    if (!s.ok()) {
      return s;
    }
    std::set<uint64_t> expected;
    versions_->addlivefiles(&expected);
    uint64_t number;
    filetype type;
    std::vector<uint64_t> logs;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type)) {
        expected.erase(number);
        if (type == klogfile && ((number >= min_log) || (number == prev_log)))
          logs.push_back(number);
      }
    }
    if (!expected.empty()) {
      char buf[50];
      snprintf(buf, sizeof(buf), "%d missing files; e.g.",
               static_cast<int>(expected.size()));
      return status::corruption(buf, tablefilename(dbname_, *(expected.begin())));
    }

    // recover in the order in which the logs were generated
    std::sort(logs.begin(), logs.end());
    for (size_t i = 0; i < logs.size(); i++) {
      s = recoverlogfile(logs[i], edit, &max_sequence);

      // the previous incarnation may not have written any manifest
      // records after allocating this log number.  so we manually
      // update the file number allocation counter in versionset.
      versions_->markfilenumberused(logs[i]);
    }

    if (s.ok()) {
      if (versions_->lastsequence() < max_sequence) {
        versions_->setlastsequence(max_sequence);
      }
    }
  }

  return s;
}

status dbimpl::recoverlogfile(uint64_t log_number,
                              versionedit* edit,
                              sequencenumber* max_sequence) {
  struct logreporter : public log::reader::reporter {
    env* env;
    logger* info_log;
    const char* fname;
    status* status;  // null if options_.paranoid_checks==false
    virtual void corruption(size_t bytes, const status& s) {
      log(info_log, "%s%s: dropping %d bytes; %s",
          (this->status == null ? "(ignoring error) " : ""),
          fname, static_cast<int>(bytes), s.tostring().c_str());
      if (this->status != null && this->status->ok()) *this->status = s;
    }
  };

  mutex_.assertheld();

  // open the log file
  std::string fname = logfilename(dbname_, log_number);
  sequentialfile* file;
  status status = env_->newsequentialfile(fname, &file);
  if (!status.ok()) {
    maybeignoreerror(&status);
    return status;
  }

  // create the log reader.
  logreporter reporter;
  reporter.env = env_;
  reporter.info_log = options_.info_log;
  reporter.fname = fname.c_str();
  reporter.status = (options_.paranoid_checks ? &status : null);
  // we intentionally make log::reader do checksumming even if
  // paranoid_checks==false so that corruptions cause entire commits
  // to be skipped instead of propagating bad information (like overly
  // large sequence numbers).
  log::reader reader(file, &reporter, true/*checksum*/,
                     0/*initial_offset*/);
  log(options_.info_log, "recovering log #%llu",
      (unsigned long long) log_number);

  // read all the records and add to a memtable
  std::string scratch;
  slice record;
  writebatch batch;
  memtable* mem = null;
  while (reader.readrecord(&record, &scratch) &&
         status.ok()) {
    if (record.size() < 12) {
      reporter.corruption(
          record.size(), status::corruption("log record too small"));
      continue;
    }
    writebatchinternal::setcontents(&batch, record);

    if (mem == null) {
      mem = new memtable(internal_comparator_);
      mem->ref();
    }
    status = writebatchinternal::insertinto(&batch, mem);
    maybeignoreerror(&status);
    if (!status.ok()) {
      break;
    }
    const sequencenumber last_seq =
        writebatchinternal::sequence(&batch) +
        writebatchinternal::count(&batch) - 1;
    if (last_seq > *max_sequence) {
      *max_sequence = last_seq;
    }

    if (mem->approximatememoryusage() > options_.write_buffer_size) {
      status = writelevel0table(mem, edit, null);
      if (!status.ok()) {
        // reflect errors immediately so that conditions like full
        // file-systems cause the db::open() to fail.
        break;
      }
      mem->unref();
      mem = null;
    }
  }

  if (status.ok() && mem != null) {
    status = writelevel0table(mem, edit, null);
    // reflect errors immediately so that conditions like full
    // file-systems cause the db::open() to fail.
  }

  if (mem != null) mem->unref();
  delete file;
  return status;
}

status dbimpl::writelevel0table(memtable* mem, versionedit* edit,
                                version* base) {
  mutex_.assertheld();
  const uint64_t start_micros = env_->nowmicros();
  filemetadata meta;
  meta.number = versions_->newfilenumber();
  pending_outputs_.insert(meta.number);
  iterator* iter = mem->newiterator();
  log(options_.info_log, "level-0 table #%llu: started",
      (unsigned long long) meta.number);

  status s;
  {
    mutex_.unlock();
    s = buildtable(dbname_, env_, options_, table_cache_, iter, &meta);
    mutex_.lock();
  }

  log(options_.info_log, "level-0 table #%llu: %lld bytes %s",
      (unsigned long long) meta.number,
      (unsigned long long) meta.file_size,
      s.tostring().c_str());
  delete iter;
  pending_outputs_.erase(meta.number);


  // note that if file_size is zero, the file has been deleted and
  // should not be added to the manifest.
  int level = 0;
  if (s.ok() && meta.file_size > 0) {
    const slice min_user_key = meta.smallest.user_key();
    const slice max_user_key = meta.largest.user_key();
    if (base != null) {
      level = base->picklevelformemtableoutput(min_user_key, max_user_key);
    }
    edit->addfile(level, meta.number, meta.file_size,
                  meta.smallest, meta.largest);
  }

  compactionstats stats;
  stats.micros = env_->nowmicros() - start_micros;
  stats.bytes_written = meta.file_size;
  stats_[level].add(stats);
  return s;
}

void dbimpl::compactmemtable() {
  mutex_.assertheld();
  assert(imm_ != null);

  // save the contents of the memtable as a new table
  versionedit edit;
  version* base = versions_->current();
  base->ref();
  status s = writelevel0table(imm_, &edit, base);
  base->unref();

  if (s.ok() && shutting_down_.acquire_load()) {
    s = status::ioerror("deleting db during memtable compaction");
  }

  // replace immutable memtable with the generated table
  if (s.ok()) {
    edit.setprevlognumber(0);
    edit.setlognumber(logfile_number_);  // earlier logs no longer needed
    s = versions_->logandapply(&edit, &mutex_);
  }

  if (s.ok()) {
    // commit to the new state
    imm_->unref();
    imm_ = null;
    has_imm_.release_store(null);
    deleteobsoletefiles();
  } else {
    recordbackgrounderror(s);
  }
}

void dbimpl::compactrange(const slice* begin, const slice* end) {
  int max_level_with_files = 1;
  {
    mutexlock l(&mutex_);
    version* base = versions_->current();
    for (int level = 1; level < config::knumlevels; level++) {
      if (base->overlapinlevel(level, begin, end)) {
        max_level_with_files = level;
      }
    }
  }
  test_compactmemtable(); // todo(sanjay): skip if memtable does not overlap
  for (int level = 0; level < max_level_with_files; level++) {
    test_compactrange(level, begin, end);
  }
}

void dbimpl::test_compactrange(int level, const slice* begin,const slice* end) {
  assert(level >= 0);
  assert(level + 1 < config::knumlevels);

  internalkey begin_storage, end_storage;

  manualcompaction manual;
  manual.level = level;
  manual.done = false;
  if (begin == null) {
    manual.begin = null;
  } else {
    begin_storage = internalkey(*begin, kmaxsequencenumber, kvaluetypeforseek);
    manual.begin = &begin_storage;
  }
  if (end == null) {
    manual.end = null;
  } else {
    end_storage = internalkey(*end, 0, static_cast<valuetype>(0));
    manual.end = &end_storage;
  }

  mutexlock l(&mutex_);
  while (!manual.done && !shutting_down_.acquire_load() && bg_error_.ok()) {
    if (manual_compaction_ == null) {  // idle
      manual_compaction_ = &manual;
      maybeschedulecompaction();
    } else {  // running either my compaction or another compaction.
      bg_cv_.wait();
    }
  }
  if (manual_compaction_ == &manual) {
    // cancel my manual compaction since we aborted early for some reason.
    manual_compaction_ = null;
  }
}

status dbimpl::test_compactmemtable() {
  // null batch means just wait for earlier writes to be done
  status s = write(writeoptions(), null);
  if (s.ok()) {
    // wait until the compaction completes
    mutexlock l(&mutex_);
    while (imm_ != null && bg_error_.ok()) {
      bg_cv_.wait();
    }
    if (imm_ != null) {
      s = bg_error_;
    }
  }
  return s;
}

void dbimpl::recordbackgrounderror(const status& s) {
  mutex_.assertheld();
  if (bg_error_.ok()) {
    bg_error_ = s;
    bg_cv_.signalall();
  }
}

void dbimpl::maybeschedulecompaction() {
  mutex_.assertheld();
  if (bg_compaction_scheduled_) {
    // already scheduled
  } else if (shutting_down_.acquire_load()) {
    // db is being deleted; no more background compactions
  } else if (!bg_error_.ok()) {
    // already got an error; no more changes
  } else if (imm_ == null &&
             manual_compaction_ == null &&
             !versions_->needscompaction()) {
    // no work to be done
  } else {
    bg_compaction_scheduled_ = true;
    env_->schedule(&dbimpl::bgwork, this);
  }
}

void dbimpl::bgwork(void* db) {
  reinterpret_cast<dbimpl*>(db)->backgroundcall();
}

void dbimpl::backgroundcall() {
  mutexlock l(&mutex_);
  assert(bg_compaction_scheduled_);
  if (shutting_down_.acquire_load()) {
    // no more background work when shutting down.
  } else if (!bg_error_.ok()) {
    // no more background work after a background error.
  } else {
    backgroundcompaction();
  }

  bg_compaction_scheduled_ = false;

  // previous compaction may have produced too many files in a level,
  // so reschedule another compaction if needed.
  maybeschedulecompaction();
  bg_cv_.signalall();
}

void dbimpl::backgroundcompaction() {
  mutex_.assertheld();

  if (imm_ != null) {
    compactmemtable();
    return;
  }

  compaction* c;
  bool is_manual = (manual_compaction_ != null);
  internalkey manual_end;
  if (is_manual) {
    manualcompaction* m = manual_compaction_;
    c = versions_->compactrange(m->level, m->begin, m->end);
    m->done = (c == null);
    if (c != null) {
      manual_end = c->input(0, c->num_input_files(0) - 1)->largest;
    }
    log(options_.info_log,
        "manual compaction at level-%d from %s .. %s; will stop at %s\n",
        m->level,
        (m->begin ? m->begin->debugstring().c_str() : "(begin)"),
        (m->end ? m->end->debugstring().c_str() : "(end)"),
        (m->done ? "(end)" : manual_end.debugstring().c_str()));
  } else {
    c = versions_->pickcompaction();
  }

  status status;
  if (c == null) {
    // nothing to do
  } else if (!is_manual && c->istrivialmove()) {
    // move file to next level
    assert(c->num_input_files(0) == 1);
    filemetadata* f = c->input(0, 0);
    c->edit()->deletefile(c->level(), f->number);
    c->edit()->addfile(c->level() + 1, f->number, f->file_size,
                       f->smallest, f->largest);
    status = versions_->logandapply(c->edit(), &mutex_);
    if (!status.ok()) {
      recordbackgrounderror(status);
    }
    versionset::levelsummarystorage tmp;
    log(options_.info_log, "moved #%lld to level-%d %lld bytes %s: %s\n",
        static_cast<unsigned long long>(f->number),
        c->level() + 1,
        static_cast<unsigned long long>(f->file_size),
        status.tostring().c_str(),
        versions_->levelsummary(&tmp));
  } else {
    compactionstate* compact = new compactionstate(c);
    status = docompactionwork(compact);
    if (!status.ok()) {
      recordbackgrounderror(status);
    }
    cleanupcompaction(compact);
    c->releaseinputs();
    deleteobsoletefiles();
  }
  delete c;

  if (status.ok()) {
    // done
  } else if (shutting_down_.acquire_load()) {
    // ignore compaction errors found during shutting down
  } else {
    log(options_.info_log,
        "compaction error: %s", status.tostring().c_str());
  }

  if (is_manual) {
    manualcompaction* m = manual_compaction_;
    if (!status.ok()) {
      m->done = true;
    }
    if (!m->done) {
      // we only compacted part of the requested range.  update *m
      // to the range that is left to be compacted.
      m->tmp_storage = manual_end;
      m->begin = &m->tmp_storage;
    }
    manual_compaction_ = null;
  }
}

void dbimpl::cleanupcompaction(compactionstate* compact) {
  mutex_.assertheld();
  if (compact->builder != null) {
    // may happen if we get a shutdown call in the middle of compaction
    compact->builder->abandon();
    delete compact->builder;
  } else {
    assert(compact->outfile == null);
  }
  delete compact->outfile;
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const compactionstate::output& out = compact->outputs[i];
    pending_outputs_.erase(out.number);
  }
  delete compact;
}

status dbimpl::opencompactionoutputfile(compactionstate* compact) {
  assert(compact != null);
  assert(compact->builder == null);
  uint64_t file_number;
  {
    mutex_.lock();
    file_number = versions_->newfilenumber();
    pending_outputs_.insert(file_number);
    compactionstate::output out;
    out.number = file_number;
    out.smallest.clear();
    out.largest.clear();
    compact->outputs.push_back(out);
    mutex_.unlock();
  }

  // make the output file
  std::string fname = tablefilename(dbname_, file_number);
  status s = env_->newwritablefile(fname, &compact->outfile);
  if (s.ok()) {
    compact->builder = new tablebuilder(options_, compact->outfile);
  }
  return s;
}

status dbimpl::finishcompactionoutputfile(compactionstate* compact,
                                          iterator* input) {
  assert(compact != null);
  assert(compact->outfile != null);
  assert(compact->builder != null);

  const uint64_t output_number = compact->current_output()->number;
  assert(output_number != 0);

  // check for iterator errors
  status s = input->status();
  const uint64_t current_entries = compact->builder->numentries();
  if (s.ok()) {
    s = compact->builder->finish();
  } else {
    compact->builder->abandon();
  }
  const uint64_t current_bytes = compact->builder->filesize();
  compact->current_output()->file_size = current_bytes;
  compact->total_bytes += current_bytes;
  delete compact->builder;
  compact->builder = null;

  // finish and check for file errors
  if (s.ok()) {
    s = compact->outfile->sync();
  }
  if (s.ok()) {
    s = compact->outfile->close();
  }
  delete compact->outfile;
  compact->outfile = null;

  if (s.ok() && current_entries > 0) {
    // verify that the table is usable
    iterator* iter = table_cache_->newiterator(readoptions(),
                                               output_number,
                                               current_bytes);
    s = iter->status();
    delete iter;
    if (s.ok()) {
      log(options_.info_log,
          "generated table #%llu: %lld keys, %lld bytes",
          (unsigned long long) output_number,
          (unsigned long long) current_entries,
          (unsigned long long) current_bytes);
    }
  }
  return s;
}


status dbimpl::installcompactionresults(compactionstate* compact) {
  mutex_.assertheld();
  log(options_.info_log,  "compacted %d@%d + %d@%d files => %lld bytes",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1,
      static_cast<long long>(compact->total_bytes));

  // add compaction outputs
  compact->compaction->addinputdeletions(compact->compaction->edit());
  const int level = compact->compaction->level();
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    const compactionstate::output& out = compact->outputs[i];
    compact->compaction->edit()->addfile(
        level + 1,
        out.number, out.file_size, out.smallest, out.largest);
  }
  return versions_->logandapply(compact->compaction->edit(), &mutex_);
}

status dbimpl::docompactionwork(compactionstate* compact) {
  const uint64_t start_micros = env_->nowmicros();
  int64_t imm_micros = 0;  // micros spent doing imm_ compactions

  log(options_.info_log,  "compacting %d@%d + %d@%d files",
      compact->compaction->num_input_files(0),
      compact->compaction->level(),
      compact->compaction->num_input_files(1),
      compact->compaction->level() + 1);

  assert(versions_->numlevelfiles(compact->compaction->level()) > 0);
  assert(compact->builder == null);
  assert(compact->outfile == null);
  if (snapshots_.empty()) {
    compact->smallest_snapshot = versions_->lastsequence();
  } else {
    compact->smallest_snapshot = snapshots_.oldest()->number_;
  }

  // release mutex while we're actually doing the compaction work
  mutex_.unlock();

  iterator* input = versions_->makeinputiterator(compact->compaction);
  input->seektofirst();
  status status;
  parsedinternalkey ikey;
  std::string current_user_key;
  bool has_current_user_key = false;
  sequencenumber last_sequence_for_key = kmaxsequencenumber;
  for (; input->valid() && !shutting_down_.acquire_load(); ) {
    // prioritize immutable compaction work
    if (has_imm_.nobarrier_load() != null) {
      const uint64_t imm_start = env_->nowmicros();
      mutex_.lock();
      if (imm_ != null) {
        compactmemtable();
        bg_cv_.signalall();  // wakeup makeroomforwrite() if necessary
      }
      mutex_.unlock();
      imm_micros += (env_->nowmicros() - imm_start);
    }

    slice key = input->key();
    if (compact->compaction->shouldstopbefore(key) &&
        compact->builder != null) {
      status = finishcompactionoutputfile(compact, input);
      if (!status.ok()) {
        break;
      }
    }

    // handle key/value, add to state, etc.
    bool drop = false;
    if (!parseinternalkey(key, &ikey)) {
      // do not hide error keys
      current_user_key.clear();
      has_current_user_key = false;
      last_sequence_for_key = kmaxsequencenumber;
    } else {
      if (!has_current_user_key ||
          user_comparator()->compare(ikey.user_key,
                                     slice(current_user_key)) != 0) {
        // first occurrence of this user key
        current_user_key.assign(ikey.user_key.data(), ikey.user_key.size());
        has_current_user_key = true;
        last_sequence_for_key = kmaxsequencenumber;
      }

      if (last_sequence_for_key <= compact->smallest_snapshot) {
        // hidden by an newer entry for same user key
        drop = true;    // (a)
      } else if (ikey.type == ktypedeletion &&
                 ikey.sequence <= compact->smallest_snapshot &&
                 compact->compaction->isbaselevelforkey(ikey.user_key)) {
        // for this user key:
        // (1) there is no data in higher levels
        // (2) data in lower levels will have larger sequence numbers
        // (3) data in layers that are being compacted here and have
        //     smaller sequence numbers will be dropped in the next
        //     few iterations of this loop (by rule (a) above).
        // therefore this deletion marker is obsolete and can be dropped.
        drop = true;
      }

      last_sequence_for_key = ikey.sequence;
    }
#if 0
    log(options_.info_log,
        "  compact: %s, seq %d, type: %d %d, drop: %d, is_base: %d, "
        "%d smallest_snapshot: %d",
        ikey.user_key.tostring().c_str(),
        (int)ikey.sequence, ikey.type, ktypevalue, drop,
        compact->compaction->isbaselevelforkey(ikey.user_key),
        (int)last_sequence_for_key, (int)compact->smallest_snapshot);
#endif

    if (!drop) {
      // open output file if necessary
      if (compact->builder == null) {
        status = opencompactionoutputfile(compact);
        if (!status.ok()) {
          break;
        }
      }
      if (compact->builder->numentries() == 0) {
        compact->current_output()->smallest.decodefrom(key);
      }
      compact->current_output()->largest.decodefrom(key);
      compact->builder->add(key, input->value());

      // close output file if it is big enough
      if (compact->builder->filesize() >=
          compact->compaction->maxoutputfilesize()) {
        status = finishcompactionoutputfile(compact, input);
        if (!status.ok()) {
          break;
        }
      }
    }

    input->next();
  }

  if (status.ok() && shutting_down_.acquire_load()) {
    status = status::ioerror("deleting db during compaction");
  }
  if (status.ok() && compact->builder != null) {
    status = finishcompactionoutputfile(compact, input);
  }
  if (status.ok()) {
    status = input->status();
  }
  delete input;
  input = null;

  compactionstats stats;
  stats.micros = env_->nowmicros() - start_micros - imm_micros;
  for (int which = 0; which < 2; which++) {
    for (int i = 0; i < compact->compaction->num_input_files(which); i++) {
      stats.bytes_read += compact->compaction->input(which, i)->file_size;
    }
  }
  for (size_t i = 0; i < compact->outputs.size(); i++) {
    stats.bytes_written += compact->outputs[i].file_size;
  }

  mutex_.lock();
  stats_[compact->compaction->level() + 1].add(stats);

  if (status.ok()) {
    status = installcompactionresults(compact);
  }
  if (!status.ok()) {
    recordbackgrounderror(status);
  }
  versionset::levelsummarystorage tmp;
  log(options_.info_log,
      "compacted to: %s", versions_->levelsummary(&tmp));
  return status;
}

namespace {
struct iterstate {
  port::mutex* mu;
  version* version;
  memtable* mem;
  memtable* imm;
};

static void cleanupiteratorstate(void* arg1, void* arg2) {
  iterstate* state = reinterpret_cast<iterstate*>(arg1);
  state->mu->lock();
  state->mem->unref();
  if (state->imm != null) state->imm->unref();
  state->version->unref();
  state->mu->unlock();
  delete state;
}
}  // namespace

iterator* dbimpl::newinternaliterator(const readoptions& options,
                                      sequencenumber* latest_snapshot,
                                      uint32_t* seed) {
  iterstate* cleanup = new iterstate;
  mutex_.lock();
  *latest_snapshot = versions_->lastsequence();

  // collect together all needed child iterators
  std::vector<iterator*> list;
  list.push_back(mem_->newiterator());
  mem_->ref();
  if (imm_ != null) {
    list.push_back(imm_->newiterator());
    imm_->ref();
  }
  versions_->current()->additerators(options, &list);
  iterator* internal_iter =
      newmergingiterator(&internal_comparator_, &list[0], list.size());
  versions_->current()->ref();

  cleanup->mu = &mutex_;
  cleanup->mem = mem_;
  cleanup->imm = imm_;
  cleanup->version = versions_->current();
  internal_iter->registercleanup(cleanupiteratorstate, cleanup, null);

  *seed = ++seed_;
  mutex_.unlock();
  return internal_iter;
}

iterator* dbimpl::test_newinternaliterator() {
  sequencenumber ignored;
  uint32_t ignored_seed;
  return newinternaliterator(readoptions(), &ignored, &ignored_seed);
}

int64_t dbimpl::test_maxnextleveloverlappingbytes() {
  mutexlock l(&mutex_);
  return versions_->maxnextleveloverlappingbytes();
}

status dbimpl::get(const readoptions& options,
                   const slice& key,
                   std::string* value) {
  status s;
  mutexlock l(&mutex_);
  sequencenumber snapshot;
  if (options.snapshot != null) {
    snapshot = reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_;
  } else {
    snapshot = versions_->lastsequence();
  }

  memtable* mem = mem_;
  memtable* imm = imm_;
  version* current = versions_->current();
  mem->ref();
  if (imm != null) imm->ref();
  current->ref();

  bool have_stat_update = false;
  version::getstats stats;

  // unlock while reading from files and memtables
  {
    mutex_.unlock();
    // first look in the memtable, then in the immutable memtable (if any).
    lookupkey lkey(key, snapshot);
    if (mem->get(lkey, value, &s)) {
      // done
    } else if (imm != null && imm->get(lkey, value, &s)) {
      // done
    } else {
      s = current->get(options, lkey, value, &stats);
      have_stat_update = true;
    }
    mutex_.lock();
  }

  if (have_stat_update && current->updatestats(stats)) {
    maybeschedulecompaction();
  }
  mem->unref();
  if (imm != null) imm->unref();
  current->unref();
  return s;
}

iterator* dbimpl::newiterator(const readoptions& options) {
  sequencenumber latest_snapshot;
  uint32_t seed;
  iterator* iter = newinternaliterator(options, &latest_snapshot, &seed);
  return newdbiterator(
      this, user_comparator(), iter,
      (options.snapshot != null
       ? reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_
       : latest_snapshot),
      seed);
}

void dbimpl::recordreadsample(slice key) {
  mutexlock l(&mutex_);
  if (versions_->current()->recordreadsample(key)) {
    maybeschedulecompaction();
  }
}

const snapshot* dbimpl::getsnapshot() {
  mutexlock l(&mutex_);
  return snapshots_.new(versions_->lastsequence());
}

void dbimpl::releasesnapshot(const snapshot* s) {
  mutexlock l(&mutex_);
  snapshots_.delete(reinterpret_cast<const snapshotimpl*>(s));
}

// convenience methods
status dbimpl::put(const writeoptions& o, const slice& key, const slice& val) {
  return db::put(o, key, val);
}

status dbimpl::delete(const writeoptions& options, const slice& key) {
  return db::delete(options, key);
}

status dbimpl::write(const writeoptions& options, writebatch* my_batch) {
  writer w(&mutex_);
  w.batch = my_batch;
  w.sync = options.sync;
  w.done = false;

  mutexlock l(&mutex_);
  writers_.push_back(&w);
  while (!w.done && &w != writers_.front()) {
    w.cv.wait();
  }
  if (w.done) {
    return w.status;
  }

  // may temporarily unlock and wait.
  status status = makeroomforwrite(my_batch == null);
  uint64_t last_sequence = versions_->lastsequence();
  writer* last_writer = &w;
  if (status.ok() && my_batch != null) {  // null batch is for compactions
    writebatch* updates = buildbatchgroup(&last_writer);
    writebatchinternal::setsequence(updates, last_sequence + 1);
    last_sequence += writebatchinternal::count(updates);

    // add to log and apply to memtable.  we can release the lock
    // during this phase since &w is currently responsible for logging
    // and protects against concurrent loggers and concurrent writes
    // into mem_.
    {
      mutex_.unlock();
      status = log_->addrecord(writebatchinternal::contents(updates));
      bool sync_error = false;
      if (status.ok() && options.sync) {
        status = logfile_->sync();
        if (!status.ok()) {
          sync_error = true;
        }
      }
      if (status.ok()) {
        status = writebatchinternal::insertinto(updates, mem_);
      }
      mutex_.lock();
      if (sync_error) {
        // the state of the log file is indeterminate: the log record we
        // just added may or may not show up when the db is re-opened.
        // so we force the db into a mode where all future writes fail.
        recordbackgrounderror(status);
      }
    }
    if (updates == tmp_batch_) tmp_batch_->clear();

    versions_->setlastsequence(last_sequence);
  }

  while (true) {
    writer* ready = writers_.front();
    writers_.pop_front();
    if (ready != &w) {
      ready->status = status;
      ready->done = true;
      ready->cv.signal();
    }
    if (ready == last_writer) break;
  }

  // notify new head of write queue
  if (!writers_.empty()) {
    writers_.front()->cv.signal();
  }

  return status;
}

// requires: writer list must be non-empty
// requires: first writer must have a non-null batch
writebatch* dbimpl::buildbatchgroup(writer** last_writer) {
  assert(!writers_.empty());
  writer* first = writers_.front();
  writebatch* result = first->batch;
  assert(result != null);

  size_t size = writebatchinternal::bytesize(first->batch);

  // allow the group to grow up to a maximum size, but if the
  // original write is small, limit the growth so we do not slow
  // down the small write too much.
  size_t max_size = 1 << 20;
  if (size <= (128<<10)) {
    max_size = size + (128<<10);
  }

  *last_writer = first;
  std::deque<writer*>::iterator iter = writers_.begin();
  ++iter;  // advance past "first"
  for (; iter != writers_.end(); ++iter) {
    writer* w = *iter;
    if (w->sync && !first->sync) {
      // do not include a sync write into a batch handled by a non-sync write.
      break;
    }

    if (w->batch != null) {
      size += writebatchinternal::bytesize(w->batch);
      if (size > max_size) {
        // do not make batch too big
        break;
      }

      // append to *result
      if (result == first->batch) {
        // switch to temporary batch instead of disturbing caller's batch
        result = tmp_batch_;
        assert(writebatchinternal::count(result) == 0);
        writebatchinternal::append(result, first->batch);
      }
      writebatchinternal::append(result, w->batch);
    }
    *last_writer = w;
  }
  return result;
}

// requires: mutex_ is held
// requires: this thread is currently at the front of the writer queue
status dbimpl::makeroomforwrite(bool force) {
  mutex_.assertheld();
  assert(!writers_.empty());
  bool allow_delay = !force;
  status s;
  while (true) {
    if (!bg_error_.ok()) {
      // yield previous error
      s = bg_error_;
      break;
    } else if (
        allow_delay &&
        versions_->numlevelfiles(0) >= config::kl0_slowdownwritestrigger) {
      // we are getting close to hitting a hard limit on the number of
      // l0 files.  rather than delaying a single write by several
      // seconds when we hit the hard limit, start delaying each
      // individual write by 1ms to reduce latency variance.  also,
      // this delay hands over some cpu to the compaction thread in
      // case it is sharing the same core as the writer.
      mutex_.unlock();
      env_->sleepformicroseconds(1000);
      allow_delay = false;  // do not delay a single write more than once
      mutex_.lock();
    } else if (!force &&
               (mem_->approximatememoryusage() <= options_.write_buffer_size)) {
      // there is room in current memtable
      break;
    } else if (imm_ != null) {
      // we have filled up the current memtable, but the previous
      // one is still being compacted, so we wait.
      log(options_.info_log, "current memtable full; waiting...\n");
      bg_cv_.wait();
    } else if (versions_->numlevelfiles(0) >= config::kl0_stopwritestrigger) {
      // there are too many level-0 files.
      log(options_.info_log, "too many l0 files; waiting...\n");
      bg_cv_.wait();
    } else {
      // attempt to switch to a new memtable and trigger compaction of old
      assert(versions_->prevlognumber() == 0);
      uint64_t new_log_number = versions_->newfilenumber();
      writablefile* lfile = null;
      s = env_->newwritablefile(logfilename(dbname_, new_log_number), &lfile);
      if (!s.ok()) {
        // avoid chewing through file number space in a tight loop.
        versions_->reusefilenumber(new_log_number);
        break;
      }
      delete log_;
      delete logfile_;
      logfile_ = lfile;
      logfile_number_ = new_log_number;
      log_ = new log::writer(lfile);
      imm_ = mem_;
      has_imm_.release_store(imm_);
      mem_ = new memtable(internal_comparator_);
      mem_->ref();
      force = false;   // do not force another compaction if have room
      maybeschedulecompaction();
    }
  }
  return s;
}

bool dbimpl::getproperty(const slice& property, std::string* value) {
  value->clear();

  mutexlock l(&mutex_);
  slice in = property;
  slice prefix("leveldb.");
  if (!in.starts_with(prefix)) return false;
  in.remove_prefix(prefix.size());

  if (in.starts_with("num-files-at-level")) {
    in.remove_prefix(strlen("num-files-at-level"));
    uint64_t level;
    bool ok = consumedecimalnumber(&in, &level) && in.empty();
    if (!ok || level >= config::knumlevels) {
      return false;
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "%d",
               versions_->numlevelfiles(static_cast<int>(level)));
      *value = buf;
      return true;
    }
  } else if (in == "stats") {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "                               compactions\n"
             "level  files size(mb) time(sec) read(mb) write(mb)\n"
             "--------------------------------------------------\n"
             );
    value->append(buf);
    for (int level = 0; level < config::knumlevels; level++) {
      int files = versions_->numlevelfiles(level);
      if (stats_[level].micros > 0 || files > 0) {
        snprintf(
            buf, sizeof(buf),
            "%3d %8d %8.0f %9.0f %8.0f %9.0f\n",
            level,
            files,
            versions_->numlevelbytes(level) / 1048576.0,
            stats_[level].micros / 1e6,
            stats_[level].bytes_read / 1048576.0,
            stats_[level].bytes_written / 1048576.0);
        value->append(buf);
      }
    }
    return true;
  } else if (in == "sstables") {
    *value = versions_->current()->debugstring();
    return true;
  }

  return false;
}

void dbimpl::getapproximatesizes(
    const range* range, int n,
    uint64_t* sizes) {
  // todo(opt): better implementation
  version* v;
  {
    mutexlock l(&mutex_);
    versions_->current()->ref();
    v = versions_->current();
  }

  for (int i = 0; i < n; i++) {
    // convert user_key into a corresponding internal key.
    internalkey k1(range[i].start, kmaxsequencenumber, kvaluetypeforseek);
    internalkey k2(range[i].limit, kmaxsequencenumber, kvaluetypeforseek);
    uint64_t start = versions_->approximateoffsetof(v, k1);
    uint64_t limit = versions_->approximateoffsetof(v, k2);
    sizes[i] = (limit >= start ? limit - start : 0);
  }

  {
    mutexlock l(&mutex_);
    v->unref();
  }
}

// default implementations of convenience methods that subclasses of db
// can call if they wish
status db::put(const writeoptions& opt, const slice& key, const slice& value) {
  writebatch batch;
  batch.put(key, value);
  return write(opt, &batch);
}

status db::delete(const writeoptions& opt, const slice& key) {
  writebatch batch;
  batch.delete(key);
  return write(opt, &batch);
}

db::~db() { }

status db::open(const options& options, const std::string& dbname,
                db** dbptr) {
  *dbptr = null;

  dbimpl* impl = new dbimpl(options, dbname);
  impl->mutex_.lock();
  versionedit edit;
  status s = impl->recover(&edit); // handles create_if_missing, error_if_exists
  if (s.ok()) {
    uint64_t new_log_number = impl->versions_->newfilenumber();
    writablefile* lfile;
    s = options.env->newwritablefile(logfilename(dbname, new_log_number),
                                     &lfile);
    if (s.ok()) {
      edit.setlognumber(new_log_number);
      impl->logfile_ = lfile;
      impl->logfile_number_ = new_log_number;
      impl->log_ = new log::writer(lfile);
      s = impl->versions_->logandapply(&edit, &impl->mutex_);
    }
    if (s.ok()) {
      impl->deleteobsoletefiles();
      impl->maybeschedulecompaction();
    }
  }
  impl->mutex_.unlock();
  if (s.ok()) {
    *dbptr = impl;
  } else {
    delete impl;
  }
  return s;
}

snapshot::~snapshot() {
}

status destroydb(const std::string& dbname, const options& options) {
  env* env = options.env;
  std::vector<std::string> filenames;
  // ignore error in case directory does not exist
  env->getchildren(dbname, &filenames);
  if (filenames.empty()) {
    return status::ok();
  }

  filelock* lock;
  const std::string lockname = lockfilename(dbname);
  status result = env->lockfile(lockname, &lock);
  if (result.ok()) {
    uint64_t number;
    filetype type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type) &&
          type != kdblockfile) {  // lock file will be deleted at end
        status del = env->deletefile(dbname + "/" + filenames[i]);
        if (result.ok() && !del.ok()) {
          result = del;
        }
      }
    }
    env->unlockfile(lock);  // ignore error since state is already gone
    env->deletefile(lockname);
    env->deletedir(dbname);  // ignore error in case dir contains other files
  }
  return result;
}

}  // namespace leveldb
