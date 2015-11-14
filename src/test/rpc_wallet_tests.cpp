// copyright (c) 2013-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"
#include "main.h"
#include "wallet/wallet.h"

#include "test/test_moorecoin.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include "univalue/univalue.h"

using namespace std;

extern univalue createargs(int nrequired, const char* address1 = null, const char* address2 = null);
extern univalue callrpc(string args);

extern cwallet* pwalletmain;

boost_fixture_test_suite(rpc_wallet_tests, testingsetup)

boost_auto_test_case(rpc_addmultisig)
{
    lock(pwalletmain->cs_wallet);

    rpcfn_type addmultisig = tablerpc["addmultisigaddress"]->actor;

    // old, 65-byte-long:
    const char address1hex[] = "0434e3e09f49ea168c5bbf53f877ff4206923858aab7c7e1df25bc263978107c95e35065a27ef6f1b27222db0ec97e0e895eaca603d3ee0d4c060ce3d8a00286c8";
    // new, compressed:
    const char address2hex[] = "0388c2037017c62240b6b72ac1a2a5f94da790596ebd06177c8572752922165cb4";

    univalue v;
    cmoorecoinaddress address;
    boost_check_no_throw(v = addmultisig(createargs(1, address1hex), false));
    address.setstring(v.get_str());
    boost_check(address.isvalid() && address.isscript());

    boost_check_no_throw(v = addmultisig(createargs(1, address1hex, address2hex), false));
    address.setstring(v.get_str());
    boost_check(address.isvalid() && address.isscript());

    boost_check_no_throw(v = addmultisig(createargs(2, address1hex, address2hex), false));
    address.setstring(v.get_str());
    boost_check(address.isvalid() && address.isscript());

    boost_check_throw(addmultisig(createargs(0), false), runtime_error);
    boost_check_throw(addmultisig(createargs(1), false), runtime_error);
    boost_check_throw(addmultisig(createargs(2, address1hex), false), runtime_error);

    boost_check_throw(addmultisig(createargs(1, ""), false), runtime_error);
    boost_check_throw(addmultisig(createargs(1, "notavalidpubkey"), false), runtime_error);

    string short1(address1hex, address1hex + sizeof(address1hex) - 2); // last byte missing
    boost_check_throw(addmultisig(createargs(2, short1.c_str()), false), runtime_error);

    string short2(address1hex + 1, address1hex + sizeof(address1hex)); // first byte missing
    boost_check_throw(addmultisig(createargs(2, short2.c_str()), false), runtime_error);
}

boost_auto_test_case(rpc_wallet)
{
    // test rpc calls for various wallet statistics
    univalue r;

    lock2(cs_main, pwalletmain->cs_wallet);

    cpubkey demopubkey = pwalletmain->generatenewkey();
    cmoorecoinaddress demoaddress = cmoorecoinaddress(ctxdestination(demopubkey.getid()));
    univalue retvalue;
    string straccount = "walletdemoaccount";
    string strpurpose = "receive";
    boost_check_no_throw({ /*initialize wallet with an account */
        cwalletdb walletdb(pwalletmain->strwalletfile);
        caccount account;
        account.vchpubkey = demopubkey;
        pwalletmain->setaddressbook(account.vchpubkey.getid(), straccount, strpurpose);
        walletdb.writeaccount(straccount, account);
    });

    cpubkey setaccountdemopubkey = pwalletmain->generatenewkey();
    cmoorecoinaddress setaccountdemoaddress = cmoorecoinaddress(ctxdestination(setaccountdemopubkey.getid()));

    /*********************************
     * 			setaccount
     *********************************/
    boost_check_no_throw(callrpc("setaccount " + setaccountdemoaddress.tostring() + " nullaccount"));
    /* 1d1zrzne3juo7zyckeyqqiqawd9y54f4xz is not owned by the test wallet. */
    boost_check_throw(callrpc("setaccount 1d1zrzne3juo7zyckeyqqiqawd9y54f4xz nullaccount"), runtime_error);
    boost_check_throw(callrpc("setaccount"), runtime_error);
    /* 1d1zrzne3juo7zyckeyqqiqawd9y54f4x (33 chars) is an illegal address (should be 34 chars) */
    boost_check_throw(callrpc("setaccount 1d1zrzne3juo7zyckeyqqiqawd9y54f4x nullaccount"), runtime_error);


    /*********************************
     *                  getbalance
     *********************************/
    boost_check_no_throw(callrpc("getbalance"));
    boost_check_no_throw(callrpc("getbalance " + demoaddress.tostring()));

    /*********************************
     * 			listunspent
     *********************************/
    boost_check_no_throw(callrpc("listunspent"));
    boost_check_throw(callrpc("listunspent string"), runtime_error);
    boost_check_throw(callrpc("listunspent 0 string"), runtime_error);
    boost_check_throw(callrpc("listunspent 0 1 not_array"), runtime_error);
    boost_check_throw(callrpc("listunspent 0 1 [] extra"), runtime_error);
    boost_check_no_throw(r = callrpc("listunspent 0 1 []"));
    boost_check(r.get_array().empty());

    /*********************************
     * 		listreceivedbyaddress
     *********************************/
    boost_check_no_throw(callrpc("listreceivedbyaddress"));
    boost_check_no_throw(callrpc("listreceivedbyaddress 0"));
    boost_check_throw(callrpc("listreceivedbyaddress not_int"), runtime_error);
    boost_check_throw(callrpc("listreceivedbyaddress 0 not_bool"), runtime_error);
    boost_check_no_throw(callrpc("listreceivedbyaddress 0 true"));
    boost_check_throw(callrpc("listreceivedbyaddress 0 true extra"), runtime_error);

    /*********************************
     * 		listreceivedbyaccount
     *********************************/
    boost_check_no_throw(callrpc("listreceivedbyaccount"));
    boost_check_no_throw(callrpc("listreceivedbyaccount 0"));
    boost_check_throw(callrpc("listreceivedbyaccount not_int"), runtime_error);
    boost_check_throw(callrpc("listreceivedbyaccount 0 not_bool"), runtime_error);
    boost_check_no_throw(callrpc("listreceivedbyaccount 0 true"));
    boost_check_throw(callrpc("listreceivedbyaccount 0 true extra"), runtime_error);

    /*********************************
     *          listsinceblock
     *********************************/
    boost_check_no_throw(callrpc("listsinceblock"));

    /*********************************
     *          listtransactions
     *********************************/
    boost_check_no_throw(callrpc("listtransactions"));
    boost_check_no_throw(callrpc("listtransactions " + demoaddress.tostring()));
    boost_check_no_throw(callrpc("listtransactions " + demoaddress.tostring() + " 20"));
    boost_check_no_throw(callrpc("listtransactions " + demoaddress.tostring() + " 20 0"));
    boost_check_throw(callrpc("listtransactions " + demoaddress.tostring() + " not_int"), runtime_error);

    /*********************************
     *          listlockunspent
     *********************************/
    boost_check_no_throw(callrpc("listlockunspent"));

    /*********************************
     *          listaccounts
     *********************************/
    boost_check_no_throw(callrpc("listaccounts"));

    /*********************************
     *          listaddressgroupings
     *********************************/
    boost_check_no_throw(callrpc("listaddressgroupings"));

    /*********************************
     * 		getrawchangeaddress
     *********************************/
    boost_check_no_throw(callrpc("getrawchangeaddress"));

    /*********************************
     * 		getnewaddress
     *********************************/
    boost_check_no_throw(callrpc("getnewaddress"));
    boost_check_no_throw(callrpc("getnewaddress getnewaddress_demoaccount"));

    /*********************************
     * 		getaccountaddress
     *********************************/
    boost_check_no_throw(callrpc("getaccountaddress \"\""));
    boost_check_no_throw(callrpc("getaccountaddress accountthatdoesntexists")); // should generate a new account
    boost_check_no_throw(retvalue = callrpc("getaccountaddress " + straccount));
    boost_check(cmoorecoinaddress(retvalue.get_str()).get() == demoaddress.get());

    /*********************************
     * 			getaccount
     *********************************/
    boost_check_throw(callrpc("getaccount"), runtime_error);
    boost_check_no_throw(callrpc("getaccount " + demoaddress.tostring()));

    /*********************************
     * 	signmessage + verifymessage
     *********************************/
    boost_check_no_throw(retvalue = callrpc("signmessage " + demoaddress.tostring() + " mymessage"));
    boost_check_throw(callrpc("signmessage"), runtime_error);
    /* should throw error because this address is not loaded in the wallet */
    boost_check_throw(callrpc("signmessage 1qfqqmud55zv3pjejztakcsqmjlt6jkjvj mymessage"), runtime_error);

    /* missing arguments */
    boost_check_throw(callrpc("verifymessage " + demoaddress.tostring()), runtime_error);
    boost_check_throw(callrpc("verifymessage " + demoaddress.tostring() + " " + retvalue.get_str()), runtime_error);
    /* illegal address */
    boost_check_throw(callrpc("verifymessage 1d1zrzne3juo7zyckeyqqiqawd9y54f4x " + retvalue.get_str() + " mymessage"), runtime_error);
    /* wrong address */
    boost_check(callrpc("verifymessage 1d1zrzne3juo7zyckeyqqiqawd9y54f4xz " + retvalue.get_str() + " mymessage").get_bool() == false);
    /* correct address and signature but wrong message */
    boost_check(callrpc("verifymessage " + demoaddress.tostring() + " " + retvalue.get_str() + " wrongmessage").get_bool() == false);
    /* correct address, message and signature*/
    boost_check(callrpc("verifymessage " + demoaddress.tostring() + " " + retvalue.get_str() + " mymessage").get_bool() == true);

    /*********************************
     * 		getaddressesbyaccount
     *********************************/
    boost_check_throw(callrpc("getaddressesbyaccount"), runtime_error);
    boost_check_no_throw(retvalue = callrpc("getaddressesbyaccount " + straccount));
    univalue arr = retvalue.get_array();
    boost_check(arr.size() > 0);
    boost_check(cmoorecoinaddress(arr[0].get_str()).get() == demoaddress.get());
}

boost_auto_test_suite_end()
