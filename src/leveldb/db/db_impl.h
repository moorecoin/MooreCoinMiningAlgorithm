// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_db_impl_h_
#define storage_leveldb_db_db_impl_h_

#include <deque>
#include <set>
#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

class memtable;
class tablecache;
class version;
class versionedit;
class versionset;

class dbimpl : public db {
 public:
  dbimpl(const options& options, const std::string& dbname);
  virtual ~dbimpl();

  // implementations of the db interface
  virtual status put(const writeoptions&, const slice& key, const slice& value);
  virtual status delete(const writeoptions&, const slice& key);
  virtual status write(const writeoptions& options, writebatch* updates);
  virtual status get(const readoptions& options,
                     const slice& key,
                     std::string* value);
  virtual iterator* newiterator(const readoptions&);
  virtual const snapshot* getsnapshot();
  virtual void releasesnapshot(const snapshot* snapshot);
  virtual bool getproperty(const slice& property, std::string* value);
  virtual void getapproximatesizes(const range* range, int n, uint64_t* sizes);
  virtual void compactrange(const slice* begin, const slice* end);

  // extra methods (for testing) that are not in the public db interface

  // compact any files in the named level that overlap [*begin,*end]
  void test_compactrange(int level, const slice* begin, const slice* end);

  // force current memtable contents to be compacted.
  status test_compactmemtable();

  // return an internal iterator over the current state of the database.
  // the keys of this iterator are internal keys (see format.h).
  // the returned iterator should be deleted when no longer needed.
  iterator* test_newinternaliterator();

  // return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t test_maxnextleveloverlappingbytes();

  // record a sample of bytes read at the specified internal key.
  // samples are taken approximately once every config::kreadbytesperiod
  // bytes.
  void recordreadsample(slice key);

 private:
  friend class db;
  struct compactionstate;
  struct writer;

  iterator* newinternaliterator(const readoptions&,
                                sequencenumber* latest_snapshot,
                                uint32_t* seed);

  status newdb();

  // recover the descriptor from persistent storage.  may do a significant
  // amount of work to recover recently logged updates.  any changes to
  // be made to the descriptor are added to *edit.
  status recover(versionedit* edit) exclusive_locks_required(mutex_);

  void maybeignoreerror(status* s) const;

  // delete any unneeded files and stale in-memory entries.
  void deleteobsoletefiles();

  // compact the in-memory write buffer to disk.  switches to a new
  // log-file/memtable and writes a new descriptor iff successful.
  // errors are recorded in bg_error_.
  void compactmemtable() exclusive_locks_required(mutex_);

  status recoverlogfile(uint64_t log_number,
                        versionedit* edit,
                        sequencenumber* max_sequence)
      exclusive_locks_required(mutex_);

  status writelevel0table(memtable* mem, versionedit* edit, version* base)
      exclusive_locks_required(mutex_);

  status makeroomforwrite(bool force /* compact even if there is room? */)
      exclusive_locks_required(mutex_);
  writebatch* buildbatchgroup(writer** last_writer);

  void recordbackgrounderror(const status& s);

  void maybeschedulecompaction() exclusive_locks_required(mutex_);
  static void bgwork(void* db);
  void backgroundcall();
  void  backgroundcompaction() exclusive_locks_required(mutex_);
  void cleanupcompaction(compactionstate* compact)
      exclusive_locks_required(mutex_);
  status docompactionwork(compactionstate* compact)
      exclusive_locks_required(mutex_);

  status opencompactionoutputfile(compactionstate* compact);
  status finishcompactionoutputfile(compactionstate* compact, iterator* input);
  status installcompactionresults(compactionstate* compact)
      exclusive_locks_required(mutex_);

  // constant after construction
  env* const env_;
  const internalkeycomparator internal_comparator_;
  const internalfilterpolicy internal_filter_policy_;
  const options options_;  // options_.comparator == &internal_comparator_
  bool owns_info_log_;
  bool owns_cache_;
  const std::string dbname_;

  // table_cache_ provides its own synchronization
  tablecache* table_cache_;

  // lock over the persistent db state.  non-null iff successfully acquired.
  filelock* db_lock_;

  // state below is protected by mutex_
  port::mutex mutex_;
  port::atomicpointer shutting_down_;
  port::condvar bg_cv_;          // signalled when background work finishes
  memtable* mem_;
  memtable* imm_;                // memtable being compacted
  port::atomicpointer has_imm_;  // so bg thread can detect non-null imm_
  writablefile* logfile_;
  uint64_t logfile_number_;
  log::writer* log_;
  uint32_t seed_;                // for sampling.

  // queue of writers.
  std::deque<writer*> writers_;
  writebatch* tmp_batch_;

  snapshotlist snapshots_;

  // set of table files to protect from deletion because they are
  // part of ongoing compactions.
  std::set<uint64_t> pending_outputs_;

  // has a background compaction been scheduled or is running?
  bool bg_compaction_scheduled_;

  // information for a manual compaction
  struct manualcompaction {
    int level;
    bool done;
    const internalkey* begin;   // null means beginning of key range
    const internalkey* end;     // null means end of key range
    internalkey tmp_storage;    // used to keep track of compaction progress
  };
  manualcompaction* manual_compaction_;

  versionset* versions_;

  // have we encountered a background error in paranoid mode?
  status bg_error_;

  // per level compaction stats.  stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct compactionstats {
    int64_t micros;
    int64_t bytes_read;
    int64_t bytes_written;

    compactionstats() : micros(0), bytes_read(0), bytes_written(0) { }

    void add(const compactionstats& c) {
      this->micros += c.micros;
      this->bytes_read += c.bytes_read;
      this->bytes_written += c.bytes_written;
    }
  };
  compactionstats stats_[config::knumlevels];

  // no copying allowed
  dbimpl(const dbimpl&);
  void operator=(const dbimpl&);

  const comparator* user_comparator() const {
    return internal_comparator_.user_comparator();
  }
};

// sanitize db options.  the caller should delete result.info_log if
// it is not equal to src.info_log.
extern options sanitizeoptions(const std::string& db,
                               const internalkeycomparator* icmp,
                               const internalfilterpolicy* ipolicy,
                               const options& src);

}  // namespace leveldb

#endif  // storage_leveldb_db_db_impl_h_
