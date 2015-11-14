// this file contains source that originates from:
// http://code.google.com/p/leveldbwin/source/browse/trunk/win32_impl_src/env_win32.h
// http://code.google.com/p/leveldbwin/source/browse/trunk/win32_impl_src/port_win32.cc
// those files dont' have any explict license headers but the 
// project (http://code.google.com/p/leveldbwin/) lists the 'new bsd license'
// as the license.
#if defined(leveldb_platform_windows)
#include <map>


#include "leveldb/env.h"

#include "port/port.h"
#include "leveldb/slice.h"
#include "util/logging.h"

#include <shlwapi.h>
#include <process.h>
#include <cstring>
#include <stdio.h>
#include <errno.h>
#include <io.h>
#include <algorithm>

#ifdef max
#undef max
#endif

#ifndef va_copy
#define va_copy(d,s) ((d) = (s))
#endif

#if defined deletefile
#undef deletefile
#endif

//declarations
namespace leveldb
{

namespace win32
{

#define disallow_copy_and_assign(typename) \
  typename(const typename&);               \
  void operator=(const typename&)

std::string getcurrentdir();
std::wstring getcurrentdirw();

static const std::string currentdir = getcurrentdir();
static const std::wstring currentdirw = getcurrentdirw();

std::string& modifypath(std::string& path);
std::wstring& modifypath(std::wstring& path);

std::string getlasterrsz();
std::wstring getlasterrszw();

size_t getpagesize();

typedef void (*scheduleproc)(void*) ;

struct workitemwrapper
{
    workitemwrapper(scheduleproc proc_,void* content_);
    scheduleproc proc;
    void* pcontent;
};

dword winapi workitemwrapperproc(lpvoid pcontent);

class win32sequentialfile : public sequentialfile
{
public:
    friend class win32env;
    virtual ~win32sequentialfile();
    virtual status read(size_t n, slice* result, char* scratch);
    virtual status skip(uint64_t n);
    bool isenable();
private:
    bool _init();
    void _cleanup();
    win32sequentialfile(const std::string& fname);
    std::string _filename;
    ::handle _hfile;
    disallow_copy_and_assign(win32sequentialfile);
};

class win32randomaccessfile : public randomaccessfile
{
public:
    friend class win32env;
    virtual ~win32randomaccessfile();
    virtual status read(uint64_t offset, size_t n, slice* result,char* scratch) const;
    bool isenable();
private:
    bool _init(lpcwstr path);
    void _cleanup();
    win32randomaccessfile(const std::string& fname);
    handle _hfile;
    const std::string _filename;
    disallow_copy_and_assign(win32randomaccessfile);
};

class win32mapfile : public writablefile
{
public:
    win32mapfile(const std::string& fname);

    ~win32mapfile();
    virtual status append(const slice& data);
    virtual status close();
    virtual status flush();
    virtual status sync();
    bool isenable();
private:
    std::string _filename;
    handle _hfile;
    size_t _page_size;
    size_t _map_size;       // how much extra memory to map at a time
    char* _base;            // the mapped region
    handle _base_handle;	
    char* _limit;           // limit of the mapped region
    char* _dst;             // where to write next  (in range [base_,limit_])
    char* _last_sync;       // where have we synced up to
    uint64_t _file_offset;  // offset of base_ in file
    //large_integer file_offset_;
    // have we done an munmap of unsynced data?
    bool _pending_sync;

    // roundup x to a multiple of y
    static size_t _roundup(size_t x, size_t y);
    size_t _truncatetopageboundary(size_t s);
    bool _unmapcurrentregion();
    bool _mapnewregion();
    disallow_copy_and_assign(win32mapfile);
    bool _init(lpcwstr path);
};

class win32filelock : public filelock
{
public:
    friend class win32env;
    virtual ~win32filelock();
    bool isenable();
private:
    bool _init(lpcwstr path);
    void _cleanup();
    win32filelock(const std::string& fname);
    handle _hfile;
    std::string _filename;
    disallow_copy_and_assign(win32filelock);
};

class win32logger : public logger
{
public: 
    friend class win32env;
    virtual ~win32logger();
    virtual void logv(const char* format, va_list ap);
private:
    explicit win32logger(writablefile* pfile);
    writablefile* _pfileproxy;
    disallow_copy_and_assign(win32logger);
};

class win32env : public env
{
public:
    win32env();
    virtual ~win32env();
    virtual status newsequentialfile(const std::string& fname,
        sequentialfile** result);

    virtual status newrandomaccessfile(const std::string& fname,
        randomaccessfile** result);
    virtual status newwritablefile(const std::string& fname,
        writablefile** result);

    virtual bool fileexists(const std::string& fname);

    virtual status getchildren(const std::string& dir,
        std::vector<std::string>* result);

    virtual status deletefile(const std::string& fname);

    virtual status createdir(const std::string& dirname);

    virtual status deletedir(const std::string& dirname);

    virtual status getfilesize(const std::string& fname, uint64_t* file_size);

    virtual status renamefile(const std::string& src,
        const std::string& target);

    virtual status lockfile(const std::string& fname, filelock** lock);

    virtual status unlockfile(filelock* lock);

    virtual void schedule(
        void (*function)(void* arg),
        void* arg);

    virtual void startthread(void (*function)(void* arg), void* arg);

    virtual status gettestdirectory(std::string* path);

    //virtual void logv(writablefile* log, const char* format, va_list ap);

    virtual status newlogger(const std::string& fname, logger** result);

    virtual uint64_t nowmicros();

    virtual void sleepformicroseconds(int micros);
};

void towidepath(const std::string& value, std::wstring& target) {
	wchar_t buffer[max_path];
	multibytetowidechar(cp_acp, 0, value.c_str(), -1, buffer, max_path);
	target = buffer;
}

void tonarrowpath(const std::wstring& value, std::string& target) {
	char buffer[max_path];
	widechartomultibyte(cp_acp, 0, value.c_str(), -1, buffer, max_path, null, null);
	target = buffer;
}

std::string getcurrentdir()
{
    char path[max_path];
    ::getmodulefilenamea(::getmodulehandlea(null),path,max_path);
    *strrchr(path,'\\') = 0;
    return std::string(path);
}

std::wstring getcurrentdirw()
{
    wchar path[max_path];
    ::getmodulefilenamew(::getmodulehandlew(null),path,max_path);
    *wcsrchr(path,l'\\') = 0;
    return std::wstring(path);
}

std::string& modifypath(std::string& path)
{
    if(path[0] == '/' || path[0] == '\\'){
        path = currentdir + path;
    }
    std::replace(path.begin(),path.end(),'/','\\');

    return path;
}

std::wstring& modifypath(std::wstring& path)
{
    if(path[0] == l'/' || path[0] == l'\\'){
        path = currentdirw + path;
    }
    std::replace(path.begin(),path.end(),l'/',l'\\');
    return path;
}

std::string getlasterrsz()
{
    lpwstr lpmsgbuf;
    formatmessagew( 
        format_message_allocate_buffer | 
        format_message_from_system | 
        format_message_ignore_inserts,
        null,
        getlasterror(),
        0, // default language
        (lpwstr) &lpmsgbuf,
        0,
        null 
        );
    std::string err;
	tonarrowpath(lpmsgbuf, err); 
    localfree( lpmsgbuf );
    return err;
}

std::wstring getlasterrszw()
{
    lpvoid lpmsgbuf;
    formatmessagew( 
        format_message_allocate_buffer | 
        format_message_from_system | 
        format_message_ignore_inserts,
        null,
        getlasterror(),
        0, // default language
        (lpwstr) &lpmsgbuf,
        0,
        null 
        );
    std::wstring err = (lpcwstr)lpmsgbuf;
    localfree(lpmsgbuf);
    return err;
}

workitemwrapper::workitemwrapper( scheduleproc proc_,void* content_ ) :
    proc(proc_),pcontent(content_)
{

}

dword winapi workitemwrapperproc(lpvoid pcontent)
{
    workitemwrapper* item = static_cast<workitemwrapper*>(pcontent);
    scheduleproc tempproc = item->proc;
    void* arg = item->pcontent;
    delete item;
    tempproc(arg);
    return 0;
}

size_t getpagesize()
{
    system_info si;
    getsysteminfo(&si);
    return std::max(si.dwpagesize,si.dwallocationgranularity);
}

const size_t g_pagesize = getpagesize();


win32sequentialfile::win32sequentialfile( const std::string& fname ) :
    _filename(fname),_hfile(null)
{
    _init();
}

win32sequentialfile::~win32sequentialfile()
{
    _cleanup();
}

status win32sequentialfile::read( size_t n, slice* result, char* scratch )
{
    status sret;
    dword hasread = 0;
    if(_hfile && readfile(_hfile,scratch,n,&hasread,null) ){
        *result = slice(scratch,hasread);
    } else {
        sret = status::ioerror(_filename, win32::getlasterrsz() );
    }
    return sret;
}

status win32sequentialfile::skip( uint64_t n )
{
    status sret;
    large_integer move,nowpointer;
    move.quadpart = n;
    if(!setfilepointerex(_hfile,move,&nowpointer,file_current)){
        sret = status::ioerror(_filename,win32::getlasterrsz());
    }
    return sret;
}

bool win32sequentialfile::isenable()
{
    return _hfile ? true : false;
}

bool win32sequentialfile::_init()
{
	std::wstring path;
	towidepath(_filename, path);
	_hfile = createfilew(path.c_str(),
                         generic_read,
                         file_share_read,
                         null,
                         open_existing,
                         file_attribute_normal,
                         null);
    return _hfile ? true : false;
}

void win32sequentialfile::_cleanup()
{
    if(_hfile){
        closehandle(_hfile);
        _hfile = null;
    }
}

win32randomaccessfile::win32randomaccessfile( const std::string& fname ) :
    _filename(fname),_hfile(null)
{
	std::wstring path;
	towidepath(fname, path);
    _init( path.c_str() );
}

win32randomaccessfile::~win32randomaccessfile()
{
    _cleanup();
}

status win32randomaccessfile::read(uint64_t offset,size_t n,slice* result,char* scratch) const
{
    status sret;
    overlapped ol = {0};
    zeromemory(&ol,sizeof(ol));
    ol.offset = (dword)offset;
    ol.offsethigh = (dword)(offset >> 32);
    dword hasread = 0;
    if(!readfile(_hfile,scratch,n,&hasread,&ol))
        sret = status::ioerror(_filename,win32::getlasterrsz());
    else
        *result = slice(scratch,hasread);
    return sret;
}

bool win32randomaccessfile::_init( lpcwstr path )
{
    bool bret = false;
    if(!_hfile)
        _hfile = ::createfilew(path,generic_read,file_share_read,null,open_existing,
        file_attribute_normal | file_flag_random_access,null);
    if(!_hfile || _hfile == invalid_handle_value )
        _hfile = null;
    else
        bret = true;
    return bret;
}

bool win32randomaccessfile::isenable()
{
    return _hfile ? true : false;
}

void win32randomaccessfile::_cleanup()
{
    if(_hfile){
        ::closehandle(_hfile);
        _hfile = null;
    }
}

size_t win32mapfile::_roundup( size_t x, size_t y )
{
    return ((x + y - 1) / y) * y;
}

size_t win32mapfile::_truncatetopageboundary( size_t s )
{
    s -= (s & (_page_size - 1));
    assert((s % _page_size) == 0);
    return s;
}

bool win32mapfile::_unmapcurrentregion()
{
    bool result = true;
    if (_base != null) {
        if (_last_sync < _limit) {
            // defer syncing this data until next sync() call, if any
            _pending_sync = true;
        }
        if (!unmapviewoffile(_base) || !closehandle(_base_handle))
            result = false;
        _file_offset += _limit - _base;
        _base = null;
        _base_handle = null;
        _limit = null;
        _last_sync = null;
        _dst = null;
        // increase the amount we map the next time, but capped at 1mb
        if (_map_size < (1<<20)) {
            _map_size *= 2;
        }
    }
    return result;
}

bool win32mapfile::_mapnewregion()
{
    assert(_base == null);
    //long newsizehigh = (long)((file_offset_ + map_size_) >> 32);
    //long newsizelow = (long)((file_offset_ + map_size_) & 0xffffffff);
    dword off_hi = (dword)(_file_offset >> 32);
    dword off_lo = (dword)(_file_offset & 0xffffffff);
    large_integer newsize;
    newsize.quadpart = _file_offset + _map_size;
    setfilepointerex(_hfile, newsize, null, file_begin);
    setendoffile(_hfile);

    _base_handle = createfilemappinga(
        _hfile,
        null,
        page_readwrite,
        0,
        0,
        0);
    if (_base_handle != null) {
        _base = (char*) mapviewoffile(_base_handle,
            file_map_all_access,
            off_hi,
            off_lo,
            _map_size);
        if (_base != null) {
            _limit = _base + _map_size;
            _dst = _base;
            _last_sync = _base;
            return true;
        }
    }
    return false;
}

win32mapfile::win32mapfile( const std::string& fname) :
    _filename(fname),
    _hfile(null),
    _page_size(win32::g_pagesize),
    _map_size(_roundup(65536, win32::g_pagesize)),
    _base(null),
    _base_handle(null),
    _limit(null),
    _dst(null),
    _last_sync(null),
    _file_offset(0),
    _pending_sync(false)
{
	std::wstring path;
	towidepath(fname, path);
    _init(path.c_str());
    assert((win32::g_pagesize & (win32::g_pagesize - 1)) == 0);
}

status win32mapfile::append( const slice& data )
{
    const char* src = data.data();
    size_t left = data.size();
    status s;
    while (left > 0) {
        assert(_base <= _dst);
        assert(_dst <= _limit);
        size_t avail = _limit - _dst;
        if (avail == 0) {
            if (!_unmapcurrentregion() ||
                !_mapnewregion()) {
                    return status::ioerror("winmmapfile.append::unmapcurrentregion or mapnewregion: ", win32::getlasterrsz());
            }
        }
        size_t n = (left <= avail) ? left : avail;
        memcpy(_dst, src, n);
        _dst += n;
        src += n;
        left -= n;
    }
    return s;
}

status win32mapfile::close()
{
    status s;
    size_t unused = _limit - _dst;
    if (!_unmapcurrentregion()) {
        s = status::ioerror("winmmapfile.close::unmapcurrentregion: ",win32::getlasterrsz());
    } else if (unused > 0) {
        // trim the extra space at the end of the file
        large_integer newsize;
        newsize.quadpart = _file_offset - unused;
        if (!setfilepointerex(_hfile, newsize, null, file_begin)) {
            s = status::ioerror("winmmapfile.close::setfilepointer: ",win32::getlasterrsz());
        } else 
            setendoffile(_hfile);
    }
    if (!closehandle(_hfile)) {
        if (s.ok()) {
            s = status::ioerror("winmmapfile.close::closehandle: ", win32::getlasterrsz());
        }
    }
    _hfile = invalid_handle_value;
    _base = null;
    _base_handle = null;
    _limit = null;

    return s;
}

status win32mapfile::sync()
{
    status s;
    if (_pending_sync) {
        // some unmapped data was not synced
        _pending_sync = false;
        if (!flushfilebuffers(_hfile)) {
            s = status::ioerror("winmmapfile.sync::flushfilebuffers: ",win32::getlasterrsz());
        }
    }
    if (_dst > _last_sync) {
        // find the beginnings of the pages that contain the first and last
        // bytes to be synced.
        size_t p1 = _truncatetopageboundary(_last_sync - _base);
        size_t p2 = _truncatetopageboundary(_dst - _base - 1);
        _last_sync = _dst;
        if (!flushviewoffile(_base + p1, p2 - p1 + _page_size)) {
            s = status::ioerror("winmmapfile.sync::flushviewoffile: ",win32::getlasterrsz());
        }
    }
    return s;
}

status win32mapfile::flush()
{
    return status::ok();
}

win32mapfile::~win32mapfile()
{
    if (_hfile != invalid_handle_value) { 
        win32mapfile::close();
    }
}

bool win32mapfile::_init( lpcwstr path )
{
    dword flag = pathfileexistsw(path) ? open_existing : create_always;
    _hfile = createfilew(path,
                         generic_read | generic_write,
                         file_share_read|file_share_delete|file_share_write,
                         null,
                         flag,
                         file_attribute_normal,
                         null);
    if(!_hfile || _hfile == invalid_handle_value)
        return false;
    else
        return true;
}

bool win32mapfile::isenable()
{
    return _hfile ? true : false;
}

win32filelock::win32filelock( const std::string& fname ) :
    _hfile(null),_filename(fname)
{
	std::wstring path;
	towidepath(fname, path);
	_init(path.c_str());
}

win32filelock::~win32filelock()
{
    _cleanup();
}

bool win32filelock::_init( lpcwstr path )
{
    bool bret = false;
    if(!_hfile)
        _hfile = ::createfilew(path,0,0,null,create_always,file_attribute_normal,null);
    if(!_hfile || _hfile == invalid_handle_value ){
        _hfile = null;
    }
    else
        bret = true;
    return bret;
}

void win32filelock::_cleanup()
{
    ::closehandle(_hfile);
    _hfile = null;
}

bool win32filelock::isenable()
{
    return _hfile ? true : false;
}

win32logger::win32logger(writablefile* pfile) : _pfileproxy(pfile)
{
    assert(_pfileproxy);
}

win32logger::~win32logger()
{
    if(_pfileproxy)
        delete _pfileproxy;
}

void win32logger::logv( const char* format, va_list ap )
{
    uint64_t thread_id = ::getcurrentthreadid();

    // we try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
        char* base;
        int bufsize;
        if (iter == 0) {
            bufsize = sizeof(buffer);
            base = buffer;
        } else {
            bufsize = 30000;
            base = new char[bufsize];
        }
        char* p = base;
        char* limit = base + bufsize;

        systemtime st;
        getlocaltime(&st);
        p += snprintf(p, limit - p,
            "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
            int(st.wyear),
            int(st.wmonth),
            int(st.wday),
            int(st.whour),
            int(st.wminute),
            int(st.wminute),
            int(st.wmilliseconds),
            static_cast<long long unsigned int>(thread_id));

        // print the message
        if (p < limit) {
            va_list backup_ap;
            va_copy(backup_ap, ap);
            p += vsnprintf(p, limit - p, format, backup_ap);
            va_end(backup_ap);
        }

        // truncate to available space if necessary
        if (p >= limit) {
            if (iter == 0) {
                continue;       // try again with larger buffer
            } else {
                p = limit - 1;
            }
        }

        // add newline if necessary
        if (p == base || p[-1] != '\n') {
            *p++ = '\n';
        }

        assert(p <= limit);
        dword haswritten = 0;
        if(_pfileproxy){
            _pfileproxy->append(slice(base, p - base));
            _pfileproxy->flush();
        }
        if (base != buffer) {
            delete[] base;
        }
        break;
    }
}

bool win32env::fileexists(const std::string& fname)
{
	std::string path = fname;
    std::wstring wpath;
	towidepath(modifypath(path), wpath);
    return ::pathfileexistsw(wpath.c_str()) ? true : false;
}

status win32env::getchildren(const std::string& dir, std::vector<std::string>* result)
{
    status sret;
    ::win32_find_dataw wfd;
    std::string path = dir;
    modifypath(path);
    path += "\\*.*";
	std::wstring wpath;
	towidepath(path, wpath);

	::handle hfind = ::findfirstfilew(wpath.c_str() ,&wfd);
    if(hfind && hfind != invalid_handle_value){
        bool hasnext = true;
        std::string child;
        while(hasnext){
            tonarrowpath(wfd.cfilename, child); 
            if(child != ".." && child != ".")  {
                result->push_back(child);
            }
            hasnext = ::findnextfilew(hfind,&wfd);
        }
        ::findclose(hfind);
    }
    else
        sret = status::ioerror(dir,"could not get children.");
    return sret;
}

void win32env::sleepformicroseconds( int micros )
{
    ::sleep((micros + 999) /1000);
}


status win32env::deletefile( const std::string& fname )
{
    status sret;
    std::string path = fname;
    std::wstring wpath;
	towidepath(modifypath(path), wpath);

    if(!::deletefilew(wpath.c_str())) {
        sret = status::ioerror(path, "could not delete file.");
    }
    return sret;
}

status win32env::getfilesize( const std::string& fname, uint64_t* file_size )
{
    status sret;
    std::string path = fname;
    std::wstring wpath;
	towidepath(modifypath(path), wpath);

    handle file = ::createfilew(wpath.c_str(),
        generic_read,file_share_read,null,open_existing,file_attribute_normal,null);
    large_integer li;
    if(::getfilesizeex(file,&li)){
        *file_size = (uint64_t)li.quadpart;
    }else
        sret = status::ioerror(path,"could not get the file size.");
    closehandle(file);
    return sret;
}

status win32env::renamefile( const std::string& src, const std::string& target )
{
    status sret;
    std::string src_path = src;
    std::wstring wsrc_path;
	towidepath(modifypath(src_path), wsrc_path);
	std::string target_path = target;
    std::wstring wtarget_path;
	towidepath(modifypath(target_path), wtarget_path);

    if(!movefilew(wsrc_path.c_str(), wtarget_path.c_str() ) ){
        dword err = getlasterror();
        if(err == 0x000000b7){
            if(!::deletefilew(wtarget_path.c_str() ) )
                sret = status::ioerror(src, "could not rename file.");
			else if(!::movefilew(wsrc_path.c_str(),
                                 wtarget_path.c_str() ) )
                sret = status::ioerror(src, "could not rename file.");    
        }
    }
    return sret;
}

status win32env::lockfile( const std::string& fname, filelock** lock )
{
    status sret;
    std::string path = fname;
    modifypath(path);
    win32filelock* _lock = new win32filelock(path);
    if(!_lock->isenable()){
        delete _lock;
        *lock = null;
        sret = status::ioerror(path, "could not lock file.");
    }
    else
        *lock = _lock;
    return sret;
}

status win32env::unlockfile( filelock* lock )
{
    status sret;
    delete lock;
    return sret;
}

void win32env::schedule( void (*function)(void* arg), void* arg )
{
    queueuserworkitem(win32::workitemwrapperproc,
                      new win32::workitemwrapper(function,arg),
                      wt_executedefault);
}

void win32env::startthread( void (*function)(void* arg), void* arg )
{
    ::_beginthread(function,0,arg);
}

status win32env::gettestdirectory( std::string* path )
{
    status sret;
    wchar temppath[max_path];
    ::gettemppathw(max_path,temppath);
	tonarrowpath(temppath, *path);
    path->append("leveldb\\test\\");
    modifypath(*path);
    return sret;
}

uint64_t win32env::nowmicros()
{
#ifndef use_vista_api
#define gettickcount64 gettickcount
#endif
    return (uint64_t)(gettickcount64()*1000);
}

static status createdirinner( const std::string& dirname )
{
    status sret;
    dword attr = ::getfileattributes(dirname.c_str());
    if (attr == invalid_file_attributes) { // doesn't exist:
      std::size_t slash = dirname.find_last_of("\\");
      if (slash != std::string::npos){
	sret = createdirinner(dirname.substr(0, slash));
	if (!sret.ok()) return sret;
      }
      bool result = ::createdirectory(dirname.c_str(), null);
      if (result == false) {
	sret = status::ioerror(dirname, "could not create directory.");
	return sret;
      }
    }
    return sret;
}

status win32env::createdir( const std::string& dirname )
{
    std::string path = dirname;
    if(path[path.length() - 1] != '\\'){
        path += '\\';
    }
    modifypath(path);

    return createdirinner(path);
}

status win32env::deletedir( const std::string& dirname )
{
    status sret;
    std::wstring path;
	towidepath(dirname, path);
    modifypath(path);
    if(!::removedirectoryw( path.c_str() ) ){
        sret = status::ioerror(dirname, "could not delete directory.");
    }
    return sret;
}

status win32env::newsequentialfile( const std::string& fname, sequentialfile** result )
{
    status sret;
    std::string path = fname;
    modifypath(path);
    win32sequentialfile* pfile = new win32sequentialfile(path);
    if(pfile->isenable()){
        *result = pfile;
    }else {
        delete pfile;
        sret = status::ioerror(path, win32::getlasterrsz());
    }
    return sret;
}

status win32env::newrandomaccessfile( const std::string& fname, randomaccessfile** result )
{
    status sret;
    std::string path = fname;
    win32randomaccessfile* pfile = new win32randomaccessfile(modifypath(path));
    if(!pfile->isenable()){
        delete pfile;
        *result = null;
        sret = status::ioerror(path, win32::getlasterrsz());
    }else
        *result = pfile;
    return sret;
}

status win32env::newlogger( const std::string& fname, logger** result )
{
    status sret;
    std::string path = fname;
    win32mapfile* pmapfile = new win32mapfile(modifypath(path));
    if(!pmapfile->isenable()){
        delete pmapfile;
        *result = null;
        sret = status::ioerror(path,"could not create a logger.");
    }else
        *result = new win32logger(pmapfile);
    return sret;
}

status win32env::newwritablefile( const std::string& fname, writablefile** result )
{
    status sret;
    std::string path = fname;
    win32mapfile* pfile = new win32mapfile(modifypath(path));
    if(!pfile->isenable()){
        *result = null;
        sret = status::ioerror(fname,win32::getlasterrsz());
    }else
        *result = pfile;
    return sret;
}

win32env::win32env()
{

}

win32env::~win32env()
{

}


}  // win32 namespace

static port::oncetype once = leveldb_once_init;
static env* default_env;
static void initdefaultenv() { default_env = new win32::win32env(); }

env* env::default() {
  port::initonce(&once, initdefaultenv);
  return default_env;
}

}  // namespace leveldb

#endif // defined(leveldb_platform_windows)
