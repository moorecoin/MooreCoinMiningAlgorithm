// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_clientversion_h
#define moorecoin_clientversion_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#else

/**
 * client versioning and copyright year
 */

//! these need to be macros, as clientversion.cpp's and moorecoin*-res.rc's voodoo requires it
#define client_version_major 0
#define client_version_minor 11
#define client_version_revision 99
#define client_version_build 0

//! set to true for release, false for prerelease or test build
#define client_version_is_release false

/**
 * copyright year (2009-this)
 * todo: update this when changing our copyright comments in the source
 */
#define copyright_year 2015

#endif //have_config_h

/**
 * converts the parameter x to a string after macro replacement on x has been performed.
 * don't merge these into one macro!
 */
#define stringize(x) do_stringize(x)
#define do_stringize(x) #x

//! copyright string used in windows .rc files
#define copyright_str "2009-" stringize(copyright_year) " the moorecoin core developers"

/**
 * moorecoind-res.rc includes this file, but it cannot cope with real c++ code.
 * windres_preproc is defined to indicate that its pre-processor is running.
 * anything other than a define should be guarded below.
 */

#if !defined(windres_preproc)

#include <string>
#include <vector>

static const int client_version =
                           1000000 * client_version_major
                         +   10000 * client_version_minor
                         +     100 * client_version_revision
                         +       1 * client_version_build;

extern const std::string client_name;
extern const std::string client_build;
extern const std::string client_date;


std::string formatfullversion();
std::string formatsubversion(const std::string& name, int nclientversion, const std::vector<std::string>& comments);

#endif // windres_preproc

#endif // moorecoin_clientversion_h
