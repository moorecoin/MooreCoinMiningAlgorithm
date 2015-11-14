// copyright (c) 2012-2015 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_mruset_h
#define moorecoin_mruset_h

#include <set>
#include <vector>
#include <utility>

/** stl-like set container that only keeps the most recent n elements. */
template <typename t>
class mruset
{
public:
    typedef t key_type;
    typedef t value_type;
    typedef typename std::set<t>::iterator iterator;
    typedef typename std::set<t>::const_iterator const_iterator;
    typedef typename std::set<t>::size_type size_type;

protected:
    std::set<t> set;
    std::vector<iterator> order;
    size_type first_used;
    size_type first_unused;
    const size_type nmaxsize;

public:
    mruset(size_type nmaxsizein = 1) : nmaxsize(nmaxsizein) { clear(); }
    iterator begin() const { return set.begin(); }
    iterator end() const { return set.end(); }
    size_type size() const { return set.size(); }
    bool empty() const { return set.empty(); }
    iterator find(const key_type& k) const { return set.find(k); }
    size_type count(const key_type& k) const { return set.count(k); }
    void clear()
    {
        set.clear();
        order.assign(nmaxsize, set.end());
        first_used = 0;
        first_unused = 0;
    }
    bool inline friend operator==(const mruset<t>& a, const mruset<t>& b) { return a.set == b.set; }
    bool inline friend operator==(const mruset<t>& a, const std::set<t>& b) { return a.set == b; }
    bool inline friend operator<(const mruset<t>& a, const mruset<t>& b) { return a.set < b.set; }
    std::pair<iterator, bool> insert(const key_type& x)
    {
        std::pair<iterator, bool> ret = set.insert(x);
        if (ret.second) {
            if (set.size() == nmaxsize + 1) {
                set.erase(order[first_used]);
                order[first_used] = set.end();
                if (++first_used == nmaxsize) first_used = 0;
            }
            order[first_unused] = ret.first;
            if (++first_unused == nmaxsize) first_unused = 0;
        }
        return ret;
    }
    size_type max_size() const { return nmaxsize; }
};

#endif // moorecoin_mruset_h
