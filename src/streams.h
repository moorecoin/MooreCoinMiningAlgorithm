// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_streams_h
#define moorecoin_streams_h

#include "support/allocators/zeroafterfree.h"
#include "serialize.h"

#include <algorithm>
#include <assert.h>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <utility>
#include <vector>

/** double ended buffer combining vector and stream-like interfaces.
 *
 * >> and << read and write unformatted data using the above serialization templates.
 * fills with data in linear time; some stringstream implementations take n^2 time.
 */
class cdatastream
{
protected:
    typedef cserializedata vector_type;
    vector_type vch;
    unsigned int nreadpos;
public:
    int ntype;
    int nversion;

    typedef vector_type::allocator_type   allocator_type;
    typedef vector_type::size_type        size_type;
    typedef vector_type::difference_type  difference_type;
    typedef vector_type::reference        reference;
    typedef vector_type::const_reference  const_reference;
    typedef vector_type::value_type       value_type;
    typedef vector_type::iterator         iterator;
    typedef vector_type::const_iterator   const_iterator;
    typedef vector_type::reverse_iterator reverse_iterator;

    explicit cdatastream(int ntypein, int nversionin)
    {
        init(ntypein, nversionin);
    }

    cdatastream(const_iterator pbegin, const_iterator pend, int ntypein, int nversionin) : vch(pbegin, pend)
    {
        init(ntypein, nversionin);
    }

#if !defined(_msc_ver) || _msc_ver >= 1300
    cdatastream(const char* pbegin, const char* pend, int ntypein, int nversionin) : vch(pbegin, pend)
    {
        init(ntypein, nversionin);
    }
#endif

    cdatastream(const vector_type& vchin, int ntypein, int nversionin) : vch(vchin.begin(), vchin.end())
    {
        init(ntypein, nversionin);
    }

    cdatastream(const std::vector<char>& vchin, int ntypein, int nversionin) : vch(vchin.begin(), vchin.end())
    {
        init(ntypein, nversionin);
    }

    cdatastream(const std::vector<unsigned char>& vchin, int ntypein, int nversionin) : vch(vchin.begin(), vchin.end())
    {
        init(ntypein, nversionin);
    }

    void init(int ntypein, int nversionin)
    {
        nreadpos = 0;
        ntype = ntypein;
        nversion = nversionin;
    }

    cdatastream& operator+=(const cdatastream& b)
    {
        vch.insert(vch.end(), b.begin(), b.end());
        return *this;
    }

    friend cdatastream operator+(const cdatastream& a, const cdatastream& b)
    {
        cdatastream ret = a;
        ret += b;
        return (ret);
    }

    std::string str() const
    {
        return (std::string(begin(), end()));
    }


    //
    // vector subset
    //
    const_iterator begin() const                     { return vch.begin() + nreadpos; }
    iterator begin()                                 { return vch.begin() + nreadpos; }
    const_iterator end() const                       { return vch.end(); }
    iterator end()                                   { return vch.end(); }
    size_type size() const                           { return vch.size() - nreadpos; }
    bool empty() const                               { return vch.size() == nreadpos; }
    void resize(size_type n, value_type c=0)         { vch.resize(n + nreadpos, c); }
    void reserve(size_type n)                        { vch.reserve(n + nreadpos); }
    const_reference operator[](size_type pos) const  { return vch[pos + nreadpos]; }
    reference operator[](size_type pos)              { return vch[pos + nreadpos]; }
    void clear()                                     { vch.clear(); nreadpos = 0; }
    iterator insert(iterator it, const char& x=char()) { return vch.insert(it, x); }
    void insert(iterator it, size_type n, const char& x) { vch.insert(it, n, x); }

    void insert(iterator it, std::vector<char>::const_iterator first, std::vector<char>::const_iterator last)
    {
        assert(last - first >= 0);
        if (it == vch.begin() + nreadpos && (unsigned int)(last - first) <= nreadpos)
        {
            // special case for inserting at the front when there's room
            nreadpos -= (last - first);
            memcpy(&vch[nreadpos], &first[0], last - first);
        }
        else
            vch.insert(it, first, last);
    }

#if !defined(_msc_ver) || _msc_ver >= 1300
    void insert(iterator it, const char* first, const char* last)
    {
        assert(last - first >= 0);
        if (it == vch.begin() + nreadpos && (unsigned int)(last - first) <= nreadpos)
        {
            // special case for inserting at the front when there's room
            nreadpos -= (last - first);
            memcpy(&vch[nreadpos], &first[0], last - first);
        }
        else
            vch.insert(it, first, last);
    }
#endif

    iterator erase(iterator it)
    {
        if (it == vch.begin() + nreadpos)
        {
            // special case for erasing from the front
            if (++nreadpos >= vch.size())
            {
                // whenever we reach the end, we take the opportunity to clear the buffer
                nreadpos = 0;
                return vch.erase(vch.begin(), vch.end());
            }
            return vch.begin() + nreadpos;
        }
        else
            return vch.erase(it);
    }

    iterator erase(iterator first, iterator last)
    {
        if (first == vch.begin() + nreadpos)
        {
            // special case for erasing from the front
            if (last == vch.end())
            {
                nreadpos = 0;
                return vch.erase(vch.begin(), vch.end());
            }
            else
            {
                nreadpos = (last - vch.begin());
                return last;
            }
        }
        else
            return vch.erase(first, last);
    }

    inline void compact()
    {
        vch.erase(vch.begin(), vch.begin() + nreadpos);
        nreadpos = 0;
    }

    bool rewind(size_type n)
    {
        // rewind by n characters if the buffer hasn't been compacted yet
        if (n > nreadpos)
            return false;
        nreadpos -= n;
        return true;
    }


    //
    // stream subset
    //
    bool eof() const             { return size() == 0; }
    cdatastream* rdbuf()         { return this; }
    int in_avail()               { return size(); }

    void settype(int n)          { ntype = n; }
    int gettype()                { return ntype; }
    void setversion(int n)       { nversion = n; }
    int getversion()             { return nversion; }
    void readversion()           { *this >> nversion; }
    void writeversion()          { *this << nversion; }

    cdatastream& read(char* pch, size_t nsize)
    {
        // read from the beginning of the buffer
        unsigned int nreadposnext = nreadpos + nsize;
        if (nreadposnext >= vch.size())
        {
            if (nreadposnext > vch.size())
            {
                throw std::ios_base::failure("cdatastream::read(): end of data");
            }
            memcpy(pch, &vch[nreadpos], nsize);
            nreadpos = 0;
            vch.clear();
            return (*this);
        }
        memcpy(pch, &vch[nreadpos], nsize);
        nreadpos = nreadposnext;
        return (*this);
    }

    cdatastream& ignore(int nsize)
    {
        // ignore from the beginning of the buffer
        assert(nsize >= 0);
        unsigned int nreadposnext = nreadpos + nsize;
        if (nreadposnext >= vch.size())
        {
            if (nreadposnext > vch.size())
                throw std::ios_base::failure("cdatastream::ignore(): end of data");
            nreadpos = 0;
            vch.clear();
            return (*this);
        }
        nreadpos = nreadposnext;
        return (*this);
    }

    cdatastream& write(const char* pch, size_t nsize)
    {
        // write to the end of the buffer
        vch.insert(vch.end(), pch, pch + nsize);
        return (*this);
    }

    template<typename stream>
    void serialize(stream& s, int ntype, int nversion) const
    {
        // special case: stream << stream concatenates like stream += stream
        if (!vch.empty())
            s.write((char*)&vch[0], vch.size() * sizeof(vch[0]));
    }

    template<typename t>
    unsigned int getserializesize(const t& obj)
    {
        // tells the size of the object if serialized to this stream
        return ::getserializesize(obj, ntype, nversion);
    }

    template<typename t>
    cdatastream& operator<<(const t& obj)
    {
        // serialize to this stream
        ::serialize(*this, obj, ntype, nversion);
        return (*this);
    }

    template<typename t>
    cdatastream& operator>>(t& obj)
    {
        // unserialize from this stream
        ::unserialize(*this, obj, ntype, nversion);
        return (*this);
    }

    void getandclear(cserializedata &data) {
        data.insert(data.end(), begin(), end());
        clear();
    }
};










/** non-refcounted raii wrapper for file*
 *
 * will automatically close the file when it goes out of scope if not null.
 * if you're returning the file pointer, return file.release().
 * if you need to close the file early, use file.fclose() instead of fclose(file).
 */
class cautofile
{
private:
    // disallow copies
    cautofile(const cautofile&);
    cautofile& operator=(const cautofile&);

    int ntype;
    int nversion;
	
    file* file;	

public:
    cautofile(file* filenew, int ntypein, int nversionin)
    {
        file = filenew;
        ntype = ntypein;
        nversion = nversionin;
    }

    ~cautofile()
    {
        fclose();
    }

    void fclose()
    {
        if (file) {
            ::fclose(file);
            file = null;
        }
    }

    /** get wrapped file* with transfer of ownership.
     * @note this will invalidate the cautofile object, and makes it the responsibility of the caller
     * of this function to clean up the returned file*.
     */
    file* release()             { file* ret = file; file = null; return ret; }

    /** get wrapped file* without transfer of ownership.
     * @note ownership of the file* will remain with this class. use this only if the scope of the
     * cautofile outlives use of the passed pointer.
     */
    file* get() const           { return file; }

    /** return true if the wrapped file* is null, false otherwise.
     */
    bool isnull() const         { return (file == null); }

    //
    // stream subset
    //
    void settype(int n)          { ntype = n; }
    int gettype()                { return ntype; }
    void setversion(int n)       { nversion = n; }
    int getversion()             { return nversion; }
    void readversion()           { *this >> nversion; }
    void writeversion()          { *this << nversion; }

    cautofile& read(char* pch, size_t nsize)
    {
        if (!file)
            throw std::ios_base::failure("cautofile::read: file handle is null");
        if (fread(pch, 1, nsize, file) != nsize)
            throw std::ios_base::failure(feof(file) ? "cautofile::read: end of file" : "cautofile::read: fread failed");
        return (*this);
    }

    cautofile& write(const char* pch, size_t nsize)
    {
        if (!file)
            throw std::ios_base::failure("cautofile::write: file handle is null");
        if (fwrite(pch, 1, nsize, file) != nsize)
            throw std::ios_base::failure("cautofile::write: write failed");
        return (*this);
    }

    template<typename t>
    unsigned int getserializesize(const t& obj)
    {
        // tells the size of the object if serialized to this stream
        return ::getserializesize(obj, ntype, nversion);
    }

    template<typename t>
    cautofile& operator<<(const t& obj)
    {
        // serialize to this stream
        if (!file)
            throw std::ios_base::failure("cautofile::operator<<: file handle is null");
        ::serialize(*this, obj, ntype, nversion);
        return (*this);
    }

    template<typename t>
    cautofile& operator>>(t& obj)
    {
        // unserialize from this stream
        if (!file)
            throw std::ios_base::failure("cautofile::operator>>: file handle is null");
        ::unserialize(*this, obj, ntype, nversion);
        return (*this);
    }
};

/** non-refcounted raii wrapper around a file* that implements a ring buffer to
 *  deserialize from. it guarantees the ability to rewind a given number of bytes.
 *
 *  will automatically close the file when it goes out of scope if not null.
 *  if you need to close the file early, use file.fclose() instead of fclose(file).
 */
class cbufferedfile
{
private:
    // disallow copies
    cbufferedfile(const cbufferedfile&);
    cbufferedfile& operator=(const cbufferedfile&);

    int ntype;
    int nversion;

    file *src;            // source file
    uint64_t nsrcpos;     // how many bytes have been read from source
    uint64_t nreadpos;    // how many bytes have been read from this
    uint64_t nreadlimit;  // up to which position we're allowed to read
    uint64_t nrewind;     // how many bytes we guarantee to rewind
    std::vector<char> vchbuf; // the buffer

protected:
    // read data from the source to fill the buffer
    bool fill() {
        unsigned int pos = nsrcpos % vchbuf.size();
        unsigned int readnow = vchbuf.size() - pos;
        unsigned int navail = vchbuf.size() - (nsrcpos - nreadpos) - nrewind;
        if (navail < readnow)
            readnow = navail;
        if (readnow == 0)
            return false;
        size_t read = fread((void*)&vchbuf[pos], 1, readnow, src);
        if (read == 0) {
            throw std::ios_base::failure(feof(src) ? "cbufferedfile::fill: end of file" : "cbufferedfile::fill: fread failed");
        } else {
            nsrcpos += read;
            return true;
        }
    }

public:
    cbufferedfile(file *filein, uint64_t nbufsize, uint64_t nrewindin, int ntypein, int nversionin) :
        nsrcpos(0), nreadpos(0), nreadlimit((uint64_t)(-1)), nrewind(nrewindin), vchbuf(nbufsize, 0)
    {
        src = filein;
        ntype = ntypein;
        nversion = nversionin;
    }

    ~cbufferedfile()
    {
        fclose();
    }

    void fclose()
    {
        if (src) {
            ::fclose(src);
            src = null;
        }
    }

    // check whether we're at the end of the source file
    bool eof() const {
        return nreadpos == nsrcpos && feof(src);
    }

    // read a number of bytes
    cbufferedfile& read(char *pch, size_t nsize) {
        if (nsize + nreadpos > nreadlimit)
            throw std::ios_base::failure("read attempted past buffer limit");
        if (nsize + nrewind > vchbuf.size())
            throw std::ios_base::failure("read larger than buffer size");
        while (nsize > 0) {
            if (nreadpos == nsrcpos)
                fill();
            unsigned int pos = nreadpos % vchbuf.size();
            size_t nnow = nsize;
            if (nnow + pos > vchbuf.size())
                nnow = vchbuf.size() - pos;
            if (nnow + nreadpos > nsrcpos)
                nnow = nsrcpos - nreadpos;
            memcpy(pch, &vchbuf[pos], nnow);
            nreadpos += nnow;
            pch += nnow;
            nsize -= nnow;
        }
        return (*this);
    }

    // return the current reading position
    uint64_t getpos() {
        return nreadpos;
    }

    // rewind to a given reading position
    bool setpos(uint64_t npos) {
        nreadpos = npos;
        if (nreadpos + nrewind < nsrcpos) {
            nreadpos = nsrcpos - nrewind;
            return false;
        } else if (nreadpos > nsrcpos) {
            nreadpos = nsrcpos;
            return false;
        } else {
            return true;
        }
    }

    bool seek(uint64_t npos) {
        long nlongpos = npos;
        if (npos != (uint64_t)nlongpos)
            return false;
        if (fseek(src, nlongpos, seek_set))
            return false;
        nlongpos = ftell(src);
        nsrcpos = nlongpos;
        nreadpos = nlongpos;
        return true;
    }

    // prevent reading beyond a certain position
    // no argument removes the limit
    bool setlimit(uint64_t npos = (uint64_t)(-1)) {
        if (npos < nreadpos)
            return false;
        nreadlimit = npos;
        return true;
    }

    template<typename t>
    cbufferedfile& operator>>(t& obj) {
        // unserialize from this stream
        ::unserialize(*this, obj, ntype, nversion);
        return (*this);
    }

    // search for a given byte in the stream, and remain positioned on it
    void findbyte(char ch) {
        while (true) {
            if (nreadpos == nsrcpos)
                fill();
            if (vchbuf[nreadpos % vchbuf.size()] == ch)
                break;
            nreadpos++;
        }
    }
};

#endif // moorecoin_streams_h
