#ifndef moorecoin_test_test_moorecoin_h
#define moorecoin_test_test_moorecoin_h

#include "txdb.h"

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

/** basic testing setup.
 * this just configures logging and chain parameters.
 */
struct basictestingsetup {
    basictestingsetup();
    ~basictestingsetup();
};

/** testing setup that configures a complete environment.
 * included are data directory, coins database, script check threads
 * and wallet (if enabled) setup.
 */
struct testingsetup: public basictestingsetup {
    ccoinsviewdb *pcoinsdbview;
    boost::filesystem::path pathtemp;
    boost::thread_group threadgroup;

    testingsetup();
    ~testingsetup();
};

#endif
