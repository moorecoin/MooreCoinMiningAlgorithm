// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_wallet_walletdb_h
#define moorecoin_wallet_walletdb_h

#include "amount.h"
#include "wallet/db.h"
#include "key.h"
#include "keystore.h"

#include <list>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

class caccount;
class caccountingentry;
struct cblocklocator;
class ckeypool;
class cmasterkey;
class cscript;
class cwallet;
class cwallettx;
class uint160;
class uint256;

/** error statuses for the wallet database */
enum dberrors
{
    db_load_ok,
    db_corrupt,
    db_noncritical_error,
    db_too_new,
    db_load_fail,
    db_need_rewrite
};

class ckeymetadata
{
public:
    static const int current_version=1;
    int nversion;
    int64_t ncreatetime; // 0 means unknown

    ckeymetadata()
    {
        setnull();
    }
    ckeymetadata(int64_t ncreatetime_)
    {
        nversion = ckeymetadata::current_version;
        ncreatetime = ncreatetime_;
    }

    add_serialize_methods;

    template <typename stream, typename operation>
    inline void serializationop(stream& s, operation ser_action, int ntype, int nversion) {
        readwrite(this->nversion);
        nversion = this->nversion;
        readwrite(ncreatetime);
    }

    void setnull()
    {
        nversion = ckeymetadata::current_version;
        ncreatetime = 0;
    }
};

/** access to the wallet database (wallet.dat) */
class cwalletdb : public cdb
{
public:
    cwalletdb(const std::string& strfilename, const char* pszmode = "r+", bool fflushonclose = true) : cdb(strfilename, pszmode, fflushonclose)
    {
    }

    bool writename(const std::string& straddress, const std::string& strname);
    bool erasename(const std::string& straddress);

    bool writepurpose(const std::string& straddress, const std::string& purpose);
    bool erasepurpose(const std::string& straddress);

    bool writetx(uint256 hash, const cwallettx& wtx);
    bool erasetx(uint256 hash);

    bool writekey(const cpubkey& vchpubkey, const cprivkey& vchprivkey, const ckeymetadata &keymeta);
    bool writecryptedkey(const cpubkey& vchpubkey, const std::vector<unsigned char>& vchcryptedsecret, const ckeymetadata &keymeta);
    bool writemasterkey(unsigned int nid, const cmasterkey& kmasterkey);

    bool writecscript(const uint160& hash, const cscript& redeemscript);

    bool writewatchonly(const cscript &script);
    bool erasewatchonly(const cscript &script);

    bool writebestblock(const cblocklocator& locator);
    bool readbestblock(cblocklocator& locator);

    bool writeorderposnext(int64_t norderposnext);

    bool writedefaultkey(const cpubkey& vchpubkey);

    bool readpool(int64_t npool, ckeypool& keypool);
    bool writepool(int64_t npool, const ckeypool& keypool);
    bool erasepool(int64_t npool);

    bool writeminversion(int nversion);

    bool readaccount(const std::string& straccount, caccount& account);
    bool writeaccount(const std::string& straccount, const caccount& account);

    /// write destination data key,value tuple to database
    bool writedestdata(const std::string &address, const std::string &key, const std::string &value);
    /// erase destination data tuple from wallet database
    bool erasedestdata(const std::string &address, const std::string &key);

    bool writeaccountingentry(const caccountingentry& acentry);
    camount getaccountcreditdebit(const std::string& straccount);
    void listaccountcreditdebit(const std::string& straccount, std::list<caccountingentry>& acentries);

    dberrors reordertransactions(cwallet* pwallet);
    dberrors loadwallet(cwallet* pwallet);
    dberrors findwallettx(cwallet* pwallet, std::vector<uint256>& vtxhash, std::vector<cwallettx>& vwtx);
    dberrors zapwallettx(cwallet* pwallet, std::vector<cwallettx>& vwtx);
    static bool recover(cdbenv& dbenv, const std::string& filename, bool fonlykeys);
    static bool recover(cdbenv& dbenv, const std::string& filename);

private:
    cwalletdb(const cwalletdb&);
    void operator=(const cwalletdb&);

    bool writeaccountingentry(const uint64_t naccentrynum, const caccountingentry& acentry);
};

bool backupwallet(const cwallet& wallet, const std::string& strdest);
void threadflushwalletdb(const std::string& strfile);

#endif // moorecoin_wallet_walletdb_h
