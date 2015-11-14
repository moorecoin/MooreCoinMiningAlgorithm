// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// this file contains the specification, but not the implementations,
// of the types/operations/etc. that should be defined by a platform
// specific port_<platform>.h file.  use this file as a reference for
// how to port this package to a new platform.

#ifndef storage_leveldb_port_port_example_h_
#define storage_leveldb_port_port_example_h_

namespace leveldb {
namespace port {

// todo(jorlow): many of these belong more in the environment class rather than
//               here. we should try moving them and see if it affects perf.

// the following boolean constant must be true on a little-endian machine
// and false otherwise.
static const bool klittleendian = true /* or some other expression */;

// ------------------ threading -------------------

// a mutex represents an exclusive lock.
class mutex {
 public:
  mutex();
  ~mutex();

  // lock the mutex.  waits until other lockers have exited.
  // will deadlock if the mutex is already locked by this thread.
  void lock();

  // unlock the mutex.
  // requires: this mutex was locked by this thread.
  void unlock();

  // optionally crash if this thread does not hold this mutex.
  // the implementation must be fast, especially if ndebug is
  // defined.  the implementation is allowed to skip all checks.
  void assertheld();
};

class condvar {
 public:
  explicit condvar(mutex* mu);
  ~condvar();

  // atomically release *mu and block on this condition variable until
  // either a call to signalall(), or a call to signal() that picks
  // this thread to wakeup.
  // requires: this thread holds *mu
  void wait();

  // if there are some threads waiting, wake up at least one of them.
  void signal();

  // wake up all waiting threads.
  void signallall();
};

// thread-safe initialization.
// used as follows:
//      static port::oncetype init_control = leveldb_once_init;
//      static void initializer() { ... do something ...; }
//      ...
//      port::initonce(&init_control, &initializer);
typedef intptr_t oncetype;
#define leveldb_once_init 0
extern void initonce(port::oncetype*, void (*initializer)());

// a type that holds a pointer that can be read or written atomically
// (i.e., without word-tearing.)
class atomicpointer {
 private:
  intptr_t rep_;
 public:
  // initialize to arbitrary value
  atomicpointer();

  // initialize to hold v
  explicit atomicpointer(void* v) : rep_(v) { }

  // read and return the stored pointer with the guarantee that no
  // later memory access (read or write) by this thread can be
  // reordered ahead of this read.
  void* acquire_load() const;

  // set v as the stored pointer with the guarantee that no earlier
  // memory access (read or write) by this thread can be reordered
  // after this store.
  void release_store(void* v);

  // read the stored pointer with no ordering guarantees.
  void* nobarrier_load() const;

  // set va as the stored pointer with no ordering guarantees.
  void nobarrier_store(void* v);
};

// ------------------ compression -------------------

// store the snappy compression of "input[0,input_length-1]" in *output.
// returns false if snappy is not supported by this port.
extern bool snappy_compress(const char* input, size_t input_length,
                            std::string* output);

// if input[0,input_length-1] looks like a valid snappy compressed
// buffer, store the size of the uncompressed data in *result and
// return true.  else return false.
extern bool snappy_getuncompressedlength(const char* input, size_t length,
                                         size_t* result);

// attempt to snappy uncompress input[0,input_length-1] into *output.
// returns true if successful, false if the input is invalid lightweight
// compressed data.
//
// requires: at least the first "n" bytes of output[] must be writable
// where "n" is the result of a successful call to
// snappy_getuncompressedlength.
extern bool snappy_uncompress(const char* input_data, size_t input_length,
                              char* output);

// ------------------ miscellaneous -------------------

// if heap profiling is not supported, returns false.
// else repeatedly calls (*func)(arg, data, n) and then returns true.
// the concatenation of all "data[0,n-1]" fragments is the heap profile.
extern bool getheapprofile(void (*func)(void*, const char*, int), void* arg);

}  // namespace port
}  // namespace leveldb

#endif  // storage_leveldb_port_port_example_h_
