// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "util/histogram.h"
#include "util/random.h"
#include "util/testutil.h"

// comma-separated list of operations to run in the specified order
//   actual benchmarks:
//
//   fillseq       -- write n values in sequential key order in async mode
//   fillseqsync   -- write n/100 values in sequential key order in sync mode
//   fillseqbatch  -- batch write n values in sequential key order in async mode
//   fillrandom    -- write n values in random key order in async mode
//   fillrandsync  -- write n/100 values in random key order in sync mode
//   fillrandbatch -- batch write n values in sequential key order in async mode
//   overwrite     -- overwrite n values in random key order in async mode
//   fillrand100k  -- write n/1000 100k values in random order in async mode
//   fillseq100k   -- write n/1000 100k values in sequential order in async mode
//   readseq       -- read n times sequentially
//   readrandom    -- read n times in random order
//   readrand100k  -- read n/1000 100k values in sequential order in async mode
static const char* flags_benchmarks =
    "fillseq,"
    "fillseqsync,"
    "fillseqbatch,"
    "fillrandom,"
    "fillrandsync,"
    "fillrandbatch,"
    "overwrite,"
    "overwritebatch,"
    "readrandom,"
    "readseq,"
    "fillrand100k,"
    "fillseq100k,"
    "readseq,"
    "readrand100k,"
    ;

// number of key/values to place in database
static int flags_num = 1000000;

// number of read operations to do.  if negative, do flags_num reads.
static int flags_reads = -1;

// size of each value
static int flags_value_size = 100;

// print histogram of operation timings
static bool flags_histogram = false;

// arrange to generate values that shrink to this fraction of
// their original size after compression
static double flags_compression_ratio = 0.5;

// page size. default 1 kb.
static int flags_page_size = 1024;

// number of pages.
// default cache size = flags_page_size * flags_num_pages = 4 mb.
static int flags_num_pages = 4096;

// if true, do not destroy the existing database.  if you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool flags_use_existing_db = false;

// if true, we allow batch writes to occur
static bool flags_transaction = true;

// if true, we enable write-ahead logging
static bool flags_wal_enabled = true;

// use the db with the following name.
static const char* flags_db = null;

inline
static void execerrorcheck(int status, char *err_msg) {
  if (status != sqlite_ok) {
    fprintf(stderr, "sql error: %s\n", err_msg);
    sqlite3_free(err_msg);
    exit(1);
  }
}

inline
static void steperrorcheck(int status) {
  if (status != sqlite_done) {
    fprintf(stderr, "sql step error: status = %d\n", status);
    exit(1);
  }
}

inline
static void errorcheck(int status) {
  if (status != sqlite_ok) {
    fprintf(stderr, "sqlite3 error: status = %d\n", status);
    exit(1);
  }
}

inline
static void walcheckpoint(sqlite3* db_) {
  // flush all writes to disk
  if (flags_wal_enabled) {
    sqlite3_wal_checkpoint_v2(db_, null, sqlite_checkpoint_full, null, null);
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
  sqlite3* db_;
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

  // state kept for progress messages
  int done_;
  int next_report_;     // when to report next

  void printheader() {
    const int kkeysize = 16;
    printenvironment();
    fprintf(stdout, "keys:       %d bytes each\n", kkeysize);
    fprintf(stdout, "values:     %d bytes each\n", flags_value_size);
    fprintf(stdout, "entries:    %d\n", num_);
    fprintf(stdout, "rawsize:    %.1f mb (estimated)\n",
            ((static_cast<int64_t>(kkeysize + flags_value_size) * num_)
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
    fprintf(stderr, "sqlite:     version %s\n", sqlite_version);

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
    db_num_(0),
    num_(flags_num),
    reads_(flags_reads < 0 ? flags_num : flags_reads),
    bytes_(0),
    rand_(301) {
    std::vector<std::string> files;
    std::string test_dir;
    env::default()->gettestdirectory(&test_dir);
    env::default()->getchildren(test_dir, &files);
    if (!flags_use_existing_db) {
      for (int i = 0; i < files.size(); i++) {
        if (slice(files[i]).starts_with("dbbench_sqlite3")) {
          std::string file_name(test_dir);
          file_name += "/";
          file_name += files[i];
          env::default()->deletefile(file_name.c_str());
        }
      }
    }
  }

  ~benchmark() {
    int status = sqlite3_close(db_);
    errorcheck(status);
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

      bytes_ = 0;
      start();

      bool known = true;
      bool write_sync = false;
      if (name == slice("fillseq")) {
        write(write_sync, sequential, fresh, num_, flags_value_size, 1);
        walcheckpoint(db_);
      } else if (name == slice("fillseqbatch")) {
        write(write_sync, sequential, fresh, num_, flags_value_size, 1000);
        walcheckpoint(db_);
      } else if (name == slice("fillrandom")) {
        write(write_sync, random, fresh, num_, flags_value_size, 1);
        walcheckpoint(db_);
      } else if (name == slice("fillrandbatch")) {
        write(write_sync, random, fresh, num_, flags_value_size, 1000);
        walcheckpoint(db_);
      } else if (name == slice("overwrite")) {
        write(write_sync, random, existing, num_, flags_value_size, 1);
        walcheckpoint(db_);
      } else if (name == slice("overwritebatch")) {
        write(write_sync, random, existing, num_, flags_value_size, 1000);
        walcheckpoint(db_);
      } else if (name == slice("fillrandsync")) {
        write_sync = true;
        write(write_sync, random, fresh, num_ / 100, flags_value_size, 1);
        walcheckpoint(db_);
      } else if (name == slice("fillseqsync")) {
        write_sync = true;
        write(write_sync, sequential, fresh, num_ / 100, flags_value_size, 1);
        walcheckpoint(db_);
      } else if (name == slice("fillrand100k")) {
        write(write_sync, random, fresh, num_ / 1000, 100 * 1000, 1);
        walcheckpoint(db_);
      } else if (name == slice("fillseq100k")) {
        write(write_sync, sequential, fresh, num_ / 1000, 100 * 1000, 1);
        walcheckpoint(db_);
      } else if (name == slice("readseq")) {
        readsequential();
      } else if (name == slice("readrandom")) {
        read(random, 1);
      } else if (name == slice("readrand100k")) {
        int n = reads_;
        reads_ /= 1000;
        read(random, 1);
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

  void open() {
    assert(db_ == null);

    int status;
    char file_name[100];
    char* err_msg = null;
    db_num_++;

    // open database
    std::string tmp_dir;
    env::default()->gettestdirectory(&tmp_dir);
    snprintf(file_name, sizeof(file_name),
             "%s/dbbench_sqlite3-%d.db",
             tmp_dir.c_str(),
             db_num_);
    status = sqlite3_open(file_name, &db_);
    if (status) {
      fprintf(stderr, "open error: %s\n", sqlite3_errmsg(db_));
      exit(1);
    }

    // change sqlite cache size
    char cache_size[100];
    snprintf(cache_size, sizeof(cache_size), "pragma cache_size = %d",
             flags_num_pages);
    status = sqlite3_exec(db_, cache_size, null, null, &err_msg);
    execerrorcheck(status, err_msg);

    // flags_page_size is defaulted to 1024
    if (flags_page_size != 1024) {
      char page_size[100];
      snprintf(page_size, sizeof(page_size), "pragma page_size = %d",
               flags_page_size);
      status = sqlite3_exec(db_, page_size, null, null, &err_msg);
      execerrorcheck(status, err_msg);
    }

    // change journal mode to wal if wal enabled flag is on
    if (flags_wal_enabled) {
      std::string wal_stmt = "pragma journal_mode = wal";

      // leveldb's default cache size is a combined 4 mb
      std::string wal_checkpoint = "pragma wal_autocheckpoint = 4096";
      status = sqlite3_exec(db_, wal_stmt.c_str(), null, null, &err_msg);
      execerrorcheck(status, err_msg);
      status = sqlite3_exec(db_, wal_checkpoint.c_str(), null, null, &err_msg);
      execerrorcheck(status, err_msg);
    }

    // change locking mode to exclusive and create tables/index for database
    std::string locking_stmt = "pragma locking_mode = exclusive";
    std::string create_stmt =
          "create table test (key blob, value blob, primary key(key))";
    std::string stmt_array[] = { locking_stmt, create_stmt };
    int stmt_array_length = sizeof(stmt_array) / sizeof(std::string);
    for (int i = 0; i < stmt_array_length; i++) {
      status = sqlite3_exec(db_, stmt_array[i].c_str(), null, null, &err_msg);
      execerrorcheck(status, err_msg);
    }
  }

  void write(bool write_sync, order order, dbstate state,
             int num_entries, int value_size, int entries_per_batch) {
    // create new database if state == fresh
    if (state == fresh) {
      if (flags_use_existing_db) {
        message_ = "skipping (--use_existing_db is true)";
        return;
      }
      sqlite3_close(db_);
      db_ = null;
      open();
      start();
    }

    if (num_entries != num_) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%d ops)", num_entries);
      message_ = msg;
    }

    char* err_msg = null;
    int status;

    sqlite3_stmt *replace_stmt, *begin_trans_stmt, *end_trans_stmt;
    std::string replace_str = "replace into test (key, value) values (?, ?)";
    std::string begin_trans_str = "begin transaction;";
    std::string end_trans_str = "end transaction;";

    // check for synchronous flag in options
    std::string sync_stmt = (write_sync) ? "pragma synchronous = full" :
                                           "pragma synchronous = off";
    status = sqlite3_exec(db_, sync_stmt.c_str(), null, null, &err_msg);
    execerrorcheck(status, err_msg);

    // preparing sqlite3 statements
    status = sqlite3_prepare_v2(db_, replace_str.c_str(), -1,
                                &replace_stmt, null);
    errorcheck(status);
    status = sqlite3_prepare_v2(db_, begin_trans_str.c_str(), -1,
                                &begin_trans_stmt, null);
    errorcheck(status);
    status = sqlite3_prepare_v2(db_, end_trans_str.c_str(), -1,
                                &end_trans_stmt, null);
    errorcheck(status);

    bool transaction = (entries_per_batch > 1);
    for (int i = 0; i < num_entries; i += entries_per_batch) {
      // begin write transaction
      if (flags_transaction && transaction) {
        status = sqlite3_step(begin_trans_stmt);
        steperrorcheck(status);
        status = sqlite3_reset(begin_trans_stmt);
        errorcheck(status);
      }

      // create and execute sql statements
      for (int j = 0; j < entries_per_batch; j++) {
        const char* value = gen_.generate(value_size).data();

        // create values for key-value pair
        const int k = (order == sequential) ? i + j :
                      (rand_.next() % num_entries);
        char key[100];
        snprintf(key, sizeof(key), "%016d", k);

        // bind kv values into replace_stmt
        status = sqlite3_bind_blob(replace_stmt, 1, key, 16, sqlite_static);
        errorcheck(status);
        status = sqlite3_bind_blob(replace_stmt, 2, value,
                                   value_size, sqlite_static);
        errorcheck(status);

        // execute replace_stmt
        bytes_ += value_size + strlen(key);
        status = sqlite3_step(replace_stmt);
        steperrorcheck(status);

        // reset sqlite statement for another use
        status = sqlite3_clear_bindings(replace_stmt);
        errorcheck(status);
        status = sqlite3_reset(replace_stmt);
        errorcheck(status);

        finishedsingleop();
      }

      // end write transaction
      if (flags_transaction && transaction) {
        status = sqlite3_step(end_trans_stmt);
        steperrorcheck(status);
        status = sqlite3_reset(end_trans_stmt);
        errorcheck(status);
      }
    }

    status = sqlite3_finalize(replace_stmt);
    errorcheck(status);
    status = sqlite3_finalize(begin_trans_stmt);
    errorcheck(status);
    status = sqlite3_finalize(end_trans_stmt);
    errorcheck(status);
  }

  void read(order order, int entries_per_batch) {
    int status;
    sqlite3_stmt *read_stmt, *begin_trans_stmt, *end_trans_stmt;

    std::string read_str = "select * from test where key = ?";
    std::string begin_trans_str = "begin transaction;";
    std::string end_trans_str = "end transaction;";

    // preparing sqlite3 statements
    status = sqlite3_prepare_v2(db_, begin_trans_str.c_str(), -1,
                                &begin_trans_stmt, null);
    errorcheck(status);
    status = sqlite3_prepare_v2(db_, end_trans_str.c_str(), -1,
                                &end_trans_stmt, null);
    errorcheck(status);
    status = sqlite3_prepare_v2(db_, read_str.c_str(), -1, &read_stmt, null);
    errorcheck(status);

    bool transaction = (entries_per_batch > 1);
    for (int i = 0; i < reads_; i += entries_per_batch) {
      // begin read transaction
      if (flags_transaction && transaction) {
        status = sqlite3_step(begin_trans_stmt);
        steperrorcheck(status);
        status = sqlite3_reset(begin_trans_stmt);
        errorcheck(status);
      }

      // create and execute sql statements
      for (int j = 0; j < entries_per_batch; j++) {
        // create key value
        char key[100];
        int k = (order == sequential) ? i + j : (rand_.next() % reads_);
        snprintf(key, sizeof(key), "%016d", k);

        // bind key value into read_stmt
        status = sqlite3_bind_blob(read_stmt, 1, key, 16, sqlite_static);
        errorcheck(status);

        // execute read statement
        while ((status = sqlite3_step(read_stmt)) == sqlite_row) {}
        steperrorcheck(status);

        // reset sqlite statement for another use
        status = sqlite3_clear_bindings(read_stmt);
        errorcheck(status);
        status = sqlite3_reset(read_stmt);
        errorcheck(status);
        finishedsingleop();
      }

      // end read transaction
      if (flags_transaction && transaction) {
        status = sqlite3_step(end_trans_stmt);
        steperrorcheck(status);
        status = sqlite3_reset(end_trans_stmt);
        errorcheck(status);
      }
    }

    status = sqlite3_finalize(read_stmt);
    errorcheck(status);
    status = sqlite3_finalize(begin_trans_stmt);
    errorcheck(status);
    status = sqlite3_finalize(end_trans_stmt);
    errorcheck(status);
  }

  void readsequential() {
    int status;
    sqlite3_stmt *pstmt;
    std::string read_str = "select * from test order by key";

    status = sqlite3_prepare_v2(db_, read_str.c_str(), -1, &pstmt, null);
    errorcheck(status);
    for (int i = 0; i < reads_ && sqlite_row == sqlite3_step(pstmt); i++) {
      bytes_ += sqlite3_column_bytes(pstmt, 1) + sqlite3_column_bytes(pstmt, 2);
      finishedsingleop();
    }

    status = sqlite3_finalize(pstmt);
    errorcheck(status);
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
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      flags_histogram = n;
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      flags_compression_ratio = d;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      flags_use_existing_db = n;
    } else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
      flags_num = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      flags_reads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      flags_value_size = n;
    } else if (leveldb::slice(argv[i]) == leveldb::slice("--no_transaction")) {
      flags_transaction = false;
    } else if (sscanf(argv[i], "--page_size=%d%c", &n, &junk) == 1) {
      flags_page_size = n;
    } else if (sscanf(argv[i], "--num_pages=%d%c", &n, &junk) == 1) {
      flags_num_pages = n;
    } else if (sscanf(argv[i], "--wal_enabled=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      flags_wal_enabled = n;
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
