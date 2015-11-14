// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

static std::string randomstring(random* rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}

namespace {
class atomiccounter {
 private:
  port::mutex mu_;
  int count_;
 public:
  atomiccounter() : count_(0) { }
  void increment() {
    incrementby(1);
  }
  void incrementby(int count) {
    mutexlock l(&mu_);
    count_ += count;
  }
  int read() {
    mutexlock l(&mu_);
    return count_;
  }
  void reset() {
    mutexlock l(&mu_);
    count_ = 0;
  }
};

void delaymilliseconds(int millis) {
  env::default()->sleepformicroseconds(millis * 1000);
}
}

// special env used to delay background operations
class specialenv : public envwrapper {
 public:
  // sstable/log sync() calls are blocked while this pointer is non-null.
  port::atomicpointer delay_data_sync_;

  // sstable/log sync() calls return an error.
  port::atomicpointer data_sync_error_;

  // simulate no-space errors while this pointer is non-null.
  port::atomicpointer no_space_;

  // simulate non-writable file system while this pointer is non-null
  port::atomicpointer non_writable_;

  // force sync of manifest files to fail while this pointer is non-null
  port::atomicpointer manifest_sync_error_;

  // force write to manifest files to fail while this pointer is non-null
  port::atomicpointer manifest_write_error_;

  bool count_random_reads_;
  atomiccounter random_read_counter_;

  explicit specialenv(env* base) : envwrapper(base) {
    delay_data_sync_.release_store(null);
    data_sync_error_.release_store(null);
    no_space_.release_store(null);
    non_writable_.release_store(null);
    count_random_reads_ = false;
    manifest_sync_error_.release_store(null);
    manifest_write_error_.release_store(null);
  }

  status newwritablefile(const std::string& f, writablefile** r) {
    class datafile : public writablefile {
     private:
      specialenv* env_;
      writablefile* base_;

     public:
      datafile(specialenv* env, writablefile* base)
          : env_(env),
            base_(base) {
      }
      ~datafile() { delete base_; }
      status append(const slice& data) {
        if (env_->no_space_.acquire_load() != null) {
          // drop writes on the floor
          return status::ok();
        } else {
          return base_->append(data);
        }
      }
      status close() { return base_->close(); }
      status flush() { return base_->flush(); }
      status sync() {
        if (env_->data_sync_error_.acquire_load() != null) {
          return status::ioerror("simulated data sync error");
        }
        while (env_->delay_data_sync_.acquire_load() != null) {
          delaymilliseconds(100);
        }
        return base_->sync();
      }
    };
    class manifestfile : public writablefile {
     private:
      specialenv* env_;
      writablefile* base_;
     public:
      manifestfile(specialenv* env, writablefile* b) : env_(env), base_(b) { }
      ~manifestfile() { delete base_; }
      status append(const slice& data) {
        if (env_->manifest_write_error_.acquire_load() != null) {
          return status::ioerror("simulated writer error");
        } else {
          return base_->append(data);
        }
      }
      status close() { return base_->close(); }
      status flush() { return base_->flush(); }
      status sync() {
        if (env_->manifest_sync_error_.acquire_load() != null) {
          return status::ioerror("simulated sync error");
        } else {
          return base_->sync();
        }
      }
    };

    if (non_writable_.acquire_load() != null) {
      return status::ioerror("simulated write error");
    }

    status s = target()->newwritablefile(f, r);
    if (s.ok()) {
      if (strstr(f.c_str(), ".ldb") != null ||
          strstr(f.c_str(), ".log") != null) {
        *r = new datafile(this, *r);
      } else if (strstr(f.c_str(), "manifest") != null) {
        *r = new manifestfile(this, *r);
      }
    }
    return s;
  }

  status newrandomaccessfile(const std::string& f, randomaccessfile** r) {
    class countingfile : public randomaccessfile {
     private:
      randomaccessfile* target_;
      atomiccounter* counter_;
     public:
      countingfile(randomaccessfile* target, atomiccounter* counter)
          : target_(target), counter_(counter) {
      }
      virtual ~countingfile() { delete target_; }
      virtual status read(uint64_t offset, size_t n, slice* result,
                          char* scratch) const {
        counter_->increment();
        return target_->read(offset, n, result, scratch);
      }
    };

    status s = target()->newrandomaccessfile(f, r);
    if (s.ok() && count_random_reads_) {
      *r = new countingfile(*r, &random_read_counter_);
    }
    return s;
  }
};

class dbtest {
 private:
  const filterpolicy* filter_policy_;

  // sequence of option configurations to try
  enum optionconfig {
    kdefault,
    kfilter,
    kuncompressed,
    kend
  };
  int option_config_;

 public:
  std::string dbname_;
  specialenv* env_;
  db* db_;

  options last_options_;

  dbtest() : option_config_(kdefault),
             env_(new specialenv(env::default())) {
    filter_policy_ = newbloomfilterpolicy(10);
    dbname_ = test::tmpdir() + "/db_test";
    destroydb(dbname_, options());
    db_ = null;
    reopen();
  }

  ~dbtest() {
    delete db_;
    destroydb(dbname_, options());
    delete env_;
    delete filter_policy_;
  }

  // switch to a fresh database with the next option configuration to
  // test.  return false if there are no more configurations to test.
  bool changeoptions() {
    option_config_++;
    if (option_config_ >= kend) {
      return false;
    } else {
      destroyandreopen();
      return true;
    }
  }

  // return the current option configuration.
  options currentoptions() {
    options options;
    switch (option_config_) {
      case kfilter:
        options.filter_policy = filter_policy_;
        break;
      case kuncompressed:
        options.compression = knocompression;
        break;
      default:
        break;
    }
    return options;
  }

  dbimpl* dbfull() {
    return reinterpret_cast<dbimpl*>(db_);
  }

  void reopen(options* options = null) {
    assert_ok(tryreopen(options));
  }

  void close() {
    delete db_;
    db_ = null;
  }

  void destroyandreopen(options* options = null) {
    delete db_;
    db_ = null;
    destroydb(dbname_, options());
    assert_ok(tryreopen(options));
  }

  status tryreopen(options* options) {
    delete db_;
    db_ = null;
    options opts;
    if (options != null) {
      opts = *options;
    } else {
      opts = currentoptions();
      opts.create_if_missing = true;
    }
    last_options_ = opts;

    return db::open(opts, dbname_, &db_);
  }

  status put(const std::string& k, const std::string& v) {
    return db_->put(writeoptions(), k, v);
  }

  status delete(const std::string& k) {
    return db_->delete(writeoptions(), k);
  }

  std::string get(const std::string& k, const snapshot* snapshot = null) {
    readoptions options;
    options.snapshot = snapshot;
    std::string result;
    status s = db_->get(options, k, &result);
    if (s.isnotfound()) {
      result = "not_found";
    } else if (!s.ok()) {
      result = s.tostring();
    }
    return result;
  }

  // return a string that contains all key,value pairs in order,
  // formatted like "(k1->v1)(k2->v2)".
  std::string contents() {
    std::vector<std::string> forward;
    std::string result;
    iterator* iter = db_->newiterator(readoptions());
    for (iter->seektofirst(); iter->valid(); iter->next()) {
      std::string s = iterstatus(iter);
      result.push_back('(');
      result.append(s);
      result.push_back(')');
      forward.push_back(s);
    }

    // check reverse iteration results are the reverse of forward results
    size_t matched = 0;
    for (iter->seektolast(); iter->valid(); iter->prev()) {
      assert_lt(matched, forward.size());
      assert_eq(iterstatus(iter), forward[forward.size() - matched - 1]);
      matched++;
    }
    assert_eq(matched, forward.size());

    delete iter;
    return result;
  }

  std::string allentriesfor(const slice& user_key) {
    iterator* iter = dbfull()->test_newinternaliterator();
    internalkey target(user_key, kmaxsequencenumber, ktypevalue);
    iter->seek(target.encode());
    std::string result;
    if (!iter->status().ok()) {
      result = iter->status().tostring();
    } else {
      result = "[ ";
      bool first = true;
      while (iter->valid()) {
        parsedinternalkey ikey;
        if (!parseinternalkey(iter->key(), &ikey)) {
          result += "corrupted";
        } else {
          if (last_options_.comparator->compare(ikey.user_key, user_key) != 0) {
            break;
          }
          if (!first) {
            result += ", ";
          }
          first = false;
          switch (ikey.type) {
            case ktypevalue:
              result += iter->value().tostring();
              break;
            case ktypedeletion:
              result += "del";
              break;
          }
        }
        iter->next();
      }
      if (!first) {
        result += " ";
      }
      result += "]";
    }
    delete iter;
    return result;
  }

  int numtablefilesatlevel(int level) {
    std::string property;
    assert_true(
        db_->getproperty("leveldb.num-files-at-level" + numbertostring(level),
                         &property));
    return atoi(property.c_str());
  }

  int totaltablefiles() {
    int result = 0;
    for (int level = 0; level < config::knumlevels; level++) {
      result += numtablefilesatlevel(level);
    }
    return result;
  }

  // return spread of files per level
  std::string filesperlevel() {
    std::string result;
    int last_non_zero_offset = 0;
    for (int level = 0; level < config::knumlevels; level++) {
      int f = numtablefilesatlevel(level);
      char buf[100];
      snprintf(buf, sizeof(buf), "%s%d", (level ? "," : ""), f);
      result += buf;
      if (f > 0) {
        last_non_zero_offset = result.size();
      }
    }
    result.resize(last_non_zero_offset);
    return result;
  }

  int countfiles() {
    std::vector<std::string> files;
    env_->getchildren(dbname_, &files);
    return static_cast<int>(files.size());
  }

  uint64_t size(const slice& start, const slice& limit) {
    range r(start, limit);
    uint64_t size;
    db_->getapproximatesizes(&r, 1, &size);
    return size;
  }

  void compact(const slice& start, const slice& limit) {
    db_->compactrange(&start, &limit);
  }

  // do n memtable compactions, each of which produces an sstable
  // covering the range [small,large].
  void maketables(int n, const std::string& small, const std::string& large) {
    for (int i = 0; i < n; i++) {
      put(small, "begin");
      put(large, "end");
      dbfull()->test_compactmemtable();
    }
  }

  // prevent pushing of new sstables into deeper levels by adding
  // tables that cover a specified range to all levels.
  void filllevels(const std::string& smallest, const std::string& largest) {
    maketables(config::knumlevels, smallest, largest);
  }

  void dumpfilecounts(const char* label) {
    fprintf(stderr, "---\n%s:\n", label);
    fprintf(stderr, "maxoverlap: %lld\n",
            static_cast<long long>(
                dbfull()->test_maxnextleveloverlappingbytes()));
    for (int level = 0; level < config::knumlevels; level++) {
      int num = numtablefilesatlevel(level);
      if (num > 0) {
        fprintf(stderr, "  level %3d : %d files\n", level, num);
      }
    }
  }

  std::string dumpsstablelist() {
    std::string property;
    db_->getproperty("leveldb.sstables", &property);
    return property;
  }

  std::string iterstatus(iterator* iter) {
    std::string result;
    if (iter->valid()) {
      result = iter->key().tostring() + "->" + iter->value().tostring();
    } else {
      result = "(invalid)";
    }
    return result;
  }

  bool deleteansstfile() {
    std::vector<std::string> filenames;
    assert_ok(env_->getchildren(dbname_, &filenames));
    uint64_t number;
    filetype type;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type) && type == ktablefile) {
        assert_ok(env_->deletefile(tablefilename(dbname_, number)));
        return true;
      }
    }
    return false;
  }

  // returns number of files renamed.
  int renameldbtosst() {
    std::vector<std::string> filenames;
    assert_ok(env_->getchildren(dbname_, &filenames));
    uint64_t number;
    filetype type;
    int files_renamed = 0;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (parsefilename(filenames[i], &number, &type) && type == ktablefile) {
        const std::string from = tablefilename(dbname_, number);
        const std::string to = ssttablefilename(dbname_, number);
        assert_ok(env_->renamefile(from, to));
        files_renamed++;
      }
    }
    return files_renamed;
  }
};

test(dbtest, empty) {
  do {
    assert_true(db_ != null);
    assert_eq("not_found", get("foo"));
  } while (changeoptions());
}

test(dbtest, readwrite) {
  do {
    assert_ok(put("foo", "v1"));
    assert_eq("v1", get("foo"));
    assert_ok(put("bar", "v2"));
    assert_ok(put("foo", "v3"));
    assert_eq("v3", get("foo"));
    assert_eq("v2", get("bar"));
  } while (changeoptions());
}

test(dbtest, putdeleteget) {
  do {
    assert_ok(db_->put(writeoptions(), "foo", "v1"));
    assert_eq("v1", get("foo"));
    assert_ok(db_->put(writeoptions(), "foo", "v2"));
    assert_eq("v2", get("foo"));
    assert_ok(db_->delete(writeoptions(), "foo"));
    assert_eq("not_found", get("foo"));
  } while (changeoptions());
}

test(dbtest, getfromimmutablelayer) {
  do {
    options options = currentoptions();
    options.env = env_;
    options.write_buffer_size = 100000;  // small write buffer
    reopen(&options);

    assert_ok(put("foo", "v1"));
    assert_eq("v1", get("foo"));

    env_->delay_data_sync_.release_store(env_);      // block sync calls
    put("k1", std::string(100000, 'x'));             // fill memtable
    put("k2", std::string(100000, 'y'));             // trigger compaction
    assert_eq("v1", get("foo"));
    env_->delay_data_sync_.release_store(null);      // release sync calls
  } while (changeoptions());
}

test(dbtest, getfromversions) {
  do {
    assert_ok(put("foo", "v1"));
    dbfull()->test_compactmemtable();
    assert_eq("v1", get("foo"));
  } while (changeoptions());
}

test(dbtest, getsnapshot) {
  do {
    // try with both a short key and a long key
    for (int i = 0; i < 2; i++) {
      std::string key = (i == 0) ? std::string("foo") : std::string(200, 'x');
      assert_ok(put(key, "v1"));
      const snapshot* s1 = db_->getsnapshot();
      assert_ok(put(key, "v2"));
      assert_eq("v2", get(key));
      assert_eq("v1", get(key, s1));
      dbfull()->test_compactmemtable();
      assert_eq("v2", get(key));
      assert_eq("v1", get(key, s1));
      db_->releasesnapshot(s1);
    }
  } while (changeoptions());
}

test(dbtest, getlevel0ordering) {
  do {
    // check that we process level-0 files in correct order.  the code
    // below generates two level-0 files where the earlier one comes
    // before the later one in the level-0 file list since the earlier
    // one has a smaller "smallest" key.
    assert_ok(put("bar", "b"));
    assert_ok(put("foo", "v1"));
    dbfull()->test_compactmemtable();
    assert_ok(put("foo", "v2"));
    dbfull()->test_compactmemtable();
    assert_eq("v2", get("foo"));
  } while (changeoptions());
}

test(dbtest, getorderedbylevels) {
  do {
    assert_ok(put("foo", "v1"));
    compact("a", "z");
    assert_eq("v1", get("foo"));
    assert_ok(put("foo", "v2"));
    assert_eq("v2", get("foo"));
    dbfull()->test_compactmemtable();
    assert_eq("v2", get("foo"));
  } while (changeoptions());
}

test(dbtest, getpickscorrectfile) {
  do {
    // arrange to have multiple files in a non-level-0 level.
    assert_ok(put("a", "va"));
    compact("a", "b");
    assert_ok(put("x", "vx"));
    compact("x", "y");
    assert_ok(put("f", "vf"));
    compact("f", "g");
    assert_eq("va", get("a"));
    assert_eq("vf", get("f"));
    assert_eq("vx", get("x"));
  } while (changeoptions());
}

test(dbtest, getencountersemptylevel) {
  do {
    // arrange for the following to happen:
    //   * sstable a in level 0
    //   * nothing in level 1
    //   * sstable b in level 2
    // then do enough get() calls to arrange for an automatic compaction
    // of sstable a.  a bug would cause the compaction to be marked as
    // occurring at level 1 (instead of the correct level 0).

    // step 1: first place sstables in levels 0 and 2
    int compaction_count = 0;
    while (numtablefilesatlevel(0) == 0 ||
           numtablefilesatlevel(2) == 0) {
      assert_le(compaction_count, 100) << "could not fill levels 0 and 2";
      compaction_count++;
      put("a", "begin");
      put("z", "end");
      dbfull()->test_compactmemtable();
    }

    // step 2: clear level 1 if necessary.
    dbfull()->test_compactrange(1, null, null);
    assert_eq(numtablefilesatlevel(0), 1);
    assert_eq(numtablefilesatlevel(1), 0);
    assert_eq(numtablefilesatlevel(2), 1);

    // step 3: read a bunch of times
    for (int i = 0; i < 1000; i++) {
      assert_eq("not_found", get("missing"));
    }

    // step 4: wait for compaction to finish
    delaymilliseconds(1000);

    assert_eq(numtablefilesatlevel(0), 0);
  } while (changeoptions());
}

test(dbtest, iterempty) {
  iterator* iter = db_->newiterator(readoptions());

  iter->seektofirst();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seektolast();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seek("foo");
  assert_eq(iterstatus(iter), "(invalid)");

  delete iter;
}

test(dbtest, itersingle) {
  assert_ok(put("a", "va"));
  iterator* iter = db_->newiterator(readoptions());

  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");
  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->va");
  iter->prev();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seektolast();
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");
  iter->seektolast();
  assert_eq(iterstatus(iter), "a->va");
  iter->prev();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seek("");
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seek("a");
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seek("b");
  assert_eq(iterstatus(iter), "(invalid)");

  delete iter;
}

test(dbtest, itermulti) {
  assert_ok(put("a", "va"));
  assert_ok(put("b", "vb"));
  assert_ok(put("c", "vc"));
  iterator* iter = db_->newiterator(readoptions());

  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "b->vb");
  iter->next();
  assert_eq(iterstatus(iter), "c->vc");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");
  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->va");
  iter->prev();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seektolast();
  assert_eq(iterstatus(iter), "c->vc");
  iter->prev();
  assert_eq(iterstatus(iter), "b->vb");
  iter->prev();
  assert_eq(iterstatus(iter), "a->va");
  iter->prev();
  assert_eq(iterstatus(iter), "(invalid)");
  iter->seektolast();
  assert_eq(iterstatus(iter), "c->vc");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seek("");
  assert_eq(iterstatus(iter), "a->va");
  iter->seek("a");
  assert_eq(iterstatus(iter), "a->va");
  iter->seek("ax");
  assert_eq(iterstatus(iter), "b->vb");
  iter->seek("b");
  assert_eq(iterstatus(iter), "b->vb");
  iter->seek("z");
  assert_eq(iterstatus(iter), "(invalid)");

  // switch from reverse to forward
  iter->seektolast();
  iter->prev();
  iter->prev();
  iter->next();
  assert_eq(iterstatus(iter), "b->vb");

  // switch from forward to reverse
  iter->seektofirst();
  iter->next();
  iter->next();
  iter->prev();
  assert_eq(iterstatus(iter), "b->vb");

  // make sure iter stays at snapshot
  assert_ok(put("a",  "va2"));
  assert_ok(put("a2", "va3"));
  assert_ok(put("b",  "vb2"));
  assert_ok(put("c",  "vc2"));
  assert_ok(delete("b"));
  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "b->vb");
  iter->next();
  assert_eq(iterstatus(iter), "c->vc");
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");
  iter->seektolast();
  assert_eq(iterstatus(iter), "c->vc");
  iter->prev();
  assert_eq(iterstatus(iter), "b->vb");
  iter->prev();
  assert_eq(iterstatus(iter), "a->va");
  iter->prev();
  assert_eq(iterstatus(iter), "(invalid)");

  delete iter;
}

test(dbtest, itersmallandlargemix) {
  assert_ok(put("a", "va"));
  assert_ok(put("b", std::string(100000, 'b')));
  assert_ok(put("c", "vc"));
  assert_ok(put("d", std::string(100000, 'd')));
  assert_ok(put("e", std::string(100000, 'e')));

  iterator* iter = db_->newiterator(readoptions());

  iter->seektofirst();
  assert_eq(iterstatus(iter), "a->va");
  iter->next();
  assert_eq(iterstatus(iter), "b->" + std::string(100000, 'b'));
  iter->next();
  assert_eq(iterstatus(iter), "c->vc");
  iter->next();
  assert_eq(iterstatus(iter), "d->" + std::string(100000, 'd'));
  iter->next();
  assert_eq(iterstatus(iter), "e->" + std::string(100000, 'e'));
  iter->next();
  assert_eq(iterstatus(iter), "(invalid)");

  iter->seektolast();
  assert_eq(iterstatus(iter), "e->" + std::string(100000, 'e'));
  iter->prev();
  assert_eq(iterstatus(iter), "d->" + std::string(100000, 'd'));
  iter->prev();
  assert_eq(iterstatus(iter), "c->vc");
  iter->prev();
  assert_eq(iterstatus(iter), "b->" + std::string(100000, 'b'));
  iter->prev();
  assert_eq(iterstatus(iter), "a->va");
  iter->prev();
  assert_eq(iterstatus(iter), "(invalid)");

  delete iter;
}

test(dbtest, itermultiwithdelete) {
  do {
    assert_ok(put("a", "va"));
    assert_ok(put("b", "vb"));
    assert_ok(put("c", "vc"));
    assert_ok(delete("b"));
    assert_eq("not_found", get("b"));

    iterator* iter = db_->newiterator(readoptions());
    iter->seek("c");
    assert_eq(iterstatus(iter), "c->vc");
    iter->prev();
    assert_eq(iterstatus(iter), "a->va");
    delete iter;
  } while (changeoptions());
}

test(dbtest, recover) {
  do {
    assert_ok(put("foo", "v1"));
    assert_ok(put("baz", "v5"));

    reopen();
    assert_eq("v1", get("foo"));

    assert_eq("v1", get("foo"));
    assert_eq("v5", get("baz"));
    assert_ok(put("bar", "v2"));
    assert_ok(put("foo", "v3"));

    reopen();
    assert_eq("v3", get("foo"));
    assert_ok(put("foo", "v4"));
    assert_eq("v4", get("foo"));
    assert_eq("v2", get("bar"));
    assert_eq("v5", get("baz"));
  } while (changeoptions());
}

test(dbtest, recoverywithemptylog) {
  do {
    assert_ok(put("foo", "v1"));
    assert_ok(put("foo", "v2"));
    reopen();
    reopen();
    assert_ok(put("foo", "v3"));
    reopen();
    assert_eq("v3", get("foo"));
  } while (changeoptions());
}

// check that writes done during a memtable compaction are recovered
// if the database is shutdown during the memtable compaction.
test(dbtest, recoverduringmemtablecompaction) {
  do {
    options options = currentoptions();
    options.env = env_;
    options.write_buffer_size = 1000000;
    reopen(&options);

    // trigger a long memtable compaction and reopen the database during it
    assert_ok(put("foo", "v1"));                         // goes to 1st log file
    assert_ok(put("big1", std::string(10000000, 'x')));  // fills memtable
    assert_ok(put("big2", std::string(1000, 'y')));      // triggers compaction
    assert_ok(put("bar", "v2"));                         // goes to new log file

    reopen(&options);
    assert_eq("v1", get("foo"));
    assert_eq("v2", get("bar"));
    assert_eq(std::string(10000000, 'x'), get("big1"));
    assert_eq(std::string(1000, 'y'), get("big2"));
  } while (changeoptions());
}

static std::string key(int i) {
  char buf[100];
  snprintf(buf, sizeof(buf), "key%06d", i);
  return std::string(buf);
}

test(dbtest, minorcompactionshappen) {
  options options = currentoptions();
  options.write_buffer_size = 10000;
  reopen(&options);

  const int n = 500;

  int starting_num_tables = totaltablefiles();
  for (int i = 0; i < n; i++) {
    assert_ok(put(key(i), key(i) + std::string(1000, 'v')));
  }
  int ending_num_tables = totaltablefiles();
  assert_gt(ending_num_tables, starting_num_tables);

  for (int i = 0; i < n; i++) {
    assert_eq(key(i) + std::string(1000, 'v'), get(key(i)));
  }

  reopen();

  for (int i = 0; i < n; i++) {
    assert_eq(key(i) + std::string(1000, 'v'), get(key(i)));
  }
}

test(dbtest, recoverwithlargelog) {
  {
    options options = currentoptions();
    reopen(&options);
    assert_ok(put("big1", std::string(200000, '1')));
    assert_ok(put("big2", std::string(200000, '2')));
    assert_ok(put("small3", std::string(10, '3')));
    assert_ok(put("small4", std::string(10, '4')));
    assert_eq(numtablefilesatlevel(0), 0);
  }

  // make sure that if we re-open with a small write buffer size that
  // we flush table files in the middle of a large log file.
  options options = currentoptions();
  options.write_buffer_size = 100000;
  reopen(&options);
  assert_eq(numtablefilesatlevel(0), 3);
  assert_eq(std::string(200000, '1'), get("big1"));
  assert_eq(std::string(200000, '2'), get("big2"));
  assert_eq(std::string(10, '3'), get("small3"));
  assert_eq(std::string(10, '4'), get("small4"));
  assert_gt(numtablefilesatlevel(0), 1);
}

test(dbtest, compactionsgeneratemultiplefiles) {
  options options = currentoptions();
  options.write_buffer_size = 100000000;        // large write buffer
  reopen(&options);

  random rnd(301);

  // write 8mb (80 values, each 100k)
  assert_eq(numtablefilesatlevel(0), 0);
  std::vector<std::string> values;
  for (int i = 0; i < 80; i++) {
    values.push_back(randomstring(&rnd, 100000));
    assert_ok(put(key(i), values[i]));
  }

  // reopening moves updates to level-0
  reopen(&options);
  dbfull()->test_compactrange(0, null, null);

  assert_eq(numtablefilesatlevel(0), 0);
  assert_gt(numtablefilesatlevel(1), 1);
  for (int i = 0; i < 80; i++) {
    assert_eq(get(key(i)), values[i]);
  }
}

test(dbtest, repeatedwritestosamekey) {
  options options = currentoptions();
  options.env = env_;
  options.write_buffer_size = 100000;  // small write buffer
  reopen(&options);

  // we must have at most one file per level except for level-0,
  // which may have up to kl0_stopwritestrigger files.
  const int kmaxfiles = config::knumlevels + config::kl0_stopwritestrigger;

  random rnd(301);
  std::string value = randomstring(&rnd, 2 * options.write_buffer_size);
  for (int i = 0; i < 5 * kmaxfiles; i++) {
    put("key", value);
    assert_le(totaltablefiles(), kmaxfiles);
    fprintf(stderr, "after %d: %d files\n", int(i+1), totaltablefiles());
  }
}

test(dbtest, sparsemerge) {
  options options = currentoptions();
  options.compression = knocompression;
  reopen(&options);

  filllevels("a", "z");

  // suppose there is:
  //    small amount of data with prefix a
  //    large amount of data with prefix b
  //    small amount of data with prefix c
  // and that recent updates have made small changes to all three prefixes.
  // check that we do not do a compaction that merges all of b in one shot.
  const std::string value(1000, 'x');
  put("a", "va");
  // write approximately 100mb of "b" values
  for (int i = 0; i < 100000; i++) {
    char key[100];
    snprintf(key, sizeof(key), "b%010d", i);
    put(key, value);
  }
  put("c", "vc");
  dbfull()->test_compactmemtable();
  dbfull()->test_compactrange(0, null, null);

  // make sparse update
  put("a",    "va2");
  put("b100", "bvalue2");
  put("c",    "vc2");
  dbfull()->test_compactmemtable();

  // compactions should not cause us to create a situation where
  // a file overlaps too much data at the next level.
  assert_le(dbfull()->test_maxnextleveloverlappingbytes(), 20*1048576);
  dbfull()->test_compactrange(0, null, null);
  assert_le(dbfull()->test_maxnextleveloverlappingbytes(), 20*1048576);
  dbfull()->test_compactrange(1, null, null);
  assert_le(dbfull()->test_maxnextleveloverlappingbytes(), 20*1048576);
}

static bool between(uint64_t val, uint64_t low, uint64_t high) {
  bool result = (val >= low) && (val <= high);
  if (!result) {
    fprintf(stderr, "value %llu is not in range [%llu, %llu]\n",
            (unsigned long long)(val),
            (unsigned long long)(low),
            (unsigned long long)(high));
  }
  return result;
}

test(dbtest, approximatesizes) {
  do {
    options options = currentoptions();
    options.write_buffer_size = 100000000;        // large write buffer
    options.compression = knocompression;
    destroyandreopen();

    assert_true(between(size("", "xyz"), 0, 0));
    reopen(&options);
    assert_true(between(size("", "xyz"), 0, 0));

    // write 8mb (80 values, each 100k)
    assert_eq(numtablefilesatlevel(0), 0);
    const int n = 80;
    static const int s1 = 100000;
    static const int s2 = 105000;  // allow some expansion from metadata
    random rnd(301);
    for (int i = 0; i < n; i++) {
      assert_ok(put(key(i), randomstring(&rnd, s1)));
    }

    // 0 because getapproximatesizes() does not account for memtable space
    assert_true(between(size("", key(50)), 0, 0));

    // check sizes across recovery by reopening a few times
    for (int run = 0; run < 3; run++) {
      reopen(&options);

      for (int compact_start = 0; compact_start < n; compact_start += 10) {
        for (int i = 0; i < n; i += 10) {
          assert_true(between(size("", key(i)), s1*i, s2*i));
          assert_true(between(size("", key(i)+".suffix"), s1*(i+1), s2*(i+1)));
          assert_true(between(size(key(i), key(i+10)), s1*10, s2*10));
        }
        assert_true(between(size("", key(50)), s1*50, s2*50));
        assert_true(between(size("", key(50)+".suffix"), s1*50, s2*50));

        std::string cstart_str = key(compact_start);
        std::string cend_str = key(compact_start + 9);
        slice cstart = cstart_str;
        slice cend = cend_str;
        dbfull()->test_compactrange(0, &cstart, &cend);
      }

      assert_eq(numtablefilesatlevel(0), 0);
      assert_gt(numtablefilesatlevel(1), 0);
    }
  } while (changeoptions());
}

test(dbtest, approximatesizes_mixofsmallandlarge) {
  do {
    options options = currentoptions();
    options.compression = knocompression;
    reopen();

    random rnd(301);
    std::string big1 = randomstring(&rnd, 100000);
    assert_ok(put(key(0), randomstring(&rnd, 10000)));
    assert_ok(put(key(1), randomstring(&rnd, 10000)));
    assert_ok(put(key(2), big1));
    assert_ok(put(key(3), randomstring(&rnd, 10000)));
    assert_ok(put(key(4), big1));
    assert_ok(put(key(5), randomstring(&rnd, 10000)));
    assert_ok(put(key(6), randomstring(&rnd, 300000)));
    assert_ok(put(key(7), randomstring(&rnd, 10000)));

    // check sizes across recovery by reopening a few times
    for (int run = 0; run < 3; run++) {
      reopen(&options);

      assert_true(between(size("", key(0)), 0, 0));
      assert_true(between(size("", key(1)), 10000, 11000));
      assert_true(between(size("", key(2)), 20000, 21000));
      assert_true(between(size("", key(3)), 120000, 121000));
      assert_true(between(size("", key(4)), 130000, 131000));
      assert_true(between(size("", key(5)), 230000, 231000));
      assert_true(between(size("", key(6)), 240000, 241000));
      assert_true(between(size("", key(7)), 540000, 541000));
      assert_true(between(size("", key(8)), 550000, 560000));

      assert_true(between(size(key(3), key(5)), 110000, 111000));

      dbfull()->test_compactrange(0, null, null);
    }
  } while (changeoptions());
}

test(dbtest, iteratorpinsref) {
  put("foo", "hello");

  // get iterator that will yield the current contents of the db.
  iterator* iter = db_->newiterator(readoptions());

  // write to force compactions
  put("foo", "newvalue1");
  for (int i = 0; i < 100; i++) {
    assert_ok(put(key(i), key(i) + std::string(100000, 'v'))); // 100k values
  }
  put("foo", "newvalue2");

  iter->seektofirst();
  assert_true(iter->valid());
  assert_eq("foo", iter->key().tostring());
  assert_eq("hello", iter->value().tostring());
  iter->next();
  assert_true(!iter->valid());
  delete iter;
}

test(dbtest, snapshot) {
  do {
    put("foo", "v1");
    const snapshot* s1 = db_->getsnapshot();
    put("foo", "v2");
    const snapshot* s2 = db_->getsnapshot();
    put("foo", "v3");
    const snapshot* s3 = db_->getsnapshot();

    put("foo", "v4");
    assert_eq("v1", get("foo", s1));
    assert_eq("v2", get("foo", s2));
    assert_eq("v3", get("foo", s3));
    assert_eq("v4", get("foo"));

    db_->releasesnapshot(s3);
    assert_eq("v1", get("foo", s1));
    assert_eq("v2", get("foo", s2));
    assert_eq("v4", get("foo"));

    db_->releasesnapshot(s1);
    assert_eq("v2", get("foo", s2));
    assert_eq("v4", get("foo"));

    db_->releasesnapshot(s2);
    assert_eq("v4", get("foo"));
  } while (changeoptions());
}

test(dbtest, hiddenvaluesareremoved) {
  do {
    random rnd(301);
    filllevels("a", "z");

    std::string big = randomstring(&rnd, 50000);
    put("foo", big);
    put("pastfoo", "v");
    const snapshot* snapshot = db_->getsnapshot();
    put("foo", "tiny");
    put("pastfoo2", "v2");        // advance sequence number one more

    assert_ok(dbfull()->test_compactmemtable());
    assert_gt(numtablefilesatlevel(0), 0);

    assert_eq(big, get("foo", snapshot));
    assert_true(between(size("", "pastfoo"), 50000, 60000));
    db_->releasesnapshot(snapshot);
    assert_eq(allentriesfor("foo"), "[ tiny, " + big + " ]");
    slice x("x");
    dbfull()->test_compactrange(0, null, &x);
    assert_eq(allentriesfor("foo"), "[ tiny ]");
    assert_eq(numtablefilesatlevel(0), 0);
    assert_ge(numtablefilesatlevel(1), 1);
    dbfull()->test_compactrange(1, null, &x);
    assert_eq(allentriesfor("foo"), "[ tiny ]");

    assert_true(between(size("", "pastfoo"), 0, 1000));
  } while (changeoptions());
}

test(dbtest, deletionmarkers1) {
  put("foo", "v1");
  assert_ok(dbfull()->test_compactmemtable());
  const int last = config::kmaxmemcompactlevel;
  assert_eq(numtablefilesatlevel(last), 1);   // foo => v1 is now in last level

  // place a table at level last-1 to prevent merging with preceding mutation
  put("a", "begin");
  put("z", "end");
  dbfull()->test_compactmemtable();
  assert_eq(numtablefilesatlevel(last), 1);
  assert_eq(numtablefilesatlevel(last-1), 1);

  delete("foo");
  put("foo", "v2");
  assert_eq(allentriesfor("foo"), "[ v2, del, v1 ]");
  assert_ok(dbfull()->test_compactmemtable());  // moves to level last-2
  assert_eq(allentriesfor("foo"), "[ v2, del, v1 ]");
  slice z("z");
  dbfull()->test_compactrange(last-2, null, &z);
  // del eliminated, but v1 remains because we aren't compacting that level
  // (del can be eliminated because v2 hides v1).
  assert_eq(allentriesfor("foo"), "[ v2, v1 ]");
  dbfull()->test_compactrange(last-1, null, null);
  // merging last-1 w/ last, so we are the base level for "foo", so
  // del is removed.  (as is v1).
  assert_eq(allentriesfor("foo"), "[ v2 ]");
}

test(dbtest, deletionmarkers2) {
  put("foo", "v1");
  assert_ok(dbfull()->test_compactmemtable());
  const int last = config::kmaxmemcompactlevel;
  assert_eq(numtablefilesatlevel(last), 1);   // foo => v1 is now in last level

  // place a table at level last-1 to prevent merging with preceding mutation
  put("a", "begin");
  put("z", "end");
  dbfull()->test_compactmemtable();
  assert_eq(numtablefilesatlevel(last), 1);
  assert_eq(numtablefilesatlevel(last-1), 1);

  delete("foo");
  assert_eq(allentriesfor("foo"), "[ del, v1 ]");
  assert_ok(dbfull()->test_compactmemtable());  // moves to level last-2
  assert_eq(allentriesfor("foo"), "[ del, v1 ]");
  dbfull()->test_compactrange(last-2, null, null);
  // del kept: "last" file overlaps
  assert_eq(allentriesfor("foo"), "[ del, v1 ]");
  dbfull()->test_compactrange(last-1, null, null);
  // merging last-1 w/ last, so we are the base level for "foo", so
  // del is removed.  (as is v1).
  assert_eq(allentriesfor("foo"), "[ ]");
}

test(dbtest, overlapinlevel0) {
  do {
    assert_eq(config::kmaxmemcompactlevel, 2) << "fix test to match config";

    // fill levels 1 and 2 to disable the pushing of new memtables to levels > 0.
    assert_ok(put("100", "v100"));
    assert_ok(put("999", "v999"));
    dbfull()->test_compactmemtable();
    assert_ok(delete("100"));
    assert_ok(delete("999"));
    dbfull()->test_compactmemtable();
    assert_eq("0,1,1", filesperlevel());

    // make files spanning the following ranges in level-0:
    //  files[0]  200 .. 900
    //  files[1]  300 .. 500
    // note that files are sorted by smallest key.
    assert_ok(put("300", "v300"));
    assert_ok(put("500", "v500"));
    dbfull()->test_compactmemtable();
    assert_ok(put("200", "v200"));
    assert_ok(put("600", "v600"));
    assert_ok(put("900", "v900"));
    dbfull()->test_compactmemtable();
    assert_eq("2,1,1", filesperlevel());

    // compact away the placeholder files we created initially
    dbfull()->test_compactrange(1, null, null);
    dbfull()->test_compactrange(2, null, null);
    assert_eq("2", filesperlevel());

    // do a memtable compaction.  before bug-fix, the compaction would
    // not detect the overlap with level-0 files and would incorrectly place
    // the deletion in a deeper level.
    assert_ok(delete("600"));
    dbfull()->test_compactmemtable();
    assert_eq("3", filesperlevel());
    assert_eq("not_found", get("600"));
  } while (changeoptions());
}

test(dbtest, l0_compactionbug_issue44_a) {
  reopen();
  assert_ok(put("b", "v"));
  reopen();
  assert_ok(delete("b"));
  assert_ok(delete("a"));
  reopen();
  assert_ok(delete("a"));
  reopen();
  assert_ok(put("a", "v"));
  reopen();
  reopen();
  assert_eq("(a->v)", contents());
  delaymilliseconds(1000);  // wait for compaction to finish
  assert_eq("(a->v)", contents());
}

test(dbtest, l0_compactionbug_issue44_b) {
  reopen();
  put("","");
  reopen();
  delete("e");
  put("","");
  reopen();
  put("c", "cv");
  reopen();
  put("","");
  reopen();
  put("","");
  delaymilliseconds(1000);  // wait for compaction to finish
  reopen();
  put("d","dv");
  reopen();
  put("","");
  reopen();
  delete("d");
  delete("b");
  reopen();
  assert_eq("(->)(c->cv)", contents());
  delaymilliseconds(1000);  // wait for compaction to finish
  assert_eq("(->)(c->cv)", contents());
}

test(dbtest, comparatorcheck) {
  class newcomparator : public comparator {
   public:
    virtual const char* name() const { return "leveldb.newcomparator"; }
    virtual int compare(const slice& a, const slice& b) const {
      return bytewisecomparator()->compare(a, b);
    }
    virtual void findshortestseparator(std::string* s, const slice& l) const {
      bytewisecomparator()->findshortestseparator(s, l);
    }
    virtual void findshortsuccessor(std::string* key) const {
      bytewisecomparator()->findshortsuccessor(key);
    }
  };
  newcomparator cmp;
  options new_options = currentoptions();
  new_options.comparator = &cmp;
  status s = tryreopen(&new_options);
  assert_true(!s.ok());
  assert_true(s.tostring().find("comparator") != std::string::npos)
      << s.tostring();
}

test(dbtest, customcomparator) {
  class numbercomparator : public comparator {
   public:
    virtual const char* name() const { return "test.numbercomparator"; }
    virtual int compare(const slice& a, const slice& b) const {
      return tonumber(a) - tonumber(b);
    }
    virtual void findshortestseparator(std::string* s, const slice& l) const {
      tonumber(*s);     // check format
      tonumber(l);      // check format
    }
    virtual void findshortsuccessor(std::string* key) const {
      tonumber(*key);   // check format
    }
   private:
    static int tonumber(const slice& x) {
      // check that there are no extra characters.
      assert_true(x.size() >= 2 && x[0] == '[' && x[x.size()-1] == ']')
          << escapestring(x);
      int val;
      char ignored;
      assert_true(sscanf(x.tostring().c_str(), "[%i]%c", &val, &ignored) == 1)
          << escapestring(x);
      return val;
    }
  };
  numbercomparator cmp;
  options new_options = currentoptions();
  new_options.create_if_missing = true;
  new_options.comparator = &cmp;
  new_options.filter_policy = null;     // cannot use bloom filters
  new_options.write_buffer_size = 1000;  // compact more often
  destroyandreopen(&new_options);
  assert_ok(put("[10]", "ten"));
  assert_ok(put("[0x14]", "twenty"));
  for (int i = 0; i < 2; i++) {
    assert_eq("ten", get("[10]"));
    assert_eq("ten", get("[0xa]"));
    assert_eq("twenty", get("[20]"));
    assert_eq("twenty", get("[0x14]"));
    assert_eq("not_found", get("[15]"));
    assert_eq("not_found", get("[0xf]"));
    compact("[0]", "[9999]");
  }

  for (int run = 0; run < 2; run++) {
    for (int i = 0; i < 1000; i++) {
      char buf[100];
      snprintf(buf, sizeof(buf), "[%d]", i*10);
      assert_ok(put(buf, buf));
    }
    compact("[0]", "[1000000]");
  }
}

test(dbtest, manualcompaction) {
  assert_eq(config::kmaxmemcompactlevel, 2)
      << "need to update this test to match kmaxmemcompactlevel";

  maketables(3, "p", "q");
  assert_eq("1,1,1", filesperlevel());

  // compaction range falls before files
  compact("", "c");
  assert_eq("1,1,1", filesperlevel());

  // compaction range falls after files
  compact("r", "z");
  assert_eq("1,1,1", filesperlevel());

  // compaction range overlaps files
  compact("p1", "p9");
  assert_eq("0,0,1", filesperlevel());

  // populate a different range
  maketables(3, "c", "e");
  assert_eq("1,1,2", filesperlevel());

  // compact just the new range
  compact("b", "f");
  assert_eq("0,0,2", filesperlevel());

  // compact all
  maketables(1, "a", "z");
  assert_eq("0,1,2", filesperlevel());
  db_->compactrange(null, null);
  assert_eq("0,0,1", filesperlevel());
}

test(dbtest, dbopen_options) {
  std::string dbname = test::tmpdir() + "/db_options_test";
  destroydb(dbname, options());

  // does not exist, and create_if_missing == false: error
  db* db = null;
  options opts;
  opts.create_if_missing = false;
  status s = db::open(opts, dbname, &db);
  assert_true(strstr(s.tostring().c_str(), "does not exist") != null);
  assert_true(db == null);

  // does not exist, and create_if_missing == true: ok
  opts.create_if_missing = true;
  s = db::open(opts, dbname, &db);
  assert_ok(s);
  assert_true(db != null);

  delete db;
  db = null;

  // does exist, and error_if_exists == true: error
  opts.create_if_missing = false;
  opts.error_if_exists = true;
  s = db::open(opts, dbname, &db);
  assert_true(strstr(s.tostring().c_str(), "exists") != null);
  assert_true(db == null);

  // does exist, and error_if_exists == false: ok
  opts.create_if_missing = true;
  opts.error_if_exists = false;
  s = db::open(opts, dbname, &db);
  assert_ok(s);
  assert_true(db != null);

  delete db;
  db = null;
}

test(dbtest, locking) {
  db* db2 = null;
  status s = db::open(currentoptions(), dbname_, &db2);
  assert_true(!s.ok()) << "locking did not prevent re-opening db";
}

// check that number of files does not grow when we are out of space
test(dbtest, nospace) {
  options options = currentoptions();
  options.env = env_;
  reopen(&options);

  assert_ok(put("foo", "v1"));
  assert_eq("v1", get("foo"));
  compact("a", "z");
  const int num_files = countfiles();
  env_->no_space_.release_store(env_);   // force out-of-space errors
  for (int i = 0; i < 10; i++) {
    for (int level = 0; level < config::knumlevels-1; level++) {
      dbfull()->test_compactrange(level, null, null);
    }
  }
  env_->no_space_.release_store(null);
  assert_lt(countfiles(), num_files + 3);
}

test(dbtest, nonwritablefilesystem) {
  options options = currentoptions();
  options.write_buffer_size = 1000;
  options.env = env_;
  reopen(&options);
  assert_ok(put("foo", "v1"));
  env_->non_writable_.release_store(env_);  // force errors for new files
  std::string big(100000, 'x');
  int errors = 0;
  for (int i = 0; i < 20; i++) {
    fprintf(stderr, "iter %d; errors %d\n", i, errors);
    if (!put("foo", big).ok()) {
      errors++;
      delaymilliseconds(100);
    }
  }
  assert_gt(errors, 0);
  env_->non_writable_.release_store(null);
}

test(dbtest, writesyncerror) {
  // check that log sync errors cause the db to disallow future writes.

  // (a) cause log sync calls to fail
  options options = currentoptions();
  options.env = env_;
  reopen(&options);
  env_->data_sync_error_.release_store(env_);

  // (b) normal write should succeed
  writeoptions w;
  assert_ok(db_->put(w, "k1", "v1"));
  assert_eq("v1", get("k1"));

  // (c) do a sync write; should fail
  w.sync = true;
  assert_true(!db_->put(w, "k2", "v2").ok());
  assert_eq("v1", get("k1"));
  assert_eq("not_found", get("k2"));

  // (d) make sync behave normally
  env_->data_sync_error_.release_store(null);

  // (e) do a non-sync write; should fail
  w.sync = false;
  assert_true(!db_->put(w, "k3", "v3").ok());
  assert_eq("v1", get("k1"));
  assert_eq("not_found", get("k2"));
  assert_eq("not_found", get("k3"));
}

test(dbtest, manifestwriteerror) {
  // test for the following problem:
  // (a) compaction produces file f
  // (b) log record containing f is written to manifest file, but sync() fails
  // (c) gc deletes f
  // (d) after reopening db, reads fail since deleted f is named in log record

  // we iterate twice.  in the second iteration, everything is the
  // same except the log record never makes it to the manifest file.
  for (int iter = 0; iter < 2; iter++) {
    port::atomicpointer* error_type = (iter == 0)
        ? &env_->manifest_sync_error_
        : &env_->manifest_write_error_;

    // insert foo=>bar mapping
    options options = currentoptions();
    options.env = env_;
    options.create_if_missing = true;
    options.error_if_exists = false;
    destroyandreopen(&options);
    assert_ok(put("foo", "bar"));
    assert_eq("bar", get("foo"));

    // memtable compaction (will succeed)
    dbfull()->test_compactmemtable();
    assert_eq("bar", get("foo"));
    const int last = config::kmaxmemcompactlevel;
    assert_eq(numtablefilesatlevel(last), 1);   // foo=>bar is now in last level

    // merging compaction (will fail)
    error_type->release_store(env_);
    dbfull()->test_compactrange(last, null, null);  // should fail
    assert_eq("bar", get("foo"));

    // recovery: should not lose data
    error_type->release_store(null);
    reopen(&options);
    assert_eq("bar", get("foo"));
  }
}

test(dbtest, missingsstfile) {
  assert_ok(put("foo", "bar"));
  assert_eq("bar", get("foo"));

  // dump the memtable to disk.
  dbfull()->test_compactmemtable();
  assert_eq("bar", get("foo"));

  close();
  assert_true(deleteansstfile());
  options options = currentoptions();
  options.paranoid_checks = true;
  status s = tryreopen(&options);
  assert_true(!s.ok());
  assert_true(s.tostring().find("issing") != std::string::npos)
      << s.tostring();
}

test(dbtest, stillreadsst) {
  assert_ok(put("foo", "bar"));
  assert_eq("bar", get("foo"));

  // dump the memtable to disk.
  dbfull()->test_compactmemtable();
  assert_eq("bar", get("foo"));
  close();
  assert_gt(renameldbtosst(), 0);
  options options = currentoptions();
  options.paranoid_checks = true;
  status s = tryreopen(&options);
  assert_true(s.ok());
  assert_eq("bar", get("foo"));
}

test(dbtest, filesdeletedaftercompaction) {
  assert_ok(put("foo", "v2"));
  compact("a", "z");
  const int num_files = countfiles();
  for (int i = 0; i < 10; i++) {
    assert_ok(put("foo", "v2"));
    compact("a", "z");
  }
  assert_eq(countfiles(), num_files);
}

test(dbtest, bloomfilter) {
  env_->count_random_reads_ = true;
  options options = currentoptions();
  options.env = env_;
  options.block_cache = newlrucache(0);  // prevent cache hits
  options.filter_policy = newbloomfilterpolicy(10);
  reopen(&options);

  // populate multiple layers
  const int n = 10000;
  for (int i = 0; i < n; i++) {
    assert_ok(put(key(i), key(i)));
  }
  compact("a", "z");
  for (int i = 0; i < n; i += 100) {
    assert_ok(put(key(i), key(i)));
  }
  dbfull()->test_compactmemtable();

  // prevent auto compactions triggered by seeks
  env_->delay_data_sync_.release_store(env_);

  // lookup present keys.  should rarely read from small sstable.
  env_->random_read_counter_.reset();
  for (int i = 0; i < n; i++) {
    assert_eq(key(i), get(key(i)));
  }
  int reads = env_->random_read_counter_.read();
  fprintf(stderr, "%d present => %d reads\n", n, reads);
  assert_ge(reads, n);
  assert_le(reads, n + 2*n/100);

  // lookup present keys.  should rarely read from either sstable.
  env_->random_read_counter_.reset();
  for (int i = 0; i < n; i++) {
    assert_eq("not_found", get(key(i) + ".missing"));
  }
  reads = env_->random_read_counter_.read();
  fprintf(stderr, "%d missing => %d reads\n", n, reads);
  assert_le(reads, 3*n/100);

  env_->delay_data_sync_.release_store(null);
  close();
  delete options.block_cache;
  delete options.filter_policy;
}

// multi-threaded test:
namespace {

static const int knumthreads = 4;
static const int ktestseconds = 10;
static const int knumkeys = 1000;

struct mtstate {
  dbtest* test;
  port::atomicpointer stop;
  port::atomicpointer counter[knumthreads];
  port::atomicpointer thread_done[knumthreads];
};

struct mtthread {
  mtstate* state;
  int id;
};

static void mtthreadbody(void* arg) {
  mtthread* t = reinterpret_cast<mtthread*>(arg);
  int id = t->id;
  db* db = t->state->test->db_;
  uintptr_t counter = 0;
  fprintf(stderr, "... starting thread %d\n", id);
  random rnd(1000 + id);
  std::string value;
  char valbuf[1500];
  while (t->state->stop.acquire_load() == null) {
    t->state->counter[id].release_store(reinterpret_cast<void*>(counter));

    int key = rnd.uniform(knumkeys);
    char keybuf[20];
    snprintf(keybuf, sizeof(keybuf), "%016d", key);

    if (rnd.onein(2)) {
      // write values of the form <key, my id, counter>.
      // we add some padding for force compactions.
      snprintf(valbuf, sizeof(valbuf), "%d.%d.%-1000d",
               key, id, static_cast<int>(counter));
      assert_ok(db->put(writeoptions(), slice(keybuf), slice(valbuf)));
    } else {
      // read a value and verify that it matches the pattern written above.
      status s = db->get(readoptions(), slice(keybuf), &value);
      if (s.isnotfound()) {
        // key has not yet been written
      } else {
        // check that the writer thread counter is >= the counter in the value
        assert_ok(s);
        int k, w, c;
        assert_eq(3, sscanf(value.c_str(), "%d.%d.%d", &k, &w, &c)) << value;
        assert_eq(k, key);
        assert_ge(w, 0);
        assert_lt(w, knumthreads);
        assert_le(static_cast<uintptr_t>(c), reinterpret_cast<uintptr_t>(
            t->state->counter[w].acquire_load()));
      }
    }
    counter++;
  }
  t->state->thread_done[id].release_store(t);
  fprintf(stderr, "... stopping thread %d after %d ops\n", id, int(counter));
}

}  // namespace

test(dbtest, multithreaded) {
  do {
    // initialize state
    mtstate mt;
    mt.test = this;
    mt.stop.release_store(0);
    for (int id = 0; id < knumthreads; id++) {
      mt.counter[id].release_store(0);
      mt.thread_done[id].release_store(0);
    }

    // start threads
    mtthread thread[knumthreads];
    for (int id = 0; id < knumthreads; id++) {
      thread[id].state = &mt;
      thread[id].id = id;
      env_->startthread(mtthreadbody, &thread[id]);
    }

    // let them run for a while
    delaymilliseconds(ktestseconds * 1000);

    // stop the threads and wait for them to finish
    mt.stop.release_store(&mt);
    for (int id = 0; id < knumthreads; id++) {
      while (mt.thread_done[id].acquire_load() == null) {
        delaymilliseconds(100);
      }
    }
  } while (changeoptions());
}

namespace {
typedef std::map<std::string, std::string> kvmap;
}

class modeldb: public db {
 public:
  class modelsnapshot : public snapshot {
   public:
    kvmap map_;
  };

  explicit modeldb(const options& options): options_(options) { }
  ~modeldb() { }
  virtual status put(const writeoptions& o, const slice& k, const slice& v) {
    return db::put(o, k, v);
  }
  virtual status delete(const writeoptions& o, const slice& key) {
    return db::delete(o, key);
  }
  virtual status get(const readoptions& options,
                     const slice& key, std::string* value) {
    assert(false);      // not implemented
    return status::notfound(key);
  }
  virtual iterator* newiterator(const readoptions& options) {
    if (options.snapshot == null) {
      kvmap* saved = new kvmap;
      *saved = map_;
      return new modeliter(saved, true);
    } else {
      const kvmap* snapshot_state =
          &(reinterpret_cast<const modelsnapshot*>(options.snapshot)->map_);
      return new modeliter(snapshot_state, false);
    }
  }
  virtual const snapshot* getsnapshot() {
    modelsnapshot* snapshot = new modelsnapshot;
    snapshot->map_ = map_;
    return snapshot;
  }

  virtual void releasesnapshot(const snapshot* snapshot) {
    delete reinterpret_cast<const modelsnapshot*>(snapshot);
  }
  virtual status write(const writeoptions& options, writebatch* batch) {
    class handler : public writebatch::handler {
     public:
      kvmap* map_;
      virtual void put(const slice& key, const slice& value) {
        (*map_)[key.tostring()] = value.tostring();
      }
      virtual void delete(const slice& key) {
        map_->erase(key.tostring());
      }
    };
    handler handler;
    handler.map_ = &map_;
    return batch->iterate(&handler);
  }

  virtual bool getproperty(const slice& property, std::string* value) {
    return false;
  }
  virtual void getapproximatesizes(const range* r, int n, uint64_t* sizes) {
    for (int i = 0; i < n; i++) {
      sizes[i] = 0;
    }
  }
  virtual void compactrange(const slice* start, const slice* end) {
  }

 private:
  class modeliter: public iterator {
   public:
    modeliter(const kvmap* map, bool owned)
        : map_(map), owned_(owned), iter_(map_->end()) {
    }
    ~modeliter() {
      if (owned_) delete map_;
    }
    virtual bool valid() const { return iter_ != map_->end(); }
    virtual void seektofirst() { iter_ = map_->begin(); }
    virtual void seektolast() {
      if (map_->empty()) {
        iter_ = map_->end();
      } else {
        iter_ = map_->find(map_->rbegin()->first);
      }
    }
    virtual void seek(const slice& k) {
      iter_ = map_->lower_bound(k.tostring());
    }
    virtual void next() { ++iter_; }
    virtual void prev() { --iter_; }
    virtual slice key() const { return iter_->first; }
    virtual slice value() const { return iter_->second; }
    virtual status status() const { return status::ok(); }
   private:
    const kvmap* const map_;
    const bool owned_;  // do we own map_
    kvmap::const_iterator iter_;
  };
  const options options_;
  kvmap map_;
};

static std::string randomkey(random* rnd) {
  int len = (rnd->onein(3)
             ? 1                // short sometimes to encourage collisions
             : (rnd->onein(100) ? rnd->skewed(10) : rnd->uniform(10)));
  return test::randomkey(rnd, len);
}

static bool compareiterators(int step,
                             db* model,
                             db* db,
                             const snapshot* model_snap,
                             const snapshot* db_snap) {
  readoptions options;
  options.snapshot = model_snap;
  iterator* miter = model->newiterator(options);
  options.snapshot = db_snap;
  iterator* dbiter = db->newiterator(options);
  bool ok = true;
  int count = 0;
  for (miter->seektofirst(), dbiter->seektofirst();
       ok && miter->valid() && dbiter->valid();
       miter->next(), dbiter->next()) {
    count++;
    if (miter->key().compare(dbiter->key()) != 0) {
      fprintf(stderr, "step %d: key mismatch: '%s' vs. '%s'\n",
              step,
              escapestring(miter->key()).c_str(),
              escapestring(dbiter->key()).c_str());
      ok = false;
      break;
    }

    if (miter->value().compare(dbiter->value()) != 0) {
      fprintf(stderr, "step %d: value mismatch for key '%s': '%s' vs. '%s'\n",
              step,
              escapestring(miter->key()).c_str(),
              escapestring(miter->value()).c_str(),
              escapestring(miter->value()).c_str());
      ok = false;
    }
  }

  if (ok) {
    if (miter->valid() != dbiter->valid()) {
      fprintf(stderr, "step %d: mismatch at end of iterators: %d vs. %d\n",
              step, miter->valid(), dbiter->valid());
      ok = false;
    }
  }
  fprintf(stderr, "%d entries compared: ok=%d\n", count, ok);
  delete miter;
  delete dbiter;
  return ok;
}

test(dbtest, randomized) {
  random rnd(test::randomseed());
  do {
    modeldb model(currentoptions());
    const int n = 10000;
    const snapshot* model_snap = null;
    const snapshot* db_snap = null;
    std::string k, v;
    for (int step = 0; step < n; step++) {
      if (step % 100 == 0) {
        fprintf(stderr, "step %d of %d\n", step, n);
      }
      // todo(sanjay): test get() works
      int p = rnd.uniform(100);
      if (p < 45) {                               // put
        k = randomkey(&rnd);
        v = randomstring(&rnd,
                         rnd.onein(20)
                         ? 100 + rnd.uniform(100)
                         : rnd.uniform(8));
        assert_ok(model.put(writeoptions(), k, v));
        assert_ok(db_->put(writeoptions(), k, v));

      } else if (p < 90) {                        // delete
        k = randomkey(&rnd);
        assert_ok(model.delete(writeoptions(), k));
        assert_ok(db_->delete(writeoptions(), k));


      } else {                                    // multi-element batch
        writebatch b;
        const int num = rnd.uniform(8);
        for (int i = 0; i < num; i++) {
          if (i == 0 || !rnd.onein(10)) {
            k = randomkey(&rnd);
          } else {
            // periodically re-use the same key from the previous iter, so
            // we have multiple entries in the write batch for the same key
          }
          if (rnd.onein(2)) {
            v = randomstring(&rnd, rnd.uniform(10));
            b.put(k, v);
          } else {
            b.delete(k);
          }
        }
        assert_ok(model.write(writeoptions(), &b));
        assert_ok(db_->write(writeoptions(), &b));
      }

      if ((step % 100) == 0) {
        assert_true(compareiterators(step, &model, db_, null, null));
        assert_true(compareiterators(step, &model, db_, model_snap, db_snap));
        // save a snapshot from each db this time that we'll use next
        // time we compare things, to make sure the current state is
        // preserved with the snapshot
        if (model_snap != null) model.releasesnapshot(model_snap);
        if (db_snap != null) db_->releasesnapshot(db_snap);

        reopen();
        assert_true(compareiterators(step, &model, db_, null, null));

        model_snap = model.getsnapshot();
        db_snap = db_->getsnapshot();
      }
    }
    if (model_snap != null) model.releasesnapshot(model_snap);
    if (db_snap != null) db_->releasesnapshot(db_snap);
  } while (changeoptions());
}

std::string makekey(unsigned int num) {
  char buf[30];
  snprintf(buf, sizeof(buf), "%016u", num);
  return std::string(buf);
}

void bm_logandapply(int iters, int num_base_files) {
  std::string dbname = test::tmpdir() + "/leveldb_test_benchmark";
  destroydb(dbname, options());

  db* db = null;
  options opts;
  opts.create_if_missing = true;
  status s = db::open(opts, dbname, &db);
  assert_ok(s);
  assert_true(db != null);

  delete db;
  db = null;

  env* env = env::default();

  port::mutex mu;
  mutexlock l(&mu);

  internalkeycomparator cmp(bytewisecomparator());
  options options;
  versionset vset(dbname, &options, null, &cmp);
  assert_ok(vset.recover());
  versionedit vbase;
  uint64_t fnum = 1;
  for (int i = 0; i < num_base_files; i++) {
    internalkey start(makekey(2*fnum), 1, ktypevalue);
    internalkey limit(makekey(2*fnum+1), 1, ktypedeletion);
    vbase.addfile(2, fnum++, 1 /* file size */, start, limit);
  }
  assert_ok(vset.logandapply(&vbase, &mu));

  uint64_t start_micros = env->nowmicros();

  for (int i = 0; i < iters; i++) {
    versionedit vedit;
    vedit.deletefile(2, fnum);
    internalkey start(makekey(2*fnum), 1, ktypevalue);
    internalkey limit(makekey(2*fnum+1), 1, ktypedeletion);
    vedit.addfile(2, fnum++, 1 /* file size */, start, limit);
    vset.logandapply(&vedit, &mu);
  }
  uint64_t stop_micros = env->nowmicros();
  unsigned int us = stop_micros - start_micros;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", num_base_files);
  fprintf(stderr,
          "bm_logandapply/%-6s   %8d iters : %9u us (%7.0f us / iter)\n",
          buf, iters, us, ((float)us) / iters);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--benchmark") {
    leveldb::bm_logandapply(1000, 1);
    leveldb::bm_logandapply(1000, 100);
    leveldb::bm_logandapply(1000, 10000);
    leveldb::bm_logandapply(100, 100000);
    return 0;
  }

  return leveldb::test::runalltests();
}
