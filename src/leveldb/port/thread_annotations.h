// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_port_thread_annotations_h_
#define storage_leveldb_port_thread_annotations_h_

// some environments provide custom macros to aid in static thread-safety
// analysis.  provide empty definitions of such macros unless they are already
// defined.

#ifndef exclusive_locks_required
#define exclusive_locks_required(...)
#endif

#ifndef shared_locks_required
#define shared_locks_required(...)
#endif

#ifndef locks_excluded
#define locks_excluded(...)
#endif

#ifndef lock_returned
#define lock_returned(x)
#endif

#ifndef lockable
#define lockable
#endif

#ifndef scoped_lockable
#define scoped_lockable
#endif

#ifndef exclusive_lock_function
#define exclusive_lock_function(...)
#endif

#ifndef shared_lock_function
#define shared_lock_function(...)
#endif

#ifndef exclusive_trylock_function
#define exclusive_trylock_function(...)
#endif

#ifndef shared_trylock_function
#define shared_trylock_function(...)
#endif

#ifndef unlock_function
#define unlock_function(...)
#endif

#ifndef no_thread_safety_analysis
#define no_thread_safety_analysis
#endif

#endif  // storage_leveldb_port_thread_annotations_h_
