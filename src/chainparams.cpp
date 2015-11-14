// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;

#include "chainparamsseeds.h"

/**
 * main network
 */
/**
 * what makes a good checkpoint block?
 * + is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + contains no strange transactions
 */

class cmainparams : public cchainparams {
public:
    cmainparams() {
        strnetworkid = "main";
        consensus.nsubsidyhalvinginterval = 210000;
        consensus.nmajorityenforceblockupgrade = 750;
        consensus.nmajorityrejectblockoutdated = 950;
        consensus.nmajoritywindow = 1000;
        consensus.powlimit = uint256s("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.npowtargettimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.npowtargetspacing = 10 * 60;
        consensus.fpowallowmindifficultyblocks = false;
        /** 
         * the message start string is designed to be unlikely to occur in normal data.
         * the characters are rarely used upper ascii, not valid as utf-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchmessagestart[0] = 0xf9;
        pchmessagestart[1] = 0xbe;
        pchmessagestart[2] = 0xb4;
        pchmessagestart[3] = 0xd9;
        valertpubkey = parsehex("04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284");
        ndefaultport = 8333;
        nminerthreads = 0;
        npruneafterheight = 100000;

        /**
         * build the genesis block. note that the output of its generation
         * transaction cannot be spent since it did not originally exist in the
         * database.
         *
         * cblock(hash=000000000019d6, ver=1, hashprevblock=00000000000000, hashmerkleroot=4a5e1e, ntime=1231006505, nbits=1d00ffff, nnonce=2083236893, vtx=1)
         *   ctransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nlocktime=0)
         *     ctxin(coutpoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
         *     ctxout(nvalue=50.00000000, scriptpubkey=0x5f1df16b2b704c8a578d0b)
         *   vmerkletree: 4a5e1e
         */
        const char* psztimestamp = "the times 03/jan/2009 chancellor on brink of second bailout for banks";
        cmutabletransaction txnew;
        txnew.nversion = 1;
        txnew.vin.resize(1);
        txnew.vout.resize(1);
        txnew.vin[0].scriptsig = cscript() << 486604799 << cscriptnum(4) << vector<unsigned char>((const unsigned char*)psztimestamp, (const unsigned char*)psztimestamp + strlen(psztimestamp));
        txnew.vout[0].nvalue = 50 * coin;
        txnew.vout[0].scriptpubkey = cscript() << parsehex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << op_checksig;
        genesis.vtx.push_back(txnew);
        genesis.hashprevblock.setnull();
        genesis.hashmerkleroot = genesis.buildmerkletree();
        genesis.nversion = 1;
        genesis.ntime    = 1231006505;
        genesis.nbits    = 0x1d00ffff;
        genesis.nnonce   = 2083236893;

        consensus.hashgenesisblock = genesis.gethash();
        assert(consensus.hashgenesisblock == uint256s("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
        assert(genesis.hashmerkleroot == uint256s("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vseeds.push_back(cdnsseeddata("moorecoin.sipa.be", "seed.moorecoin.sipa.be")); // pieter wuille
        vseeds.push_back(cdnsseeddata("bluematt.me", "dnsseed.bluematt.me")); // matt corallo
        vseeds.push_back(cdnsseeddata("dashjr.org", "dnsseed.moorecoin.dashjr.org")); // luke dashjr
        vseeds.push_back(cdnsseeddata("moorecoinstats.com", "seed.moorecoinstats.com")); // christian decker
        vseeds.push_back(cdnsseeddata("xf2.org", "bitseed.xf2.org")); // jeff garzik
        vseeds.push_back(cdnsseeddata("moorecoin.jonasschnelli.ch", "seed.moorecoin.jonasschnelli.ch")); // jonas schnelli

        base58prefixes[pubkey_address] = std::vector<unsigned char>(1,0);
        base58prefixes[script_address] = std::vector<unsigned char>(1,5);
        base58prefixes[secret_key] =     std::vector<unsigned char>(1,128);
        base58prefixes[ext_public_key] = boost::assign::list_of(0x04)(0x88)(0xb2)(0x1e).convert_to_container<std::vector<unsigned char> >();
        base58prefixes[ext_secret_key] = boost::assign::list_of(0x04)(0x88)(0xad)(0xe4).convert_to_container<std::vector<unsigned char> >();

        vfixedseeds = std::vector<seedspec6>(pnseed6_main, pnseed6_main + arraylen(pnseed6_main));

        frequirerpcpassword = true;
        fminingrequirespeers = true;
        fdefaultconsistencychecks = false;
        frequirestandard = true;
        fmineblocksondemand = false;
        ftestnettobedeprecatedfieldrpc = false;

        checkpointdata = (checkpoints::ccheckpointdata) {
            boost::assign::map_list_of
            ( 11111, uint256s("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d"))
            ( 33333, uint256s("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6"))
            ( 74000, uint256s("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20"))
            (105000, uint256s("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97"))
            (134444, uint256s("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe"))
            (168000, uint256s("0x000000000000099e61ea72015e79632f216fe6cb33d7899acb35b75c8303b763"))
            (193000, uint256s("0x000000000000059f452a5f7340de6682a977387c17010ff6e6c3bd83ca8b1317"))
            (210000, uint256s("0x000000000000048b95347e83192f69cf0366076336c639f9b7228e9ba171342e"))
            (216116, uint256s("0x00000000000001b4f4b433e81ee46494af945cf96014816a4e2370f11b23df4e"))
            (225430, uint256s("0x00000000000001c108384350f74090433e7fcf79a606b8e797f065b130575932"))
            (250000, uint256s("0x000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214"))
            (279000, uint256s("0x0000000000000001ae8c72a0b0c301f67e3afca10e819efa9041e458e9bd7e40"))
            (295000, uint256s("0x00000000000000004d9b4ef50f0f9d686fd69db2e03af35a100370c64632a983")),
            1397080064, // * unix timestamp of last checkpoint block
            36544669,   // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the setbestchain debug.log lines)
            60000.0     // * estimated number of transactions per day after checkpoint
        };
    }
};
static cmainparams mainparams;

/**
 * testnet (v3)
 */
class ctestnetparams : public cmainparams {
public:
    ctestnetparams() {
        strnetworkid = "test";
        consensus.nmajorityenforceblockupgrade = 51;
        consensus.nmajorityrejectblockoutdated = 75;
        consensus.nmajoritywindow = 100;
        consensus.fpowallowmindifficultyblocks = true;
        pchmessagestart[0] = 0x0b;
        pchmessagestart[1] = 0x11;
        pchmessagestart[2] = 0x09;
        pchmessagestart[3] = 0x07;
        valertpubkey = parsehex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
        ndefaultport = 18333;
        nminerthreads = 0;
        npruneafterheight = 1000;

        //! modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.ntime = 1296688602;
        genesis.nnonce = 414098458;
        consensus.hashgenesisblock = genesis.gethash();
        assert(consensus.hashgenesisblock == uint256s("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"));

        vfixedseeds.clear();
        vseeds.clear();
        vseeds.push_back(cdnsseeddata("alexykot.me", "testnet-seed.alexykot.me"));
        vseeds.push_back(cdnsseeddata("moorecoin.petertodd.org", "testnet-seed.moorecoin.petertodd.org"));
        vseeds.push_back(cdnsseeddata("bluematt.me", "testnet-seed.bluematt.me"));
        vseeds.push_back(cdnsseeddata("moorecoin.schildbach.de", "testnet-seed.moorecoin.schildbach.de"));

        base58prefixes[pubkey_address] = std::vector<unsigned char>(1,111);
        base58prefixes[script_address] = std::vector<unsigned char>(1,196);
        base58prefixes[secret_key] =     std::vector<unsigned char>(1,239);
        base58prefixes[ext_public_key] = boost::assign::list_of(0x04)(0x35)(0x87)(0xcf).convert_to_container<std::vector<unsigned char> >();
        base58prefixes[ext_secret_key] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        vfixedseeds = std::vector<seedspec6>(pnseed6_test, pnseed6_test + arraylen(pnseed6_test));

        frequirerpcpassword = true;
        fminingrequirespeers = true;
        fdefaultconsistencychecks = false;
        frequirestandard = false;
        fmineblocksondemand = false;
        ftestnettobedeprecatedfieldrpc = true;

        checkpointdata = (checkpoints::ccheckpointdata) {
            boost::assign::map_list_of
            ( 546, uint256s("000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70")),
            1337966069,
            1488,
            300
        };

    }
};
static ctestnetparams testnetparams;

/**
 * regression test
 */
class cregtestparams : public ctestnetparams {
public:
    cregtestparams() {
        strnetworkid = "regtest";
        consensus.nsubsidyhalvinginterval = 150;
        consensus.nmajorityenforceblockupgrade = 750;
        consensus.nmajorityrejectblockoutdated = 950;
        consensus.nmajoritywindow = 1000;
        consensus.powlimit = uint256s("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        pchmessagestart[0] = 0xfa;
        pchmessagestart[1] = 0xbf;
        pchmessagestart[2] = 0xb5;
        pchmessagestart[3] = 0xda;
        nminerthreads = 1;
        genesis.ntime = 1296688602;
        genesis.nbits = 0x207fffff;
        genesis.nnonce = 2;
        consensus.hashgenesisblock = genesis.gethash();
        ndefaultport = 18444;
        assert(consensus.hashgenesisblock == uint256s("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
        npruneafterheight = 1000;

        vfixedseeds.clear(); //! regtest mode doesn't have any fixed seeds.
        vseeds.clear();  //! regtest mode doesn't have any dns seeds.

        frequirerpcpassword = false;
        fminingrequirespeers = false;
        fdefaultconsistencychecks = true;
        frequirestandard = false;
        fmineblocksondemand = true;
        ftestnettobedeprecatedfieldrpc = false;

        checkpointdata = (checkpoints::ccheckpointdata){
            boost::assign::map_list_of
            ( 0, uint256s("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")),
            0,
            0,
            0
        };
    }
};
static cregtestparams regtestparams;

static cchainparams *pcurrentparams = 0;

const cchainparams &params() {
    assert(pcurrentparams);
    return *pcurrentparams;
}

cchainparams &params(cbasechainparams::network network) {
    switch (network) {
        case cbasechainparams::main:
            return mainparams;
        case cbasechainparams::testnet:
            return testnetparams;
        case cbasechainparams::regtest:
            return regtestparams;
        default:
            assert(false && "unimplemented network");
            return mainparams;
    }
}

void selectparams(cbasechainparams::network network) {
    selectbaseparams(network);
    pcurrentparams = &params(network);
}

bool selectparamsfromcommandline()
{
    cbasechainparams::network network = networkidfromcommandline();
    if (network == cbasechainparams::max_network_types)
        return false;

    selectparams(network);
    return true;
}
