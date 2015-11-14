// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_txdb_h
#define moorecoin_txdb_h

#include "coins.h"
#include "leveldbwrapper.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class cblockfileinfo;
class cblockindex;
struct cdisktxpos;
class uint256;

//! -dbcache default (mib)
static const int64_t ndefaultdbcache = 100;
//! max. -dbcache in (mib)
static const int64_t nmaxdbcache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache in (mib)
static const int64_t nmindbcache = 4;

/** ccoinsview backed by the leveldb coin database (chainstate/) */
class ccoinsviewdb : public ccoinsview
{
protected:
    cleveldbwrapper db;
public:
    ccoinsviewdb(size_t ncachesize, bool fmemory = false, bool fwipe = false);

    bool getcoins(const uint256 &txid, ccoins &coins) const;
    bool havecoins(const uint256 &txid) const;
    uint256 getbestblock() const;
    bool batchwrite(ccoinsmap &mapcoins, const uint256 &hashblock);
    bool getstats(ccoinsstats &stats) const;
};

/** access to the block database (blocks/index/) */
class cblocktreedb : public cleveldbwrapper
{
public:
    cblocktreedb(size_t ncachesize, bool fmemory = false, bool fwipe = false);
private:
    cblocktreedb(const cblocktreedb&);
    void operator=(const cblocktreedb&);
public:
    bool writebatchsync(const std::vector<std::pair<int, const cblockfileinfo*> >& fileinfo, int nlastfile, const std::vector<const cblockindex*>& blockinfo);
    bool readblockfileinfo(int nfile, cblockfileinfo &fileinfo);
    bool readlastblockfile(int &nfile);
    bool writereindexing(bool freindex);
    bool readreindexing(bool &freindex);
    bool readtxindex(const uint256 &txid, cdisktxpos &pos);
    bool writetxindex(const std::vector<std::pair<uint256, cdisktxpos> > &list);
    bool writeflag(const std::string &name, bool fvalue);
    bool readflag(const std::string &name, bool &fvalue);
    bool loadblockindexguts();
};

#endif // moorecoin_txdb_h
