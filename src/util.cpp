// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "util.h"

#include "chainparamsbase.h"
#include "random.h"
#include "serialize.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "utiltime.h"

#include <stdarg.h>

#if (defined(__freebsd__) || defined(__openbsd__) || defined(__dragonfly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef win32
// for posix_fallocate
#ifdef __linux__

#ifdef _posix_c_source
#undef _posix_c_source
#endif

#define _posix_c_source 200112l

#endif // __linux__

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _msc_ver
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#ifdef _win32_winnt
#undef _win32_winnt
#endif
#define _win32_winnt 0x0501

#ifdef _win32_ie
#undef _win32_ie
#endif
#define _win32_ie 0x0501

#define win32_lean_and_mean 1
#ifndef nominmax
#define nominmax
#endif

#include <io.h> /* for _commit */
#include <shlobj.h>
#endif

#ifdef have_sys_prctl_h
#include <sys/prctl.h>
#endif

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>
#include <openssl/rand.h>

// work around clang compilation problem in boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// see also: http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=cannot_find_function
namespace boost {

    namespace program_options {
        std::string to_internal(const std::string&);
    }

} // namespace boost

using namespace std;

map<string, string> mapargs;
map<string, vector<string> > mapmultiargs;
bool fdebug = false;
bool fprinttoconsole = false;
bool fprinttodebuglog = true;
bool fdaemon = false;
bool fserver = false;
string strmiscwarning;
bool flogtimestamps = false;
bool flogips = false;
volatile bool freopendebuglog = false;
ctranslationinterface translationinterface;

/** init openssl library multithreading support */
static ccriticalsection** ppmutexopenssl;
void locking_callback(int mode, int i, const char* file, int line)
{
    if (mode & crypto_lock) {
        enter_critical_section(*ppmutexopenssl[i]);
    } else {
        leave_critical_section(*ppmutexopenssl[i]);
    }
}

// init
class cinit
{
public:
    cinit()
    {
        // init openssl library multithreading support
        ppmutexopenssl = (ccriticalsection**)openssl_malloc(crypto_num_locks() * sizeof(ccriticalsection*));
        for (int i = 0; i < crypto_num_locks(); i++)
            ppmutexopenssl[i] = new ccriticalsection();
        crypto_set_locking_callback(locking_callback);

#ifdef win32
        // seed openssl prng with current contents of the screen
        rand_screen();
#endif

        // seed openssl prng with performance counter
        randaddseed();
    }
    ~cinit()
    {
        // securely erase the memory used by the prng
        rand_cleanup();
        // shutdown openssl library multithreading support
        crypto_set_locking_callback(null);
        for (int i = 0; i < crypto_num_locks(); i++)
            delete ppmutexopenssl[i];
        openssl_free(ppmutexopenssl);
    }
}
instance_of_cinit;

/**
 * logprintf() has been broken a couple of times now
 * by well-meaning people adding mutexes in the most straightforward way.
 * it breaks because it may be called by global destructors during shutdown.
 * since the order of destruction of static/global objects is undefined,
 * defining a mutex as a global object doesn't work (the mutex gets
 * destroyed, and then some later destructor calls outputdebugstringf,
 * maybe indirectly, and you get a core dump at shutdown trying to lock
 * the mutex).
 */

static boost::once_flag debugprintinitflag = boost_once_init;
/**
 * we use boost::call_once() to make sure these are initialized
 * in a thread-safe manner the first time called:
 */
static file* fileout = null;
static boost::mutex* mutexdebuglog = null;

static void debugprintinit()
{
    assert(fileout == null);
    assert(mutexdebuglog == null);

    boost::filesystem::path pathdebug = getdatadir() / "debug.log";
    fileout = fopen(pathdebug.string().c_str(), "a");
    if (fileout) setbuf(fileout, null); // unbuffered

    mutexdebuglog = new boost::mutex();
}

bool logacceptcategory(const char* category)
{
    if (category != null)
    {
        if (!fdebug)
            return false;

        // give each thread quick access to -debug settings.
        // this helps prevent issues debugging global destructors,
        // where mapmultiargs might be deleted before another
        // global destructor calls logprint()
        static boost::thread_specific_ptr<set<string> > ptrcategory;
        if (ptrcategory.get() == null)
        {
            const vector<string>& categories = mapmultiargs["-debug"];
            ptrcategory.reset(new set<string>(categories.begin(), categories.end()));
            // thread_specific_ptr automatically deletes the set when the thread ends.
        }
        const set<string>& setcategories = *ptrcategory.get();

        // if not debugging everything and not debugging specific category, logprint does nothing.
        if (setcategories.count(string("")) == 0 &&
            setcategories.count(string("1")) == 0 &&
            setcategories.count(string(category)) == 0)
            return false;
    }
    return true;
}

int logprintstr(const std::string &str)
{
    int ret = 0; // returns total number of characters written
    if (fprinttoconsole)
    {
        // print to console
        ret = fwrite(str.data(), 1, str.size(), stdout);
        fflush(stdout);
    }
    else if (fprinttodebuglog && arebaseparamsconfigured())
    {
        static bool fstartednewline = true;
        boost::call_once(&debugprintinit, debugprintinitflag);

        if (fileout == null)
            return ret;

        boost::mutex::scoped_lock scoped_lock(*mutexdebuglog);

        // reopen the log file, if requested
        if (freopendebuglog) {
            freopendebuglog = false;
            boost::filesystem::path pathdebug = getdatadir() / "debug.log";
            if (freopen(pathdebug.string().c_str(),"a",fileout) != null)
                setbuf(fileout, null); // unbuffered
        }

        // debug print useful for profiling
        if (flogtimestamps && fstartednewline)
            ret += fprintf(fileout, "%s ", datetimestrformat("%y-%m-%d %h:%m:%s", gettime()).c_str());
        if (!str.empty() && str[str.size()-1] == '\n')
            fstartednewline = true;
        else
            fstartednewline = false;

        ret = fwrite(str.data(), 1, str.size(), fileout);
    }

    return ret;
}

static void interpretnegativesetting(string name, map<string, string>& mapsettingsret)
{
    // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
    if (name.find("-no") == 0)
    {
        std::string positive("-");
        positive.append(name.begin()+3, name.end());
        if (mapsettingsret.count(positive) == 0)
        {
            bool value = !getboolarg(name, false);
            mapsettingsret[positive] = (value ? "1" : "0");
        }
    }
}

void parseparameters(int argc, const char* const argv[])
{
    mapargs.clear();
    mapmultiargs.clear();

    for (int i = 1; i < argc; i++)
    {
        std::string str(argv[i]);
        std::string strvalue;
        size_t is_index = str.find('=');
        if (is_index != std::string::npos)
        {
            strvalue = str.substr(is_index+1);
            str = str.substr(0, is_index);
        }
#ifdef win32
        boost::to_lower(str);
        if (boost::algorithm::starts_with(str, "/"))
            str = "-" + str.substr(1);
#endif

        if (str[0] != '-')
            break;

        // interpret --foo as -foo.
        // if both --foo and -foo are set, the last takes effect.
        if (str.length() > 1 && str[1] == '-')
            str = str.substr(1);

        mapargs[str] = strvalue;
        mapmultiargs[str].push_back(strvalue);
    }

    // new 0.6 features:
    boost_foreach(const pairtype(string,string)& entry, mapargs)
    {
        // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
        interpretnegativesetting(entry.first, mapargs);
    }
}

std::string getarg(const std::string& strarg, const std::string& strdefault)
{
    if (mapargs.count(strarg))
        return mapargs[strarg];
    return strdefault;
}

int64_t getarg(const std::string& strarg, int64_t ndefault)
{
    if (mapargs.count(strarg))
        return atoi64(mapargs[strarg]);
    return ndefault;
}

bool getboolarg(const std::string& strarg, bool fdefault)
{
    if (mapargs.count(strarg))
    {
        if (mapargs[strarg].empty())
            return true;
        return (atoi(mapargs[strarg]) != 0);
    }
    return fdefault;
}

bool softsetarg(const std::string& strarg, const std::string& strvalue)
{
    if (mapargs.count(strarg))
        return false;
    mapargs[strarg] = strvalue;
    return true;
}

bool softsetboolarg(const std::string& strarg, bool fvalue)
{
    if (fvalue)
        return softsetarg(strarg, std::string("1"));
    else
        return softsetarg(strarg, std::string("0"));
}

static const int screenwidth = 79;
static const int optindent = 2;
static const int msgindent = 7;

std::string helpmessagegroup(const std::string &message) {
    return std::string(message) + std::string("\n\n");
}

std::string helpmessageopt(const std::string &option, const std::string &message) {
    return std::string(optindent,' ') + std::string(option) +
           std::string("\n") + std::string(msgindent,' ') +
           formatparagraph(message, screenwidth - msgindent, msgindent) +
           std::string("\n\n");
}

static std::string formatexception(const std::exception* pex, const char* pszthread)
{
#ifdef win32
    char pszmodule[max_path] = "";
    getmodulefilenamea(null, pszmodule, sizeof(pszmodule));
#else
    const char* pszmodule = "moorecoin";
#endif
    if (pex)
        return strprintf(
            "exception: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszmodule, pszthread);
    else
        return strprintf(
            "unknown exception       \n%s in %s       \n", pszmodule, pszthread);
}

void printexceptioncontinue(const std::exception* pex, const char* pszthread)
{
    std::string message = formatexception(pex, pszthread);
    logprintf("\n\n************************\n%s\n", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strmiscwarning = message;
}

boost::filesystem::path getdefaultdatadir()
{
    namespace fs = boost::filesystem;
    // windows < vista: c:\documents and settings\username\application data\moorecoin
    // windows >= vista: c:\users\username\appdata\roaming\moorecoin
    // mac: ~/library/application support/moorecoin
    // unix: ~/.moorecoin
#ifdef win32
    // windows
    return getspecialfolderpath(csidl_appdata) / "moorecoin";
#else
    fs::path pathret;
    char* pszhome = getenv("home");
    if (pszhome == null || strlen(pszhome) == 0)
        pathret = fs::path("/");
    else
        pathret = fs::path(pszhome);
#ifdef mac_osx
    // mac
    pathret /= "library/application support";
    trycreatedirectory(pathret);
    return pathret / "moorecoin";
#else
    // unix
    return pathret / ".moorecoin";
#endif
#endif
}

static boost::filesystem::path pathcached;
static boost::filesystem::path pathcachednetspecific;
static ccriticalsection cspathcached;

const boost::filesystem::path &getdatadir(bool fnetspecific)
{
    namespace fs = boost::filesystem;

    lock(cspathcached);

    fs::path &path = fnetspecific ? pathcachednetspecific : pathcached;

    // this can be called during exceptions by logprintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (mapargs.count("-datadir")) {
        path = fs::system_complete(mapargs["-datadir"]);
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = getdefaultdatadir();
    }
    if (fnetspecific)
        path /= baseparams().datadir();

    fs::create_directories(path);

    return path;
}

void cleardatadircache()
{
    pathcached = boost::filesystem::path();
    pathcachednetspecific = boost::filesystem::path();
}

boost::filesystem::path getconfigfile()
{
    boost::filesystem::path pathconfigfile(getarg("-conf", "moorecoin.conf"));
    if (!pathconfigfile.is_complete())
        pathconfigfile = getdatadir(false) / pathconfigfile;

    return pathconfigfile;
}

void readconfigfile(map<string, string>& mapsettingsret,
                    map<string, vector<string> >& mapmultisettingsret)
{
    boost::filesystem::ifstream streamconfig(getconfigfile());
    if (!streamconfig.good())
        return; // no moorecoin.conf file is ok

    set<string> setoptions;
    setoptions.insert("*");

    for (boost::program_options::detail::config_file_iterator it(streamconfig, setoptions), end; it != end; ++it)
    {
        // don't overwrite existing settings so command line settings override moorecoin.conf
        string strkey = string("-") + it->string_key;
        if (mapsettingsret.count(strkey) == 0)
        {
            mapsettingsret[strkey] = it->value[0];
            // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
            interpretnegativesetting(strkey, mapsettingsret);
        }
        mapmultisettingsret[strkey].push_back(it->value[0]);
    }
    // if datadir is changed in .conf file:
    cleardatadircache();
}

#ifndef win32
boost::filesystem::path getpidfile()
{
    boost::filesystem::path pathpidfile(getarg("-pid", "moorecoind.pid"));
    if (!pathpidfile.is_complete()) pathpidfile = getdatadir() / pathpidfile;
    return pathpidfile;
}

void createpidfile(const boost::filesystem::path &path, pid_t pid)
{
    file* file = fopen(path.string().c_str(), "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}
#endif

bool renameover(boost::filesystem::path src, boost::filesystem::path dest)
{
#ifdef win32
    return movefileexa(src.string().c_str(), dest.string().c_str(),
                       movefile_replace_existing) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* win32 */
}

/**
 * ignores exceptions thrown by boost's create_directory if the requested directory exists.
 * specifically handles case where path p exists, but it wasn't possible for the user to
 * write to the parent directory.
 */
bool trycreatedirectory(const boost::filesystem::path& p)
{
    try
    {
        return boost::filesystem::create_directory(p);
    } catch (const boost::filesystem::filesystem_error&) {
        if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p))
            throw;
    }

    // create_directory didn't create the directory, it had to have existed already
    return false;
}

void filecommit(file *fileout)
{
    fflush(fileout); // harmless if redundantly called
#ifdef win32
    handle hfile = (handle)_get_osfhandle(_fileno(fileout));
    flushfilebuffers(hfile);
#else
    #if defined(__linux__) || defined(__netbsd__)
    fdatasync(fileno(fileout));
    #elif defined(__apple__) && defined(f_fullfsync)
    fcntl(fileno(fileout), f_fullfsync, 0);
    #else
    fsync(fileno(fileout));
    #endif
#endif
}

bool truncatefile(file *file, unsigned int length) {
#if defined(win32)
    return _chsize(_fileno(file), length) == 0;
#else
    return ftruncate(fileno(file), length) == 0;
#endif
}

/**
 * this function tries to raise the file descriptor limit to the requested number.
 * it returns the actual file descriptor limit (which may be more or less than nminfd)
 */
int raisefiledescriptorlimit(int nminfd) {
#if defined(win32)
    return 2048;
#else
    struct rlimit limitfd;
    if (getrlimit(rlimit_nofile, &limitfd) != -1) {
        if (limitfd.rlim_cur < (rlim_t)nminfd) {
            limitfd.rlim_cur = nminfd;
            if (limitfd.rlim_cur > limitfd.rlim_max)
                limitfd.rlim_cur = limitfd.rlim_max;
            setrlimit(rlimit_nofile, &limitfd);
            getrlimit(rlimit_nofile, &limitfd);
        }
        return limitfd.rlim_cur;
    }
    return nminfd; // getrlimit failed, assume it's fine
#endif
}

/**
 * this function tries to make a particular range of a file allocated (corresponding to disk space)
 * it is advisory, and the range specified in the arguments will never contain live data
 */
void allocatefilerange(file *file, unsigned int offset, unsigned int length) {
#if defined(win32)
    // windows-specific version
    handle hfile = (handle)_get_osfhandle(_fileno(file));
    large_integer nfilesize;
    int64_t nendpos = (int64_t)offset + length;
    nfilesize.u.lowpart = nendpos & 0xffffffff;
    nfilesize.u.highpart = nendpos >> 32;
    setfilepointerex(hfile, nfilesize, 0, file_begin);
    setendoffile(hfile);
#elif defined(mac_osx)
    // osx specific version
    fstore_t fst;
    fst.fst_flags = f_allocatecontig;
    fst.fst_posmode = f_peofposmode;
    fst.fst_offset = 0;
    fst.fst_length = (off_t)offset + length;
    fst.fst_bytesalloc = 0;
    if (fcntl(fileno(file), f_preallocate, &fst) == -1) {
        fst.fst_flags = f_allocateall;
        fcntl(fileno(file), f_preallocate, &fst);
    }
    ftruncate(fileno(file), fst.fst_length);
#elif defined(__linux__)
    // version using posix_fallocate
    off_t nendpos = (off_t)offset + length;
    posix_fallocate(fileno(file), 0, nendpos);
#else
    // fallback version
    // todo: just write one byte per block
    static const char buf[65536] = {};
    fseek(file, offset, seek_set);
    while (length > 0) {
        unsigned int now = 65536;
        if (length < now)
            now = length;
        fwrite(buf, 1, now, file); // allowed to fail; this function is advisory anyway
        length -= now;
    }
#endif
}

void shrinkdebugfile()
{
    // scroll debug.log if it's getting too big
    boost::filesystem::path pathlog = getdatadir() / "debug.log";
    file* file = fopen(pathlog.string().c_str(), "r");
    if (file && boost::filesystem::file_size(pathlog) > 10 * 1000000)
    {
        // restart the file with some of the end
        std::vector <char> vch(200000,0);
        fseek(file, -((long)vch.size()), seek_end);
        int nbytes = fread(begin_ptr(vch), 1, vch.size(), file);
        fclose(file);

        file = fopen(pathlog.string().c_str(), "w");
        if (file)
        {
            fwrite(begin_ptr(vch), 1, nbytes, file);
            fclose(file);
        }
    }
    else if (file != null)
        fclose(file);
}

#ifdef win32
boost::filesystem::path getspecialfolderpath(int nfolder, bool fcreate)
{
    namespace fs = boost::filesystem;

    char pszpath[max_path] = "";

    if(shgetspecialfolderpatha(null, pszpath, nfolder, fcreate))
    {
        return fs::path(pszpath);
    }

    logprintf("shgetspecialfolderpatha() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

boost::filesystem::path gettemppath() {
#if boost_filesystem_version == 3
    return boost::filesystem::temp_directory_path();
#else
    // todo: remove when we don't support filesystem v2 anymore
    boost::filesystem::path path;
#ifdef win32
    char pszpath[max_path] = "";

    if (gettemppatha(max_path, pszpath))
        path = boost::filesystem::path(pszpath);
#else
    path = boost::filesystem::path("/tmp");
#endif
    if (path.empty() || !boost::filesystem::is_directory(path)) {
        logprintf("gettemppath(): failed to find temp path\n");
        return boost::filesystem::path("");
    }
    return path;
#endif
}

void runcommand(const std::string& strcommand)
{
    int nerr = ::system(strcommand.c_str());
    if (nerr)
        logprintf("runcommand error: system(%s) returned %d\n", strcommand, nerr);
}

void renamethread(const char* name)
{
#if defined(pr_set_name)
    // only the first 15 characters are used (16 - nul terminator)
    ::prctl(pr_set_name, name, 0, 0, 0);
#elif (defined(__freebsd__) || defined(__openbsd__) || defined(__dragonfly__))
    pthread_set_name_np(pthread_self(), name);

#elif defined(mac_osx)
    pthread_setname_np(name);
#else
    // prevent warnings for unused parameters...
    (void)name;
#endif
}

void setupenvironment()
{
    // on most posix systems (e.g. linux, but not bsd) the environment's locale
    // may be invalid, in which case the "c" locale is used as fallback.
#if !defined(win32) && !defined(mac_osx) && !defined(__freebsd__) && !defined(__openbsd__)
    try {
        std::locale(""); // raises a runtime error if current locale is invalid
    } catch (const std::runtime_error&) {
        setenv("lc_all", "c", 1);
    }
#endif
    // the path locale is lazy initialized and to avoid deinitialization errors
    // in multithreading environments, it is set explicitly by the main thread.
    // a dummy locale is used to extract the internal default locale, used by
    // boost::filesystem::path, which is then used to explicitly imbue the path.
    std::locale loc = boost::filesystem::path::imbue(std::locale::classic());
    boost::filesystem::path::imbue(loc);
}

void setthreadpriority(int npriority)
{
#ifdef win32
    setthreadpriority(getcurrentthread(), npriority);
#else // win32
#ifdef prio_thread
    setpriority(prio_thread, 0, npriority);
#else // prio_thread
    setpriority(prio_process, 0, npriority);
#endif // prio_thread
#endif // win32
}
