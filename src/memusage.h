// copyright (c) 2015 the moorecoin developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_memusage_h
#define moorecoin_memusage_h

#include <stdlib.h>

#include <map>
#include <set>
#include <vector>

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

namespace memusage
{

/** compute the total memory used by allocating alloc bytes. */
static size_t mallocusage(size_t alloc);

/** compute the memory used for dynamically allocated but owned data structures.
 *  for generic data types, this is *not* recursive. dynamicusage(vector<vector<int> >)
 *  will compute the memory used for the vector<int>'s, but not for the ints inside.
 *  this is for efficiency reasons, as these functions are intended to be fast. if
 *  application data structures require more accurate inner accounting, they should
 *  do the recursion themselves, or use more efficient caching + updating on modification.
 */
template<typename x> static size_t dynamicusage(const std::vector<x>& v);
template<typename x> static size_t dynamicusage(const std::set<x>& s);
template<typename x, typename y> static size_t dynamicusage(const std::map<x, y>& m);
template<typename x, typename y> static size_t dynamicusage(const boost::unordered_set<x, y>& s);
template<typename x, typename y, typename z> static size_t dynamicusage(const boost::unordered_map<x, y, z>& s);
template<typename x> static size_t dynamicusage(const x& x);

static inline size_t mallocusage(size_t alloc)
{
    // measured on libc6 2.19 on linux.
    if (sizeof(void*) == 8) {
        return ((alloc + 31) >> 4) << 4;
    } else if (sizeof(void*) == 4) {
        return ((alloc + 15) >> 3) << 3;
    } else {
        assert(0);
    }
}

// stl data structures

template<typename x>
struct stl_tree_node
{
private:
    int color;
    void* parent;
    void* left;
    void* right;
    x x;
};

template<typename x>
static inline size_t dynamicusage(const std::vector<x>& v)
{
    return mallocusage(v.capacity() * sizeof(x));
}

template<typename x>
static inline size_t dynamicusage(const std::set<x>& s)
{
    return mallocusage(sizeof(stl_tree_node<x>)) * s.size();
}

template<typename x, typename y>
static inline size_t dynamicusage(const std::map<x, y>& m)
{
    return mallocusage(sizeof(stl_tree_node<std::pair<const x, y> >)) * m.size();
}

// boost data structures

template<typename x>
struct boost_unordered_node : private x
{
private:
    void* ptr;
};

template<typename x, typename y>
static inline size_t dynamicusage(const boost::unordered_set<x, y>& s)
{
    return mallocusage(sizeof(boost_unordered_node<x>)) * s.size() + mallocusage(sizeof(void*) * s.bucket_count());
}

template<typename x, typename y, typename z>
static inline size_t dynamicusage(const boost::unordered_map<x, y, z>& m)
{
    return mallocusage(sizeof(boost_unordered_node<std::pair<const x, y> >)) * m.size() + mallocusage(sizeof(void*) * m.bucket_count());
}

// dispatch to class method as fallback

template<typename x>
static inline size_t dynamicusage(const x& x)
{
    return x.dynamicmemoryusage();
}

}

#endif
