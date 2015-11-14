// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_limitedmap_h
#define moorecoin_limitedmap_h

#include <assert.h>
#include <map>

/** stl-like map container that only keeps the n elements with the highest value. */
template <typename k, typename v>
class limitedmap
{
public:
    typedef k key_type;
    typedef v mapped_type;
    typedef std::pair<const key_type, mapped_type> value_type;
    typedef typename std::map<k, v>::const_iterator const_iterator;
    typedef typename std::map<k, v>::size_type size_type;

protected:
    std::map<k, v> map;
    typedef typename std::map<k, v>::iterator iterator;
    std::multimap<v, iterator> rmap;
    typedef typename std::multimap<v, iterator>::iterator rmap_iterator;
    size_type nmaxsize;

public:
    limitedmap(size_type nmaxsizein = 0) { nmaxsize = nmaxsizein; }
    const_iterator begin() const { return map.begin(); }
    const_iterator end() const { return map.end(); }
    size_type size() const { return map.size(); }
    bool empty() const { return map.empty(); }
    const_iterator find(const key_type& k) const { return map.find(k); }
    size_type count(const key_type& k) const { return map.count(k); }
    void insert(const value_type& x)
    {
        std::pair<iterator, bool> ret = map.insert(x);
        if (ret.second) {
            if (nmaxsize && map.size() == nmaxsize) {
                map.erase(rmap.begin()->second);
                rmap.erase(rmap.begin());
            }
            rmap.insert(make_pair(x.second, ret.first));
        }
        return;
    }
    void erase(const key_type& k)
    {
        iterator ittarget = map.find(k);
        if (ittarget == map.end())
            return;
        std::pair<rmap_iterator, rmap_iterator> itpair = rmap.equal_range(ittarget->second);
        for (rmap_iterator it = itpair.first; it != itpair.second; ++it)
            if (it->second == ittarget) {
                rmap.erase(it);
                map.erase(ittarget);
                return;
            }
        // shouldn't ever get here
        assert(0);
    }
    void update(const_iterator itin, const mapped_type& v)
    {
        // todo: when we switch to c++11, use map.erase(itin, itin) to get the non-const iterator.
        iterator ittarget = map.find(itin->first);
        if (ittarget == map.end())
            return;
        std::pair<rmap_iterator, rmap_iterator> itpair = rmap.equal_range(ittarget->second);
        for (rmap_iterator it = itpair.first; it != itpair.second; ++it)
            if (it->second == ittarget) {
                rmap.erase(it);
                ittarget->second = v;
                rmap.insert(make_pair(v, ittarget));
                return;
            }
        // shouldn't ever get here
        assert(0);
    }
    size_type max_size() const { return nmaxsize; }
    size_type max_size(size_type s)
    {
        if (s)
            while (map.size() > s) {
                map.erase(rmap.begin()->second);
                rmap.erase(rmap.begin());
            }
        nmaxsize = s;
        return nmaxsize;
    }
};

#endif // moorecoin_limitedmap_h
