// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#define boost_test_module moorecoin test suite

#include "test_moorecoin.h"

#include "key.h"
#include "main.h"
#include "random.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#ifdef enable_wallet
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

cclientuiinterface uiinterface; // declared but not defined in ui_interface.h
cwallet* pwalletmain;

extern bool fprinttoconsole;
extern void noui_connect();

basictestingsetup::basictestingsetup()
{
        ecc_start();
        setupenvironment();
        fprinttodebuglog = false; // don't want to write to debug.log file
        fcheckblockindex = true;
        selectparams(cbasechainparams::main);
}
basictestingsetup::~basictestingsetup()
{
        ecc_stop();
}

testingsetup::testingsetup()
{
#ifdef enable_wallet
        bitdb.makemock();
#endif
        cleardatadircache();
        pathtemp = gettemppath() / strprintf("test_moorecoin_%lu_%i", (unsigned long)gettime(), (int)(getrand(100000)));
        boost::filesystem::create_directories(pathtemp);
        mapargs["-datadir"] = pathtemp.string();
        pblocktree = new cblocktreedb(1 << 20, true);
        pcoinsdbview = new ccoinsviewdb(1 << 23, true);
        pcoinstip = new ccoinsviewcache(pcoinsdbview);
        initblockindex();
#ifdef enable_wallet
        bool ffirstrun;
        pwalletmain = new cwallet("wallet.dat");
        pwalletmain->loadwallet(ffirstrun);
        registervalidationinterface(pwalletmain);
#endif
        nscriptcheckthreads = 3;
        for (int i=0; i < nscriptcheckthreads-1; i++)
            threadgroup.create_thread(&threadscriptcheck);
        registernodesignals(getnodesignals());
}

testingsetup::~testingsetup()
{
        unregisternodesignals(getnodesignals());
        threadgroup.interrupt_all();
        threadgroup.join_all();
#ifdef enable_wallet
        unregistervalidationinterface(pwalletmain);
        delete pwalletmain;
        pwalletmain = null;
#endif
        unloadblockindex();
        delete pcoinstip;
        delete pcoinsdbview;
        delete pblocktree;
#ifdef enable_wallet
        bitdb.flush(true);
        bitdb.reset();
#endif
        boost::filesystem::remove_all(pathtemp);
}

void shutdown(void* parg)
{
  exit(0);
}

void startshutdown()
{
  exit(0);
}

bool shutdownrequested()
{
  return false;
}
