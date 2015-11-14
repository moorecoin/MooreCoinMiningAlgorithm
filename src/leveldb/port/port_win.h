// leveldb copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// see port_example.h for documentation for the following types/functions.

// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//  * redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * neither the name of the university of california, berkeley nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
// 
// this software is provided by the regents and contributors ``as is'' and any
// express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are
// disclaimed. in no event shall the regents and contributors be liable for any
// direct, indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused and
// on any theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use of this
// software, even if advised of the possibility of such damage.
//

#ifndef storage_leveldb_port_port_win_h_
#define storage_leveldb_port_port_win_h_

#ifdef _msc_ver
#define snprintf _snprintf
#define close _close
#define fread_unlocked _fread_nolock
#endif

#include <string>
#include <stdint.h>
#ifdef snappy
#include <snappy.h>
#endif

namespace leveldb {
namespace port {

// windows is little endian (for now :p)
static const bool klittleendian = true;

class condvar;

class mutex {
 public:
  mutex();
  ~mutex();

  void lock();
  void unlock();
  void assertheld();

 private:
  friend class condvar;
  // critical sections are more efficient than mutexes
  // but they are not recursive and can only be used to synchronize threads within the same process
  // we use opaque void * to avoid including windows.h in port_win.h
  void * cs_;

  // no copying
  mutex(const mutex&);
  void operator=(const mutex&);
};

// the win32 api offers a dependable condition variable mechanism, but only starting with
// windows 2008 and vista
// no matter what we will implement our own condition variable with a semaphore
// implementation as described in a paper written by andrew d. birrell in 2003
class condvar {
 public:
  explicit condvar(mutex* mu);
  ~condvar();
  void wait();
  void signal();
  void signalall();
 private:
  mutex* mu_;
  
  mutex wait_mtx_;
  long waiting_;
  
  void * sem1_;
  void * sem2_;
  
  
};

class oncetype {
public:
//    oncetype() : init_(false) {}
    oncetype(const oncetype &once) : init_(once.init_) {}
    oncetype(bool f) : init_(f) {}
    void initonce(void (*initializer)()) {
        mutex_.lock();
        if (!init_) {
            init_ = true;
            initializer();
        }
        mutex_.unlock();
    }

private:
    bool init_;
    mutex mutex_;
};

#define leveldb_once_init false
extern void initonce(port::oncetype*, void (*initializer)());

// storage for a lock-free pointer
class atomicpointer {
 private:
  void * rep_;
 public:
  atomicpointer() : rep_(null) { }
  explicit atomicpointer(void* v); 
  void* acquire_load() const;

  void release_store(void* v);

  void* nobarrier_load() const;

  void nobarrier_store(void* v);
};

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

}
}

#endif  // storage_leveldb_port_port_win_h_
