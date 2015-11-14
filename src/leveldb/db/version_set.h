// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// the representation of a dbimpl consists of a set of versions.  the
// newest version is called "current".  older versions may be kept
// around to provide a consistent view to live iterators.
//
// each version keeps track of a set of table files per level.  the
// entire set of versions is maintained in a versionset.
//
// version,versionset are thread-compatible, but require external
// synchronization on all accesses.

#ifndef storage_leveldb_db_version_set_h_
#define storage_leveldb_db_version_set_h_

#include <map>
#include <set>
#include <vector>
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

namespace log { class writer; }

class compaction;
class iterator;
class memtable;
class tablebuilder;
class tablecache;
class version;
class versionset;
class writablefile;

// return the smallest index i such that files[i]->largest >= key.
// return files.size() if there is no such file.
// requires: "files" contains a sorted list of non-overlapping files.
extern int findfile(const internalkeycomparator& icmp,
                    const std::vector<filemetadata*>& files,
                    const slice& key);

// returns true iff some file in "files" overlaps the user key range
// [*smallest,*largest].
// smallest==null represents a key smaller than all keys in the db.
// largest==null represents a key largest than all keys in the db.
// requires: if disjoint_sorted_files, files[] contains disjoint ranges
//           in sorted order.
extern bool somefileoverlapsrange(
    const internalkeycomparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<filemetadata*>& files,
    const slice* smallest_user_key,
    const slice* largest_user_key);

class version {
 public:
  // append to *iters a sequence of iterators that will
  // yield the contents of this version when merged together.
  // requires: this version has been saved (see versionset::saveto)
  void additerators(const readoptions&, std::vector<iterator*>* iters);

  // lookup the value for key.  if found, store it in *val and
  // return ok.  else return a non-ok status.  fills *stats.
  // requires: lock is not held
  struct getstats {
    filemetadata* seek_file;
    int seek_file_level;
  };
  status get(const readoptions&, const lookupkey& key, std::string* val,
             getstats* stats);

  // adds "stats" into the current state.  returns true if a new
  // compaction may need to be triggered, false otherwise.
  // requires: lock is held
  bool updatestats(const getstats& stats);

  // record a sample of bytes read at the specified internal key.
  // samples are taken approximately once every config::kreadbytesperiod
  // bytes.  returns true if a new compaction may need to be triggered.
  // requires: lock is held
  bool recordreadsample(slice key);

  // reference count management (so versions do not disappear out from
  // under live iterators)
  void ref();
  void unref();

  void getoverlappinginputs(
      int level,
      const internalkey* begin,         // null means before all keys
      const internalkey* end,           // null means after all keys
      std::vector<filemetadata*>* inputs);

  // returns true iff some file in the specified level overlaps
  // some part of [*smallest_user_key,*largest_user_key].
  // smallest_user_key==null represents a key smaller than all keys in the db.
  // largest_user_key==null represents a key largest than all keys in the db.
  bool overlapinlevel(int level,
                      const slice* smallest_user_key,
                      const slice* largest_user_key);

  // return the level at which we should place a new memtable compaction
  // result that covers the range [smallest_user_key,largest_user_key].
  int picklevelformemtableoutput(const slice& smallest_user_key,
                                 const slice& largest_user_key);

  int numfiles(int level) const { return files_[level].size(); }

  // return a human readable string that describes this version's contents.
  std::string debugstring() const;

 private:
  friend class compaction;
  friend class versionset;

  class levelfilenumiterator;
  iterator* newconcatenatingiterator(const readoptions&, int level) const;

  // call func(arg, level, f) for every file that overlaps user_key in
  // order from newest to oldest.  if an invocation of func returns
  // false, makes no more calls.
  //
  // requires: user portion of internal_key == user_key.
  void foreachoverlapping(slice user_key, slice internal_key,
                          void* arg,
                          bool (*func)(void*, int, filemetadata*));

  versionset* vset_;            // versionset to which this version belongs
  version* next_;               // next version in linked list
  version* prev_;               // previous version in linked list
  int refs_;                    // number of live refs to this version

  // list of files per level
  std::vector<filemetadata*> files_[config::knumlevels];

  // next file to compact based on seek stats.
  filemetadata* file_to_compact_;
  int file_to_compact_level_;

  // level that should be compacted next and its compaction score.
  // score < 1 means compaction is not strictly needed.  these fields
  // are initialized by finalize().
  double compaction_score_;
  int compaction_level_;

  explicit version(versionset* vset)
      : vset_(vset), next_(this), prev_(this), refs_(0),
        file_to_compact_(null),
        file_to_compact_level_(-1),
        compaction_score_(-1),
        compaction_level_(-1) {
  }

  ~version();

  // no copying allowed
  version(const version&);
  void operator=(const version&);
};

class versionset {
 public:
  versionset(const std::string& dbname,
             const options* options,
             tablecache* table_cache,
             const internalkeycomparator*);
  ~versionset();

  // apply *edit to the current version to form a new descriptor that
  // is both saved to persistent state and installed as the new
  // current version.  will release *mu while actually writing to the file.
  // requires: *mu is held on entry.
  // requires: no other thread concurrently calls logandapply()
  status logandapply(versionedit* edit, port::mutex* mu)
      exclusive_locks_required(mu);

  // recover the last saved descriptor from persistent storage.
  status recover();

  // return the current version.
  version* current() const { return current_; }

  // return the current manifest file number
  uint64_t manifestfilenumber() const { return manifest_file_number_; }

  // allocate and return a new file number
  uint64_t newfilenumber() { return next_file_number_++; }

  // arrange to reuse "file_number" unless a newer file number has
  // already been allocated.
  // requires: "file_number" was returned by a call to newfilenumber().
  void reusefilenumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

  // return the number of table files at the specified level.
  int numlevelfiles(int level) const;

  // return the combined file size of all files at the specified level.
  int64_t numlevelbytes(int level) const;

  // return the last sequence number.
  uint64_t lastsequence() const { return last_sequence_; }

  // set the last sequence number to s.
  void setlastsequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_ = s;
  }

  // mark the specified file number as used.
  void markfilenumberused(uint64_t number);

  // return the current log file number.
  uint64_t lognumber() const { return log_number_; }

  // return the log file number for the log file that is currently
  // being compacted, or zero if there is no such log file.
  uint64_t prevlognumber() const { return prev_log_number_; }

  // pick level and inputs for a new compaction.
  // returns null if there is no compaction to be done.
  // otherwise returns a pointer to a heap-allocated object that
  // describes the compaction.  caller should delete the result.
  compaction* pickcompaction();

  // return a compaction object for compacting the range [begin,end] in
  // the specified level.  returns null if there is nothing in that
  // level that overlaps the specified range.  caller should delete
  // the result.
  compaction* compactrange(
      int level,
      const internalkey* begin,
      const internalkey* end);

  // return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t maxnextleveloverlappingbytes();

  // create an iterator that reads over the compaction inputs for "*c".
  // the caller should delete the iterator when no longer needed.
  iterator* makeinputiterator(compaction* c);

  // returns true iff some level needs a compaction.
  bool needscompaction() const {
    version* v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != null);
  }

  // add all files listed in any live version to *live.
  // may also mutate some internal state.
  void addlivefiles(std::set<uint64_t>* live);

  // return the approximate offset in the database of the data for
  // "key" as of version "v".
  uint64_t approximateoffsetof(version* v, const internalkey& key);

  // return a human-readable short (single-line) summary of the number
  // of files per level.  uses *scratch as backing store.
  struct levelsummarystorage {
    char buffer[100];
  };
  const char* levelsummary(levelsummarystorage* scratch) const;

 private:
  class builder;

  friend class compaction;
  friend class version;

  void finalize(version* v);

  void getrange(const std::vector<filemetadata*>& inputs,
                internalkey* smallest,
                internalkey* largest);

  void getrange2(const std::vector<filemetadata*>& inputs1,
                 const std::vector<filemetadata*>& inputs2,
                 internalkey* smallest,
                 internalkey* largest);

  void setupotherinputs(compaction* c);

  // save current contents to *log
  status writesnapshot(log::writer* log);

  void appendversion(version* v);

  env* const env_;
  const std::string dbname_;
  const options* const options_;
  tablecache* const table_cache_;
  const internalkeycomparator icmp_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted

  // opened lazily
  writablefile* descriptor_file_;
  log::writer* descriptor_log_;
  version dummy_versions_;  // head of circular doubly-linked list of versions.
  version* current_;        // == dummy_versions_.prev_

  // per-level key at which the next compaction at that level should start.
  // either an empty string, or a valid internalkey.
  std::string compact_pointer_[config::knumlevels];

  // no copying allowed
  versionset(const versionset&);
  void operator=(const versionset&);
};

// a compaction encapsulates information about a compaction.
class compaction {
 public:
  ~compaction();

  // return the level that is being compacted.  inputs from "level"
  // and "level+1" will be merged to produce a set of "level+1" files.
  int level() const { return level_; }

  // return the object that holds the edits to the descriptor done
  // by this compaction.
  versionedit* edit() { return &edit_; }

  // "which" must be either 0 or 1
  int num_input_files(int which) const { return inputs_[which].size(); }

  // return the ith input file at "level()+which" ("which" must be 0 or 1).
  filemetadata* input(int which, int i) const { return inputs_[which][i]; }

  // maximum size of files to build during this compaction.
  uint64_t maxoutputfilesize() const { return max_output_file_size_; }

  // is this a trivial compaction that can be implemented by just
  // moving a single input file to the next level (no merging or splitting)
  bool istrivialmove() const;

  // add all inputs to this compaction as delete operations to *edit.
  void addinputdeletions(versionedit* edit);

  // returns true if the information we have available guarantees that
  // the compaction is producing data in "level+1" for which no data exists
  // in levels greater than "level+1".
  bool isbaselevelforkey(const slice& user_key);

  // returns true iff we should stop building the current output
  // before processing "internal_key".
  bool shouldstopbefore(const slice& internal_key);

  // release the input version for the compaction, once the compaction
  // is successful.
  void releaseinputs();

 private:
  friend class version;
  friend class versionset;

  explicit compaction(int level);

  int level_;
  uint64_t max_output_file_size_;
  version* input_version_;
  versionedit edit_;

  // each compaction reads inputs from "level_" and "level_+1"
  std::vector<filemetadata*> inputs_[2];      // the two sets of inputs

  // state used to check for number of of overlapping grandparent files
  // (parent == level_ + 1, grandparent == level_ + 2)
  std::vector<filemetadata*> grandparents_;
  size_t grandparent_index_;  // index in grandparent_starts_
  bool seen_key_;             // some output key has been seen
  int64_t overlapped_bytes_;  // bytes of overlap between current output
                              // and grandparent files

  // state for implementing isbaselevelforkey

  // level_ptrs_ holds indices into input_version_->levels_: our state
  // is that we are positioned at one of the file ranges for each
  // higher level than the ones involved in this compaction (i.e. for
  // all l >= level_ + 2).
  size_t level_ptrs_[config::knumlevels];
};

}  // namespace leveldb

#endif  // storage_leveldb_db_version_set_h_
