// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include <cstddef>

#if defined(have_sys_select_h)
#include <sys/select.h>
#endif

// prior to glibc_2.14, memcpy was aliased to memmove.
extern "c" void* memmove(void* a, const void* b, size_t c);
extern "c" void* memcpy(void* a, const void* b, size_t c)
{
    return memmove(a, b, c);
}

extern "c" void __chk_fail(void) __attribute__((__noreturn__));
extern "c" fdelt_type __fdelt_warn(fdelt_type a)
{
    if (a >= fd_setsize)
        __chk_fail();
    return a / __nfdbits;
}
extern "c" fdelt_type __fdelt_chk(fdelt_type) __attribute__((weak, alias("__fdelt_warn")));
