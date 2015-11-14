// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_set.h"

#include <algorithm>
#include <stdio.h>
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "leveldb/env.h"
#include "leveldb/table_builder.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

static const int ktargetfilesize = 2 * 1048576;

// maximum bytes of overlaps in grandparent (i.e., level+2) before we
// stop building a single file in a level->level+1 compaction.
static const int64_t kmaxgrandparentoverlapbytes = 10 * ktargetfilesize;

// maximum number of bytes in all compacted files.  we avoid expanding
// the lower level file set of a compaction if it would make the
// total compaction cover more than this many bytes.
static const int64_t kexpandedcompactionbytesizelimit = 25 * ktargetfilesize;

static double maxbytesforlevel(int level) {
  // note: the result for level zero is not really used since we set
  // the level-0 compaction threshold based on number of files.
  double result = 10 * 1048576.0;  // result for both level-0 and level-1
  while (level > 1) {
    result *= 10;
    level--;
  }
  return result;
}

static uint64_t maxfilesizeforlevel(int level) {
  return ktargetfilesize;  // we could vary per level to reduce number of files?
}

static int64_t totalfilesize(const std::vector<filemetadata*>& files) {
  int64_t sum = 0;
  for (size_t i = 0; i < files.size(); i++) {
    sum += files[i]->file_size;
  }
  return sum;
}

version::~version() {
  assert(refs_ == 0);

  // remove from linked list
  prev_->next_ = next_;
  next_->prev_ = prev_;

  // drop references to files
  for (int level = 0; level < config::knumlevels; level++) {
    for (size_t i = 0; i < files_[level].size(); i++) {
      filemetadata* f = files_[level][i];
      assert(f->refs > 0);
      f->refs--;
      if (f->refs <= 0) {
        delete f;
      }
    }
  }
}

int findfile(const internalkeycomparator& icmp,
             const std::vector<filemetadata*>& files,
             const slice& key) {
  uint32_t left = 0;
  uint32_t right = files.size();
  while (left < right) {
    uint32_t mid = (left + right) / 2;
    const filemetadata* f = files[mid];
    if (icmp.internalkeycomparator::compare(f->largest.encode(), key) < 0) {
      // key at "mid.largest" is < "target".  therefore all
      // files at or before "mid" are uninteresting.
      left = mid + 1;
    } else {
      // key at "mid.largest" is >= "target".  therefore all files
      // after "mid" are uninteresting.
      right = mid;
    }
  }
  return right;
}

static bool afterfile(const comparator* ucmp,
                      const slice* user_key, const filemetadata* f) {
  // null user_key occurs before all keys and is therefore never after *f
  return (user_key != null &&
          ucmp->compare(*user_key, f->largest.user_key()) > 0);
}

static bool beforefile(const comparator* ucmp,
                       const slice* user_key, const filemetadata* f) {
  // null user_key occurs after all keys and is therefore never before *f
  return (user_key != null &&
          ucmp->compare(*user_key, f->smallest.user_key()) < 0);
}

bool somefileoverlapsrange(
    const internalkeycomparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<filemetadata*>& files,
    const slice* smallest_user_key,
    const slice* largest_user_key) {
  const comparator* ucmp = icmp.user_comparator();
  if (!disjoint_sorted_files) {
    // need to check against all files
    for (size_t i = 0; i < files.size(); i++) {
      const filemetadata* f = files[i];
      if (afterfile(ucmp, smallest_user_key, f) ||
          beforefile(ucmp, largest_user_key, f)) {
        // no overlap
      } else {
        return true;  // overlap
      }
    }
    return false;
  }

  // binary search over file list
  uint32_t index = 0;
  if (smallest_user_key != null) {
    // find the earliest possible internal key for smallest_user_key
    internalkey small(*smallest_user_key, kmaxsequencenumber,kvaluetypeforseek);
    index = findfile(icmp, files, small.encode());
  }

  if (index >= files.size()) {
    // beginning of range is after all files, so no overlap.
    return false;
  }

  return !beforefile(ucmp, largest_user_key, files[index]);
}

// an internal iterator.  for a given version/level pair, yields
// information about the files in the level.  for a given entry, key()
// is the largest key that occurs in the file, and value() is an
// 16-byte value containing the file number and file size, both
// encoded using encodefixed64.
class version::levelfilenumiterator : public iterator {
 public:
  levelfilenumiterator(const internalkeycomparator& icmp,
                       const std::vector<filemetadata*>* flist)
      : icmp_(icmp),
        flist_(flist),
        index_(flist->size()) {        // marks as invalid
  }
  virtual bool valid() const {
    return index_ < flist_->size();
  }
  virtual void seek(const slice& target) {
    index_ = findfile(icmp_, *flist_, target);
  }
  virtual void seektofirst() { index_ = 0; }
  virtual void seektolast() {
    index_ = flist_->empty() ? 0 : flist_->size() - 1;
  }
  virtual void next() {
    assert(valid());
    index_++;
  }
  virtual void prev() {
    assert(valid());
    if (index_ == 0) {
      index_ = flist_->size();  // marks as invalid
    } else {
      index_--;
    }
  }
  slice key() const {
    assert(valid());
    return (*flist_)[index_]->largest.encode();
  }
  slice value() const {
    assert(valid());
    encodefixed64(value_buf_, (*flist_)[index_]->number);
    encodefixed64(value_buf_+8, (*flist_)[index_]->file_size);
    return slice(value_buf_, sizeof(value_buf_));
  }
  virtual status status() const { return status::ok(); }
 private:
  const internalkeycomparator icmp_;
  const std::vector<filemetadata*>* const flist_;
  uint32_t index_;

  // backing store for value().  holds the file number and size.
  mutable char value_buf_[16];
};

static iterator* getfileiterator(void* arg,
                                 const readoptions& options,
                                 const slice& file_value) {
  tablecache* cache = reinterpret_cast<tablecache*>(arg);
  if (file_value.size() != 16) {
    return newerroriterator(
        status::corruption("filereader invoked with unexpected value"));
  } else {
    return cache->newiterator(options,
                              decodefixed64(file_value.data()),
                              decodefixed64(file_value.data() + 8));
  }
}

iterator* version::newconcatenatingiterator(const readoptions& options,
                                            int level) const {
  return newtwoleveliterator(
      new levelfilenumiterator(vset_->icmp_, &files_[level]),
      &getfileiterator, vset_->table_cache_, options);
}

void version::additerators(const readoptions& options,
                           std::vector<iterator*>* iters) {
  // merge all level zero files together since they may overlap
  for (size_t i = 0; i < files_[0].size(); i++) {
    iters->push_back(
        vset_->table_cache_->newiterator(
            options, files_[0][i]->number, files_[0][i]->file_size));
  }

  // for levels > 0, we can use a concatenating iterator that sequentially
  // walks through the non-overlapping files in the level, opening them
  // lazily.
  for (int level = 1; level < config::knumlevels; level++) {
    if (!files_[level].empty()) {
      iters->push_back(newconcatenatingiterator(options, level));
    }
  }
}

// callback from tablecache::get()
namespace {
enum saverstate {
  knotfound,
  kfound,
  kdeleted,
  kcorrupt,
};
struct saver {
  saverstate state;
  const comparator* ucmp;
  slice user_key;
  std::string* value;
};
}
static void savevalue(void* arg, const slice& ikey, const slice& v) {
  saver* s = reinterpret_cast<saver*>(arg);
  parsedinternalkey parsed_key;
  if (!parseinternalkey(ikey, &parsed_key)) {
    s->state = kcorrupt;
  } else {
    if (s->ucmp->compare(parsed_key.user_key, s->user_key) == 0) {
      s->state = (parsed_key.type == ktypevalue) ? kfound : kdeleted;
      if (s->state == kfound) {
        s->value->assign(v.data(), v.size());
      }
    }
  }
}

static bool newestfirst(filemetadata* a, filemetadata* b) {
  return a->number > b->number;
}

void version::foreachoverlapping(slice user_key, slice internal_key,
                                 void* arg,
                                 bool (*func)(void*, int, filemetadata*)) {
  // todo(sanjay): change version::get() to use this function.
  const comparator* ucmp = vset_->icmp_.user_comparator();

  // search level-0 in order from newest to oldest.
  std::vector<filemetadata*> tmp;
  tmp.reserve(files_[0].size());
  for (uint32_t i = 0; i < files_[0].size(); i++) {
    filemetadata* f = files_[0][i];
    if (ucmp->compare(user_key, f->smallest.user_key()) >= 0 &&
        ucmp->compare(user_key, f->largest.user_key()) <= 0) {
      tmp.push_back(f);
    }
  }
  if (!tmp.empty()) {
    std::sort(tmp.begin(), tmp.end(), newestfirst);
    for (uint32_t i = 0; i < tmp.size(); i++) {
      if (!(*func)(arg, 0, tmp[i])) {
        return;
      }
    }
  }

  // search other levels.
  for (int level = 1; level < config::knumlevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

    // binary search to find earliest index whose largest key >= internal_key.
    uint32_t index = findfile(vset_->icmp_, files_[level], internal_key);
    if (index < num_files) {
      filemetadata* f = files_[level][index];
      if (ucmp->compare(user_key, f->smallest.user_key()) < 0) {
        // all of "f" is past any data for user_key
      } else {
        if (!(*func)(arg, level, f)) {
          return;
        }
      }
    }
  }
}

status version::get(const readoptions& options,
                    const lookupkey& k,
                    std::string* value,
                    getstats* stats) {
  slice ikey = k.internal_key();
  slice user_key = k.user_key();
  const comparator* ucmp = vset_->icmp_.user_comparator();
  status s;

  stats->seek_file = null;
  stats->seek_file_level = -1;
  filemetadata* last_file_read = null;
  int last_file_read_level = -1;

  // we can search level-by-level since entries never hop across
  // levels.  therefore we are guaranteed that if we find data
  // in an smaller level, later levels are irrelevant.
  std::vector<filemetadata*> tmp;
  filemetadata* tmp2;
  for (int level = 0; level < config::knumlevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

    // get the list of files to search in this level
    filemetadata* const* files = &files_[level][0];
    if (level == 0) {
      // level-0 files may overlap each other.  find all files that
      // overlap user_key and process them in order from newest to oldest.
      tmp.reserve(num_files);
      for (uint32_t i = 0; i < num_files; i++) {
        filemetadata* f = files[i];
        if (ucmp->compare(user_key, f->smallest.user_key()) >= 0 &&
            ucmp->compare(user_key, f->largest.user_key()) <= 0) {
          tmp.push_back(f);
        }
      }
      if (tmp.empty()) continue;

      std::sort(tmp.begin(), tmp.end(), newestfirst);
      files = &tmp[0];
      num_files = tmp.size();
    } else {
      // binary search to find earliest index whose largest key >= ikey.
      uint32_t index = findfile(vset_->icmp_, files_[level], ikey);
      if (index >= num_files) {
        files = null;
        num_files = 0;
      } else {
        tmp2 = files[index];
        if (ucmp->compare(user_key, tmp2->smallest.user_key()) < 0) {
          // all of "tmp2" is past any data for user_key
          files = null;
          num_files = 0;
        } else {
          files = &tmp2;
          num_files = 1;
        }
      }
    }

    for (uint32_t i = 0; i < num_files; ++i) {
      if (last_file_read != null && stats->seek_file == null) {
        // we have had more than one seek for this read.  charge the 1st file.
        stats->seek_file = last_file_read;
        stats->seek_file_level = last_file_read_level;
      }

      filemetadata* f = files[i];
      last_file_read = f;
      last_file_read_level = level;

      saver saver;
      saver.state = knotfound;
      saver.ucmp = ucmp;
      saver.user_key = user_key;
      saver.value = value;
      s = vset_->table_cache_->get(options, f->number, f->file_size,
                                   ikey, &saver, savevalue);
      if (!s.ok()) {
        return s;
      }
      switch (saver.state) {
        case knotfound:
          break;      // keep searching in other files
        case kfound:
          return s;
        case kdeleted:
          s = status::notfound(slice());  // use empty error message for speed
          return s;
        case kcorrupt:
          s = status::corruption("corrupted key for ", user_key);
          return s;
      }
    }
  }

  return status::notfound(slice());  // use an empty error message for speed
}

bool version::updatestats(const getstats& stats) {
  filemetadata* f = stats.seek_file;
  if (f != null) {
    f->allowed_seeks--;
    if (f->allowed_seeks <= 0 && file_to_compact_ == null) {
      file_to_compact_ = f;
      file_to_compact_level_ = stats.seek_file_level;
      return true;
    }
  }
  return false;
}

bool version::recordreadsample(slice internal_key) {
  parsedinternalkey ikey;
  if (!parseinternalkey(internal_key, &ikey)) {
    return false;
  }

  struct state {
    getstats stats;  // holds first matching file
    int matches;

    static bool match(void* arg, int level, filemetadata* f) {
      state* state = reinterpret_cast<state*>(arg);
      state->matches++;
      if (state->matches == 1) {
        // remember first match.
        state->stats.seek_file = f;
        state->stats.seek_file_level = level;
      }
      // we can stop iterating once we have a second match.
      return state->matches < 2;
    }
  };

  state state;
  state.matches = 0;
  foreachoverlapping(ikey.user_key, internal_key, &state, &state::match);

  // must have at least two matches since we want to merge across
  // files. but what if we have a single file that contains many
  // overwrites and deletions?  should we have another mechanism for
  // finding such files?
  if (state.matches >= 2) {
    // 1mb cost is about 1 seek (see comment in builder::apply).
    return updatestats(state.stats);
  }
  return false;
}

void version::ref() {
  ++refs_;
}

void version::unref() {
  assert(this != &vset_->dummy_versions_);
  assert(refs_ >= 1);
  --refs_;
  if (refs_ == 0) {
    delete this;
  }
}

bool version::overlapinlevel(int level,
                             const slice* smallest_user_key,
                             const slice* largest_user_key) {
  return somefileoverlapsrange(vset_->icmp_, (level > 0), files_[level],
                               smallest_user_key, largest_user_key);
}

int version::picklevelformemtableoutput(
    const slice& smallest_user_key,
    const slice& largest_user_key) {
  int level = 0;
  if (!overlapinlevel(0, &smallest_user_key, &largest_user_key)) {
    // push to next level if there is no overlap in next level,
    // and the #bytes overlapping in the level after that are limited.
    internalkey start(smallest_user_key, kmaxsequencenumber, kvaluetypeforseek);
    internalkey limit(largest_user_key, 0, static_cast<valuetype>(0));
    std::vector<filemetadata*> overlaps;
    while (level < config::kmaxmemcompactlevel) {
      if (overlapinlevel(level + 1, &smallest_user_key, &largest_user_key)) {
        break;
      }
      if (level + 2 < config::knumlevels) {
        // check that file does not overlap too many grandparent bytes.
        getoverlappinginputs(level + 2, &start, &limit, &overlaps);
        const int64_t sum = totalfilesize(overlaps);
        if (sum > kmaxgrandparentoverlapbytes) {
          break;
        }
      }
      level++;
    }
  }
  return level;
}

// store in "*inputs" all files in "level" that overlap [begin,end]
void version::getoverlappinginputs(
    int level,
    const internalkey* begin,
    const internalkey* end,
    std::vector<filemetadata*>* inputs) {
  assert(level >= 0);
  assert(level < config::knumlevels);
  inputs->clear();
  slice user_begin, user_end;
  if (begin != null) {
    user_begin = begin->user_key();
  }
  if (end != null) {
    user_end = end->user_key();
  }
  const comparator* user_cmp = vset_->icmp_.user_comparator();
  for (size_t i = 0; i < files_[level].size(); ) {
    filemetadata* f = files_[level][i++];
    const slice file_start = f->smallest.user_key();
    const slice file_limit = f->largest.user_key();
    if (begin != null && user_cmp->compare(file_limit, user_begin) < 0) {
      // "f" is completely before specified range; skip it
    } else if (end != null && user_cmp->compare(file_start, user_end) > 0) {
      // "f" is completely after specified range; skip it
    } else {
      inputs->push_back(f);
      if (level == 0) {
        // level-0 files may overlap each other.  so check if the newly
        // added file has expanded the range.  if so, restart search.
        if (begin != null && user_cmp->compare(file_start, user_begin) < 0) {
          user_begin = file_start;
          inputs->clear();
          i = 0;
        } else if (end != null && user_cmp->compare(file_limit, user_end) > 0) {
          user_end = file_limit;
          inputs->clear();
          i = 0;
        }
      }
    }
  }
}

std::string version::debugstring() const {
  std::string r;
  for (int level = 0; level < config::knumlevels; level++) {
    // e.g.,
    //   --- level 1 ---
    //   17:123['a' .. 'd']
    //   20:43['e' .. 'g']
    r.append("--- level ");
    appendnumberto(&r, level);
    r.append(" ---\n");
    const std::vector<filemetadata*>& files = files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      r.push_back(' ');
      appendnumberto(&r, files[i]->number);
      r.push_back(':');
      appendnumberto(&r, files[i]->file_size);
      r.append("[");
      r.append(files[i]->smallest.debugstring());
      r.append(" .. ");
      r.append(files[i]->largest.debugstring());
      r.append("]\n");
    }
  }
  return r;
}

// a helper class so we can efficiently apply a whole sequence
// of edits to a particular state without creating intermediate
// versions that contain full copies of the intermediate state.
class versionset::builder {
 private:
  // helper to sort by v->files_[file_number].smallest
  struct bysmallestkey {
    const internalkeycomparator* internal_comparator;

    bool operator()(filemetadata* f1, filemetadata* f2) const {
      int r = internal_comparator->compare(f1->smallest, f2->smallest);
      if (r != 0) {
        return (r < 0);
      } else {
        // break ties by file number
        return (f1->number < f2->number);
      }
    }
  };

  typedef std::set<filemetadata*, bysmallestkey> fileset;
  struct levelstate {
    std::set<uint64_t> deleted_files;
    fileset* added_files;
  };

  versionset* vset_;
  version* base_;
  levelstate levels_[config::knumlevels];

 public:
  // initialize a builder with the files from *base and other info from *vset
  builder(versionset* vset, version* base)
      : vset_(vset),
        base_(base) {
    base_->ref();
    bysmallestkey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::knumlevels; level++) {
      levels_[level].added_files = new fileset(cmp);
    }
  }

  ~builder() {
    for (int level = 0; level < config::knumlevels; level++) {
      const fileset* added = levels_[level].added_files;
      std::vector<filemetadata*> to_unref;
      to_unref.reserve(added->size());
      for (fileset::const_iterator it = added->begin();
          it != added->end(); ++it) {
        to_unref.push_back(*it);
      }
      delete added;
      for (uint32_t i = 0; i < to_unref.size(); i++) {
        filemetadata* f = to_unref[i];
        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
    }
    base_->unref();
  }

  // apply all of the edits in *edit to the current state.
  void apply(versionedit* edit) {
    // update compaction pointers
    for (size_t i = 0; i < edit->compact_pointers_.size(); i++) {
      const int level = edit->compact_pointers_[i].first;
      vset_->compact_pointer_[level] =
          edit->compact_pointers_[i].second.encode().tostring();
    }

    // delete files
    const versionedit::deletedfileset& del = edit->deleted_files_;
    for (versionedit::deletedfileset::const_iterator iter = del.begin();
         iter != del.end();
         ++iter) {
      const int level = iter->first;
      const uint64_t number = iter->second;
      levels_[level].deleted_files.insert(number);
    }

    // add new files
    for (size_t i = 0; i < edit->new_files_.size(); i++) {
      const int level = edit->new_files_[i].first;
      filemetadata* f = new filemetadata(edit->new_files_[i].second);
      f->refs = 1;

      // we arrange to automatically compact this file after
      // a certain number of seeks.  let's assume:
      //   (1) one seek costs 10ms
      //   (2) writing or reading 1mb costs 10ms (100mb/s)
      //   (3) a compaction of 1mb does 25mb of io:
      //         1mb read from this level
      //         10-12mb read from next level (boundaries may be misaligned)
      //         10-12mb written to next level
      // this implies that 25 seeks cost the same as the compaction
      // of 1mb of data.  i.e., one seek costs approximately the
      // same as the compaction of 40kb of data.  we are a little
      // conservative and allow approximately one seek for every 16kb
      // of data before triggering a compaction.
      f->allowed_seeks = (f->file_size / 16384);
      if (f->allowed_seeks < 100) f->allowed_seeks = 100;

      levels_[level].deleted_files.erase(f->number);
      levels_[level].added_files->insert(f);
    }
  }

  // save the current state in *v.
  void saveto(version* v) {
    bysmallestkey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::knumlevels; level++) {
      // merge the set of added files with the set of pre-existing files.
      // drop any deleted files.  store the result in *v.
      const std::vector<filemetadata*>& base_files = base_->files_[level];
      std::vector<filemetadata*>::const_iterator base_iter = base_files.begin();
      std::vector<filemetadata*>::const_iterator base_end = base_files.end();
      const fileset* added = levels_[level].added_files;
      v->files_[level].reserve(base_files.size() + added->size());
      for (fileset::const_iterator added_iter = added->begin();
           added_iter != added->end();
           ++added_iter) {
        // add all smaller files listed in base_
        for (std::vector<filemetadata*>::const_iterator bpos
                 = std::upper_bound(base_iter, base_end, *added_iter, cmp);
             base_iter != bpos;
             ++base_iter) {
          maybeaddfile(v, level, *base_iter);
        }

        maybeaddfile(v, level, *added_iter);
      }

      // add remaining base files
      for (; base_iter != base_end; ++base_iter) {
        maybeaddfile(v, level, *base_iter);
      }

#ifndef ndebug
      // make sure there is no overlap in levels > 0
      if (level > 0) {
        for (uint32_t i = 1; i < v->files_[level].size(); i++) {
          const internalkey& prev_end = v->files_[level][i-1]->largest;
          const internalkey& this_begin = v->files_[level][i]->smallest;
          if (vset_->icmp_.compare(prev_end, this_begin) >= 0) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    prev_end.debugstring().c_str(),
                    this_begin.debugstring().c_str());
            abort();
          }
        }
      }
#endif
    }
  }

  void maybeaddfile(version* v, int level, filemetadata* f) {
    if (levels_[level].deleted_files.count(f->number) > 0) {
      // file is deleted: do nothing
    } else {
      std::vector<filemetadata*>* files = &v->files_[level];
      if (level > 0 && !files->empty()) {
        // must not overlap
        assert(vset_->icmp_.compare((*files)[files->size()-1]->largest,
                                    f->smallest) < 0);
      }
      f->refs++;
      files->push_back(f);
    }
  }
};

versionset::versionset(const std::string& dbname,
                       const options* options,
                       tablecache* table_cache,
                       const internalkeycomparator* cmp)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      next_file_number_(2),
      manifest_file_number_(0),  // filled by recover()
      last_sequence_(0),
      log_number_(0),
      prev_log_number_(0),
      descriptor_file_(null),
      descriptor_log_(null),
      dummy_versions_(this),
      current_(null) {
  appendversion(new version(this));
}

versionset::~versionset() {
  current_->unref();
  assert(dummy_versions_.next_ == &dummy_versions_);  // list must be empty
  delete descriptor_log_;
  delete descriptor_file_;
}

void versionset::appendversion(version* v) {
  // make "v" current
  assert(v->refs_ == 0);
  assert(v != current_);
  if (current_ != null) {
    current_->unref();
  }
  current_ = v;
  v->ref();

  // append to linked list
  v->prev_ = dummy_versions_.prev_;
  v->next_ = &dummy_versions_;
  v->prev_->next_ = v;
  v->next_->prev_ = v;
}

status versionset::logandapply(versionedit* edit, port::mutex* mu) {
  if (edit->has_log_number_) {
    assert(edit->log_number_ >= log_number_);
    assert(edit->log_number_ < next_file_number_);
  } else {
    edit->setlognumber(log_number_);
  }

  if (!edit->has_prev_log_number_) {
    edit->setprevlognumber(prev_log_number_);
  }

  edit->setnextfile(next_file_number_);
  edit->setlastsequence(last_sequence_);

  version* v = new version(this);
  {
    builder builder(this, current_);
    builder.apply(edit);
    builder.saveto(v);
  }
  finalize(v);

  // initialize new descriptor log file if necessary by creating
  // a temporary file that contains a snapshot of the current version.
  std::string new_manifest_file;
  status s;
  if (descriptor_log_ == null) {
    // no reason to unlock *mu here since we only hit this path in the
    // first call to logandapply (when opening the database).
    assert(descriptor_file_ == null);
    new_manifest_file = descriptorfilename(dbname_, manifest_file_number_);
    edit->setnextfile(next_file_number_);
    s = env_->newwritablefile(new_manifest_file, &descriptor_file_);
    if (s.ok()) {
      descriptor_log_ = new log::writer(descriptor_file_);
      s = writesnapshot(descriptor_log_);
    }
  }

  // unlock during expensive manifest log write
  {
    mu->unlock();

    // write new record to manifest log
    if (s.ok()) {
      std::string record;
      edit->encodeto(&record);
      s = descriptor_log_->addrecord(record);
      if (s.ok()) {
        s = descriptor_file_->sync();
      }
      if (!s.ok()) {
        log(options_->info_log, "manifest write: %s\n", s.tostring().c_str());
      }
    }

    // if we just created a new descriptor file, install it by writing a
    // new current file that points to it.
    if (s.ok() && !new_manifest_file.empty()) {
      s = setcurrentfile(env_, dbname_, manifest_file_number_);
    }

    mu->lock();
  }

  // install the new version
  if (s.ok()) {
    appendversion(v);
    log_number_ = edit->log_number_;
    prev_log_number_ = edit->prev_log_number_;
  } else {
    delete v;
    if (!new_manifest_file.empty()) {
      delete descriptor_log_;
      delete descriptor_file_;
      descriptor_log_ = null;
      descriptor_file_ = null;
      env_->deletefile(new_manifest_file);
    }
  }

  return s;
}

status versionset::recover() {
  struct logreporter : public log::reader::reporter {
    status* status;
    virtual void corruption(size_t bytes, const status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  // read "current" file, which contains a pointer to the current manifest file
  std::string current;
  status s = readfiletostring(env_, currentfilename(dbname_), &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return status::corruption("current file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname_ + "/" + current;
  sequentialfile* file;
  s = env_->newsequentialfile(dscname, &file);
  if (!s.ok()) {
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  builder builder(this, current_);

  {
    logreporter reporter;
    reporter.status = &s;
    log::reader reader(file, &reporter, true/*checksum*/, 0/*initial_offset*/);
    slice record;
    std::string scratch;
    while (reader.readrecord(&record, &scratch) && s.ok()) {
      versionedit edit;
      s = edit.decodefrom(record);
      if (s.ok()) {
        if (edit.has_comparator_ &&
            edit.comparator_ != icmp_.user_comparator()->name()) {
          s = status::invalidargument(
              edit.comparator_ + " does not match existing comparator ",
              icmp_.user_comparator()->name());
        }
      }

      if (s.ok()) {
        builder.apply(&edit);
      }

      if (edit.has_log_number_) {
        log_number = edit.log_number_;
        have_log_number = true;
      }

      if (edit.has_prev_log_number_) {
        prev_log_number = edit.prev_log_number_;
        have_prev_log_number = true;
      }

      if (edit.has_next_file_number_) {
        next_file = edit.next_file_number_;
        have_next_file = true;
      }

      if (edit.has_last_sequence_) {
        last_sequence = edit.last_sequence_;
        have_last_sequence = true;
      }
    }
  }
  delete file;
  file = null;

  if (s.ok()) {
    if (!have_next_file) {
      s = status::corruption("no meta-nextfile entry in descriptor");
    } else if (!have_log_number) {
      s = status::corruption("no meta-lognumber entry in descriptor");
    } else if (!have_last_sequence) {
      s = status::corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }

    markfilenumberused(prev_log_number);
    markfilenumberused(log_number);
  }

  if (s.ok()) {
    version* v = new version(this);
    builder.saveto(v);
    // install recovered version
    finalize(v);
    appendversion(v);
    manifest_file_number_ = next_file;
    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    log_number_ = log_number;
    prev_log_number_ = prev_log_number;
  }

  return s;
}

void versionset::markfilenumberused(uint64_t number) {
  if (next_file_number_ <= number) {
    next_file_number_ = number + 1;
  }
}

void versionset::finalize(version* v) {
  // precomputed best level for next compaction
  int best_level = -1;
  double best_score = -1;

  for (int level = 0; level < config::knumlevels-1; level++) {
    double score;
    if (level == 0) {
      // we treat level-0 specially by bounding the number of files
      // instead of number of bytes for two reasons:
      //
      // (1) with larger write-buffer sizes, it is nice not to do too
      // many level-0 compactions.
      //
      // (2) the files in level-0 are merged on every read and
      // therefore we wish to avoid too many files when the individual
      // file size is small (perhaps because of a small write-buffer
      // setting, or very high compression ratios, or lots of
      // overwrites/deletions).
      score = v->files_[level].size() /
          static_cast<double>(config::kl0_compactiontrigger);
    } else {
      // compute the ratio of current size to size limit.
      const uint64_t level_bytes = totalfilesize(v->files_[level]);
      score = static_cast<double>(level_bytes) / maxbytesforlevel(level);
    }

    if (score > best_score) {
      best_level = level;
      best_score = score;
    }
  }

  v->compaction_level_ = best_level;
  v->compaction_score_ = best_score;
}

status versionset::writesnapshot(log::writer* log) {
  // todo: break up into multiple records to reduce memory usage on recovery?

  // save metadata
  versionedit edit;
  edit.setcomparatorname(icmp_.user_comparator()->name());

  // save compaction pointers
  for (int level = 0; level < config::knumlevels; level++) {
    if (!compact_pointer_[level].empty()) {
      internalkey key;
      key.decodefrom(compact_pointer_[level]);
      edit.setcompactpointer(level, key);
    }
  }

  // save files
  for (int level = 0; level < config::knumlevels; level++) {
    const std::vector<filemetadata*>& files = current_->files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      const filemetadata* f = files[i];
      edit.addfile(level, f->number, f->file_size, f->smallest, f->largest);
    }
  }

  std::string record;
  edit.encodeto(&record);
  return log->addrecord(record);
}

int versionset::numlevelfiles(int level) const {
  assert(level >= 0);
  assert(level < config::knumlevels);
  return current_->files_[level].size();
}

const char* versionset::levelsummary(levelsummarystorage* scratch) const {
  // update code if knumlevels changes
  assert(config::knumlevels == 7);
  snprintf(scratch->buffer, sizeof(scratch->buffer),
           "files[ %d %d %d %d %d %d %d ]",
           int(current_->files_[0].size()),
           int(current_->files_[1].size()),
           int(current_->files_[2].size()),
           int(current_->files_[3].size()),
           int(current_->files_[4].size()),
           int(current_->files_[5].size()),
           int(current_->files_[6].size()));
  return scratch->buffer;
}

uint64_t versionset::approximateoffsetof(version* v, const internalkey& ikey) {
  uint64_t result = 0;
  for (int level = 0; level < config::knumlevels; level++) {
    const std::vector<filemetadata*>& files = v->files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      if (icmp_.compare(files[i]->largest, ikey) <= 0) {
        // entire file is before "ikey", so just add the file size
        result += files[i]->file_size;
      } else if (icmp_.compare(files[i]->smallest, ikey) > 0) {
        // entire file is after "ikey", so ignore
        if (level > 0) {
          // files other than level 0 are sorted by meta->smallest, so
          // no further files in this level will contain data for
          // "ikey".
          break;
        }
      } else {
        // "ikey" falls in the range for this table.  add the
        // approximate offset of "ikey" within the table.
        table* tableptr;
        iterator* iter = table_cache_->newiterator(
            readoptions(), files[i]->number, files[i]->file_size, &tableptr);
        if (tableptr != null) {
          result += tableptr->approximateoffsetof(ikey.encode());
        }
        delete iter;
      }
    }
  }
  return result;
}

void versionset::addlivefiles(std::set<uint64_t>* live) {
  for (version* v = dummy_versions_.next_;
       v != &dummy_versions_;
       v = v->next_) {
    for (int level = 0; level < config::knumlevels; level++) {
      const std::vector<filemetadata*>& files = v->files_[level];
      for (size_t i = 0; i < files.size(); i++) {
        live->insert(files[i]->number);
      }
    }
  }
}

int64_t versionset::numlevelbytes(int level) const {
  assert(level >= 0);
  assert(level < config::knumlevels);
  return totalfilesize(current_->files_[level]);
}

int64_t versionset::maxnextleveloverlappingbytes() {
  int64_t result = 0;
  std::vector<filemetadata*> overlaps;
  for (int level = 1; level < config::knumlevels - 1; level++) {
    for (size_t i = 0; i < current_->files_[level].size(); i++) {
      const filemetadata* f = current_->files_[level][i];
      current_->getoverlappinginputs(level+1, &f->smallest, &f->largest,
                                     &overlaps);
      const int64_t sum = totalfilesize(overlaps);
      if (sum > result) {
        result = sum;
      }
    }
  }
  return result;
}

// stores the minimal range that covers all entries in inputs in
// *smallest, *largest.
// requires: inputs is not empty
void versionset::getrange(const std::vector<filemetadata*>& inputs,
                          internalkey* smallest,
                          internalkey* largest) {
  assert(!inputs.empty());
  smallest->clear();
  largest->clear();
  for (size_t i = 0; i < inputs.size(); i++) {
    filemetadata* f = inputs[i];
    if (i == 0) {
      *smallest = f->smallest;
      *largest = f->largest;
    } else {
      if (icmp_.compare(f->smallest, *smallest) < 0) {
        *smallest = f->smallest;
      }
      if (icmp_.compare(f->largest, *largest) > 0) {
        *largest = f->largest;
      }
    }
  }
}

// stores the minimal range that covers all entries in inputs1 and inputs2
// in *smallest, *largest.
// requires: inputs is not empty
void versionset::getrange2(const std::vector<filemetadata*>& inputs1,
                           const std::vector<filemetadata*>& inputs2,
                           internalkey* smallest,
                           internalkey* largest) {
  std::vector<filemetadata*> all = inputs1;
  all.insert(all.end(), inputs2.begin(), inputs2.end());
  getrange(all, smallest, largest);
}

iterator* versionset::makeinputiterator(compaction* c) {
  readoptions options;
  options.verify_checksums = options_->paranoid_checks;
  options.fill_cache = false;

  // level-0 files have to be merged together.  for other levels,
  // we will make a concatenating iterator per level.
  // todo(opt): use concatenating iterator for level-0 if there is no overlap
  const int space = (c->level() == 0 ? c->inputs_[0].size() + 1 : 2);
  iterator** list = new iterator*[space];
  int num = 0;
  for (int which = 0; which < 2; which++) {
    if (!c->inputs_[which].empty()) {
      if (c->level() + which == 0) {
        const std::vector<filemetadata*>& files = c->inputs_[which];
        for (size_t i = 0; i < files.size(); i++) {
          list[num++] = table_cache_->newiterator(
              options, files[i]->number, files[i]->file_size);
        }
      } else {
        // create concatenating iterator for the files from this level
        list[num++] = newtwoleveliterator(
            new version::levelfilenumiterator(icmp_, &c->inputs_[which]),
            &getfileiterator, table_cache_, options);
      }
    }
  }
  assert(num <= space);
  iterator* result = newmergingiterator(&icmp_, list, num);
  delete[] list;
  return result;
}

compaction* versionset::pickcompaction() {
  compaction* c;
  int level;

  // we prefer compactions triggered by too much data in a level over
  // the compactions triggered by seeks.
  const bool size_compaction = (current_->compaction_score_ >= 1);
  const bool seek_compaction = (current_->file_to_compact_ != null);
  if (size_compaction) {
    level = current_->compaction_level_;
    assert(level >= 0);
    assert(level+1 < config::knumlevels);
    c = new compaction(level);

    // pick the first file that comes after compact_pointer_[level]
    for (size_t i = 0; i < current_->files_[level].size(); i++) {
      filemetadata* f = current_->files_[level][i];
      if (compact_pointer_[level].empty() ||
          icmp_.compare(f->largest.encode(), compact_pointer_[level]) > 0) {
        c->inputs_[0].push_back(f);
        break;
      }
    }
    if (c->inputs_[0].empty()) {
      // wrap-around to the beginning of the key space
      c->inputs_[0].push_back(current_->files_[level][0]);
    }
  } else if (seek_compaction) {
    level = current_->file_to_compact_level_;
    c = new compaction(level);
    c->inputs_[0].push_back(current_->file_to_compact_);
  } else {
    return null;
  }

  c->input_version_ = current_;
  c->input_version_->ref();

  // files in level 0 may overlap each other, so pick up all overlapping ones
  if (level == 0) {
    internalkey smallest, largest;
    getrange(c->inputs_[0], &smallest, &largest);
    // note that the next call will discard the file we placed in
    // c->inputs_[0] earlier and replace it with an overlapping set
    // which will include the picked file.
    current_->getoverlappinginputs(0, &smallest, &largest, &c->inputs_[0]);
    assert(!c->inputs_[0].empty());
  }

  setupotherinputs(c);

  return c;
}

void versionset::setupotherinputs(compaction* c) {
  const int level = c->level();
  internalkey smallest, largest;
  getrange(c->inputs_[0], &smallest, &largest);

  current_->getoverlappinginputs(level+1, &smallest, &largest, &c->inputs_[1]);

  // get entire range covered by compaction
  internalkey all_start, all_limit;
  getrange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);

  // see if we can grow the number of inputs in "level" without
  // changing the number of "level+1" files we pick up.
  if (!c->inputs_[1].empty()) {
    std::vector<filemetadata*> expanded0;
    current_->getoverlappinginputs(level, &all_start, &all_limit, &expanded0);
    const int64_t inputs0_size = totalfilesize(c->inputs_[0]);
    const int64_t inputs1_size = totalfilesize(c->inputs_[1]);
    const int64_t expanded0_size = totalfilesize(expanded0);
    if (expanded0.size() > c->inputs_[0].size() &&
        inputs1_size + expanded0_size < kexpandedcompactionbytesizelimit) {
      internalkey new_start, new_limit;
      getrange(expanded0, &new_start, &new_limit);
      std::vector<filemetadata*> expanded1;
      current_->getoverlappinginputs(level+1, &new_start, &new_limit,
                                     &expanded1);
      if (expanded1.size() == c->inputs_[1].size()) {
        log(options_->info_log,
            "expanding@%d %d+%d (%ld+%ld bytes) to %d+%d (%ld+%ld bytes)\n",
            level,
            int(c->inputs_[0].size()),
            int(c->inputs_[1].size()),
            long(inputs0_size), long(inputs1_size),
            int(expanded0.size()),
            int(expanded1.size()),
            long(expanded0_size), long(inputs1_size));
        smallest = new_start;
        largest = new_limit;
        c->inputs_[0] = expanded0;
        c->inputs_[1] = expanded1;
        getrange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);
      }
    }
  }

  // compute the set of grandparent files that overlap this compaction
  // (parent == level+1; grandparent == level+2)
  if (level + 2 < config::knumlevels) {
    current_->getoverlappinginputs(level + 2, &all_start, &all_limit,
                                   &c->grandparents_);
  }

  if (false) {
    log(options_->info_log, "compacting %d '%s' .. '%s'",
        level,
        smallest.debugstring().c_str(),
        largest.debugstring().c_str());
  }

  // update the place where we will do the next compaction for this level.
  // we update this immediately instead of waiting for the versionedit
  // to be applied so that if the compaction fails, we will try a different
  // key range next time.
  compact_pointer_[level] = largest.encode().tostring();
  c->edit_.setcompactpointer(level, largest);
}

compaction* versionset::compactrange(
    int level,
    const internalkey* begin,
    const internalkey* end) {
  std::vector<filemetadata*> inputs;
  current_->getoverlappinginputs(level, begin, end, &inputs);
  if (inputs.empty()) {
    return null;
  }

  // avoid compacting too much in one shot in case the range is large.
  // but we cannot do this for level-0 since level-0 files can overlap
  // and we must not pick one file and drop another older file if the
  // two files overlap.
  if (level > 0) {
    const uint64_t limit = maxfilesizeforlevel(level);
    uint64_t total = 0;
    for (size_t i = 0; i < inputs.size(); i++) {
      uint64_t s = inputs[i]->file_size;
      total += s;
      if (total >= limit) {
        inputs.resize(i + 1);
        break;
      }
    }
  }

  compaction* c = new compaction(level);
  c->input_version_ = current_;
  c->input_version_->ref();
  c->inputs_[0] = inputs;
  setupotherinputs(c);
  return c;
}

compaction::compaction(int level)
    : level_(level),
      max_output_file_size_(maxfilesizeforlevel(level)),
      input_version_(null),
      grandparent_index_(0),
      seen_key_(false),
      overlapped_bytes_(0) {
  for (int i = 0; i < config::knumlevels; i++) {
    level_ptrs_[i] = 0;
  }
}

compaction::~compaction() {
  if (input_version_ != null) {
    input_version_->unref();
  }
}

bool compaction::istrivialmove() const {
  // avoid a move if there is lots of overlapping grandparent data.
  // otherwise, the move could create a parent file that will require
  // a very expensive merge later on.
  return (num_input_files(0) == 1 &&
          num_input_files(1) == 0 &&
          totalfilesize(grandparents_) <= kmaxgrandparentoverlapbytes);
}

void compaction::addinputdeletions(versionedit* edit) {
  for (int which = 0; which < 2; which++) {
    for (size_t i = 0; i < inputs_[which].size(); i++) {
      edit->deletefile(level_ + which, inputs_[which][i]->number);
    }
  }
}

bool compaction::isbaselevelforkey(const slice& user_key) {
  // maybe use binary search to find right entry instead of linear search?
  const comparator* user_cmp = input_version_->vset_->icmp_.user_comparator();
  for (int lvl = level_ + 2; lvl < config::knumlevels; lvl++) {
    const std::vector<filemetadata*>& files = input_version_->files_[lvl];
    for (; level_ptrs_[lvl] < files.size(); ) {
      filemetadata* f = files[level_ptrs_[lvl]];
      if (user_cmp->compare(user_key, f->largest.user_key()) <= 0) {
        // we've advanced far enough
        if (user_cmp->compare(user_key, f->smallest.user_key()) >= 0) {
          // key falls in this file's range, so definitely not base level
          return false;
        }
        break;
      }
      level_ptrs_[lvl]++;
    }
  }
  return true;
}

bool compaction::shouldstopbefore(const slice& internal_key) {
  // scan to find earliest grandparent file that contains key.
  const internalkeycomparator* icmp = &input_version_->vset_->icmp_;
  while (grandparent_index_ < grandparents_.size() &&
      icmp->compare(internal_key,
                    grandparents_[grandparent_index_]->largest.encode()) > 0) {
    if (seen_key_) {
      overlapped_bytes_ += grandparents_[grandparent_index_]->file_size;
    }
    grandparent_index_++;
  }
  seen_key_ = true;

  if (overlapped_bytes_ > kmaxgrandparentoverlapbytes) {
    // too much overlap for current output; start new output
    overlapped_bytes_ = 0;
    return true;
  } else {
    return false;
  }
}

void compaction::releaseinputs() {
  if (input_version_ != null) {
    input_version_->unref();
    input_version_ = null;
  }
}

}  // namespace leveldb
