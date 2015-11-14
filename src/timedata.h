// copyright (c) 2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_timedata_h
#define moorecoin_timedata_h

#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <vector>

class cnetaddr;

/** 
 * median filter over a stream of values.
 * returns the median of the last n numbers
 */
template <typename t>
class cmedianfilter
{
private:
    std::vector<t> vvalues;
    std::vector<t> vsorted;
    unsigned int nsize;

public:
    cmedianfilter(unsigned int size, t initial_value) : nsize(size)
    {
        vvalues.reserve(size);
        vvalues.push_back(initial_value);
        vsorted = vvalues;
    }

    void input(t value)
    {
        if (vvalues.size() == nsize) {
            vvalues.erase(vvalues.begin());
        }
        vvalues.push_back(value);

        vsorted.resize(vvalues.size());
        std::copy(vvalues.begin(), vvalues.end(), vsorted.begin());
        std::sort(vsorted.begin(), vsorted.end());
    }

    t median() const
    {
        int size = vsorted.size();
        assert(size > 0);
        if (size & 1) // odd number of elements
        {
            return vsorted[size / 2];
        } else // even number of elements
        {
            return (vsorted[size / 2 - 1] + vsorted[size / 2]) / 2;
        }
    }

    int size() const
    {
        return vvalues.size();
    }

    std::vector<t> sorted() const
    {
        return vsorted;
    }
};

/** functions to keep track of adjusted p2p time */
int64_t gettimeoffset();
int64_t getadjustedtime();
void addtimedata(const cnetaddr& ip, int64_t ntime);

#endif // moorecoin_timedata_h
