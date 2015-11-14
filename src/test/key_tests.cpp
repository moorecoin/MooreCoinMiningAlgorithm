// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "base58.h"
#include "script/script.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_moorecoin.h"

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;

static const string strsecret1     ("5hxwvvfubhxpyyps3tjkw6fq9je9j18thftkzjhhfmfiwtmabrj");
static const string strsecret2     ("5kc4ejrdjv152fgwp386vd1i2nyc5kkfsmyv1ngy1vgdxghqvy3");
static const string strsecret1c    ("kwr371tja9u2rfsmzjtnun2pxxp3wpzu2afrhtcta6kxeudm1vew");
static const string strsecret2c    ("l3hq7a8feqwjkw1m2gnkdw28546vp5miewcczsqud9kcaxrjds3g");
static const cmoorecoinaddress addr1 ("1qfqqmud55zv3pjejztakcsqmjlt6jkjvj");
static const cmoorecoinaddress addr2 ("1f5y5e5fmc5yzdjtb9hlaue43gdxekxenj");
static const cmoorecoinaddress addr1c("1nojrossxpbkfchujxt4hadjrxre9fxiqs");
static const cmoorecoinaddress addr2c("1crj2hym1cxwzhaxlqtiglyggnt9wqqsds");


static const string straddressbad("1hv9lc3snhzxwj4zk6fb38tembryq2cbif");


#ifdef key_tests_dumpinfo
void dumpkeyinfo(uint256 privkey)
{
    ckey key;
    key.resize(32);
    memcpy(&secret[0], &privkey, 32);
    vector<unsigned char> sec;
    sec.resize(32);
    memcpy(&sec[0], &secret[0], 32);
    printf("  * secret (hex): %s\n", hexstr(sec).c_str());

    for (int ncompressed=0; ncompressed<2; ncompressed++)
    {
        bool fcompressed = ncompressed == 1;
        printf("  * %s:\n", fcompressed ? "compressed" : "uncompressed");
        cmoorecoinsecret bsecret;
        bsecret.setsecret(secret, fcompressed);
        printf("    * secret (base58): %s\n", bsecret.tostring().c_str());
        ckey key;
        key.setsecret(secret, fcompressed);
        vector<unsigned char> vchpubkey = key.getpubkey();
        printf("    * pubkey (hex): %s\n", hexstr(vchpubkey).c_str());
        printf("    * address (base58): %s\n", cmoorecoinaddress(vchpubkey).tostring().c_str());
    }
}
#endif


boost_fixture_test_suite(key_tests, basictestingsetup)

boost_auto_test_case(key_test1)
{
    cmoorecoinsecret bsecret1, bsecret2, bsecret1c, bsecret2c, baddress1;
    boost_check( bsecret1.setstring (strsecret1));
    boost_check( bsecret2.setstring (strsecret2));
    boost_check( bsecret1c.setstring(strsecret1c));
    boost_check( bsecret2c.setstring(strsecret2c));
    boost_check(!baddress1.setstring(straddressbad));

    ckey key1  = bsecret1.getkey();
    boost_check(key1.iscompressed() == false);
    ckey key2  = bsecret2.getkey();
    boost_check(key2.iscompressed() == false);
    ckey key1c = bsecret1c.getkey();
    boost_check(key1c.iscompressed() == true);
    ckey key2c = bsecret2c.getkey();
    boost_check(key2c.iscompressed() == true);

    cpubkey pubkey1  = key1. getpubkey();
    cpubkey pubkey2  = key2. getpubkey();
    cpubkey pubkey1c = key1c.getpubkey();
    cpubkey pubkey2c = key2c.getpubkey();

    boost_check(key1.verifypubkey(pubkey1));
    boost_check(!key1.verifypubkey(pubkey1c));
    boost_check(!key1.verifypubkey(pubkey2));
    boost_check(!key1.verifypubkey(pubkey2c));

    boost_check(!key1c.verifypubkey(pubkey1));
    boost_check(key1c.verifypubkey(pubkey1c));
    boost_check(!key1c.verifypubkey(pubkey2));
    boost_check(!key1c.verifypubkey(pubkey2c));

    boost_check(!key2.verifypubkey(pubkey1));
    boost_check(!key2.verifypubkey(pubkey1c));
    boost_check(key2.verifypubkey(pubkey2));
    boost_check(!key2.verifypubkey(pubkey2c));

    boost_check(!key2c.verifypubkey(pubkey1));
    boost_check(!key2c.verifypubkey(pubkey1c));
    boost_check(!key2c.verifypubkey(pubkey2));
    boost_check(key2c.verifypubkey(pubkey2c));

    boost_check(addr1.get()  == ctxdestination(pubkey1.getid()));
    boost_check(addr2.get()  == ctxdestination(pubkey2.getid()));
    boost_check(addr1c.get() == ctxdestination(pubkey1c.getid()));
    boost_check(addr2c.get() == ctxdestination(pubkey2c.getid()));

    for (int n=0; n<16; n++)
    {
        string strmsg = strprintf("very secret message %i: 11", n);
        uint256 hashmsg = hash(strmsg.begin(), strmsg.end());

        // normal signatures

        vector<unsigned char> sign1, sign2, sign1c, sign2c;

        boost_check(key1.sign (hashmsg, sign1));
        boost_check(key2.sign (hashmsg, sign2));
        boost_check(key1c.sign(hashmsg, sign1c));
        boost_check(key2c.sign(hashmsg, sign2c));

        boost_check( pubkey1.verify(hashmsg, sign1));
        boost_check(!pubkey1.verify(hashmsg, sign2));
        boost_check( pubkey1.verify(hashmsg, sign1c));
        boost_check(!pubkey1.verify(hashmsg, sign2c));

        boost_check(!pubkey2.verify(hashmsg, sign1));
        boost_check( pubkey2.verify(hashmsg, sign2));
        boost_check(!pubkey2.verify(hashmsg, sign1c));
        boost_check( pubkey2.verify(hashmsg, sign2c));

        boost_check( pubkey1c.verify(hashmsg, sign1));
        boost_check(!pubkey1c.verify(hashmsg, sign2));
        boost_check( pubkey1c.verify(hashmsg, sign1c));
        boost_check(!pubkey1c.verify(hashmsg, sign2c));

        boost_check(!pubkey2c.verify(hashmsg, sign1));
        boost_check( pubkey2c.verify(hashmsg, sign2));
        boost_check(!pubkey2c.verify(hashmsg, sign1c));
        boost_check( pubkey2c.verify(hashmsg, sign2c));

        // compact signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1c, csign2c;

        boost_check(key1.signcompact (hashmsg, csign1));
        boost_check(key2.signcompact (hashmsg, csign2));
        boost_check(key1c.signcompact(hashmsg, csign1c));
        boost_check(key2c.signcompact(hashmsg, csign2c));

        cpubkey rkey1, rkey2, rkey1c, rkey2c;

        boost_check(rkey1.recovercompact (hashmsg, csign1));
        boost_check(rkey2.recovercompact (hashmsg, csign2));
        boost_check(rkey1c.recovercompact(hashmsg, csign1c));
        boost_check(rkey2c.recovercompact(hashmsg, csign2c));

        boost_check(rkey1  == pubkey1);
        boost_check(rkey2  == pubkey2);
        boost_check(rkey1c == pubkey1c);
        boost_check(rkey2c == pubkey2c);
    }

    // test deterministic signing

    std::vector<unsigned char> detsig, detsigc;
    string strmsg = "very deterministic message";
    uint256 hashmsg = hash(strmsg.begin(), strmsg.end());
    boost_check(key1.sign(hashmsg, detsig));
    boost_check(key1c.sign(hashmsg, detsigc));
    boost_check(detsig == detsigc);
    boost_check(detsig == parsehex("304402205dbbddda71772d95ce91cd2d14b592cfbc1dd0aabd6a394b6c2d377bbe59d31d022014ddda21494a4e221f0824f0b8b924c43fa43c0ad57dccdaa11f81a6bd4582f6"));
    boost_check(key2.sign(hashmsg, detsig));
    boost_check(key2c.sign(hashmsg, detsigc));
    boost_check(detsig == detsigc);
    boost_check(detsig == parsehex("3044022052d8a32079c11e79db95af63bb9600c5b04f21a9ca33dc129c2bfa8ac9dc1cd5022061d8ae5e0f6c1a16bde3719c64c2fd70e404b6428ab9a69566962e8771b5944d"));
    boost_check(key1.signcompact(hashmsg, detsig));
    boost_check(key1c.signcompact(hashmsg, detsigc));
    boost_check(detsig == parsehex("1c5dbbddda71772d95ce91cd2d14b592cfbc1dd0aabd6a394b6c2d377bbe59d31d14ddda21494a4e221f0824f0b8b924c43fa43c0ad57dccdaa11f81a6bd4582f6"));
    boost_check(detsigc == parsehex("205dbbddda71772d95ce91cd2d14b592cfbc1dd0aabd6a394b6c2d377bbe59d31d14ddda21494a4e221f0824f0b8b924c43fa43c0ad57dccdaa11f81a6bd4582f6"));
    boost_check(key2.signcompact(hashmsg, detsig));
    boost_check(key2c.signcompact(hashmsg, detsigc));
    boost_check(detsig == parsehex("1c52d8a32079c11e79db95af63bb9600c5b04f21a9ca33dc129c2bfa8ac9dc1cd561d8ae5e0f6c1a16bde3719c64c2fd70e404b6428ab9a69566962e8771b5944d"));
    boost_check(detsigc == parsehex("2052d8a32079c11e79db95af63bb9600c5b04f21a9ca33dc129c2bfa8ac9dc1cd561d8ae5e0f6c1a16bde3719c64c2fd70e404b6428ab9a69566962e8771b5944d"));
}

boost_auto_test_suite_end()
