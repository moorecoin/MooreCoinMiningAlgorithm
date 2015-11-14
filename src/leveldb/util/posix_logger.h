// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// logger implementation that can be shared by all environments
// where enough posix functionality is available.

#ifndef storage_leveldb_util_posix_logger_h_
#define storage_leveldb_util_posix_logger_h_

#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "leveldb/env.h"

namespace leveldb {

class posixlogger : public logger {
 private:
  file* file_;
  uint64_t (*gettid_)();  // return the thread id for the current thread
 public:
  posixlogger(file* f, uint64_t (*gettid)()) : file_(f), gettid_(gettid) { }
  virtual ~posixlogger() {
    fclose(file_);
  }
  virtual void logv(const char* format, va_list ap) {
    const uint64_t thread_id = (*gettid_)();

    // we try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 30000;
        base = new char[bufsize];
      }
      char* p = base;
      char* limit = base + bufsize;

      struct timeval now_tv;
      gettimeofday(&now_tv, null);
      const time_t seconds = now_tv.tv_sec;
      struct tm t;
      localtime_r(&seconds, &t);
      p += snprintf(p, limit - p,
                    "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    static_cast<int>(now_tv.tv_usec),
                    static_cast<long long unsigned int>(thread_id));

      // print the message
      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // truncate to available space if necessary
      if (p >= limit) {
        if (iter == 0) {
          continue;       // try again with larger buffer
        } else {
          p = limit - 1;
        }
      }

      // add newline if necessary
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      fwrite(base, 1, p - base, file_);
      fflush(file_);
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }
};

}  // namespace leveldb

#endif  // storage_leveldb_util_posix_logger_h_
