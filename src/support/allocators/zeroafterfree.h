// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_support_allocators_zeroafterfree_h
#define moorecoin_support_allocators_zeroafterfree_h

#include "support/cleanse.h"

#include <memory>
#include <vector>

template <typename t>
struct zero_after_free_allocator : public std::allocator<t> {
    // msvc8 default copy constructor is broken
    typedef std::allocator<t> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;
    zero_after_free_allocator() throw() {}
    zero_after_free_allocator(const zero_after_free_allocator& a) throw() : base(a) {}
    template <typename u>
    zero_after_free_allocator(const zero_after_free_allocator<u>& a) throw() : base(a)
    {
    }
    ~zero_after_free_allocator() throw() {}
    template <typename _other>
    struct rebind {
        typedef zero_after_free_allocator<_other> other;
    };

    void deallocate(t* p, std::size_t n)
    {
        if (p != null)
            memory_cleanse(p, sizeof(t) * n);
        std::allocator<t>::deallocate(p, n);
    }
};

// byte-vector that clears its contents before deletion.
typedef std::vector<char, zero_after_free_allocator<char> > cserializedata;

#endif // moorecoin_support_allocators_zeroafterfree_h
