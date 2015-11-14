// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#include "util.h"
#include "uritests.h"

#ifdef enable_wallet
#include "paymentservertests.h"
#endif

#include <qcoreapplication>
#include <qobject>
#include <qtest>

#if defined(qt_staticplugin) && qt_version < 0x050000
#include <qtplugin>
q_import_plugin(qcncodecs)
q_import_plugin(qjpcodecs)
q_import_plugin(qtwcodecs)
q_import_plugin(qkrcodecs)
#endif

// this is all you need to run all the tests
int main(int argc, char *argv[])
{
    setupenvironment();
    bool finvalid = false;

    // don't remove this, it's needed to access
    // qcoreapplication:: in the tests
    qcoreapplication app(argc, argv);
    app.setapplicationname("moorecoin-qt-test");

    uritests test1;
    if (qtest::qexec(&test1) != 0)
        finvalid = true;
#ifdef enable_wallet
    paymentservertests test2;
    if (qtest::qexec(&test2) != 0)
        finvalid = true;
#endif

    return finvalid;
}
