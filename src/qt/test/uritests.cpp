// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "uritests.h"

#include "guiutil.h"
#include "walletmodel.h"

#include <qurl>

void uritests::uritests()
{
    sendcoinsrecipient rv;
    qurl uri;
    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?req-dontexist="));
    qverify(!guiutil::parsemoorecoinuri(uri, &rv));

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?dontexist="));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.label == qstring());
    qverify(rv.amount == 0);

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?label=wikipedia example address"));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.label == qstring("wikipedia example address"));
    qverify(rv.amount == 0);

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?amount=0.001"));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.label == qstring());
    qverify(rv.amount == 100000);

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?amount=1.001"));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.label == qstring());
    qverify(rv.amount == 100100000);

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?amount=100&label=wikipedia example"));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.amount == 10000000000ll);
    qverify(rv.label == qstring("wikipedia example"));

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?message=wikipedia example address"));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.label == qstring());

    qverify(guiutil::parsemoorecoinuri("moorecoin://175twpb8k1s7nmh4zx6rewf9wqrczv245w?message=wikipedia example address", &rv));
    qverify(rv.address == qstring("175twpb8k1s7nmh4zx6rewf9wqrczv245w"));
    qverify(rv.label == qstring());

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?req-message=wikipedia example address"));
    qverify(guiutil::parsemoorecoinuri(uri, &rv));

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?amount=1,000&label=wikipedia example"));
    qverify(!guiutil::parsemoorecoinuri(uri, &rv));

    uri.seturl(qstring("moorecoin:175twpb8k1s7nmh4zx6rewf9wqrczv245w?amount=1,000.0&label=wikipedia example"));
    qverify(!guiutil::parsemoorecoinuri(uri, &rv));
}
