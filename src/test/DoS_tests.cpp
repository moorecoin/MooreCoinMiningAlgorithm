// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

//
// unit tests for denial-of-service detection/prevention code
//



#include "keystore.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "script/sign.h"
#include "serialize.h"
#include "util.h"

#include "test/test_moorecoin.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

// tests this internal-to-main.cpp method:
extern bool addorphantx(const ctransaction& tx, nodeid peer);
extern void eraseorphansfor(nodeid peer);
extern unsigned int limitorphantxsize(unsigned int nmaxorphans);
struct corphantx {
    ctransaction tx;
    nodeid frompeer;
};
extern std::map<uint256, corphantx> maporphantransactions;
extern std::map<uint256, std::set<uint256> > maporphantransactionsbyprev;

cservice ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return cservice(cnetaddr(s), params().getdefaultport());
}

boost_fixture_test_suite(dos_tests, testingsetup)

boost_auto_test_case(dos_banning)
{
    cnode::clearbanned();
    caddress addr1(ip(0xa0b0c001));
    cnode dummynode1(invalid_socket, addr1, "", true);
    dummynode1.nversion = 1;
    misbehaving(dummynode1.getid(), 100); // should get banned
    sendmessages(&dummynode1, false);
    boost_check(cnode::isbanned(addr1));
    boost_check(!cnode::isbanned(ip(0xa0b0c001|0x0000ff00))); // different ip, not banned

    caddress addr2(ip(0xa0b0c002));
    cnode dummynode2(invalid_socket, addr2, "", true);
    dummynode2.nversion = 1;
    misbehaving(dummynode2.getid(), 50);
    sendmessages(&dummynode2, false);
    boost_check(!cnode::isbanned(addr2)); // 2 not banned yet...
    boost_check(cnode::isbanned(addr1));  // ... but 1 still should be
    misbehaving(dummynode2.getid(), 50);
    sendmessages(&dummynode2, false);
    boost_check(cnode::isbanned(addr2));
}

boost_auto_test_case(dos_banscore)
{
    cnode::clearbanned();
    mapargs["-banscore"] = "111"; // because 11 is my favorite number
    caddress addr1(ip(0xa0b0c001));
    cnode dummynode1(invalid_socket, addr1, "", true);
    dummynode1.nversion = 1;
    misbehaving(dummynode1.getid(), 100);
    sendmessages(&dummynode1, false);
    boost_check(!cnode::isbanned(addr1));
    misbehaving(dummynode1.getid(), 10);
    sendmessages(&dummynode1, false);
    boost_check(!cnode::isbanned(addr1));
    misbehaving(dummynode1.getid(), 1);
    sendmessages(&dummynode1, false);
    boost_check(cnode::isbanned(addr1));
    mapargs.erase("-banscore");
}

boost_auto_test_case(dos_bantime)
{
    cnode::clearbanned();
    int64_t nstarttime = gettime();
    setmocktime(nstarttime); // overrides future calls to gettime()

    caddress addr(ip(0xa0b0c001));
    cnode dummynode(invalid_socket, addr, "", true);
    dummynode.nversion = 1;

    misbehaving(dummynode.getid(), 100);
    sendmessages(&dummynode, false);
    boost_check(cnode::isbanned(addr));

    setmocktime(nstarttime+60*60);
    boost_check(cnode::isbanned(addr));

    setmocktime(nstarttime+60*60*24+1);
    boost_check(!cnode::isbanned(addr));
}

ctransaction randomorphan()
{
    std::map<uint256, corphantx>::iterator it;
    it = maporphantransactions.lower_bound(getrandhash());
    if (it == maporphantransactions.end())
        it = maporphantransactions.begin();
    return it->second.tx;
}

boost_auto_test_case(dos_maporphans)
{
    ckey key;
    key.makenewkey(true);
    cbasickeystore keystore;
    keystore.addkey(key);

    // 50 orphan transactions:
    for (int i = 0; i < 50; i++)
    {
        cmutabletransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = getrandhash();
        tx.vin[0].scriptsig << op_1;
        tx.vout.resize(1);
        tx.vout[0].nvalue = 1*cent;
        tx.vout[0].scriptpubkey = getscriptfordestination(key.getpubkey().getid());

        addorphantx(tx, i);
    }

    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++)
    {
        ctransaction txprev = randomorphan();

        cmutabletransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = txprev.gethash();
        tx.vout.resize(1);
        tx.vout[0].nvalue = 1*cent;
        tx.vout[0].scriptpubkey = getscriptfordestination(key.getpubkey().getid());
        signsignature(keystore, txprev, tx, 0);

        addorphantx(tx, i);
    }

    // this really-big orphan should be ignored:
    for (int i = 0; i < 10; i++)
    {
        ctransaction txprev = randomorphan();

        cmutabletransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nvalue = 1*cent;
        tx.vout[0].scriptpubkey = getscriptfordestination(key.getpubkey().getid());
        tx.vin.resize(500);
        for (unsigned int j = 0; j < tx.vin.size(); j++)
        {
            tx.vin[j].prevout.n = j;
            tx.vin[j].prevout.hash = txprev.gethash();
        }
        signsignature(keystore, txprev, tx, 0);
        // re-use same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < tx.vin.size(); j++)
            tx.vin[j].scriptsig = tx.vin[0].scriptsig;

        boost_check(!addorphantx(tx, i));
    }

    // test eraseorphansfor:
    for (nodeid i = 0; i < 3; i++)
    {
        size_t sizebefore = maporphantransactions.size();
        eraseorphansfor(i);
        boost_check(maporphantransactions.size() < sizebefore);
    }

    // test limitorphantxsize() function:
    limitorphantxsize(40);
    boost_check(maporphantransactions.size() <= 40);
    limitorphantxsize(10);
    boost_check(maporphantransactions.size() <= 10);
    limitorphantxsize(0);
    boost_check(maporphantransactions.empty());
    boost_check(maporphantransactionsbyprev.empty());
}

boost_auto_test_suite_end()
