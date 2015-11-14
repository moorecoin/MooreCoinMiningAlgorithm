// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "hash.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef enable_wallet
#include "wallet/wallet.h"
#endif

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//
// moorecoinminer
//

//
// unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. when we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// the corphan class keeps track of these 'temporary orphans' while
// createblock is figuring out which transactions to include.
//
class corphan
{
public:
    const ctransaction* ptx;
    set<uint256> setdependson;
    cfeerate feerate;
    double dpriority;

    corphan(const ctransaction* ptxin) : ptx(ptxin), feerate(0), dpriority(0)
    {
    }
};

uint64_t nlastblocktx = 0;
uint64_t nlastblocksize = 0;

// we want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, cfeerate, const ctransaction*> txpriority;
class txprioritycompare
{
    bool byfee;

public:
    txprioritycompare(bool _byfee) : byfee(_byfee) { }

    bool operator()(const txpriority& a, const txpriority& b)
    {
        if (byfee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void updatetime(cblockheader* pblock, const consensus::params& consensusparams, const cblockindex* pindexprev)
{
    pblock->ntime = std::max(pindexprev->getmediantimepast()+1, getadjustedtime());

    // updating time can change work required on testnet:
    if (consensusparams.fpowallowmindifficultyblocks)
        pblock->nbits = getnextworkrequired(pindexprev, pblock, consensusparams);
}

cblocktemplate* createnewblock(const cscript& scriptpubkeyin)
{
    const cchainparams& chainparams = params();
    // create new block
    auto_ptr<cblocktemplate> pblocktemplate(new cblocktemplate());
    if(!pblocktemplate.get())
        return null;
    cblock *pblock = &pblocktemplate->block; // pointer for convenience

    // -regtest only: allow overriding block.nversion with
    // -blockversion=n to test forking scenarios
    if (params().mineblocksondemand())
        pblock->nversion = getarg("-blockversion", pblock->nversion);

    // create coinbase tx
    cmutabletransaction txnew;
    txnew.vin.resize(1);
    txnew.vin[0].prevout.setnull();
    txnew.vout.resize(1);
    txnew.vout[0].scriptpubkey = scriptpubkeyin;

    // add dummy coinbase tx as first transaction
    pblock->vtx.push_back(ctransaction());
    pblocktemplate->vtxfees.push_back(-1); // updated at end
    pblocktemplate->vtxsigops.push_back(-1); // updated at end

    // largest block you're willing to create:
    unsigned int nblockmaxsize = getarg("-blockmaxsize", default_block_max_size);
    // limit to betweeen 1k and max_block_size-1k for sanity:
    nblockmaxsize = std::max((unsigned int)1000, std::min((unsigned int)(max_block_size-1000), nblockmaxsize));

    // how much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nblockprioritysize = getarg("-blockprioritysize", default_block_priority_size);
    nblockprioritysize = std::min(nblockmaxsize, nblockprioritysize);

    // minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nblockminsize = getarg("-blockminsize", default_block_min_size);
    nblockminsize = std::min(nblockmaxsize, nblockminsize);

    // collect memory pool transactions into the block
    camount nfees = 0;

    {
        lock2(cs_main, mempool.cs);
        cblockindex* pindexprev = chainactive.tip();
        const int nheight = pindexprev->nheight + 1;
        pblock->ntime = getadjustedtime();
        ccoinsviewcache view(pcoinstip);

        // priority order to process transactions
        list<corphan> vorphan; // list memory doesn't move
        map<uint256, vector<corphan*> > mapdependers;
        bool fprintpriority = getboolarg("-printpriority", false);

        // this vector will be sorted into a priority queue:
        vector<txpriority> vecpriority;
        vecpriority.reserve(mempool.maptx.size());
        for (map<uint256, ctxmempoolentry>::iterator mi = mempool.maptx.begin();
             mi != mempool.maptx.end(); ++mi)
        {
            const ctransaction& tx = mi->second.gettx();
            if (tx.iscoinbase() || !isfinaltx(tx, nheight, pblock->ntime))
                continue;

            corphan* porphan = null;
            double dpriority = 0;
            camount ntotalin = 0;
            bool fmissinginputs = false;
            boost_foreach(const ctxin& txin, tx.vin)
            {
                // read prev transaction
                if (!view.havecoins(txin.prevout.hash))
                {
                    // this should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.maptx.count(txin.prevout.hash))
                    {
                        logprintf("error: mempool transaction missing input\n");
                        if (fdebug) assert("mempool transaction missing input" == 0);
                        fmissinginputs = true;
                        if (porphan)
                            vorphan.pop_back();
                        break;
                    }

                    // has to wait for dependencies
                    if (!porphan)
                    {
                        // use list for automatic deletion
                        vorphan.push_back(corphan(&tx));
                        porphan = &vorphan.back();
                    }
                    mapdependers[txin.prevout.hash].push_back(porphan);
                    porphan->setdependson.insert(txin.prevout.hash);
                    ntotalin += mempool.maptx[txin.prevout.hash].gettx().vout[txin.prevout.n].nvalue;
                    continue;
                }
                const ccoins* coins = view.accesscoins(txin.prevout.hash);
                assert(coins);

                camount nvaluein = coins->vout[txin.prevout.n].nvalue;
                ntotalin += nvaluein;

                int nconf = nheight - coins->nheight;

                dpriority += (double)nvaluein * nconf;
            }
            if (fmissinginputs) continue;

            // priority is sum(valuein * age) / modified_txsize
            unsigned int ntxsize = ::getserializesize(tx, ser_network, protocol_version);
            dpriority = tx.computepriority(dpriority, ntxsize);

            uint256 hash = tx.gethash();
            mempool.applydeltas(hash, dpriority, ntotalin);

            cfeerate feerate(ntotalin-tx.getvalueout(), ntxsize);

            if (porphan)
            {
                porphan->dpriority = dpriority;
                porphan->feerate = feerate;
            }
            else
                vecpriority.push_back(txpriority(dpriority, feerate, &mi->second.gettx()));
        }

        // collect transactions into block
        uint64_t nblocksize = 1000;
        uint64_t nblocktx = 0;
        int nblocksigops = 100;
        bool fsortedbyfee = (nblockprioritysize <= 0);

        txprioritycompare comparer(fsortedbyfee);
        std::make_heap(vecpriority.begin(), vecpriority.end(), comparer);

        while (!vecpriority.empty())
        {
            // take highest priority transaction off the priority queue:
            double dpriority = vecpriority.front().get<0>();
            cfeerate feerate = vecpriority.front().get<1>();
            const ctransaction& tx = *(vecpriority.front().get<2>());

            std::pop_heap(vecpriority.begin(), vecpriority.end(), comparer);
            vecpriority.pop_back();

            // size limits
            unsigned int ntxsize = ::getserializesize(tx, ser_network, protocol_version);
            if (nblocksize + ntxsize >= nblockmaxsize)
                continue;

            // legacy limits on sigops:
            unsigned int ntxsigops = getlegacysigopcount(tx);
            if (nblocksigops + ntxsigops >= max_block_sigops)
                continue;

            // skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.gethash();
            double dprioritydelta = 0;
            camount nfeedelta = 0;
            mempool.applydeltas(hash, dprioritydelta, nfeedelta);
            if (fsortedbyfee && (dprioritydelta <= 0) && (nfeedelta <= 0) && (feerate < ::minrelaytxfee) && (nblocksize + ntxsize >= nblockminsize))
                continue;

            // prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fsortedbyfee &&
                ((nblocksize + ntxsize >= nblockprioritysize) || !allowfree(dpriority)))
            {
                fsortedbyfee = true;
                comparer = txprioritycompare(fsortedbyfee);
                std::make_heap(vecpriority.begin(), vecpriority.end(), comparer);
            }

            if (!view.haveinputs(tx))
                continue;

            camount ntxfees = view.getvaluein(tx)-tx.getvalueout();

            ntxsigops += getp2shsigopcount(tx, view);
            if (nblocksigops + ntxsigops >= max_block_sigops)
                continue;

            // note that flags: we don't want to set mempool/isstandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            cvalidationstate state;
            if (!checkinputs(tx, state, view, true, mandatory_script_verify_flags, true))
                continue;

            updatecoins(tx, state, view, nheight);

            // added
            pblock->vtx.push_back(tx);
            pblocktemplate->vtxfees.push_back(ntxfees);
            pblocktemplate->vtxsigops.push_back(ntxsigops);
            nblocksize += ntxsize;
            ++nblocktx;
            nblocksigops += ntxsigops;
            nfees += ntxfees;

            if (fprintpriority)
            {
                logprintf("priority %.1f fee %s txid %s\n",
                    dpriority, feerate.tostring(), tx.gethash().tostring());
            }

            // add transactions that depend on this one to the priority queue
            if (mapdependers.count(hash))
            {
                boost_foreach(corphan* porphan, mapdependers[hash])
                {
                    if (!porphan->setdependson.empty())
                    {
                        porphan->setdependson.erase(hash);
                        if (porphan->setdependson.empty())
                        {
                            vecpriority.push_back(txpriority(porphan->dpriority, porphan->feerate, porphan->ptx));
                            std::push_heap(vecpriority.begin(), vecpriority.end(), comparer);
                        }
                    }
                }
            }
        }

        nlastblocktx = nblocktx;
        nlastblocksize = nblocksize;
        logprintf("createnewblock(): total size %u\n", nblocksize);

        // compute final coinbase transaction.
        txnew.vout[0].nvalue = nfees + getblocksubsidy(nheight, chainparams.getconsensus());
        txnew.vin[0].scriptsig = cscript() << nheight << op_0;
        pblock->vtx[0] = txnew;
        pblocktemplate->vtxfees[0] = -nfees;

        // fill in header
        pblock->hashprevblock  = pindexprev->getblockhash();
        updatetime(pblock, params().getconsensus(), pindexprev);
        pblock->nbits          = getnextworkrequired(pindexprev, pblock, params().getconsensus());
        pblock->nnonce         = 0;
        pblocktemplate->vtxsigops[0] = getlegacysigopcount(pblock->vtx[0]);

        cvalidationstate state;
        if (!testblockvalidity(state, *pblock, pindexprev, false, false))
            throw std::runtime_error("createnewblock(): testblockvalidity failed");
    }

    return pblocktemplate.release();
}

void incrementextranonce(cblock* pblock, cblockindex* pindexprev, unsigned int& nextranonce)
{
    // update nextranonce
    static uint256 hashprevblock;
    if (hashprevblock != pblock->hashprevblock)
    {
        nextranonce = 0;
        hashprevblock = pblock->hashprevblock;
    }
    ++nextranonce;
    unsigned int nheight = pindexprev->nheight+1; // height first in coinbase required for block.version=2
    cmutabletransaction tmoorecoinbase(pblock->vtx[0]);
    tmoorecoinbase.vin[0].scriptsig = (cscript() << nheight << cscriptnum(nextranonce)) + coinbase_flags;
    assert(tmoorecoinbase.vin[0].scriptsig.size() <= 100);

    pblock->vtx[0] = tmoorecoinbase;
    pblock->hashmerkleroot = pblock->buildmerkletree();
}

#ifdef enable_wallet
//////////////////////////////////////////////////////////////////////////////
//
// internal miner
//

//
// scanhash scans nonces looking for a hash with at least some zero bits.
// the nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nnonce starts over at
// zero.
//
bool static scanhash(const cblockheader *pblock, uint32_t& nnonce, uint256 *phash)
{
    // write the first 76 bytes of the block header to a double-sha256 state.
    chash256 hasher;
    cdatastream ss(ser_network, protocol_version);
    ss << *pblock;
    assert(ss.size() == 80);
    hasher.write((unsigned char*)&ss[0], 76);

    while (true) {
        nnonce++;

        // write the last 4 bytes of the block header (the nonce) to a copy of
        // the double-sha256 state, and compute the result.
        chash256(hasher).write((unsigned char*)&nnonce, 4).finalize((unsigned char*)phash);

        // return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((uint16_t*)phash)[15] == 0)
            return true;

        // if nothing found after trying for a while, return -1
        if ((nnonce & 0xfff) == 0)
            return false;
    }
}

cblocktemplate* createnewblockwithkey(creservekey& reservekey)
{
    cpubkey pubkey;
    if (!reservekey.getreservedkey(pubkey))
        return null;

    cscript scriptpubkey = cscript() << tobytevector(pubkey) << op_checksig;
    return createnewblock(scriptpubkey);
}

static bool processblockfound(cblock* pblock, cwallet& wallet, creservekey& reservekey)
{
    logprintf("%s\n", pblock->tostring());
    logprintf("generated %s\n", formatmoney(pblock->vtx[0].vout[0].nvalue));

    // found a solution
    {
        lock(cs_main);
        if (pblock->hashprevblock != chainactive.tip()->getblockhash())
            return error("moorecoinminer: generated block is stale");
    }

    // remove key from key pool
    reservekey.keepkey();

    // track how many getdata requests this block gets
    {
        lock(wallet.cs_wallet);
        wallet.maprequestcount[pblock->gethash()] = 0;
    }

    // process this block the same as if we had received it from another node
    cvalidationstate state;
    if (!processnewblock(state, null, pblock, true, null))
        return error("moorecoinminer: processnewblock, block not accepted");

    return true;
}

void static moorecoinminer(cwallet *pwallet)
{
    logprintf("moorecoinminer started\n");
    setthreadpriority(thread_priority_lowest);
    renamethread("moorecoin-miner");
    const cchainparams& chainparams = params();

    // each thread has its own key and counter
    creservekey reservekey(pwallet);
    unsigned int nextranonce = 0;

    try {
        while (true) {
            if (chainparams.miningrequirespeers()) {
                // busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. in regtest mode we expect to fly solo.
                do {
                    bool fvnodesempty;
                    {
                        lock(cs_vnodes);
                        fvnodesempty = vnodes.empty();
                    }
                    if (!fvnodesempty && !isinitialblockdownload())
                        break;
                    millisleep(1000);
                } while (true);
            }

            //
            // create new block
            //
            unsigned int ntransactionsupdatedlast = mempool.gettransactionsupdated();
            cblockindex* pindexprev = chainactive.tip();

            auto_ptr<cblocktemplate> pblocktemplate(createnewblockwithkey(reservekey));
            if (!pblocktemplate.get())
            {
                logprintf("error in moorecoinminer: keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            cblock *pblock = &pblocktemplate->block;
            incrementextranonce(pblock, pindexprev, nextranonce);

            logprintf("running moorecoinminer with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::getserializesize(*pblock, ser_network, protocol_version));

            //
            // search
            //
            int64_t nstart = gettime();
            arith_uint256 hashtarget = arith_uint256().setcompact(pblock->nbits);
            uint256 hash;
            uint32_t nnonce = 0;
            while (true) {
                // check if something found
                if (scanhash(pblock, nnonce, &hash))
                {
                    if (uinttoarith256(hash) <= hashtarget)
                    {
                        // found a solution
                        pblock->nnonce = nnonce;
                        assert(hash == pblock->gethash());

                        setthreadpriority(thread_priority_normal);
                        logprintf("moorecoinminer:\n");
                        logprintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.gethex(), hashtarget.gethex());
                        processblockfound(pblock, *pwallet, reservekey);
                        setthreadpriority(thread_priority_lowest);

                        // in regression test mode, stop mining after a block is found.
                        if (chainparams.mineblocksondemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                }

                // check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // regtest mode doesn't require peers
                if (vnodes.empty() && chainparams.miningrequirespeers())
                    break;
                if (nnonce >= 0xffff0000)
                    break;
                if (mempool.gettransactionsupdated() != ntransactionsupdatedlast && gettime() - nstart > 60)
                    break;
                if (pindexprev != chainactive.tip())
                    break;

                // update ntime every few seconds
                updatetime(pblock, chainparams.getconsensus(), pindexprev);
                if (chainparams.getconsensus().fpowallowmindifficultyblocks)
                {
                    // changing pblock->ntime can change work required on testnet:
                    hashtarget.setcompact(pblock->nbits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        logprintf("moorecoinminer terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        logprintf("moorecoinminer runtime error: %s\n", e.what());
        return;
    }
}

void generatemoorecoins(bool fgenerate, cwallet* pwallet, int nthreads)
{
    static boost::thread_group* minerthreads = null;

    if (nthreads < 0) {
        // in regtest threads defaults to 1
        if (params().defaultminerthreads())
            nthreads = params().defaultminerthreads();
        else
            nthreads = boost::thread::hardware_concurrency();
    }

    if (minerthreads != null)
    {
        minerthreads->interrupt_all();
        delete minerthreads;
        minerthreads = null;
    }

    if (nthreads == 0 || !fgenerate)
        return;

    minerthreads = new boost::thread_group();
    for (int i = 0; i < nthreads; i++)
        minerthreads->create_thread(boost::bind(&moorecoinminer, pwallet));
}

#endif // enable_wallet
