// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "db/db_impl.h"
#include "db/version_set.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/write_batch.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"

// comma-separated list of operations to run in the specified order
//   actual benchmarks:
//      fillseq       -- write n values in sequential key order in async mode
//      fillrandom    -- write n values in random key order in async mode
//      overwrite     -- overwrite n values in random key order in async mode
//      fillsync      -- write n/100 values in random key order in sync mode
//      fill100k      -- write n/1000 100k values in random order in async mode
//      deleteseq     -- delete n keys in sequential order
//      deleterandom  -- delete n keys in random order
//      readseq       -- read n times sequentially
//      readreverse   -- read n times in reverse order
//      readrandom    -- read n times in random order
//      readmissing   -- read n missing keys in random order
//      readhot       -- read n times in random order from 1% section of db
//      seekrandom    -- n random seeks
//      crc32c        -- repeated crc32c of 4k of data
//      acquireload   -- load n*1000 times
//   meta operations:
//      compact     -- compact the entire db
//      stats       -- print db stats
//      sstables    -- print sstable info
//      heapprofile -- dump a heap profile (if supported by this port)
static const char* flags_benchmarks =
    "fillseq,"
    "fillsync,"
    "fillrandom,"
    "overwrite,"
    "readrandom,"
    "readrandom,"  // extra run to allow previous compactions to quiesce
    "readseq,"
    "readreverse,"
    "compact,"
    "readrandom,"
    "readseq,"
    "readreverse,"
    "fill100k,"
    "crc32c,"
    "snappycomp,"
    "snappyuncomp,"
    "acquireload,"
    ;

// number of key/values to place in database
static int flags_num = 1000000;

// number of read operations to do.  if negative, do flags_num reads.
static int flags_reads = -1;

// number of concurrent threads to run.
static int flags_threads = 1;

// size of each value
static int flags_value_size = 100;

// arrange to generate values that shrink to this fraction of
// their original size after compression
static double flags_compression_ratio = 0.5;

// print histogram of operation timings
static bool flags_histogram = false;

// number of bytes to buffer in memtable before compacting
// (initialized to default value by "main")
static int flags_write_buffer_size = 0;

// number of bytes to use as a cache of uncompressed data.
// negative means use default settings.
static int flags_cache_size = -1;

// maximum number of files to keep open at the same time (use default if == 0)
static int flags_open_files = 0;

// bloom filter bits per key.
// negative means use default settings.
static int flags_bloom_bits = -1;

// if true, do not destroy the existing database.  if you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool flags_use_existing_db = false;

// use the db with the following name.
static const char* flags_db = null;

namespace leveldb {

namespace {

// helper for quickly generating random data.
class randomgenerator {
 private:
  std::string data_;
  int pos_;

 public:
  randomgenerator() {
    // we use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32kb), and also
    // large enough to serve all typical value sizes we want to write.
    random rnd(301);
    std::string piece;
    while (data_.size() < 1048576) {
      // add a short fragment that is as compressible as specified
      // by flags_compression_ratio.
      test::compressiblestring(&rnd, flags_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  slice generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return slice(data_.data() + pos_ - len, len);
  }
};

static slice trimspace(slice s) {
  size_t start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  size_t limit = s.size();
  while (limit > start && isspace(s[limit-1])) {
    limit--;
  }
  return slice(s.data() + start, limit - start);
}

static void appendwithspace(std::string* str, slice msg) {
  if (msg.empty()) return;
  if (!str->empty()) {
    str->push_back(' ');
  }
  str->append(msg.data(), msg.size());
}

class stats {
 private:
  double start_;
  double finish_;
  double seconds_;
  int done_;
  int next_report_;
  int64_t bytes_;
  double last_op_finish_;
  histogram hist_;
  std::string message_;

 public:
  stats() { start(); }

  void start() {
    next_report_ = 100;
    last_op_finish_ = start_;
    hist_.clear();
    done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    start_ = env::default()->nowmicros();
    finish_ = start_;
    message_.clear();
  }

  void merge(const stats& other) {
    hist_.merge(other.hist_);
    done_ += other.done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
  }

  void stop() {
    finish_ = env::default()->nowmicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void addmessage(slice msg) {
    appendwithspace(&message_, msg);
  }

  void finishedsingleop() {
    if (flags_histogram) {
      double now = env::default()->nowmicros();
      double micros = now - last_op_finish_;
      hist_.add(micros);
      if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {
      if      (next_report_ < 1000)   next_report_ += 100;
      else if (next_report_ < 5000)   next_report_ += 500;
      else if (next_report_ < 10000)  next_report_ += 1000;
      else if (next_report_ < 50000)  next_report_ += 5000;
      else if (next_report_ < 100000) next_report_ += 10000;
      else if (next_report_ < 500000) next_report_ += 50000;
      else                            next_report_ += 100000;
      fprintf(stderr, "... finished %d ops%30s\r", done_, "");
      fflush(stderr);
    }
  }

  void addbytes(int64_t n) {
    bytes_ += n;
  }

  void report(const slice& name) {
    // pretend at least one op was done in case we are running a benchmark
    // that does not call finishedsingleop().
    if (done_ < 1) done_ = 1;

    std::string extra;
    if (bytes_ > 0) {
      // rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      double elapsed = (finish_ - start_) * 1e-6;
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f mb/s",
               (bytes_ / 1048576.0) / elapsed);
      extra = rate;
    }
    appendwithspace(&extra, message_);

    fprintf(stdout, "%-12s : %11.3f micros/op;%s%s\n",
            name.tostring().c_str(),
            seconds_ * 1e6 / done_,
            (extra.empty() ? "" : " "),
            extra.c_str());
    if (flags_histogram) {
      fprintf(stdout, "microseconds per op:\n%s\n", hist_.tostring().c_str());
    }
    fflush(stdout);
  }
};

// state shared by all concurrent executions of the same benchmark.
struct sharedstate {
  port::mutex mu;
  port::condvar cv;
  int total;

  // each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized;
  int num_done;
  bool start;

  sharedstate() : cv(&mu) { }
};

// per-thread state for concurrent executions of the same benchmark.
struct threadstate {
  int tid;             // 0..n-1 when running in n threads
  random rand;         // has different seeds for different threads
  stats stats;
  sharedstate* shared;

  threadstate(int index)
      : tid(index),
        rand(1000 + index) {
  }
};

}  // namespace

class benchmark {
 private:
  cache* cache_;
  const filterpolicy* filter_policy_;
  db* db_;
  int num_;
  int value_size_;
  int entries_per_batch_;
  writeoptions write_options_;
  int reads_;
  int heap_counter_;

  void printheader() {
    const int kkeysize = 16;
    printenvironment();
    fprintf(stdout, "keys:       %d bytes each\n", kkeysize);
    fprintf(stdout, "values:     %d bytes each (%d bytes after compression)\n",
            flags_value_size,
            static_cast<int>(flags_value_size * flags_compression_ratio + 0.5));
    fprintf(stdout, "entries:    %d\n", num_);
    fprintf(stdout, "rawsize:    %.1f mb (estimated)\n",
            ((static_cast<int64_t>(kkeysize + flags_value_size) * num_)
             / 1048576.0));
    fprintf(stdout, "filesize:   %.1f mb (estimated)\n",
            (((kkeysize + flags_value_size * flags_compression_ratio) * num_)
             / 1048576.0));
    printwarnings();
    fprintf(stdout, "------------------------------------------------\n");
  }

  void printwarnings() {
#if defined(__gnuc__) && !defined(__optimize__)
    fprintf(stdout,
            "warning: optimization is disabled: benchmarks unnecessarily slow\n"
            );
#endif
#ifndef ndebug
    fprintf(stdout,
            "warning: assertions are enabled; benchmarks unnecessarily slow\n");
#endif

    // see if snappy is working by attempting to compress a compressible string
    const char text[] = "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy";
    std::string compressed;
    if (!port::snappy_compress(text, sizeof(text), &compressed)) {
      fprintf(stdout, "warning: snappy compression is not enabled\n");
    } else if (compressed.size() >= sizeof(text)) {
      fprintf(stdout, "warning: snappy compression is not effective\n");
    }
  }

  void printenvironment() {
    fprintf(stderr, "leveldb:    version %d.%d\n",
            kmajorversion, kminorversion);

#if defined(__linux)
    time_t now = time(null);
    fprintf(stderr, "date:       %s", ctime(&now));  // ctime() adds newline

    file* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != null) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != null) {
        const char* sep = strchr(line, ':');
        if (sep == null) {
          continue;
        }
        slice key = trimspace(slice(line, sep - 1 - line));
        slice val = trimspace(slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.tostring();
        } else if (key == "cache size") {
          cache_size = val.tostring();
        }
      }
      fclose(cpuinfo);
      fprintf(stderr, "cpu:        %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "cpucache:   %s\n", cache_size.c_str());
    }
#endif
  }

 public:
  benchmark()
  : cache_(flags_cache_size >= 0 ? newlrucache(flags_cache_size) : null),
    filter_policy_(flags_bloom_bits >= 0
                   ? newbloomfilterpolicy(flags_bloom_bits)
                   : null),
    db_(null),
    num_(flags_num),
    value_size_(flags_value_size),
    entries_per_batch_(1),
    reads_(flags_reads < 0 ? flags_num : flags_reads),
    heap_counter_(0) {
    std::vector<std::string> files;
    env::default()->getchildren(flags_db, &files);
    for (size_t i = 0; i < files.size(); i++) {
      if (slice(files[i]).starts_with("heap-")) {
        env::default()->deletefile(std::string(flags_db) + "/" + files[i]);
      }
    }
    if (!flags_use_existing_db) {
      destroydb(flags_db, options());
    }
  }

  ~benchmark() {
    delete db_;
    delete cache_;
    delete filter_policy_;
  }

  void run() {
    printheader();
    open();

    const char* benchmarks = flags_benchmarks;
    while (benchmarks != null) {
      const char* sep = strchr(benchmarks, ',');
      slice name;
      if (sep == null) {
        name = benchmarks;
        benchmarks = null;
      } else {
        name = slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      // reset parameters that may be overridden below
      num_ = flags_num;
      reads_ = (flags_reads < 0 ? flags_num : flags_reads);
      value_size_ = flags_value_size;
      entries_per_batch_ = 1;
      write_options_ = writeoptions();

      void (benchmark::*method)(threadstate*) = null;
      bool fresh_db = false;
      int num_threads = flags_threads;

      if (name == slice("fillseq")) {
        fresh_db = true;
        method = &benchmark::writeseq;
      } else if (name == slice("fillbatch")) {
        fresh_db = true;
        entries_per_batch_ = 1000;
        method = &benchmark::writeseq;
      } else if (name == slice("fillrandom")) {
        fresh_db = true;
        method = &benchmark::writerandom;
      } else if (name == slice("overwrite")) {
        fresh_db = false;
        method = &benchmark::writerandom;
      } else if (name == slice("fillsync")) {
        fresh_db = true;
        num_ /= 1000;
        write_options_.sync = true;
        method = &benchmark::writerandom;
      } else if (name == slice("fill100k")) {
        fresh_db = true;
        num_ /= 1000;
        value_size_ = 100 * 1000;
        method = &benchmark::writerandom;
      } else if (name == slice("readseq")) {
        method = &benchmark::readsequential;
      } else if (name == slice("readreverse")) {
        method = &benchmark::readreverse;
      } else if (name == slice("readrandom")) {
        method = &benchmark::readrandom;
      } else if (name == slice("readmissing")) {
        method = &benchmark::readmissing;
      } else if (name == slice("seekrandom")) {
        method = &benchmark::seekrandom;
      } else if (name == slice("readhot")) {
        method = &benchmark::readhot;
      } else if (name == slice("readrandomsmall")) {
        reads_ /= 1000;
        method = &benchmark::readrandom;
      } else if (name == slice("deleteseq")) {
        method = &benchmark::deleteseq;
      } else if (name == slice("deleterandom")) {
        method = &benchmark::deleterandom;
      } else if (name == slice("readwhilewriting")) {
        num_threads++;  // add extra thread for writing
        method = &benchmark::readwhilewriting;
      } else if (name == slice("compact")) {
        method = &benchmark::compact;
      } else if (name == slice("crc32c")) {
        method = &benchmark::crc32c;
      } else if (name == slice("acquireload")) {
        method = &benchmark::acquireload;
      } else if (name == slice("snappycomp")) {
        method = &benchmark::snappycompress;
      } else if (name == slice("snappyuncomp")) {
        method = &benchmark::snappyuncompress;
      } else if (name == slice("heapprofile")) {
        heapprofile();
      } else if (name == slice("stats")) {
        printstats("leveldb.stats");
      } else if (name == slice("sstables")) {
        printstats("leveldb.sstables");
      } else {
        if (name != slice()) {  // no error message for empty name
          fprintf(stderr, "unknown benchmark '%s'\n", name.tostring().c_str());
        }
      }

      if (fresh_db) {
        if (flags_use_existing_db) {
          fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                  name.tostring().c_str());
          method = null;
        } else {
          delete db_;
          db_ = null;
          destroydb(flags_db, options());
          open();
        }
      }

      if (method != null) {
        runbenchmark(num_threads, name, method);
      }
    }
  }

 private:
  struct threadarg {
    benchmark* bm;
    sharedstate* shared;
    threadstate* thread;
    void (benchmark::*method)(threadstate*);
  };

  static void threadbody(void* v) {
    threadarg* arg = reinterpret_cast<threadarg*>(v);
    sharedstate* shared = arg->shared;
    threadstate* thread = arg->thread;
    {
      mutexlock l(&shared->mu);
      shared->num_initialized++;
      if (shared->num_initialized >= shared->total) {
        shared->cv.signalall();
      }
      while (!shared->start) {
        shared->cv.wait();
      }
    }

    thread->stats.start();
    (arg->bm->*(arg->method))(thread);
    thread->stats.stop();

    {
      mutexlock l(&shared->mu);
      shared->num_done++;
      if (shared->num_done >= shared->total) {
        shared->cv.signalall();
      }
    }
  }

  void runbenchmark(int n, slice name,
                    void (benchmark::*method)(threadstate*)) {
    sharedstate shared;
    shared.total = n;
    shared.num_initialized = 0;
    shared.num_done = 0;
    shared.start = false;

    threadarg* arg = new threadarg[n];
    for (int i = 0; i < n; i++) {
      arg[i].bm = this;
      arg[i].method = method;
      arg[i].shared = &shared;
      arg[i].thread = new threadstate(i);
      arg[i].thread->shared = &shared;
      env::default()->startthread(threadbody, &arg[i]);
    }

    shared.mu.lock();
    while (shared.num_initialized < n) {
      shared.cv.wait();
    }

    shared.start = true;
    shared.cv.signalall();
    while (shared.num_done < n) {
      shared.cv.wait();
    }
    shared.mu.unlock();

    for (int i = 1; i < n; i++) {
      arg[0].thread->stats.merge(arg[i].thread->stats);
    }
    arg[0].thread->stats.report(name);

    for (int i = 0; i < n; i++) {
      delete arg[i].thread;
    }
    delete[] arg;
  }

  void crc32c(threadstate* thread) {
    // checksum about 500mb of data total
    const int size = 4096;
    const char* label = "(4k per op)";
    std::string data(size, 'x');
    int64_t bytes = 0;
    uint32_t crc = 0;
    while (bytes < 500 * 1048576) {
      crc = crc32c::value(data.data(), size);
      thread->stats.finishedsingleop();
      bytes += size;
    }
    // print so result is not dead
    fprintf(stderr, "... crc=0x%x\r", static_cast<unsigned int>(crc));

    thread->stats.addbytes(bytes);
    thread->stats.addmessage(label);
  }

  void acquireload(threadstate* thread) {
    int dummy;
    port::atomicpointer ap(&dummy);
    int count = 0;
    void *ptr = null;
    thread->stats.addmessage("(each op is 1000 loads)");
    while (count < 100000) {
      for (int i = 0; i < 1000; i++) {
        ptr = ap.acquire_load();
      }
      count++;
      thread->stats.finishedsingleop();
    }
    if (ptr == null) exit(1); // disable unused variable warning.
  }

  void snappycompress(threadstate* thread) {
    randomgenerator gen;
    slice input = gen.generate(options().block_size);
    int64_t bytes = 0;
    int64_t produced = 0;
    bool ok = true;
    std::string compressed;
    while (ok && bytes < 1024 * 1048576) {  // compress 1g
      ok = port::snappy_compress(input.data(), input.size(), &compressed);
      produced += compressed.size();
      bytes += input.size();
      thread->stats.finishedsingleop();
    }

    if (!ok) {
      thread->stats.addmessage("(snappy failure)");
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "(output: %.1f%%)",
               (produced * 100.0) / bytes);
      thread->stats.addmessage(buf);
      thread->stats.addbytes(bytes);
    }
  }

  void snappyuncompress(threadstate* thread) {
    randomgenerator gen;
    slice input = gen.generate(options().block_size);
    std::string compressed;
    bool ok = port::snappy_compress(input.data(), input.size(), &compressed);
    int64_t bytes = 0;
    char* uncompressed = new char[input.size()];
    while (ok && bytes < 1024 * 1048576) {  // compress 1g
      ok =  port::snappy_uncompress(compressed.data(), compressed.size(),
                                    uncompressed);
      bytes += input.size();
      thread->stats.finishedsingleop();
    }
    delete[] uncompressed;

    if (!ok) {
      thread->stats.addmessage("(snappy failure)");
    } else {
      thread->stats.addbytes(bytes);
    }
  }

  void open() {
    assert(db_ == null);
    options options;
    options.create_if_missing = !flags_use_existing_db;
    options.block_cache = cache_;
    options.write_buffer_size = flags_write_buffer_size;
    options.max_open_files = flags_open_files;
    options.filter_policy = filter_policy_;
    status s = db::open(options, flags_db, &db_);
    if (!s.ok()) {
      fprintf(stderr, "open error: %s\n", s.tostring().c_str());
      exit(1);
    }
  }

  void writeseq(threadstate* thread) {
    dowrite(thread, true);
  }

  void writerandom(threadstate* thread) {
    dowrite(thread, false);
  }

  void dowrite(threadstate* thread, bool seq) {
    if (num_ != flags_num) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%d ops)", num_);
      thread->stats.addmessage(msg);
    }

    randomgenerator gen;
    writebatch batch;
    status s;
    int64_t bytes = 0;
    for (int i = 0; i < num_; i += entries_per_batch_) {
      batch.clear();
      for (int j = 0; j < entries_per_batch_; j++) {
        const int k = seq ? i+j : (thread->rand.next() % flags_num);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        batch.put(key, gen.generate(value_size_));
        bytes += value_size_ + strlen(key);
        thread->stats.finishedsingleop();
      }
      s = db_->write(write_options_, &batch);
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.tostring().c_str());
        exit(1);
      }
    }
    thread->stats.addbytes(bytes);
  }

  void readsequential(threadstate* thread) {
    iterator* iter = db_->newiterator(readoptions());
    int i = 0;
    int64_t bytes = 0;
    for (iter->seektofirst(); i < reads_ && iter->valid(); iter->next()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.finishedsingleop();
      ++i;
    }
    delete iter;
    thread->stats.addbytes(bytes);
  }

  void readreverse(threadstate* thread) {
    iterator* iter = db_->newiterator(readoptions());
    int i = 0;
    int64_t bytes = 0;
    for (iter->seektolast(); i < reads_ && iter->valid(); iter->prev()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.finishedsingleop();
      ++i;
    }
    delete iter;
    thread->stats.addbytes(bytes);
  }

  void readrandom(threadstate* thread) {
    readoptions options;
    std::string value;
    int found = 0;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.next() % flags_num;
      snprintf(key, sizeof(key), "%016d", k);
      if (db_->get(options, key, &value).ok()) {
        found++;
      }
      thread->stats.finishedsingleop();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%d of %d found)", found, num_);
    thread->stats.addmessage(msg);
  }

  void readmissing(threadstate* thread) {
    readoptions options;
    std::string value;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.next() % flags_num;
      snprintf(key, sizeof(key), "%016d.", k);
      db_->get(options, key, &value);
      thread->stats.finishedsingleop();
    }
  }

  void readhot(threadstate* thread) {
    readoptions options;
    std::string value;
    const int range = (flags_num + 99) / 100;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.next() % range;
      snprintf(key, sizeof(key), "%016d", k);
      db_->get(options, key, &value);
      thread->stats.finishedsingleop();
    }
  }

  void seekrandom(threadstate* thread) {
    readoptions options;
    int found = 0;
    for (int i = 0; i < reads_; i++) {
      iterator* iter = db_->newiterator(options);
      char key[100];
      const int k = thread->rand.next() % flags_num;
      snprintf(key, sizeof(key), "%016d", k);
      iter->seek(key);
      if (iter->valid() && iter->key() == key) found++;
      delete iter;
      thread->stats.finishedsingleop();
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "(%d of %d found)", found, num_);
    thread->stats.addmessage(msg);
  }

  void dodelete(threadstate* thread, bool seq) {
    randomgenerator gen;
    writebatch batch;
    status s;
    for (int i = 0; i < num_; i += entries_per_batch_) {
      batch.clear();
      for (int j = 0; j < entries_per_batch_; j++) {
        const int k = seq ? i+j : (thread->rand.next() % flags_num);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        batch.delete(key);
        thread->stats.finishedsingleop();
      }
      s = db_->write(write_options_, &batch);
      if (!s.ok()) {
        fprintf(stderr, "del error: %s\n", s.tostring().c_str());
        exit(1);
      }
    }
  }

  void deleteseq(threadstate* thread) {
    dodelete(thread, true);
  }

  void deleterandom(threadstate* thread) {
    dodelete(thread, false);
  }

  void readwhilewriting(threadstate* thread) {
    if (thread->tid > 0) {
      readrandom(thread);
    } else {
      // special thread that keeps writing until other threads are done.
      randomgenerator gen;
      while (true) {
        {
          mutexlock l(&thread->shared->mu);
          if (thread->shared->num_done + 1 >= thread->shared->num_initialized) {
            // other threads have finished
            break;
          }
        }

        const int k = thread->rand.next() % flags_num;
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);
        status s = db_->put(write_options_, key, gen.generate(value_size_));
        if (!s.ok()) {
          fprintf(stderr, "put error: %s\n", s.tostring().c_str());
          exit(1);
        }
      }

      // do not count any of the preceding work/delay in stats.
      thread->stats.start();
    }
  }

  void compact(threadstate* thread) {
    db_->compactrange(null, null);
  }

  void printstats(const char* key) {
    std::string stats;
    if (!db_->getproperty(key, &stats)) {
      stats = "(failed)";
    }
    fprintf(stdout, "\n%s\n", stats.c_str());
  }

  static void writetofile(void* arg, const char* buf, int n) {
    reinterpret_cast<writablefile*>(arg)->append(slice(buf, n));
  }

  void heapprofile() {
    char fname[100];
    snprintf(fname, sizeof(fname), "%s/heap-%04d", flags_db, ++heap_counter_);
    writablefile* file;
    status s = env::default()->newwritablefile(fname, &file);
    if (!s.ok()) {
      fprintf(stderr, "%s\n", s.tostring().c_str());
      return;
    }
    bool ok = port::getheapprofile(writetofile, file);
    delete file;
    if (!ok) {
      fprintf(stderr, "heap profiling not supported\n");
      env::default()->deletefile(fname);
    }
  }
};

}  // namespace leveldb

int main(int argc, char** argv) {
  flags_write_buffer_size = leveldb::options().write_buffer_size;
  flags_open_files = leveldb::options().max_open_files;
  std::string default_db_path;

  for (int i = 1; i < argc; i++) {
    double d;
    int n;
    char junk;
    if (leveldb::slice(argv[i]).starts_with("--benchmarks=")) {
      flags_benchmarks = argv[i] + strlen("--benchmarks=");
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      flags_compression_ratio = d;
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      flags_histogram = n;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      flags_use_existing_db = n;
    } else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
      flags_num = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      flags_reads = n;
    } else if (sscanf(argv[i], "--threads=%d%c", &n, &junk) == 1) {
      flags_threads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      flags_value_size = n;
    } else if (sscanf(argv[i], "--write_buffer_size=%d%c", &n, &junk) == 1) {
      flags_write_buffer_size = n;
    } else if (sscanf(argv[i], "--cache_size=%d%c", &n, &junk) == 1) {
      flags_cache_size = n;
    } else if (sscanf(argv[i], "--bloom_bits=%d%c", &n, &junk) == 1) {
      flags_bloom_bits = n;
    } else if (sscanf(argv[i], "--open_files=%d%c", &n, &junk) == 1) {
      flags_open_files = n;
    } else if (strncmp(argv[i], "--db=", 5) == 0) {
      flags_db = argv[i] + 5;
    } else {
      fprintf(stderr, "invalid flag '%s'\n", argv[i]);
      exit(1);
    }
  }

  // choose a location for the test database if none given with --db=<path>
  if (flags_db == null) {
      leveldb::env::default()->gettestdirectory(&default_db_path);
      default_db_path += "/dbbench";
      flags_db = default_db_path.c_str();
  }

  leveldb::benchmark benchmark;
  benchmark.run();
  return 0;
}
