// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_init_h
#define moorecoin_init_h

#include <string>

class cscheduler;
class cwallet;

namespace boost
{
class thread_group;
} // namespace boost

extern cwallet* pwalletmain;

void startshutdown();
bool shutdownrequested();
void shutdown();
bool appinit2(boost::thread_group& threadgroup, cscheduler& scheduler);

/** the help message mode determines what help message to show */
enum helpmessagemode {
    hmm_moorecoind,
    hmm_moorecoin_qt
};

/** help for options shared between ui and daemon (for -help) */
std::string helpmessage(helpmessagemode mode);
/** returns licensing information (for -version) */
std::string licenseinfo();

#endif // moorecoin_init_h
