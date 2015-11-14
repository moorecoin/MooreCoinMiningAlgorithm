// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"

#include "util.h"

#include <assert.h>

/**
 * main network
 */
class cbasemainparams : public cbasechainparams
{
public:
    cbasemainparams()
    {
        nrpcport = 8332;
    }
};
static cbasemainparams mainparams;

/**
 * testnet (v3)
 */
class cbasetestnetparams : public cbasemainparams
{
public:
    cbasetestnetparams()
    {
        nrpcport = 18332;
        strdatadir = "testnet3";
    }
};
static cbasetestnetparams testnetparams;

/*
 * regression test
 */
class cbaseregtestparams : public cbasetestnetparams
{
public:
    cbaseregtestparams()
    {
        strdatadir = "regtest";
    }
};
static cbaseregtestparams regtestparams;

/*
 * unit test
 */
class cbaseunittestparams : public cbasemainparams
{
public:
    cbaseunittestparams()
    {
        strdatadir = "unittest";
    }
};
static cbaseunittestparams unittestparams;

static cbasechainparams* pcurrentbaseparams = 0;

const cbasechainparams& baseparams()
{
    assert(pcurrentbaseparams);
    return *pcurrentbaseparams;
}

void selectbaseparams(cbasechainparams::network network)
{
    switch (network) {
    case cbasechainparams::main:
        pcurrentbaseparams = &mainparams;
        break;
    case cbasechainparams::testnet:
        pcurrentbaseparams = &testnetparams;
        break;
    case cbasechainparams::regtest:
        pcurrentbaseparams = &regtestparams;
        break;
    default:
        assert(false && "unimplemented network");
        return;
    }
}

cbasechainparams::network networkidfromcommandline()
{
    bool fregtest = getboolarg("-regtest", false);
    bool ftestnet = getboolarg("-testnet", false);

    if (ftestnet && fregtest)
        return cbasechainparams::max_network_types;
    if (fregtest)
        return cbasechainparams::regtest;
    if (ftestnet)
        return cbasechainparams::testnet;
    return cbasechainparams::main;
}

bool selectbaseparamsfromcommandline()
{
    cbasechainparams::network network = networkidfromcommandline();
    if (network == cbasechainparams::max_network_types)
        return false;

    selectbaseparams(network);
    return true;
}

bool arebaseparamsconfigured()
{
    return pcurrentbaseparams != null;
}
