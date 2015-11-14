// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include "test/test_moorecoin.h"

#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

// how many times to run all the tests to have a chance to catch errors that only show up with particular random shuffles
#define run_tests 100

// some tests fail 1% of the time due to bad luck.
// we repeat those tests this many times and only complain if all iterations of the test fail
#define random_repeats 5

using namespace std;

typedef set<pair<const cwallettx*,unsigned int> > coinset;

boost_fixture_test_suite(wallet_tests, testingsetup)

static cwallet wallet;
static vector<coutput> vcoins;

static void add_coin(const camount& nvalue, int nage = 6*24, bool fisfromme = false, int ninput=0)
{
    static int nextlocktime = 0;
    cmutabletransaction tx;
    tx.nlocktime = nextlocktime++;        // so all transactions get different hashes
    tx.vout.resize(ninput+1);
    tx.vout[ninput].nvalue = nvalue;
    if (fisfromme) {
        // isfromme() returns (getdebit() > 0), and getdebit() is 0 if vin.empty(),
        // so stop vin being empty, and cache a non-zero debit to fake out isfromme()
        tx.vin.resize(1);
    }
    cwallettx* wtx = new cwallettx(&wallet, tx);
    if (fisfromme)
    {
        wtx->fdebitcached = true;
        wtx->ndebitcached = 1;
    }
    coutput output(wtx, ninput, nage, true);
    vcoins.push_back(output);
}

static void empty_wallet(void)
{
    boost_foreach(coutput output, vcoins)
        delete output.tx;
    vcoins.clear();
}

static bool equal_sets(coinset a, coinset b)
{
    pair<coinset::iterator, coinset::iterator> ret = mismatch(a.begin(), a.end(), b.begin());
    return ret.first == a.end() && ret.second == b.end();
}

boost_auto_test_case(coin_selection_tests)
{
    coinset setcoinsret, setcoinsret2;
    camount nvalueret;

    lock(wallet.cs_wallet);

    // test multiple times to allow for differences in the shuffle order
    for (int i = 0; i < run_tests; i++)
    {
        empty_wallet();

        // with an empty wallet we can't even pay one cent
        boost_check(!wallet.selectcoinsminconf( 1 * cent, 1, 6, vcoins, setcoinsret, nvalueret));

        add_coin(1*cent, 4);        // add a new 1 cent coin

        // with a new 1 cent coin, we still can't find a mature 1 cent
        boost_check(!wallet.selectcoinsminconf( 1 * cent, 1, 6, vcoins, setcoinsret, nvalueret));

        // but we can find a new 1 cent
        boost_check( wallet.selectcoinsminconf( 1 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1 * cent);

        add_coin(2*cent);           // add a mature 2 cent coin

        // we can't make 3 cents of mature coins
        boost_check(!wallet.selectcoinsminconf( 3 * cent, 1, 6, vcoins, setcoinsret, nvalueret));

        // we can make 3 cents of new  coins
        boost_check( wallet.selectcoinsminconf( 3 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 3 * cent);

        add_coin(5*cent);           // add a mature 5 cent coin,
        add_coin(10*cent, 3, true); // a new 10 cent coin sent from one of our own addresses
        add_coin(20*cent);          // and a mature 20 cent coin

        // now we have new: 1+10=11 (of which 10 was self-sent), and mature: 2+5+20=27.  total = 38

        // we can't make 38 cents only if we disallow new coins:
        boost_check(!wallet.selectcoinsminconf(38 * cent, 1, 6, vcoins, setcoinsret, nvalueret));
        // we can't even make 37 cents if we don't allow new coins even if they're from us
        boost_check(!wallet.selectcoinsminconf(38 * cent, 6, 6, vcoins, setcoinsret, nvalueret));
        // but we can make 37 cents if we accept new coins from ourself
        boost_check( wallet.selectcoinsminconf(37 * cent, 1, 6, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 37 * cent);
        // and we can make 38 cents if we accept all new coins
        boost_check( wallet.selectcoinsminconf(38 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 38 * cent);

        // try making 34 cents from 1,2,5,10,20 - we can't do it exactly
        boost_check( wallet.selectcoinsminconf(34 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_gt(nvalueret, 34 * cent);         // but should get more than 34 cents
        boost_check_equal(setcoinsret.size(), 3u);     // the best should be 20+10+5.  it's incredibly unlikely the 1 or 2 got included (but possible)

        // when we try making 7 cents, the smaller coins (1,2,5) are enough.  we should see just 2+5
        boost_check( wallet.selectcoinsminconf( 7 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 7 * cent);
        boost_check_equal(setcoinsret.size(), 2u);

        // when we try making 8 cents, the smaller coins (1,2,5) are exactly enough.
        boost_check( wallet.selectcoinsminconf( 8 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check(nvalueret == 8 * cent);
        boost_check_equal(setcoinsret.size(), 3u);

        // when we try making 9 cents, no subset of smaller coins is enough, and we get the next bigger coin (10)
        boost_check( wallet.selectcoinsminconf( 9 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 10 * cent);
        boost_check_equal(setcoinsret.size(), 1u);

        // now clear out the wallet and start again to test choosing between subsets of smaller coins and the next biggest coin
        empty_wallet();

        add_coin( 6*cent);
        add_coin( 7*cent);
        add_coin( 8*cent);
        add_coin(20*cent);
        add_coin(30*cent); // now we have 6+7+8+20+30 = 71 cents total

        // check that we have 71 and not 72
        boost_check( wallet.selectcoinsminconf(71 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check(!wallet.selectcoinsminconf(72 * cent, 1, 1, vcoins, setcoinsret, nvalueret));

        // now try making 16 cents.  the best smaller coins can do is 6+7+8 = 21; not as good at the next biggest coin, 20
        boost_check( wallet.selectcoinsminconf(16 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 20 * cent); // we should get 20 in one coin
        boost_check_equal(setcoinsret.size(), 1u);

        add_coin( 5*cent); // now we have 5+6+7+8+20+30 = 75 cents total

        // now if we try making 16 cents again, the smaller coins can make 5+6+7 = 18 cents, better than the next biggest coin, 20
        boost_check( wallet.selectcoinsminconf(16 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 18 * cent); // we should get 18 in 3 coins
        boost_check_equal(setcoinsret.size(), 3u);

        add_coin( 18*cent); // now we have 5+6+7+8+18+20+30

        // and now if we try making 16 cents again, the smaller coins can make 5+6+7 = 18 cents, the same as the next biggest coin, 18
        boost_check( wallet.selectcoinsminconf(16 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 18 * cent);  // we should get 18 in 1 coin
        boost_check_equal(setcoinsret.size(), 1u); // because in the event of a tie, the biggest coin wins

        // now try making 11 cents.  we should get 5+6
        boost_check( wallet.selectcoinsminconf(11 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 11 * cent);
        boost_check_equal(setcoinsret.size(), 2u);

        // check that the smallest bigger coin is used
        add_coin( 1*coin);
        add_coin( 2*coin);
        add_coin( 3*coin);
        add_coin( 4*coin); // now we have 5+6+7+8+18+20+30+100+200+300+400 = 1094 cents
        boost_check( wallet.selectcoinsminconf(95 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1 * coin);  // we should get 1 btc in 1 coin
        boost_check_equal(setcoinsret.size(), 1u);

        boost_check( wallet.selectcoinsminconf(195 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 2 * coin);  // we should get 2 btc in 1 coin
        boost_check_equal(setcoinsret.size(), 1u);

        // empty the wallet and start again, now with fractions of a cent, to test sub-cent change avoidance
        empty_wallet();
        add_coin(0.1*cent);
        add_coin(0.2*cent);
        add_coin(0.3*cent);
        add_coin(0.4*cent);
        add_coin(0.5*cent);

        // try making 1 cent from 0.1 + 0.2 + 0.3 + 0.4 + 0.5 = 1.5 cents
        // we'll get sub-cent change whatever happens, so can expect 1.0 exactly
        boost_check( wallet.selectcoinsminconf(1 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1 * cent);

        // but if we add a bigger coin, making it possible to avoid sub-cent change, things change:
        add_coin(1111*cent);

        // try making 1 cent from 0.1 + 0.2 + 0.3 + 0.4 + 0.5 + 1111 = 1112.5 cents
        boost_check( wallet.selectcoinsminconf(1 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1 * cent); // we should get the exact amount

        // if we add more sub-cent coins:
        add_coin(0.6*cent);
        add_coin(0.7*cent);

        // and try again to make 1.0 cents, we can still make 1.0 cents
        boost_check( wallet.selectcoinsminconf(1 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1 * cent); // we should get the exact amount

        // run the 'mtgox' test (see http://blockexplorer.com/tx/29a3efd3ef04f9153d47a990bd7b048a4b2d213daaa5fb8ed670fb85f13bdbcf)
        // they tried to consolidate 10 50k coins into one 500k coin, and ended up with 50k in change
        empty_wallet();
        for (int i = 0; i < 20; i++)
            add_coin(50000 * coin);

        boost_check( wallet.selectcoinsminconf(500000 * coin, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 500000 * coin); // we should get the exact amount
        boost_check_equal(setcoinsret.size(), 10u); // in ten coins

        // if there's not enough in the smaller coins to make at least 1 cent change (0.5+0.6+0.7 < 1.0+1.0),
        // we need to try finding an exact subset anyway

        // sometimes it will fail, and so we use the next biggest coin:
        empty_wallet();
        add_coin(0.5 * cent);
        add_coin(0.6 * cent);
        add_coin(0.7 * cent);
        add_coin(1111 * cent);
        boost_check( wallet.selectcoinsminconf(1 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1111 * cent); // we get the bigger coin
        boost_check_equal(setcoinsret.size(), 1u);

        // but sometimes it's possible, and we use an exact subset (0.4 + 0.6 = 1.0)
        empty_wallet();
        add_coin(0.4 * cent);
        add_coin(0.6 * cent);
        add_coin(0.8 * cent);
        add_coin(1111 * cent);
        boost_check( wallet.selectcoinsminconf(1 * cent, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1 * cent);   // we should get the exact amount
        boost_check_equal(setcoinsret.size(), 2u); // in two coins 0.4+0.6

        // test avoiding sub-cent change
        empty_wallet();
        add_coin(0.0005 * coin);
        add_coin(0.01 * coin);
        add_coin(1 * coin);

        // trying to make 1.0001 from these three coins
        boost_check( wallet.selectcoinsminconf(1.0001 * coin, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1.0105 * coin);   // we should get all coins
        boost_check_equal(setcoinsret.size(), 3u);

        // but if we try to make 0.999, we should take the bigger of the two small coins to avoid sub-cent change
        boost_check( wallet.selectcoinsminconf(0.999 * coin, 1, 1, vcoins, setcoinsret, nvalueret));
        boost_check_equal(nvalueret, 1.01 * coin);   // we should get 1 + 0.01
        boost_check_equal(setcoinsret.size(), 2u);

        // test randomness
        {
            empty_wallet();
            for (int i2 = 0; i2 < 100; i2++)
                add_coin(coin);

            // picking 50 from 100 coins doesn't depend on the shuffle,
            // but does depend on randomness in the stochastic approximation code
            boost_check(wallet.selectcoinsminconf(50 * coin, 1, 6, vcoins, setcoinsret , nvalueret));
            boost_check(wallet.selectcoinsminconf(50 * coin, 1, 6, vcoins, setcoinsret2, nvalueret));
            boost_check(!equal_sets(setcoinsret, setcoinsret2));

            int fails = 0;
            for (int i = 0; i < random_repeats; i++)
            {
                // selecting 1 from 100 identical coins depends on the shuffle; this test will fail 1% of the time
                // run the test random_repeats times and only complain if all of them fail
                boost_check(wallet.selectcoinsminconf(coin, 1, 6, vcoins, setcoinsret , nvalueret));
                boost_check(wallet.selectcoinsminconf(coin, 1, 6, vcoins, setcoinsret2, nvalueret));
                if (equal_sets(setcoinsret, setcoinsret2))
                    fails++;
            }
            boost_check_ne(fails, random_repeats);

            // add 75 cents in small change.  not enough to make 90 cents,
            // then try making 90 cents.  there are multiple competing "smallest bigger" coins,
            // one of which should be picked at random
            add_coin( 5*cent); add_coin(10*cent); add_coin(15*cent); add_coin(20*cent); add_coin(25*cent);

            fails = 0;
            for (int i = 0; i < random_repeats; i++)
            {
                // selecting 1 from 100 identical coins depends on the shuffle; this test will fail 1% of the time
                // run the test random_repeats times and only complain if all of them fail
                boost_check(wallet.selectcoinsminconf(90*cent, 1, 6, vcoins, setcoinsret , nvalueret));
                boost_check(wallet.selectcoinsminconf(90*cent, 1, 6, vcoins, setcoinsret2, nvalueret));
                if (equal_sets(setcoinsret, setcoinsret2))
                    fails++;
            }
            boost_check_ne(fails, random_repeats);
        }
    }
    empty_wallet();
}

boost_auto_test_suite_end()
