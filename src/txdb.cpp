// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "chainparams.h"
#include "hash.h"
#include "main.h"
#include "pow.h"
#include "uint256.h"

#include <stdint.h>

#include <boost/thread.hpp>

using namespace std;

static const char db_coins = 'c';
static const char db_block_files = 'f';
static const char db_txindex = 't';
static const char db_block_index = 'b';

static const char db_best_block = 'b';
static const char db_flag = 'f';
static const char db_reindex_flag = 'r';
static const char db_last_block = 'l';


void static batchwritecoins(cleveldbbatch &batch, const uint256 &hash, const ccoins &coins) {
    if (coins.ispruned())
        batch.erase(make_pair(db_coins, hash));
    else
        batch.write(make_pair(db_coins, hash), coins);
}

void static batchwritehashbestchain(cleveldbbatch &batch, const uint256 &hash) {
    batch.write(db_best_block, hash);
}

ccoinsviewdb::ccoinsviewdb(size_t ncachesize, bool fmemory, bool fwipe) : db(getdatadir() / "chainstate", ncachesize, fmemory, fwipe) {
}

bool ccoinsviewdb::getcoins(const uint256 &txid, ccoins &coins) const {
    return db.read(make_pair(db_coins, txid), coins);
}

bool ccoinsviewdb::havecoins(const uint256 &txid) const {
    return db.exists(make_pair(db_coins, txid));
}

uint256 ccoinsviewdb::getbestblock() const {
    uint256 hashbestchain;
    if (!db.read(db_best_block, hashbestchain))
        return uint256();
    return hashbestchain;
}

bool ccoinsviewdb::batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock) {
    cleveldbbatch batch;
    size_t count = 0;
    size_t changed = 0;
    for (ccoinsmap::iterator it = mapcoins.begin(); it != mapcoins.end();) {
        if (it->second.flags & ccoinscacheentry::dirty) {
            batchwritecoins(batch, it->first, it->second.coins);
            changed++;
        }
        count++;
        ccoinsmap::iterator itold = it++;
        mapcoins.erase(itold);
    }
    if (!hashblock.isnull())
        batchwritehashbestchain(batch, hashblock);

    logprint("coindb", "committing %u changed transactions (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return db.writebatch(batch);
}

cblocktreedb::cblocktreedb(size_t ncachesize, bool fmemory, bool fwipe) : cleveldbwrapper(getdatadir() / "blocks" / "index", ncachesize, fmemory, fwipe) {
}

bool cblocktreedb::readblockfileinfo(int nfile, cblockfileinfo &info) {
    return read(make_pair(db_block_files, nfile), info);
}

bool cblocktreedb::writereindexing(bool freindexing) {
    if (freindexing)
        return write(db_reindex_flag, '1');
    else
        return erase(db_reindex_flag);
}

bool cblocktreedb::readreindexing(bool &freindexing) {
    freindexing = exists(db_reindex_flag);
    return true;
}

bool cblocktreedb::readlastblockfile(int &nfile) {
    return read(db_last_block, nfile);
}

bool ccoinsviewdb::getstats(ccoinsstats &stats) const {
    /* it seems that there are no "const iterators" for leveldb.  since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    boost::scoped_ptr<leveldb::iterator> pcursor(const_cast<cleveldbwrapper*>(&db)->newiterator());
    pcursor->seektofirst();

    chashwriter ss(ser_gethash, protocol_version);
    stats.hashblock = getbestblock();
    ss << stats.hashblock;
    camount ntotalamount = 0;
    while (pcursor->valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::slice slkey = pcursor->key();
            cdatastream sskey(slkey.data(), slkey.data()+slkey.size(), ser_disk, client_version);
            char chtype;
            sskey >> chtype;
            if (chtype == db_coins) {
                leveldb::slice slvalue = pcursor->value();
                cdatastream ssvalue(slvalue.data(), slvalue.data()+slvalue.size(), ser_disk, client_version);
                ccoins coins;
                ssvalue >> coins;
                uint256 txhash;
                sskey >> txhash;
                ss << txhash;
                ss << varint(coins.nversion);
                ss << (coins.fcoinbase ? 'c' : 'n');
                ss << varint(coins.nheight);
                stats.ntransactions++;
                for (unsigned int i=0; i<coins.vout.size(); i++) {
                    const ctxout &out = coins.vout[i];
                    if (!out.isnull()) {
                        stats.ntransactionoutputs++;
                        ss << varint(i+1);
                        ss << out;
                        ntotalamount += out.nvalue;
                    }
                }
                stats.nserializedsize += 32 + slvalue.size();
                ss << varint(0);
            }
            pcursor->next();
        } catch (const std::exception& e) {
            return error("%s: deserialize or i/o error - %s", __func__, e.what());
        }
    }
    {
        lock(cs_main);
        stats.nheight = mapblockindex.find(stats.hashblock)->second->nheight;
    }
    stats.hashserialized = ss.gethash();
    stats.ntotalamount = ntotalamount;
    return true;
}

bool cblocktreedb::writebatchsync(const std::vector<std::pair<int, const cblockfileinfo*> >& fileinfo, int nlastfile, const std::vector<const cblockindex*>& blockinfo) {
    cleveldbbatch batch;
    for (std::vector<std::pair<int, const cblockfileinfo*> >::const_iterator it=fileinfo.begin(); it != fileinfo.end(); it++) {
        batch.write(make_pair(db_block_files, it->first), *it->second);
    }
    batch.write(db_last_block, nlastfile);
    for (std::vector<const cblockindex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.write(make_pair(db_block_index, (*it)->getblockhash()), cdiskblockindex(*it));
    }
    return writebatch(batch, true);
}

bool cblocktreedb::readtxindex(const uint256 &txid, cdisktxpos &pos) {
    return read(make_pair(db_txindex, txid), pos);
}

bool cblocktreedb::writetxindex(const std::vector<std::pair<uint256, cdisktxpos> >&vect) {
    cleveldbbatch batch;
    for (std::vector<std::pair<uint256,cdisktxpos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.write(make_pair(db_txindex, it->first), it->second);
    return writebatch(batch);
}

bool cblocktreedb::writeflag(const std::string &name, bool fvalue) {
    return write(std::make_pair(db_flag, name), fvalue ? '1' : '0');
}

bool cblocktreedb::readflag(const std::string &name, bool &fvalue) {
    char ch;
    if (!read(std::make_pair(db_flag, name), ch))
        return false;
    fvalue = ch == '1';
    return true;
}

bool cblocktreedb::loadblockindexguts()
{
    boost::scoped_ptr<leveldb::iterator> pcursor(newiterator());

    cdatastream sskeyset(ser_disk, client_version);
    sskeyset << make_pair(db_block_index, uint256());
    pcursor->seek(sskeyset.str());

    // load mapblockindex
    while (pcursor->valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::slice slkey = pcursor->key();
            cdatastream sskey(slkey.data(), slkey.data()+slkey.size(), ser_disk, client_version);
            char chtype;
            sskey >> chtype;
            if (chtype == db_block_index) {
                leveldb::slice slvalue = pcursor->value();
                cdatastream ssvalue(slvalue.data(), slvalue.data()+slvalue.size(), ser_disk, client_version);
                cdiskblockindex diskindex;
                ssvalue >> diskindex;

                // construct block index object
                cblockindex* pindexnew = insertblockindex(diskindex.getblockhash());
                pindexnew->pprev          = insertblockindex(diskindex.hashprev);
                pindexnew->nheight        = diskindex.nheight;
                pindexnew->nfile          = diskindex.nfile;
                pindexnew->ndatapos       = diskindex.ndatapos;
                pindexnew->nundopos       = diskindex.nundopos;
                pindexnew->nversion       = diskindex.nversion;
                pindexnew->hashmerkleroot = diskindex.hashmerkleroot;
                pindexnew->ntime          = diskindex.ntime;
                pindexnew->nbits          = diskindex.nbits;
                pindexnew->nnonce         = diskindex.nnonce;
                pindexnew->nstatus        = diskindex.nstatus;
                pindexnew->ntx            = diskindex.ntx;

                if (!checkproofofwork(pindexnew->getblockhash(), pindexnew->nbits, params().getconsensus()))
                    return error("loadblockindex(): checkproofofwork failed: %s", pindexnew->tostring());

                pcursor->next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (const std::exception& e) {
            return error("%s: deserialize or i/o error - %s", __func__, e.what());
        }
    }

    return true;
}
