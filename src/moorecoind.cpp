// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "clientversion.h"
#include "rpcserver.h"
#include "init.h"
#include "main.h"
#include "noui.h"
#include "scheduler.h"
#include "util.h"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

/* introduction text for doxygen: */

/*! \mainpage developer documentation
 *
 * \section intro_sec introduction
 *
 * this is the developer documentation of the reference client for an experimental new digital currency called moorecoin (https://www.moorecoin.org/),
 * which enables instant payments to anyone, anywhere in the world. moorecoin uses peer-to-peer technology to operate
 * with no central authority: managing transactions and issuing money are carried out collectively by the network.
 *
 * the software is a community-driven open source project, released under the mit license.
 *
 * \section navigation
 * use the buttons <code>namespaces</code>, <code>classes</code> or <code>files</code> at the top of the page to start navigating the code.
 */

static bool fdaemon;

void waitforshutdown(boost::thread_group* threadgroup)
{
    bool fshutdown = shutdownrequested();
    // tell the main threads to shutdown.
    while (!fshutdown)
    {
        millisleep(200);
        fshutdown = shutdownrequested();
    }
    if (threadgroup)
    {
        threadgroup->interrupt_all();
        threadgroup->join_all();
    }
}

//////////////////////////////////////////////////////////////////////////////
//
// start
//
bool appinit(int argc, char* argv[])
{
    boost::thread_group threadgroup;
    cscheduler scheduler;

    bool fret = false;

    //
    // parameters
    //
    // if qt is used, parameters/moorecoin.conf are parsed in qt/moorecoin.cpp's main()
    parseparameters(argc, argv);

    // process help and version before taking care about datadir
    if (mapargs.count("-?") || mapargs.count("-help") || mapargs.count("-version"))
    {
        std::string strusage = _("moorecoin core daemon") + " " + _("version") + " " + formatfullversion() + "\n";

        if (mapargs.count("-version"))
        {
            strusage += licenseinfo();
        }
        else
        {
            strusage += "\n" + _("usage:") + "\n" +
                  "  moorecoind [options]                     " + _("start moorecoin core daemon") + "\n";

            strusage += "\n" + helpmessage(hmm_moorecoind);
        }

        fprintf(stdout, "%s", strusage.c_str());
        return false;
    }

    try
    {
        if (!boost::filesystem::is_directory(getdatadir(false)))
        {
            fprintf(stderr, "error: specified data directory \"%s\" does not exist.\n", mapargs["-datadir"].c_str());
            return false;
        }
        try
        {
            readconfigfile(mapargs, mapmultiargs);
        } catch (const std::exception& e) {
            fprintf(stderr,"error reading configuration file: %s\n", e.what());
            return false;
        }
        // check for -testnet or -regtest parameter (params() calls are only valid after this clause)
        if (!selectparamsfromcommandline()) {
            fprintf(stderr, "error: invalid combination of -regtest and -testnet.\n");
            return false;
        }

        // command-line rpc
        bool fcommandline = false;
        for (int i = 1; i < argc; i++)
            if (!isswitchchar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "moorecoin:"))
                fcommandline = true;

        if (fcommandline)
        {
            fprintf(stderr, "error: there is no rpc client functionality in moorecoind anymore. use the moorecoin-cli utility instead.\n");
            exit(1);
        }
#ifndef win32
        fdaemon = getboolarg("-daemon", false);
        if (fdaemon)
        {
            fprintf(stdout, "moorecoin server starting\n");

            // daemonize
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0) // parent process, pid is child process id
            {
                return true;
            }
            // child process falls through to rest of initialization

            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "error: setsid() returned %d errno %d\n", sid, errno);
        }
#endif
        softsetboolarg("-server", true);

        fret = appinit2(threadgroup, scheduler);
    }
    catch (const std::exception& e) {
        printexceptioncontinue(&e, "appinit()");
    } catch (...) {
        printexceptioncontinue(null, "appinit()");
    }

    if (!fret)
    {
        threadgroup.interrupt_all();
        // threadgroup.join_all(); was left out intentionally here, because we didn't re-test all of
        // the startup-failure cases to make sure they don't result in a hang due to some
        // thread-blocking-waiting-for-another-thread-during-startup case
    } else {
        waitforshutdown(&threadgroup);
    }
    shutdown();

    return fret;
}

int main(int argc, char* argv[])
{
    setupenvironment();

    // connect moorecoind signal handlers
    noui_connect();

    return (appinit(argc, argv) ? 0 : 1);
}
