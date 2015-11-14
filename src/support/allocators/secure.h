// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_support_allocators_secure_h
#define moorecoin_support_allocators_secure_h

#include "support/pagelocker.h"

#include <string>

//
// allocator that locks its contents from being paged
// out of memory and clears its contents before deletion.
//
template <typename t>
struct secure_allocator : public std::allocator<t> {
    // msvc8 default copy constructor is broken
    typedef std::allocator<t> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;
    secure_allocator() throw() {}
    secure_allocator(const secure_allocator& a) throw() : base(a) {}
    template <typename u>
    secure_allocator(const secure_allocator<u>& a) throw() : base(a)
    {
    }
    ~secure_allocator() throw() {}
    template <typename _other>
    struct rebind {
        typedef secure_allocator<_other> other;
    };

    t* allocate(std::size_t n, const void* hint = 0)
    {
        t* p;
        p = std::allocator<t>::allocate(n, hint);
        if (p != null)
            lockedpagemanager::instance().lockrange(p, sizeof(t) * n);
        return p;
    }

    void deallocate(t* p, std::size_t n)
    {
        if (p != null) {
            memory_cleanse(p, sizeof(t) * n);
            lockedpagemanager::instance().unlockrange(p, sizeof(t) * n);
        }
        std::allocator<t>::deallocate(p, n);
    }
};

// this is exactly like std::string, but with a custom allocator.
typedef std::basic_string<char, std::char_traits<char>, secure_allocator<char> > securestring;

#endif // moorecoin_support_allocators_secure_h
