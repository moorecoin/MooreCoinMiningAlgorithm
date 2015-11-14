// copyright (c) 2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "key.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_moorecoin.h"

#include <string>
#include <vector>

struct testderivation {
    std::string pub;
    std::string prv;
    unsigned int nchild;
};

struct testvector {
    std::string strhexmaster;
    std::vector<testderivation> vderive;

    testvector(std::string strhexmasterin) : strhexmaster(strhexmasterin) {}

    testvector& operator()(std::string pub, std::string prv, unsigned int nchild) {
        vderive.push_back(testderivation());
        testderivation &der = vderive.back();
        der.pub = pub;
        der.prv = prv;
        der.nchild = nchild;
        return *this;
    }
};

testvector test1 =
  testvector("000102030405060708090a0b0c0d0e0f")
    ("xpub661mymwaqrbcftxgs5syjabqqg9ylmc4q1rdap9gse8nqtwybghepy2gz29esfjqjocu1rupje8ytgqsefd265tmg7usudfdp6w1egmcet8",
     "xprv9s21zrqh143k3qtdl4lxw2f7hek3wjud2nw2nrk4stbpy6cq3jppqjichkvvvnkmpgjxwutg6lnf5kejmrnnu3tgtrbejgk33yugbxrmphi",
     0x80000000)
    ("xpub68gmy5edvgibqvfpdqkbbchxa5htiqg55crxyuxoqrkfdbfa1wejwgp6lhhwbzenk1vtsftfuhcdrfp1bgwq9xv5ski8px9rl2dzxvggdnw",
     "xprv9uhrzzhk6kajc1avxpdap4mdc3sqknxdipvvkx8br5nglnv1txvuxt4cv1rgl5hj6kcesndyuhd7owgt11ezg7xnxhrnyesvkzy7d2bhkj7",
     1)
    ("xpub6asuarnxkpbfewhqn6e3mwbcdtgzisqn1wxn9bjcm47ssikhjjf3ufhkknawbwmigj7wf5umash7syyq527hqck2axyysaa7xmalppuckwq",
     "xprv9wtymmfdv23n2tdng573qoesfrrwkqgweibmlntzniatzvr9bmlnvsxqu53kw1umypxlgboyzqaxwtcg8msy3h2eu4pwcqdnrnrva1xe8fs",
     0x80000002)
    ("xpub6d4bdpcp2gt577vvch3r8wdksczwzqzmmum3pwbmwvvjrzwqy4vungqfjpmm3no2dfdfgtsxxpg5ujh7n7epu4trkrx7x7dogt5uv6fclw5",
     "xprv9z4pot5vbttmtdrtwfwqmoh1taj2axgvzfqsb8c9xaxkymcfzxbdptwmt7fwuezg3ryjh4ktypqsaewrinmjanttpgp4mltj34bhnzx7uim",
     2)
    ("xpub6fha3pjlck84bayejxfw2sp4xrrfd1jynxeleu8eqn3vdfzmbqbqagjayiljtawm6zlrqumv1zactj37sr62cfn7fe5jnj7dh8zl4fiylhv",
     "xprva2jdekcsnnzky6ubcvivfjskyq1mdyahrjijr5idh2wwlsed4hsb2tyh8rfqmuph7f7rtyzttdrbdqqsunu5mm3wdvuakrhsc34sj7in334",
     1000000000)
    ("xpub6h1lxwlakswfhvm6rvpel9p4kfrzsw7abd2ttkwp3ssqvnya8fsvqntecyfgjs2uafcxuphiykro49s8ygastvxeybvpamhgw6cfjodrthy",
     "xprva41z7zogvvwxvsgdkuhdy1skmdb533pjdz7j6n6mv6us3ze1ai8fha8kmhscgpwmj4wgglyqjgpie1rfsruouihuzrepsl39unde3bbdu76",
     0);

testvector test2 =
  testvector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("xpub661mymwaqrbcfw31yewpkmuc5thy2pst5bdmsktwqcff8syamruapscgu8ed9w6odmsgv6zz8idoc4a6mr8bdztjy47ljhkj8ub7wegudub",
     "xprv9s21zrqh143k31xysdqppdxsxrtucvj2inhm5nutrgigg5e2dtalgdso3pgz6ssrdk4pfmm8nspsbhnqpqm55qn3lqftt2emdexvysczc2u",
     0)
    ("xpub69h7f5d8ksrgmmdjg2khpak8sr3djmwadkxj3zuxv27cprr9lgpeygmxubc6wb7erfvrnkzjxoummdznezpbzb7ap6r1d3tgfxhmwmkqtph",
     "xprv9vhkqa6ev4spzhyqzznht2nptpcjkudkgy38fbwlvgadx45zo9wqrut3dkynjwih2yjd9mkrocezxo1ex8g81dwsm1fwqwpwkes3v86pgkt",
     0xffffffff)
    ("xpub6asavgeehlbnwdqv6ukmhvzgqag8gr6riv3fxxpj8ksbh9ebxaeyblz85ysdhkildbrqsarlq1unrts8rujihjadmbu4zn9h8lznnbc5y4a",
     "xprv9wsp6b7kry3vj9m1zsnlvn3xh8rdspp1mh7faar7arlcqmktr2vidyeeeg2muctawcd6vnxvrcjfy2krgvsfawnzmjuhc2ymyrmagcepdu9",
     1)
    ("xpub6df8uhdarytz3fwda8tvfsvvah8dp3283my7p2v4see2wywmg5mg5ewvvmdmvcqconjxgowau9dcwh89lojfz537wtfunkau47el2dhhkon",
     "xprv9zfnwc6h2clgpmsa46vutjzbcfj8yajgg8cx1e5stjh45bbciytrxsd25uepvuesf9yog62tgaqthjxajppdbrchuws6t8xa2eckaddw4ef",
     0xfffffffe)
    ("xpub6erapfzwunrhlckdtchtcxd75rbzs1ed54g1lkbuhqvhqkqhmkhgbmjbzrkrgzw4koxb5jahwky4alhy2grbgrjadmzqlcgjvljuzzvrcel",
     "xprva1rpra33e1jq7ifknaktfpgnxpmw2yvmhqlqymmrj4xjxxwypdps3xz7iaxn8l39njgvyuosexzu6rcxflj8hfstjsyqblynmpcqe2vbfwc",
     2)
    ("xpub6fncn6nszzaw5tw7cgr9bi15uv96glzhjdstkxxxvclsuxbgxpdsnlfbdpq8p9hmgsapme5hqtz3emm2rny5agb9rxpvgyy3bdw6eegatqt",
     "xprva2nrnbfzabcdryrewet9ea4lvtjcgsqrmzxhx98mmrotbir7yrkcexw7nadnhm8dq38egfsh6dqa9qwtyefmlecbyjuuekgw4bypjcr9e7j",
     0);

void runtest(const testvector &test) {
    std::vector<unsigned char> seed = parsehex(test.strhexmaster);
    cextkey key;
    cextpubkey pubkey;
    key.setmaster(&seed[0], seed.size());
    pubkey = key.neuter();
    boost_foreach(const testderivation &derive, test.vderive) {
        unsigned char data[74];
        key.encode(data);
        pubkey.encode(data);
        // test private key
        cmoorecoinextkey b58key; b58key.setkey(key);
        boost_check(b58key.tostring() == derive.prv);
        // test public key
        cmoorecoinextpubkey b58pubkey; b58pubkey.setkey(pubkey);
        boost_check(b58pubkey.tostring() == derive.pub);
        // derive new keys
        cextkey keynew;
        boost_check(key.derive(keynew, derive.nchild));
        cextpubkey pubkeynew = keynew.neuter();
        if (!(derive.nchild & 0x80000000)) {
            // compare with public derivation
            cextpubkey pubkeynew2;
            boost_check(pubkey.derive(pubkeynew2, derive.nchild));
            boost_check(pubkeynew == pubkeynew2);
        }
        key = keynew;
        pubkey = pubkeynew;
    }
}

boost_fixture_test_suite(bip32_tests, basictestingsetup)

boost_auto_test_case(bip32_test1) {
    runtest(test1);
}

boost_auto_test_case(bip32_test2) {
    runtest(test2);
}

boost_auto_test_suite_end()
