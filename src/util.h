// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

/**
 * server/client environment: argument handling, config file parsing,
 * logging, thread wrappers
 */
#ifndef moorecoin_util_h
#define moorecoin_util_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "compat.h"
#include "tinyformat.h"
#include "utiltime.h"

#include <exception>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread/exceptions.hpp>

/** signals for translation. */
class ctranslationinterface
{
public:
    /** translate a message to the native language of the user. */
    boost::signals2::signal<std::string (const char* psz)> translate;
};

extern std::map<std::string, std::string> mapargs;
extern std::map<std::string, std::vector<std::string> > mapmultiargs;
extern bool fdebug;
extern bool fprinttoconsole;
extern bool fprinttodebuglog;
extern bool fserver;
extern std::string strmiscwarning;
extern bool flogtimestamps;
extern bool flogips;
extern volatile bool freopendebuglog;
extern ctranslationinterface translationinterface;

/**
 * translation function: call translate signal on ui interface, which returns a boost::optional result.
 * if no translation slot is registered, nothing is returned, and simply return the input.
 */
inline std::string _(const char* psz)
{
    boost::optional<std::string> rv = translationinterface.translate(psz);
    return rv ? (*rv) : psz;
}

void setupenvironment();

/** return true if log accepts specified category */
bool logacceptcategory(const char* category);
/** send a string to the log output */
int logprintstr(const std::string &str);

#define logprintf(...) logprint(null, __va_args__)

/**
 * when we switch to c++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define make_error_and_log_func(n)                                        \
    /**   print to debug.log if -debug=category switch is given or category is null. */ \
    template<tinyformat_argtypes(n)>                                          \
    static inline int logprint(const char* category, const char* format, tinyformat_varargs(n))  \
    {                                                                         \
        if(!logacceptcategory(category)) return 0;                            \
        return logprintstr(tfm::format(format, tinyformat_passargs(n))); \
    }                                                                         \
    /**   log error and return false */                                        \
    template<tinyformat_argtypes(n)>                                          \
    static inline bool error(const char* format, tinyformat_varargs(n))                     \
    {                                                                         \
        logprintstr("error: " + tfm::format(format, tinyformat_passargs(n)) + "\n"); \
        return false;                                                         \
    }

tinyformat_foreach_argnum(make_error_and_log_func)

/**
 * zero-arg versions of logging and error, these are not covered by
 * tinyformat_foreach_argnum
 */
static inline int logprint(const char* category, const char* format)
{
    if(!logacceptcategory(category)) return 0;
    return logprintstr(format);
}
static inline bool error(const char* format)
{
    logprintstr(std::string("error: ") + format + "\n");
    return false;
}

void printexceptioncontinue(const std::exception *pex, const char* pszthread);
void parseparameters(int argc, const char*const argv[]);
void filecommit(file *fileout);
bool truncatefile(file *file, unsigned int length);
int raisefiledescriptorlimit(int nminfd);
void allocatefilerange(file *file, unsigned int offset, unsigned int length);
bool renameover(boost::filesystem::path src, boost::filesystem::path dest);
bool trycreatedirectory(const boost::filesystem::path& p);
boost::filesystem::path getdefaultdatadir();
const boost::filesystem::path &getdatadir(bool fnetspecific = true);
void cleardatadircache();
boost::filesystem::path getconfigfile();
#ifndef win32
boost::filesystem::path getpidfile();
void createpidfile(const boost::filesystem::path &path, pid_t pid);
#endif
void readconfigfile(std::map<std::string, std::string>& mapsettingsret, std::map<std::string, std::vector<std::string> >& mapmultisettingsret);
#ifdef win32
boost::filesystem::path getspecialfolderpath(int nfolder, bool fcreate = true);
#endif
boost::filesystem::path gettemppath();
void shrinkdebugfile();
void runcommand(const std::string& strcommand);

inline bool isswitchchar(char c)
{
#ifdef win32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * return string argument or default value
 *
 * @param strarg argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string getarg(const std::string& strarg, const std::string& strdefault);

/**
 * return integer argument or default value
 *
 * @param strarg argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t getarg(const std::string& strarg, int64_t ndefault);

/**
 * return boolean argument or default value
 *
 * @param strarg argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool getboolarg(const std::string& strarg, bool fdefault);

/**
 * set an argument if it doesn't already have a value
 *
 * @param strarg argument to set (e.g. "-foo")
 * @param strvalue value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool softsetarg(const std::string& strarg, const std::string& strvalue);

/**
 * set a boolean argument if it doesn't already have a value
 *
 * @param strarg argument to set (e.g. "-foo")
 * @param fvalue value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool softsetboolarg(const std::string& strarg, bool fvalue);

/**
 * format a string to be used as group of options in help messages
 *
 * @param message group name (e.g. "rpc server options:")
 * @return the formatted string
 */
std::string helpmessagegroup(const std::string& message);

/**
 * format a string to be used as option description in help messages
 *
 * @param option option message (e.g. "-rpcuser=<user>")
 * @param message option description (e.g. "username for json-rpc connections")
 * @return the formatted string
 */
std::string helpmessageopt(const std::string& option, const std::string& message);

void setthreadpriority(int npriority);
void renamethread(const char* name);

/**
 * .. and a wrapper that just calls func once
 */
template <typename callable> void tracethread(const char* name,  callable func)
{
    std::string s = strprintf("moorecoin-%s", name);
    renamethread(s.c_str());
    try
    {
        logprintf("%s thread start\n", name);
        func();
        logprintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted&)
    {
        logprintf("%s thread interrupt\n", name);
        throw;
    }
    catch (const std::exception& e) {
        printexceptioncontinue(&e, name);
        throw;
    }
    catch (...) {
        printexceptioncontinue(null, name);
        throw;
    }
}

#endif // moorecoin_util_h
