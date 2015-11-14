// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include <stdlib.h>
#include <kcpolydb.h>
#include "util/histogram.h"
#include "util/random.h"
#include "util/testutil.h"

// comma-separated list of operations to run in the specified order
//   actual benchmarks:
//
//   fillseq       -- write n values in sequential key order in async mode
//   fillrandom    -- write n values in random key order in async mode
//   overwrite     -- overwrite n values in random key order in async mode
//   fillseqsync   -- write n/100 values in sequential key order in sync mode
//   fillrandsync  -- write n/100 values in random key order in sync mode
//   fillrand100k  -- write n/1000 100k values in random order in async mode
//   fillseq100k   -- write n/1000 100k values in seq order in async mode
//   readseq       -- read n times sequentially
//   readseq100k   -- read n/1000 100k values in sequential order in async mode
//   readrand100k  -- read n/1000 100k values in sequential order in async mode
//   readrandom    -- read n times in random order
static const char* flags_benchmarks =
    "fillseq,"
    "fillseqsync,"
    "fillrandsync,"
    "fillrandom,"
    "overwrite,"
    "readrandom,"
    "readseq,"
    "fillrand100k,"
    "fillseq100k,"
    "readseq100k,"
    "readrand100k,"
    ;

// number of key/values to place in database
static int flags_num = 1000000;

// number of read operations to do.  if negative, do flags_num reads.
static int flags_reads = -1;

// size of each value
static int flags_value_size = 100;

// arrange to generate values that shrink to this fraction of
// their original size after compression
static double flags_compression_ratio = 0.5;

// print histogram of operation timings
static bool flags_histogram = false;

// cache size. default 4 mb
static int flags_cache_size = 4194304;

// page size. default 1 kb
static int flags_page_size = 1024;

// if true, do not destroy the existing database.  if you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool flags_use_existing_db = false;

// compression flag. if true, compression is on. if false, compression
// is off.
static bool flags_compression = true;

// use the db with the following name.
static const char* flags_db = null;

inline
static void dbsynchronize(kyotocabinet::treedb* db_)
{
  // synchronize will flush writes to disk
  if (!db_->synchronize()) {
    fprintf(stderr, "synchronize error: %s\n", db_->error().name());
  }
}

namespace leveldb {

// helper for quickly generating random data.
namespace {
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

  slice generate(int len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return slice(data_.data() + pos_ - len, len);
  }
};

static slice trimspace(slice s) {
  int start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  int limit = s.size();
  while (limit > start && isspace(s[limit-1])) {
    limit--;
  }
  return slice(s.data() + start, limit - start);
}

}  // namespace

class benchmark {
 private:
  kyotocabinet::treedb* db_;
  int db_num_;
  int num_;
  int reads_;
  double start_;
  double last_op_finish_;
  int64_t bytes_;
  std::string message_;
  histogram hist_;
  randomgenerator gen_;
  random rand_;
  kyotocabinet::lzocompressor<kyotocabinet::lzo::raw> comp_;

  // state kept for progress messages
  int done_;
  int next_report_;     // when to report next

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
  }

  void printenvironment() {
    fprintf(stderr, "kyoto cabinet:    version %s, lib ver %d, lib rev %d\n",
            kyotocabinet::version, kyotocabinet::libver, kyotocabinet::librev);

#if defined(__linux)
    time_t now = time(null);
    fprintf(stderr, "date:           %s", ctime(&now));  // ctime() adds newline

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
      fprintf(stderr, "cpu:            %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "cpucache:       %s\n", cache_size.c_str());
    }
#endif
  }

  void start() {
    start_ = env::default()->nowmicros() * 1e-6;
    bytes_ = 0;
    message_.clear();
    last_op_finish_ = start_;
    hist_.clear();
    done_ = 0;
    next_report_ = 100;
  }

  void finishedsingleop() {
    if (flags_histogram) {
      double now = env::default()->nowmicros() * 1e-6;
      double micros = (now - last_op_finish_) * 1e6;
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

  void stop(const slice& name) {
    double finish = env::default()->nowmicros() * 1e-6;

    // pretend at least one op was done in case we are running a benchmark
    // that does not call finishedsingleop().
    if (done_ < 1) done_ = 1;

    if (bytes_ > 0) {
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f mb/s",
               (bytes_ / 1048576.0) / (finish - start_));
      if (!message_.empty()) {
        message_  = std::string(rate) + " " + message_;
      } else {
        message_ = rate;
      }
    }

    fprintf(stdout, "%-12s : %11.3f micros/op;%s%s\n",
            name.tostring().c_str(),
            (finish - start_) * 1e6 / done_,
            (message_.empty() ? "" : " "),
            message_.c_str());
    if (flags_histogram) {
      fprintf(stdout, "microseconds per op:\n%s\n", hist_.tostring().c_str());
    }
    fflush(stdout);
  }

 public:
  enum order {
    sequential,
    random
  };
  enum dbstate {
    fresh,
    existing
  };

  benchmark()
  : db_(null),
    num_(flags_num),
    reads_(flags_reads < 0 ? flags_num : flags_reads),
    bytes_(0),
    rand_(301) {
    std::vector<std::string> files;
    std::string test_dir;
    env::default()->gettestdirectory(&test_dir);
    env::default()->getchildren(test_dir.c_str(), &files);
    if (!flags_use_existing_db) {
      for (int i = 0; i < files.size(); i++) {
        if (slice(files[i]).starts_with("dbbench_polydb")) {
          std::string file_name(test_dir);
          file_name += "/";
          file_name += files[i];
          env::default()->deletefile(file_name.c_str());
        }
      }
    }
  }

  ~benchmark() {
    if (!db_->close()) {
      fprintf(stderr, "close error: %s\n", db_->error().name());
    }
  }

  void run() {
    printheader();
    open(false);

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

      start();

      bool known = true;
      bool write_sync = false;
      if (name == slice("fillseq")) {
        write(write_sync, sequential, fresh, num_, flags_value_size, 1);
        dbsynchronize(db_);
      } else if (name == slice("fillrandom")) {
        write(write_sync, random, fresh, num_, flags_value_size, 1);
        dbsynchronize(db_);
      } else if (name == slice("overwrite")) {
        write(write_sync, random, existing, num_, flags_value_size, 1);
        dbsynchronize(db_);
      } else if (name == slice("fillrandsync")) {
        write_sync = true;
        write(write_sync, random, fresh, num_ / 100, flags_value_size, 1);
        dbsynchronize(db_);
      } else if (name == slice("fillseqsync")) {
        write_sync = true;
        write(write_sync, sequential, fresh, num_ / 100, flags_value_size, 1);
        dbsynchronize(db_);
      } else if (name == slice("fillrand100k")) {
        write(write_sync, random, fresh, num_ / 1000, 100 * 1000, 1);
        dbsynchronize(db_);
      } else if (name == slice("fillseq100k")) {
        write(write_sync, sequential, fresh, num_ / 1000, 100 * 1000, 1);
        dbsynchronize(db_);
      } else if (name == slice("readseq")) {
        readsequential();
      } else if (name == slice("readrandom")) {
        readrandom();
      } else if (name == slice("readrand100k")) {
        int n = reads_;
        reads_ /= 1000;
        readrandom();
        reads_ = n;
      } else if (name == slice("readseq100k")) {
        int n = reads_;
        reads_ /= 1000;
        readsequential();
        reads_ = n;
      } else {
        known = false;
        if (name != slice()) {  // no error message for empty name
          fprintf(stderr, "unknown benchmark '%s'\n", name.tostring().c_str());
        }
      }
      if (known) {
        stop(name);
      }
    }
  }

 private:
    void open(bool sync) {
    assert(db_ == null);

    // initialize db_
    db_ = new kyotocabinet::treedb();
    char file_name[100];
    db_num_++;
    std::string test_dir;
    env::default()->gettestdirectory(&test_dir);
    snprintf(file_name, sizeof(file_name),
             "%s/dbbench_polydb-%d.kct",
             test_dir.c_str(),
             db_num_);

    // create tuning options and open the database
    int open_options = kyotocabinet::polydb::owriter |
                       kyotocabinet::polydb::ocreate;
    int tune_options = kyotocabinet::treedb::tsmall |
        kyotocabinet::treedb::tlinear;
    if (flags_compression) {
      tune_options |= kyotocabinet::treedb::tcompress;
      db_->tune_compressor(&comp_);
    }
    db_->tune_options(tune_options);
    db_->tune_page_cache(flags_cache_size);
    db_->tune_page(flags_page_size);
    db_->tune_map(256ll<<20);
    if (sync) {
      open_options |= kyotocabinet::polydb::oautosync;
    }
    if (!db_->open(file_name, open_options)) {
      fprintf(stderr, "open error: %s\n", db_->error().name());
    }
  }

  void write(bool sync, order order, dbstate state,
             int num_entries, int value_size, int entries_per_batch) {
    // create new database if state == fresh
    if (state == fresh) {
      if (flags_use_existing_db) {
        message_ = "skipping (--use_existing_db is true)";
        return;
      }
      delete db_;
      db_ = null;
      open(sync);
      start();  // do not count time taken to destroy/open
    }

    if (num_entries != num_) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%d ops)", num_entries);
      message_ = msg;
    }

    // write to database
    for (int i = 0; i < num_entries; i++)
    {
      const int k = (order == sequential) ? i : (rand_.next() % num_entries);
      char key[100];
      snprintf(key, sizeof(key), "%016d", k);
      bytes_ += value_size + strlen(key);
      std::string cpp_key = key;
      if (!db_->set(cpp_key, gen_.generate(value_size).tostring())) {
        fprintf(stderr, "set error: %s\n", db_->error().name());
      }
      finishedsingleop();
    }
  }

  void readsequential() {
    kyotocabinet::db::cursor* cur = db_->cursor();
    cur->jump();
    std::string ckey, cvalue;
    while (cur->get(&ckey, &cvalue, true)) {
      bytes_ += ckey.size() + cvalue.size();
      finishedsingleop();
    }
    delete cur;
  }

  void readrandom() {
    std::string value;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = rand_.next() % reads_;
      snprintf(key, sizeof(key), "%016d", k);
      db_->get(key, &value);
      finishedsingleop();
    }
  }
};

}  // namespace leveldb

int main(int argc, char** argv) {
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
    } else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
      flags_num = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      flags_reads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      flags_value_size = n;
    } else if (sscanf(argv[i], "--cache_size=%d%c", &n, &junk) == 1) {
      flags_cache_size = n;
    } else if (sscanf(argv[i], "--page_size=%d%c", &n, &junk) == 1) {
      flags_page_size = n;
    } else if (sscanf(argv[i], "--compression=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      flags_compression = (n == 1) ? true : false;
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
