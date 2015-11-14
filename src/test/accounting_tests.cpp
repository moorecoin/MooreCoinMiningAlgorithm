// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"
#include "wallet/walletdb.h"

#include "test/test_moorecoin.h"

#include <stdint.h>

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

extern cwallet* pwalletmain;

boost_fixture_test_suite(accounting_tests, testingsetup)

static void
getresults(cwalletdb& walletdb, std::map<camount, caccountingentry>& results)
{
    std::list<caccountingentry> aes;

    results.clear();
    boost_check(walletdb.reordertransactions(pwalletmain) == db_load_ok);
    walletdb.listaccountcreditdebit("", aes);
    boost_foreach(caccountingentry& ae, aes)
    {
        results[ae.norderpos] = ae;
    }
}

boost_auto_test_case(acc_orderupgrade)
{
    cwalletdb walletdb(pwalletmain->strwalletfile);
    std::vector<cwallettx*> vpwtx;
    cwallettx wtx;
    caccountingentry ae;
    std::map<camount, caccountingentry> results;

    lock(pwalletmain->cs_wallet);

    ae.straccount = "";
    ae.ncreditdebit = 1;
    ae.ntime = 1333333333;
    ae.strotheraccount = "b";
    ae.strcomment = "";
    walletdb.writeaccountingentry(ae);

    wtx.mapvalue["comment"] = "z";
    pwalletmain->addtowallet(wtx, false, &walletdb);
    vpwtx.push_back(&pwalletmain->mapwallet[wtx.gethash()]);
    vpwtx[0]->ntimereceived = (unsigned int)1333333335;
    vpwtx[0]->norderpos = -1;

    ae.ntime = 1333333336;
    ae.strotheraccount = "c";
    walletdb.writeaccountingentry(ae);

    getresults(walletdb, results);

    boost_check(pwalletmain->norderposnext == 3);
    boost_check(2 == results.size());
    boost_check(results[0].ntime == 1333333333);
    boost_check(results[0].strcomment.empty());
    boost_check(1 == vpwtx[0]->norderpos);
    boost_check(results[2].ntime == 1333333336);
    boost_check(results[2].strotheraccount == "c");


    ae.ntime = 1333333330;
    ae.strotheraccount = "d";
    ae.norderpos = pwalletmain->incorderposnext();
    walletdb.writeaccountingentry(ae);

    getresults(walletdb, results);

    boost_check(results.size() == 3);
    boost_check(pwalletmain->norderposnext == 4);
    boost_check(results[0].ntime == 1333333333);
    boost_check(1 == vpwtx[0]->norderpos);
    boost_check(results[2].ntime == 1333333336);
    boost_check(results[3].ntime == 1333333330);
    boost_check(results[3].strcomment.empty());


    wtx.mapvalue["comment"] = "y";
    {
        cmutabletransaction tx(wtx);
        --tx.nlocktime;  // just to change the hash :)
        *static_cast<ctransaction*>(&wtx) = ctransaction(tx);
    }
    pwalletmain->addtowallet(wtx, false, &walletdb);
    vpwtx.push_back(&pwalletmain->mapwallet[wtx.gethash()]);
    vpwtx[1]->ntimereceived = (unsigned int)1333333336;

    wtx.mapvalue["comment"] = "x";
    {
        cmutabletransaction tx(wtx);
        --tx.nlocktime;  // just to change the hash :)
        *static_cast<ctransaction*>(&wtx) = ctransaction(tx);
    }
    pwalletmain->addtowallet(wtx, false, &walletdb);
    vpwtx.push_back(&pwalletmain->mapwallet[wtx.gethash()]);
    vpwtx[2]->ntimereceived = (unsigned int)1333333329;
    vpwtx[2]->norderpos = -1;

    getresults(walletdb, results);

    boost_check(results.size() == 3);
    boost_check(pwalletmain->norderposnext == 6);
    boost_check(0 == vpwtx[2]->norderpos);
    boost_check(results[1].ntime == 1333333333);
    boost_check(2 == vpwtx[0]->norderpos);
    boost_check(results[3].ntime == 1333333336);
    boost_check(results[4].ntime == 1333333330);
    boost_check(results[4].strcomment.empty());
    boost_check(5 == vpwtx[1]->norderpos);


    ae.ntime = 1333333334;
    ae.strotheraccount = "e";
    ae.norderpos = -1;
    walletdb.writeaccountingentry(ae);

    getresults(walletdb, results);

    boost_check(results.size() == 4);
    boost_check(pwalletmain->norderposnext == 7);
    boost_check(0 == vpwtx[2]->norderpos);
    boost_check(results[1].ntime == 1333333333);
    boost_check(2 == vpwtx[0]->norderpos);
    boost_check(results[3].ntime == 1333333336);
    boost_check(results[3].strcomment.empty());
    boost_check(results[4].ntime == 1333333330);
    boost_check(results[4].strcomment.empty());
    boost_check(results[5].ntime == 1333333334);
    boost_check(6 == vpwtx[1]->norderpos);
}

boost_auto_test_suite_end()
