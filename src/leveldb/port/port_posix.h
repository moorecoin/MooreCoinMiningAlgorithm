// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// see port_example.h for documentation for the following types/functions.

#ifndef storage_leveldb_port_port_posix_h_
#define storage_leveldb_port_port_posix_h_

#undef platform_is_little_endian
#if defined(os_macosx)
  #include <machine/endian.h>
  #if defined(__darwin_little_endian) && defined(__darwin_byte_order)
    #define platform_is_little_endian \
        (__darwin_byte_order == __darwin_little_endian)
  #endif
#elif defined(os_solaris)
  #include <sys/isa_defs.h>
  #ifdef _little_endian
    #define platform_is_little_endian true
  #else
    #define platform_is_little_endian false
  #endif
#elif defined(os_freebsd) || defined(os_openbsd) ||\
      defined(os_netbsd) || defined(os_dragonflybsd)
  #include <sys/types.h>
  #include <sys/endian.h>
  #define platform_is_little_endian (_byte_order == _little_endian)
#elif defined(os_hpux)
  #define platform_is_little_endian false
#elif defined(os_android)
  // due to a bug in the ndk x86 <sys/endian.h> definition,
  // _byte_order must be used instead of __byte_order on android.
  // see http://code.google.com/p/android/issues/detail?id=39824
  #include <endian.h>
  #define platform_is_little_endian  (_byte_order == _little_endian)
#else
  #include <endian.h>
#endif

#include <pthread.h>
#ifdef snappy
#include <snappy.h>
#endif
#include <stdint.h>
#include <string>
#include "port/atomic_pointer.h"

#ifndef platform_is_little_endian
#define platform_is_little_endian (__byte_order == __little_endian)
#endif

#if defined(os_macosx) || defined(os_solaris) || defined(os_freebsd) ||\
    defined(os_netbsd) || defined(os_openbsd) || defined(os_dragonflybsd) ||\
    defined(os_android) || defined(os_hpux) || defined(cygwin)
// use fread/fwrite/fflush on platforms without _unlocked variants
#define fread_unlocked fread
#define fwrite_unlocked fwrite
#define fflush_unlocked fflush
#endif

#if defined(os_freebsd) ||\
    defined(os_openbsd) || defined(os_dragonflybsd)
// use fsync() on platforms without fdatasync()
#define fdatasync fsync
#endif

#if defined(os_macosx)
#define fdatasync(fd) fcntl(fd, f_fullfsync, 0)
#endif

#if defined(os_android) && __android_api__ < 9
// fdatasync() was only introduced in api level 9 on android. use fsync()
// when targetting older platforms.
#define fdatasync fsync
#endif

namespace leveldb {
namespace port {

static const bool klittleendian = platform_is_little_endian;
#undef platform_is_little_endian

class condvar;

class mutex {
 public:
  mutex();
  ~mutex();

  void lock();
  void unlock();
  void assertheld() { }

 private:
  friend class condvar;
  pthread_mutex_t mu_;

  // no copying
  mutex(const mutex&);
  void operator=(const mutex&);
};

class condvar {
 public:
  explicit condvar(mutex* mu);
  ~condvar();
  void wait();
  void signal();
  void signalall();
 private:
  pthread_cond_t cv_;
  mutex* mu_;
};

typedef pthread_once_t oncetype;
#define leveldb_once_init pthread_once_init
extern void initonce(oncetype* once, void (*initializer)());

inline bool snappy_compress(const char* input, size_t length,
                            ::std::string* output) {
#ifdef snappy
  output->resize(snappy::maxcompressedlength(length));
  size_t outlen;
  snappy::rawcompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#endif

  return false;
}

inline bool snappy_getuncompressedlength(const char* input, size_t length,
                                         size_t* result) {
#ifdef snappy
  return snappy::getuncompressedlength(input, length, result);
#else
  return false;
#endif
}

inline bool snappy_uncompress(const char* input, size_t length,
                              char* output) {
#ifdef snappy
  return snappy::rawuncompress(input, length, output);
#else
  return false;
#endif
}

inline bool getheapprofile(void (*func)(void*, const char*, int), void* arg) {
  return false;
}

} // namespace port
} // namespace leveldb

#endif  // storage_leveldb_port_port_posix_h_
