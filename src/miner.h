// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_miner_h
#define moorecoin_miner_h

#include "primitives/block.h"

#include <stdint.h>

class cblockindex;
class creservekey;
class cscript;
class cwallet;
namespace consensus { struct params; };

struct cblocktemplate
{
    cblock block;
    std::vector<camount> vtxfees;
    std::vector<int64_t> vtxsigops;
};

/** run the miner threads */
void generatemoorecoins(bool fgenerate, cwallet* pwallet, int nthreads);
/** generate a new block, without valid proof-of-work */
cblocktemplate* createnewblock(const cscript& scriptpubkeyin);
cblocktemplate* createnewblockwithkey(creservekey& reservekey);
/** modify the extranonce in a block */
void incrementextranonce(cblock* pblock, cblockindex* pindexprev, unsigned int& nextranonce);
void updatetime(cblockheader* pblock, const consensus::params& consensusparams, const cblockindex* pindexprev);

#endif // moorecoin_miner_h
