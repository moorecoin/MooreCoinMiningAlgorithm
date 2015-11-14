// copyright (c) 2013-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "clientversion.h"
#include "consensus/validation.h"
#include "main.h"
#include "test/test_moorecoin.h"
#include "utiltime.h"

#include <cstdio>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/test/unit_test.hpp>


boost_fixture_test_suite(checkblock_tests, basictestingsetup)

bool read_block(const std::string& filename, cblock& block)
{
    namespace fs = boost::filesystem;
    fs::path testfile = fs::current_path() / "data" / filename;
#ifdef test_data_dir
    if (!fs::exists(testfile))
    {
        testfile = fs::path(boost_pp_stringize(test_data_dir)) / filename;
    }
#endif
    file* fp = fopen(testfile.string().c_str(), "rb");
    if (!fp) return false;

    fseek(fp, 8, seek_set); // skip msgheader/size

    cautofile filein(fp, ser_disk, client_version);
    if (filein.isnull()) return false;

    filein >> block;

    return true;
}

boost_auto_test_case(may15)
{
    // putting a 1mb binary file in the git repository is not a great
    // idea, so this test is only run if you manually download
    // test/data/mar12fork.dat from
    // http://sourceforge.net/projects/moorecoin/files/moorecoin/blockchain/mar12fork.dat/download
    unsigned int tmay15 = 1368576000;
    setmocktime(tmay15); // test as if it was right at may 15

    cblock forkingblock;
    if (read_block("mar12fork.dat", forkingblock))
    {
        cvalidationstate state;

        // after may 15'th, big blocks are ok:
        forkingblock.ntime = tmay15; // invalidates pow
        boost_check(checkblock(forkingblock, state, false, false));
    }

    setmocktime(0);
}

boost_auto_test_suite_end()
