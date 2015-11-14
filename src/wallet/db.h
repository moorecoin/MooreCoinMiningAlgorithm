// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_wallet_db_h
#define moorecoin_wallet_db_h

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "sync.h"
#include "version.h"

#include <map>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include <db_cxx.h>

extern unsigned int nwalletdbupdated;

class cdbenv
{
private:
    bool fdbenvinit;
    bool fmockdb;
    // don't change into boost::filesystem::path, as that can result in
    // shutdown problems/crashes caused by a static initialized internal pointer.
    std::string strpath;

    void envshutdown();

public:
    mutable ccriticalsection cs_db;
    dbenv *dbenv;
    std::map<std::string, int> mapfileusecount;
    std::map<std::string, db*> mapdb;

    cdbenv();
    ~cdbenv();
    void reset();

    void makemock();
    bool ismock() { return fmockdb; }

    /**
     * verify that database file strfile is ok. if it is not,
     * call the callback to try to recover.
     * this must be called before strfile is opened.
     * returns true if strfile is ok.
     */
    enum verifyresult { verify_ok,
                        recover_ok,
                        recover_fail };
    verifyresult verify(const std::string& strfile, bool (*recoverfunc)(cdbenv& dbenv, const std::string& strfile));
    /**
     * salvage data from a file that verify says is bad.
     * faggressive sets the db_aggressive flag (see berkeley db->verify() method documentation).
     * appends binary key/value pairs to vresult, returns true if successful.
     * note: reads the entire database into memory, so cannot be used
     * for huge databases.
     */
    typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > keyvalpair;
    bool salvage(const std::string& strfile, bool faggressive, std::vector<keyvalpair>& vresult);

    bool open(const boost::filesystem::path& path);
    void close();
    void flush(bool fshutdown);
    void checkpointlsn(const std::string& strfile);

    void closedb(const std::string& strfile);
    bool removedb(const std::string& strfile);

    dbtxn* txnbegin(int flags = db_txn_write_nosync)
    {
        dbtxn* ptxn = null;
        int ret = dbenv->txn_begin(null, &ptxn, flags);
        if (!ptxn || ret != 0)
            return null;
        return ptxn;
    }
};

extern cdbenv bitdb;


/** raii class that provides access to a berkeley database */
class cdb
{
protected:
    db* pdb;
    std::string strfile;
    dbtxn* activetxn;
    bool freadonly;
    bool fflushonclose;

    explicit cdb(const std::string& strfilename, const char* pszmode = "r+", bool fflushonclosein=true);
    ~cdb() { close(); }

public:
    void flush();
    void close();

private:
    cdb(const cdb&);
    void operator=(const cdb&);

protected:
    template <typename k, typename t>
    bool read(const k& key, t& value)
    {
        if (!pdb)
            return false;

        // key
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(1000);
        sskey << key;
        dbt datkey(&sskey[0], sskey.size());

        // read
        dbt datvalue;
        datvalue.set_flags(db_dbt_malloc);
        int ret = pdb->get(activetxn, &datkey, &datvalue, 0);
        memset(datkey.get_data(), 0, datkey.get_size());
        if (datvalue.get_data() == null)
            return false;

        // unserialize value
        try {
            cdatastream ssvalue((char*)datvalue.get_data(), (char*)datvalue.get_data() + datvalue.get_size(), ser_disk, client_version);
            ssvalue >> value;
        } catch (const std::exception&) {
            return false;
        }

        // clear and free memory
        memset(datvalue.get_data(), 0, datvalue.get_size());
        free(datvalue.get_data());
        return (ret == 0);
    }

    template <typename k, typename t>
    bool write(const k& key, const t& value, bool foverwrite = true)
    {
        if (!pdb)
            return false;
        if (freadonly)
            assert(!"write called on database in read-only mode");

        // key
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(1000);
        sskey << key;
        dbt datkey(&sskey[0], sskey.size());

        // value
        cdatastream ssvalue(ser_disk, client_version);
        ssvalue.reserve(10000);
        ssvalue << value;
        dbt datvalue(&ssvalue[0], ssvalue.size());

        // write
        int ret = pdb->put(activetxn, &datkey, &datvalue, (foverwrite ? 0 : db_nooverwrite));

        // clear memory in case it was a private key
        memset(datkey.get_data(), 0, datkey.get_size());
        memset(datvalue.get_data(), 0, datvalue.get_size());
        return (ret == 0);
    }

    template <typename k>
    bool erase(const k& key)
    {
        if (!pdb)
            return false;
        if (freadonly)
            assert(!"erase called on database in read-only mode");

        // key
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(1000);
        sskey << key;
        dbt datkey(&sskey[0], sskey.size());

        // erase
        int ret = pdb->del(activetxn, &datkey, 0);

        // clear memory
        memset(datkey.get_data(), 0, datkey.get_size());
        return (ret == 0 || ret == db_notfound);
    }

    template <typename k>
    bool exists(const k& key)
    {
        if (!pdb)
            return false;

        // key
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(1000);
        sskey << key;
        dbt datkey(&sskey[0], sskey.size());

        // exists
        int ret = pdb->exists(activetxn, &datkey, 0);

        // clear memory
        memset(datkey.get_data(), 0, datkey.get_size());
        return (ret == 0);
    }

    dbc* getcursor()
    {
        if (!pdb)
            return null;
        dbc* pcursor = null;
        int ret = pdb->cursor(null, &pcursor, 0);
        if (ret != 0)
            return null;
        return pcursor;
    }

    int readatcursor(dbc* pcursor, cdatastream& sskey, cdatastream& ssvalue, unsigned int fflags = db_next)
    {
        // read at cursor
        dbt datkey;
        if (fflags == db_set || fflags == db_set_range || fflags == db_get_both || fflags == db_get_both_range) {
            datkey.set_data(&sskey[0]);
            datkey.set_size(sskey.size());
        }
        dbt datvalue;
        if (fflags == db_get_both || fflags == db_get_both_range) {
            datvalue.set_data(&ssvalue[0]);
            datvalue.set_size(ssvalue.size());
        }
        datkey.set_flags(db_dbt_malloc);
        datvalue.set_flags(db_dbt_malloc);
        int ret = pcursor->get(&datkey, &datvalue, fflags);
        if (ret != 0)
            return ret;
        else if (datkey.get_data() == null || datvalue.get_data() == null)
            return 99999;

        // convert to streams
        sskey.settype(ser_disk);
        sskey.clear();
        sskey.write((char*)datkey.get_data(), datkey.get_size());
        ssvalue.settype(ser_disk);
        ssvalue.clear();
        ssvalue.write((char*)datvalue.get_data(), datvalue.get_size());

        // clear and free memory
        memset(datkey.get_data(), 0, datkey.get_size());
        memset(datvalue.get_data(), 0, datvalue.get_size());
        free(datkey.get_data());
        free(datvalue.get_data());
        return 0;
    }

public:
    bool txnbegin()
    {
        if (!pdb || activetxn)
            return false;
        dbtxn* ptxn = bitdb.txnbegin();
        if (!ptxn)
            return false;
        activetxn = ptxn;
        return true;
    }

    bool txncommit()
    {
        if (!pdb || !activetxn)
            return false;
        int ret = activetxn->commit(0);
        activetxn = null;
        return (ret == 0);
    }

    bool txnabort()
    {
        if (!pdb || !activetxn)
            return false;
        int ret = activetxn->abort();
        activetxn = null;
        return (ret == 0);
    }

    bool readversion(int& nversion)
    {
        nversion = 0;
        return read(std::string("version"), nversion);
    }

    bool writeversion(int nversion)
    {
        return write(std::string("version"), nversion);
    }

    bool static rewrite(const std::string& strfile, const char* pszskip = null);
};

#endif // moorecoin_wallet_db_h
