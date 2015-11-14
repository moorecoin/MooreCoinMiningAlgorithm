// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

// atomicpointer provides storage for a lock-free pointer.
// platform-dependent implementation of atomicpointer:
// - if the platform provides a cheap barrier, we use it with raw pointers
// - if <atomic> is present (on newer versions of gcc, it is), we use
//   a <atomic>-based atomicpointer.  however we prefer the memory
//   barrier based version, because at least on a gcc 4.4 32-bit build
//   on linux, we have encountered a buggy <atomic> implementation.
//   also, some <atomic> implementations are much slower than a memory-barrier
//   based implementation (~16ns for <atomic> based acquire-load vs. ~1ns for
//   a barrier based acquire-load).
// this code is based on atomicops-internals-* in google's perftools:
// http://code.google.com/p/google-perftools/source/browse/#svn%2ftrunk%2fsrc%2fbase

#ifndef port_atomic_pointer_h_
#define port_atomic_pointer_h_

#include <stdint.h>
#ifdef leveldb_atomic_present
#include <atomic>
#endif
#ifdef os_win
#include <windows.h>
#endif
#ifdef os_macosx
#include <libkern/osatomic.h>
#endif

#if defined(_m_x64) || defined(__x86_64__)
#define arch_cpu_x86_family 1
#elif defined(_m_ix86) || defined(__i386__) || defined(__i386)
#define arch_cpu_x86_family 1
#elif defined(__armel__)
#define arch_cpu_arm_family 1
#elif defined(__ppc__) || defined(__powerpc__) || defined(__powerpc64__)
#define arch_cpu_ppc_family 1
#endif

namespace leveldb {
namespace port {

// define memorybarrier() if available
// windows on x86
#if defined(os_win) && defined(compiler_msvc) && defined(arch_cpu_x86_family)
// windows.h already provides a memorybarrier(void) macro
// http://msdn.microsoft.com/en-us/library/ms684208(v=vs.85).aspx
#define leveldb_have_memory_barrier

// mac os
#elif defined(os_macosx)
inline void memorybarrier() {
  osmemorybarrier();
}
#define leveldb_have_memory_barrier

// gcc on x86
#elif defined(arch_cpu_x86_family) && defined(__gnuc__)
inline void memorybarrier() {
  // see http://gcc.gnu.org/ml/gcc/2003-04/msg01180.html for a discussion on
  // this idiom. also see http://en.wikipedia.org/wiki/memory_ordering.
  __asm__ __volatile__("" : : : "memory");
}
#define leveldb_have_memory_barrier

// sun studio
#elif defined(arch_cpu_x86_family) && defined(__sunpro_cc)
inline void memorybarrier() {
  // see http://gcc.gnu.org/ml/gcc/2003-04/msg01180.html for a discussion on
  // this idiom. also see http://en.wikipedia.org/wiki/memory_ordering.
  asm volatile("" : : : "memory");
}
#define leveldb_have_memory_barrier

// arm linux
#elif defined(arch_cpu_arm_family) && defined(__linux__)
typedef void (*linuxkernelmemorybarrierfunc)(void);
// the linux arm kernel provides a highly optimized device-specific memory
// barrier function at a fixed memory address that is mapped in every
// user-level process.
//
// this beats using cpu-specific instructions which are, on single-core
// devices, un-necessary and very costly (e.g. armv7-a "dmb" takes more
// than 180ns on a cortex-a8 like the one on a nexus one). benchmarking
// shows that the extra function call cost is completely negligible on
// multi-core devices.
//
inline void memorybarrier() {
  (*(linuxkernelmemorybarrierfunc)0xffff0fa0)();
}
#define leveldb_have_memory_barrier

// ppc
#elif defined(arch_cpu_ppc_family) && defined(__gnuc__)
inline void memorybarrier() {
  // todo for some powerpc expert: is there a cheaper suitable variant?
  // perhaps by having separate barriers for acquire and release ops.
  asm volatile("sync" : : : "memory");
}
#define leveldb_have_memory_barrier

#endif

// atomicpointer built using platform-specific memorybarrier()
#if defined(leveldb_have_memory_barrier)
class atomicpointer {
 private:
  void* rep_;
 public:
  atomicpointer() { }
  explicit atomicpointer(void* p) : rep_(p) {}
  inline void* nobarrier_load() const { return rep_; }
  inline void nobarrier_store(void* v) { rep_ = v; }
  inline void* acquire_load() const {
    void* result = rep_;
    memorybarrier();
    return result;
  }
  inline void release_store(void* v) {
    memorybarrier();
    rep_ = v;
  }
};

// atomicpointer based on <cstdatomic>
#elif defined(leveldb_atomic_present)
class atomicpointer {
 private:
  std::atomic<void*> rep_;
 public:
  atomicpointer() { }
  explicit atomicpointer(void* v) : rep_(v) { }
  inline void* acquire_load() const {
    return rep_.load(std::memory_order_acquire);
  }
  inline void release_store(void* v) {
    rep_.store(v, std::memory_order_release);
  }
  inline void* nobarrier_load() const {
    return rep_.load(std::memory_order_relaxed);
  }
  inline void nobarrier_store(void* v) {
    rep_.store(v, std::memory_order_relaxed);
  }
};

// atomic pointer based on sparc memory barriers
#elif defined(__sparcv9) && defined(__gnuc__)
class atomicpointer {
 private:
  void* rep_;
 public:
  atomicpointer() { }
  explicit atomicpointer(void* v) : rep_(v) { }
  inline void* acquire_load() const {
    void* val;
    __asm__ __volatile__ (
        "ldx [%[rep_]], %[val] \n\t"
         "membar #loadload|#loadstore \n\t"
        : [val] "=r" (val)
        : [rep_] "r" (&rep_)
        : "memory");
    return val;
  }
  inline void release_store(void* v) {
    __asm__ __volatile__ (
        "membar #loadstore|#storestore \n\t"
        "stx %[v], [%[rep_]] \n\t"
        :
        : [rep_] "r" (&rep_), [v] "r" (v)
        : "memory");
  }
  inline void* nobarrier_load() const { return rep_; }
  inline void nobarrier_store(void* v) { rep_ = v; }
};

// atomic pointer based on ia64 acq/rel
#elif defined(__ia64) && defined(__gnuc__)
class atomicpointer {
 private:
  void* rep_;
 public:
  atomicpointer() { }
  explicit atomicpointer(void* v) : rep_(v) { }
  inline void* acquire_load() const {
    void* val    ;
    __asm__ __volatile__ (
        "ld8.acq %[val] = [%[rep_]] \n\t"
        : [val] "=r" (val)
        : [rep_] "r" (&rep_)
        : "memory"
        );
    return val;
  }
  inline void release_store(void* v) {
    __asm__ __volatile__ (
        "st8.rel [%[rep_]] = %[v]  \n\t"
        :
        : [rep_] "r" (&rep_), [v] "r" (v)
        : "memory"
        );
  }
  inline void* nobarrier_load() const { return rep_; }
  inline void nobarrier_store(void* v) { rep_ = v; }
};

// we have neither memorybarrier(), nor <atomic>
#else
#error please implement atomicpointer for this platform.

#endif

#undef leveldb_have_memory_barrier
#undef arch_cpu_x86_family
#undef arch_cpu_arm_family
#undef arch_cpu_ppc_family

}  // namespace port
}  // namespace leveldb

#endif  // port_atomic_pointer_h_
