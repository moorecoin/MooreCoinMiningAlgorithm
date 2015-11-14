// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2012 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_threadsafety_h
#define moorecoin_threadsafety_h

#ifdef __clang__
// tl;dr add guarded_by(mutex) to member variables. the others are
// rarely necessary. ex: int nfoo guarded_by(cs_foo);
//
// see http://clang.llvm.org/docs/languageextensions.html#threadsafety
// for documentation.  the clang compiler can do advanced static analysis
// of locking when given the -wthread-safety option.
#define lockable __attribute__((lockable))
#define scoped_lockable __attribute__((scoped_lockable))
#define guarded_by(x) __attribute__((guarded_by(x)))
#define guarded_var __attribute__((guarded_var))
#define pt_guarded_by(x) __attribute__((pt_guarded_by(x)))
#define pt_guarded_var __attribute__((pt_guarded_var))
#define acquired_after(...) __attribute__((acquired_after(__va_args__)))
#define acquired_before(...) __attribute__((acquired_before(__va_args__)))
#define exclusive_lock_function(...) __attribute__((exclusive_lock_function(__va_args__)))
#define shared_lock_function(...) __attribute__((shared_lock_function(__va_args__)))
#define exclusive_trylock_function(...) __attribute__((exclusive_trylock_function(__va_args__)))
#define shared_trylock_function(...) __attribute__((shared_trylock_function(__va_args__)))
#define unlock_function(...) __attribute__((unlock_function(__va_args__)))
#define lock_returned(x) __attribute__((lock_returned(x)))
#define locks_excluded(...) __attribute__((locks_excluded(__va_args__)))
#define exclusive_locks_required(...) __attribute__((exclusive_locks_required(__va_args__)))
#define shared_locks_required(...) __attribute__((shared_locks_required(__va_args__)))
#define no_thread_safety_analysis __attribute__((no_thread_safety_analysis))
#else
#define lockable
#define scoped_lockable
#define guarded_by(x)
#define guarded_var
#define pt_guarded_by(x)
#define pt_guarded_var
#define acquired_after(...)
#define acquired_before(...)
#define exclusive_lock_function(...)
#define shared_lock_function(...)
#define exclusive_trylock_function(...)
#define shared_trylock_function(...)
#define unlock_function(...)
#define lock_returned(x)
#define locks_excluded(...)
#define exclusive_locks_required(...)
#define shared_locks_required(...)
#define no_thread_safety_analysis
#endif // __gnuc__

#endif // moorecoin_threadsafety_h
