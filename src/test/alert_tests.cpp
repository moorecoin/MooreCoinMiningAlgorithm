// copyright (c) 2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

//
// unit tests for alert system
//

#include "alert.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "data/alerttests.raw.h"

#include "main.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "utilstrencodings.h"

#include "test/test_moorecoin.h"

#include <fstream>

#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

#if 0
//
// alerttests contains 7 alerts, generated with this code:
// (signandsave code not shown, alert signing key is secret)
//
{
    calert alert;
    alert.nrelayuntil   = 60;
    alert.nexpiration   = 24 * 60 * 60;
    alert.nid           = 1;
    alert.ncancel       = 0;   // cancels previous messages up to this id number
    alert.nminver       = 0;  // these versions are protocol versions
    alert.nmaxver       = 999001;
    alert.npriority     = 1;
    alert.strcomment    = "alert comment";
    alert.strstatusbar  = "alert 1";

    signandsave(alert, "test/alerttests");

    alert.setsubver.insert(std::string("/satoshi:0.1.0/"));
    alert.strstatusbar  = "alert 1 for satoshi 0.1.0";
    signandsave(alert, "test/alerttests");

    alert.setsubver.insert(std::string("/satoshi:0.2.0/"));
    alert.strstatusbar  = "alert 1 for satoshi 0.1.0, 0.2.0";
    signandsave(alert, "test/alerttests");

    alert.setsubver.clear();
    ++alert.nid;
    alert.ncancel = 1;
    alert.npriority = 100;
    alert.strstatusbar  = "alert 2, cancels 1";
    signandsave(alert, "test/alerttests");

    alert.nexpiration += 60;
    ++alert.nid;
    signandsave(alert, "test/alerttests");

    ++alert.nid;
    alert.nminver = 11;
    alert.nmaxver = 22;
    signandsave(alert, "test/alerttests");

    ++alert.nid;
    alert.strstatusbar  = "alert 2 for satoshi 0.1.0";
    alert.setsubver.insert(std::string("/satoshi:0.1.0/"));
    signandsave(alert, "test/alerttests");

    ++alert.nid;
    alert.nminver = 0;
    alert.nmaxver = 999999;
    alert.strstatusbar  = "evil alert'; /bin/ls; echo '";
    alert.setsubver.clear();
    signandsave(alert, "test/alerttests");
}
#endif

struct readalerts : public testingsetup
{
    readalerts()
    {
        std::vector<unsigned char> vch(alert_tests::alerttests, alert_tests::alerttests + sizeof(alert_tests::alerttests));
        cdatastream stream(vch, ser_disk, client_version);
        try {
            while (!stream.eof())
            {
                calert alert;
                stream >> alert;
                alerts.push_back(alert);
            }
        }
        catch (const std::exception&) { }
    }
    ~readalerts() { }

    static std::vector<std::string> read_lines(boost::filesystem::path filepath)
    {
        std::vector<std::string> result;

        std::ifstream f(filepath.string().c_str());
        std::string line;
        while (std::getline(f,line))
            result.push_back(line);

        return result;
    }

    std::vector<calert> alerts;
};

boost_fixture_test_suite(alert_tests, readalerts)


boost_auto_test_case(alertapplies)
{
    setmocktime(11);
    const std::vector<unsigned char>& alertkey = params(cbasechainparams::main).alertkey();

    boost_foreach(const calert& alert, alerts)
    {
        boost_check(alert.checksignature(alertkey));
    }

    boost_check(alerts.size() >= 3);

    // matches:
    boost_check(alerts[0].appliesto(1, ""));
    boost_check(alerts[0].appliesto(999001, ""));
    boost_check(alerts[0].appliesto(1, "/satoshi:11.11.11/"));

    boost_check(alerts[1].appliesto(1, "/satoshi:0.1.0/"));
    boost_check(alerts[1].appliesto(999001, "/satoshi:0.1.0/"));

    boost_check(alerts[2].appliesto(1, "/satoshi:0.1.0/"));
    boost_check(alerts[2].appliesto(1, "/satoshi:0.2.0/"));

    // don't match:
    boost_check(!alerts[0].appliesto(-1, ""));
    boost_check(!alerts[0].appliesto(999002, ""));

    boost_check(!alerts[1].appliesto(1, ""));
    boost_check(!alerts[1].appliesto(1, "satoshi:0.1.0"));
    boost_check(!alerts[1].appliesto(1, "/satoshi:0.1.0"));
    boost_check(!alerts[1].appliesto(1, "satoshi:0.1.0/"));
    boost_check(!alerts[1].appliesto(-1, "/satoshi:0.1.0/"));
    boost_check(!alerts[1].appliesto(999002, "/satoshi:0.1.0/"));
    boost_check(!alerts[1].appliesto(1, "/satoshi:0.2.0/"));

    boost_check(!alerts[2].appliesto(1, "/satoshi:0.3.0/"));

    setmocktime(0);
}


boost_auto_test_case(alertnotify)
{
    setmocktime(11);
    const std::vector<unsigned char>& alertkey = params(cbasechainparams::main).alertkey();

    boost::filesystem::path temp = gettemppath() / "alertnotify.txt";
    boost::filesystem::remove(temp);

    mapargs["-alertnotify"] = std::string("echo %s >> ") + temp.string();

    boost_foreach(calert alert, alerts)
        alert.processalert(alertkey, false);

    std::vector<std::string> r = read_lines(temp);
    boost_check_equal(r.size(), 4u);

// windows built-in echo semantics are different than posixy shells. quotes and
// whitespace are printed literally.

#ifndef win32
    boost_check_equal(r[0], "alert 1");
    boost_check_equal(r[1], "alert 2, cancels 1");
    boost_check_equal(r[2], "alert 2, cancels 1");
    boost_check_equal(r[3], "evil alert; /bin/ls; echo "); // single-quotes should be removed
#else
    boost_check_equal(r[0], "'alert 1' ");
    boost_check_equal(r[1], "'alert 2, cancels 1' ");
    boost_check_equal(r[2], "'alert 2, cancels 1' ");
    boost_check_equal(r[3], "'evil alert; /bin/ls; echo ' ");
#endif
    boost::filesystem::remove(temp);

    setmocktime(0);
}

static bool falsefunc() { return false; }

boost_auto_test_case(partitionalert)
{
    // test partitioncheck
    ccriticalsection csdummy;
    cblockindex indexdummy[100];
    cchainparams& params = params(cbasechainparams::main);
    int64_t npowtargetspacing = params.getconsensus().npowtargetspacing;

    // generate fake blockchain timestamps relative to
    // an arbitrary time:
    int64_t now = 1427379054;
    setmocktime(now);
    for (int i = 0; i < 100; i++)
    {
        indexdummy[i].phashblock = null;
        if (i == 0) indexdummy[i].pprev = null;
        else indexdummy[i].pprev = &indexdummy[i-1];
        indexdummy[i].nheight = i;
        indexdummy[i].ntime = now - (100-i)*npowtargetspacing;
        // other members don't matter, the partition check code doesn't
        // use them
    }

    // test 1: chain with blocks every npowtargetspacing seconds,
    // as normal, no worries:
    partitioncheck(falsefunc, csdummy, &indexdummy[99], npowtargetspacing);
    boost_check(strmiscwarning.empty());

    // test 2: go 3.5 hours without a block, expect a warning:
    now += 3*60*60+30*60;
    setmocktime(now);
    partitioncheck(falsefunc, csdummy, &indexdummy[99], npowtargetspacing);
    boost_check(!strmiscwarning.empty());
    boost_test_message(std::string("got alert text: ")+strmiscwarning);
    strmiscwarning = "";

    // test 3: test the "partition alerts only go off once per day"
    // code:
    now += 60*10;
    setmocktime(now);
    partitioncheck(falsefunc, csdummy, &indexdummy[99], npowtargetspacing);
    boost_check(strmiscwarning.empty());

    // test 4: get 2.5 times as many blocks as expected:
    now += 60*60*24; // pretend it is a day later
    setmocktime(now);
    int64_t quickspacing = npowtargetspacing*2/5;
    for (int i = 0; i < 100; i++) // tweak chain timestamps:
        indexdummy[i].ntime = now - (100-i)*quickspacing;
    partitioncheck(falsefunc, csdummy, &indexdummy[99], npowtargetspacing);
    boost_check(!strmiscwarning.empty());
    boost_test_message(std::string("got alert text: ")+strmiscwarning);
    strmiscwarning = "";

    setmocktime(0);
}

boost_auto_test_suite_end()
