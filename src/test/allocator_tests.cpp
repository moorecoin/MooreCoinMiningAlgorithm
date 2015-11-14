// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"

#include "support/allocators/secure.h"
#include "test/test_moorecoin.h"

#include <boost/test/unit_test.hpp>

boost_fixture_test_suite(allocator_tests, basictestingsetup)

// dummy memory page locker for platform independent tests
static const void *last_lock_addr, *last_unlock_addr;
static size_t last_lock_len, last_unlock_len;
class testlocker
{
public:
    bool lock(const void *addr, size_t len)
    {
        last_lock_addr = addr;
        last_lock_len = len;
        return true;
    }
    bool unlock(const void *addr, size_t len)
    {
        last_unlock_addr = addr;
        last_unlock_len = len;
        return true;
    }
};

boost_auto_test_case(test_lockedpagemanagerbase)
{
    const size_t test_page_size = 4096;
    lockedpagemanagerbase<testlocker> lpm(test_page_size);
    size_t addr;
    last_lock_addr = last_unlock_addr = 0;
    last_lock_len = last_unlock_len = 0;

    /* try large number of small objects */
    addr = 0;
    for(int i=0; i<1000; ++i)
    {
        lpm.lockrange(reinterpret_cast<void*>(addr), 33);
        addr += 33;
    }
    /* try small number of page-sized objects, straddling two pages */
    addr = test_page_size*100 + 53;
    for(int i=0; i<100; ++i)
    {
        lpm.lockrange(reinterpret_cast<void*>(addr), test_page_size);
        addr += test_page_size;
    }
    /* try small number of page-sized objects aligned to exactly one page */
    addr = test_page_size*300;
    for(int i=0; i<100; ++i)
    {
        lpm.lockrange(reinterpret_cast<void*>(addr), test_page_size);
        addr += test_page_size;
    }
    /* one very large object, straddling pages */
    lpm.lockrange(reinterpret_cast<void*>(test_page_size*600+1), test_page_size*500);
    boost_check(last_lock_addr == reinterpret_cast<void*>(test_page_size*(600+500)));
    /* one very large object, page aligned */
    lpm.lockrange(reinterpret_cast<void*>(test_page_size*1200), test_page_size*500-1);
    boost_check(last_lock_addr == reinterpret_cast<void*>(test_page_size*(1200+500-1)));

    boost_check(lpm.getlockedpagecount() == (
        (1000*33+test_page_size-1)/test_page_size + // small objects
        101 + 100 +  // page-sized objects
        501 + 500)); // large objects
    boost_check((last_lock_len & (test_page_size-1)) == 0); // always lock entire pages
    boost_check(last_unlock_len == 0); // nothing unlocked yet

    /* and unlock again */
    addr = 0;
    for(int i=0; i<1000; ++i)
    {
        lpm.unlockrange(reinterpret_cast<void*>(addr), 33);
        addr += 33;
    }
    addr = test_page_size*100 + 53;
    for(int i=0; i<100; ++i)
    {
        lpm.unlockrange(reinterpret_cast<void*>(addr), test_page_size);
        addr += test_page_size;
    }
    addr = test_page_size*300;
    for(int i=0; i<100; ++i)
    {
        lpm.unlockrange(reinterpret_cast<void*>(addr), test_page_size);
        addr += test_page_size;
    }
    lpm.unlockrange(reinterpret_cast<void*>(test_page_size*600+1), test_page_size*500);
    lpm.unlockrange(reinterpret_cast<void*>(test_page_size*1200), test_page_size*500-1);

    /* check that everything is released */
    boost_check(lpm.getlockedpagecount() == 0);

    /* a few and unlocks of size zero (should have no effect) */
    addr = 0;
    for(int i=0; i<1000; ++i)
    {
        lpm.lockrange(reinterpret_cast<void*>(addr), 0);
        addr += 1;
    }
    boost_check(lpm.getlockedpagecount() == 0);
    addr = 0;
    for(int i=0; i<1000; ++i)
    {
        lpm.unlockrange(reinterpret_cast<void*>(addr), 0);
        addr += 1;
    }
    boost_check(lpm.getlockedpagecount() == 0);
    boost_check((last_unlock_len & (test_page_size-1)) == 0); // always unlock entire pages
}

boost_auto_test_suite_end()
