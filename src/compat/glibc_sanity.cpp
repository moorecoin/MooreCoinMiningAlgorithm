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

extern "c" void* memcpy(void* a, const void* b, size_t c);
void* memcpy_int(void* a, const void* b, size_t c)
{
    return memcpy(a, b, c);
}

namespace
{
// trigger: use the memcpy_int wrapper which calls our internal memcpy.
//   a direct call to memcpy may be optimized away by the compiler.
// test: fill an array with a sequence of integers. memcpy to a new empty array.
//   verify that the arrays are equal. use an odd size to decrease the odds of
//   the call being optimized away.
template <unsigned int t>
bool sanity_test_memcpy()
{
    unsigned int memcpy_test[t];
    unsigned int memcpy_verify[t] = {};
    for (unsigned int i = 0; i != t; ++i)
        memcpy_test[i] = i;

    memcpy_int(memcpy_verify, memcpy_test, sizeof(memcpy_test));

    for (unsigned int i = 0; i != t; ++i) {
        if (memcpy_verify[i] != i)
            return false;
    }
    return true;
}

#if defined(have_sys_select_h)
// trigger: call fd_set to trigger __fdelt_chk. fortify_source must be defined
//   as >0 and optimizations must be set to at least -o2.
// test: add a file descriptor to an empty fd_set. verify that it has been
//   correctly added.
bool sanity_test_fdelt()
{
    fd_set fds;
    fd_zero(&fds);
    fd_set(0, &fds);
    return fd_isset(0, &fds);
}
#endif

} // anon namespace

bool glibc_sanity_test()
{
#if defined(have_sys_select_h)
    if (!sanity_test_fdelt())
        return false;
#endif
    return sanity_test_memcpy<1025>();
}
