// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "support/pagelocker.h"

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#ifdef win32
#ifdef _win32_winnt
#undef _win32_winnt
#endif
#define _win32_winnt 0x0501
#define win32_lean_and_mean 1
#ifndef nominmax
#define nominmax
#endif
#include <windows.h>
// this is used to attempt to keep keying material out of swap
// note that virtuallock does not provide this as a guarantee on windows,
// but, in practice, memory that has been virtuallock'd almost never gets written to
// the pagefile except in rare circumstances where memory is extremely low.
#else
#include <sys/mman.h>
#include <limits.h> // for pagesize
#include <unistd.h> // for sysconf
#endif

lockedpagemanager* lockedpagemanager::_instance = null;
boost::once_flag lockedpagemanager::init_flag = boost_once_init;

/** determine system page size in bytes */
static inline size_t getsystempagesize()
{
    size_t page_size;
#if defined(win32)
    system_info ssysinfo;
    getsysteminfo(&ssysinfo);
    page_size = ssysinfo.dwpagesize;
#elif defined(pagesize) // defined in limits.h
    page_size = pagesize;
#else                   // assume some posix os
    page_size = sysconf(_sc_pagesize);
#endif
    return page_size;
}

bool memorypagelocker::lock(const void* addr, size_t len)
{
#ifdef win32
    return virtuallock(const_cast<void*>(addr), len) != 0;
#else
    return mlock(addr, len) == 0;
#endif
}

bool memorypagelocker::unlock(const void* addr, size_t len)
{
#ifdef win32
    return virtualunlock(const_cast<void*>(addr), len) != 0;
#else
    return munlock(addr, len) == 0;
#endif
}

lockedpagemanager::lockedpagemanager() : lockedpagemanagerbase<memorypagelocker>(getsystempagesize())
{
}
