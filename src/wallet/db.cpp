// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "db.h"

#include "addrman.h"
#include "hash.h"
#include "protocol.h"
#include "util.h"
#include "utilstrencodings.h"

#include <stdint.h>

#ifndef win32
#include <sys/stat.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/version.hpp>

using namespace std;


unsigned int nwalletdbupdated;


//
// cdb
//

cdbenv bitdb;

void cdbenv::envshutdown()
{
    if (!fdbenvinit)
        return;

    fdbenvinit = false;
    int ret = dbenv->close(0);
    if (ret != 0)
        logprintf("cdbenv::envshutdown: error %d shutting down database environment: %s\n", ret, dbenv::strerror(ret));
    if (!fmockdb)
        dbenv(0).remove(strpath.c_str(), 0);
}

void cdbenv::reset()
{
    delete dbenv;
    dbenv = new dbenv(db_cxx_no_exceptions);
    fdbenvinit = false;
    fmockdb = false;
}

cdbenv::cdbenv() : dbenv(null)
{
    reset();
}

cdbenv::~cdbenv()
{
    envshutdown();
    delete dbenv;
    dbenv = null;
}

void cdbenv::close()
{
    envshutdown();
}

bool cdbenv::open(const boost::filesystem::path& pathin)
{
    if (fdbenvinit)
        return true;

    boost::this_thread::interruption_point();

    strpath = pathin.string();
    boost::filesystem::path pathlogdir = pathin / "database";
    trycreatedirectory(pathlogdir);
    boost::filesystem::path patherrorfile = pathin / "db.log";
    logprintf("cdbenv::open: logdir=%s errorfile=%s\n", pathlogdir.string(), patherrorfile.string());

    unsigned int nenvflags = 0;
    if (getboolarg("-privdb", true))
        nenvflags |= db_private;

    dbenv->set_lg_dir(pathlogdir.string().c_str());
    dbenv->set_cachesize(0, 0x100000, 1); // 1 mib should be enough for just the wallet
    dbenv->set_lg_bsize(0x10000);
    dbenv->set_lg_max(1048576);
    dbenv->set_lk_max_locks(40000);
    dbenv->set_lk_max_objects(40000);
    dbenv->set_errfile(fopen(patherrorfile.string().c_str(), "a")); /// debug
    dbenv->set_flags(db_auto_commit, 1);
    dbenv->set_flags(db_txn_write_nosync, 1);
    dbenv->log_set_config(db_log_auto_remove, 1);
    int ret = dbenv->open(strpath.c_str(),
                         db_create |
                             db_init_lock |
                             db_init_log |
                             db_init_mpool |
                             db_init_txn |
                             db_thread |
                             db_recover |
                             nenvflags,
                         s_irusr | s_iwusr);
    if (ret != 0)
        return error("cdbenv::open: error %d opening database environment: %s\n", ret, dbenv::strerror(ret));

    fdbenvinit = true;
    fmockdb = false;
    return true;
}

void cdbenv::makemock()
{
    if (fdbenvinit)
        throw runtime_error("cdbenv::makemock: already initialized");

    boost::this_thread::interruption_point();

    logprint("db", "cdbenv::makemock\n");

    dbenv->set_cachesize(1, 0, 1);
    dbenv->set_lg_bsize(10485760 * 4);
    dbenv->set_lg_max(10485760);
    dbenv->set_lk_max_locks(10000);
    dbenv->set_lk_max_objects(10000);
    dbenv->set_flags(db_auto_commit, 1);
    dbenv->log_set_config(db_log_in_memory, 1);
    int ret = dbenv->open(null,
                         db_create |
                             db_init_lock |
                             db_init_log |
                             db_init_mpool |
                             db_init_txn |
                             db_thread |
                             db_private,
                         s_irusr | s_iwusr);
    if (ret > 0)
        throw runtime_error(strprintf("cdbenv::makemock: error %d opening database environment.", ret));

    fdbenvinit = true;
    fmockdb = true;
}

cdbenv::verifyresult cdbenv::verify(const std::string& strfile, bool (*recoverfunc)(cdbenv& dbenv, const std::string& strfile))
{
    lock(cs_db);
    assert(mapfileusecount.count(strfile) == 0);

    db db(dbenv, 0);
    int result = db.verify(strfile.c_str(), null, null, 0);
    if (result == 0)
        return verify_ok;
    else if (recoverfunc == null)
        return recover_fail;

    // try to recover:
    bool frecovered = (*recoverfunc)(*this, strfile);
    return (frecovered ? recover_ok : recover_fail);
}

bool cdbenv::salvage(const std::string& strfile, bool faggressive, std::vector<cdbenv::keyvalpair>& vresult)
{
    lock(cs_db);
    assert(mapfileusecount.count(strfile) == 0);

    u_int32_t flags = db_salvage;
    if (faggressive)
        flags |= db_aggressive;

    stringstream strdump;

    db db(dbenv, 0);
    int result = db.verify(strfile.c_str(), null, &strdump, flags);
    if (result == db_verify_bad) {
        logprintf("cdbenv::salvage: database salvage found errors, all data may not be recoverable.\n");
        if (!faggressive) {
            logprintf("cdbenv::salvage: rerun with aggressive mode to ignore errors and continue.\n");
            return false;
        }
    }
    if (result != 0 && result != db_verify_bad) {
        logprintf("cdbenv::salvage: database salvage failed with result %d.\n", result);
        return false;
    }

    // format of bdb dump is ascii lines:
    // header lines...
    // header=end
    // hexadecimal key
    // hexadecimal value
    // ... repeated
    // data=end

    string strline;
    while (!strdump.eof() && strline != "header=end")
        getline(strdump, strline); // skip past header

    std::string keyhex, valuehex;
    while (!strdump.eof() && keyhex != "data=end") {
        getline(strdump, keyhex);
        if (keyhex != "data_end") {
            getline(strdump, valuehex);
            vresult.push_back(make_pair(parsehex(keyhex), parsehex(valuehex)));
        }
    }

    return (result == 0);
}


void cdbenv::checkpointlsn(const std::string& strfile)
{
    dbenv->txn_checkpoint(0, 0, 0);
    if (fmockdb)
        return;
    dbenv->lsn_reset(strfile.c_str(), 0);
}


cdb::cdb(const std::string& strfilename, const char* pszmode, bool fflushonclosein) : pdb(null), activetxn(null)
{
    int ret;
    freadonly = (!strchr(pszmode, '+') && !strchr(pszmode, 'w'));
    fflushonclose = fflushonclosein;
    if (strfilename.empty())
        return;

    bool fcreate = strchr(pszmode, 'c') != null;
    unsigned int nflags = db_thread;
    if (fcreate)
        nflags |= db_create;

    {
        lock(bitdb.cs_db);
        if (!bitdb.open(getdatadir()))
            throw runtime_error("cdb: failed to open database environment.");

        strfile = strfilename;
        ++bitdb.mapfileusecount[strfile];
        pdb = bitdb.mapdb[strfile];
        if (pdb == null) {
            pdb = new db(bitdb.dbenv, 0);

            bool fmockdb = bitdb.ismock();
            if (fmockdb) {
                dbmpoolfile* mpf = pdb->get_mpf();
                ret = mpf->set_flags(db_mpool_nofile, 1);
                if (ret != 0)
                    throw runtime_error(strprintf("cdb: failed to configure for no temp file backing for database %s", strfile));
            }

            ret = pdb->open(null,                               // txn pointer
                            fmockdb ? null : strfile.c_str(),   // filename
                            fmockdb ? strfile.c_str() : "main", // logical db name
                            db_btree,                           // database type
                            nflags,                             // flags
                            0);

            if (ret != 0) {
                delete pdb;
                pdb = null;
                --bitdb.mapfileusecount[strfile];
                strfile = "";
                throw runtime_error(strprintf("cdb: error %d, can't open database %s", ret, strfile));
            }

            if (fcreate && !exists(string("version"))) {
                bool ftmp = freadonly;
                freadonly = false;
                writeversion(client_version);
                freadonly = ftmp;
            }

            bitdb.mapdb[strfile] = pdb;
        }
    }
}

void cdb::flush()
{
    if (activetxn)
        return;

    // flush database activity from memory pool to disk log
    unsigned int nminutes = 0;
    if (freadonly)
        nminutes = 1;

    bitdb.dbenv->txn_checkpoint(nminutes ? getarg("-dblogsize", 100) * 1024 : 0, nminutes, 0);
}

void cdb::close()
{
    if (!pdb)
        return;
    if (activetxn)
        activetxn->abort();
    activetxn = null;
    pdb = null;

    if (fflushonclose)
        flush();

    {
        lock(bitdb.cs_db);
        --bitdb.mapfileusecount[strfile];
    }
}

void cdbenv::closedb(const string& strfile)
{
    {
        lock(cs_db);
        if (mapdb[strfile] != null) {
            // close the database handle
            db* pdb = mapdb[strfile];
            pdb->close(0);
            delete pdb;
            mapdb[strfile] = null;
        }
    }
}

bool cdbenv::removedb(const string& strfile)
{
    this->closedb(strfile);

    lock(cs_db);
    int rc = dbenv->dbremove(null, strfile.c_str(), null, db_auto_commit);
    return (rc == 0);
}

bool cdb::rewrite(const string& strfile, const char* pszskip)
{
    while (true) {
        {
            lock(bitdb.cs_db);
            if (!bitdb.mapfileusecount.count(strfile) || bitdb.mapfileusecount[strfile] == 0) {
                // flush log data to the dat file
                bitdb.closedb(strfile);
                bitdb.checkpointlsn(strfile);
                bitdb.mapfileusecount.erase(strfile);

                bool fsuccess = true;
                logprintf("cdb::rewrite: rewriting %s...\n", strfile);
                string strfileres = strfile + ".rewrite";
                { // surround usage of db with extra {}
                    cdb db(strfile.c_str(), "r");
                    db* pdbcopy = new db(bitdb.dbenv, 0);

                    int ret = pdbcopy->open(null,               // txn pointer
                                            strfileres.c_str(), // filename
                                            "main",             // logical db name
                                            db_btree,           // database type
                                            db_create,          // flags
                                            0);
                    if (ret > 0) {
                        logprintf("cdb::rewrite: can't create database file %s\n", strfileres);
                        fsuccess = false;
                    }

                    dbc* pcursor = db.getcursor();
                    if (pcursor)
                        while (fsuccess) {
                            cdatastream sskey(ser_disk, client_version);
                            cdatastream ssvalue(ser_disk, client_version);
                            int ret = db.readatcursor(pcursor, sskey, ssvalue, db_next);
                            if (ret == db_notfound) {
                                pcursor->close();
                                break;
                            } else if (ret != 0) {
                                pcursor->close();
                                fsuccess = false;
                                break;
                            }
                            if (pszskip &&
                                strncmp(&sskey[0], pszskip, std::min(sskey.size(), strlen(pszskip))) == 0)
                                continue;
                            if (strncmp(&sskey[0], "\x07version", 8) == 0) {
                                // update version:
                                ssvalue.clear();
                                ssvalue << client_version;
                            }
                            dbt datkey(&sskey[0], sskey.size());
                            dbt datvalue(&ssvalue[0], ssvalue.size());
                            int ret2 = pdbcopy->put(null, &datkey, &datvalue, db_nooverwrite);
                            if (ret2 > 0)
                                fsuccess = false;
                        }
                    if (fsuccess) {
                        db.close();
                        bitdb.closedb(strfile);
                        if (pdbcopy->close(0))
                            fsuccess = false;
                        delete pdbcopy;
                    }
                }
                if (fsuccess) {
                    db dba(bitdb.dbenv, 0);
                    if (dba.remove(strfile.c_str(), null, 0))
                        fsuccess = false;
                    db dbb(bitdb.dbenv, 0);
                    if (dbb.rename(strfileres.c_str(), null, strfile.c_str(), 0))
                        fsuccess = false;
                }
                if (!fsuccess)
                    logprintf("cdb::rewrite: failed to rewrite database file %s\n", strfileres);
                return fsuccess;
            }
        }
        millisleep(100);
    }
    return false;
}


void cdbenv::flush(bool fshutdown)
{
    int64_t nstart = gettimemillis();
    // flush log data to the actual data file on all files that are not in use
    logprint("db", "cdbenv::flush: flush(%s)%s\n", fshutdown ? "true" : "false", fdbenvinit ? "" : " database not started");
    if (!fdbenvinit)
        return;
    {
        lock(cs_db);
        map<string, int>::iterator mi = mapfileusecount.begin();
        while (mi != mapfileusecount.end()) {
            string strfile = (*mi).first;
            int nrefcount = (*mi).second;
            logprint("db", "cdbenv::flush: flushing %s (refcount = %d)...\n", strfile, nrefcount);
            if (nrefcount == 0) {
                // move log data to the dat file
                closedb(strfile);
                logprint("db", "cdbenv::flush: %s checkpoint\n", strfile);
                dbenv->txn_checkpoint(0, 0, 0);
                logprint("db", "cdbenv::flush: %s detach\n", strfile);
                if (!fmockdb)
                    dbenv->lsn_reset(strfile.c_str(), 0);
                logprint("db", "cdbenv::flush: %s closed\n", strfile);
                mapfileusecount.erase(mi++);
            } else
                mi++;
        }
        logprint("db", "cdbenv::flush: flush(%s)%s took %15dms\n", fshutdown ? "true" : "false", fdbenvinit ? "" : " database not started", gettimemillis() - nstart);
        if (fshutdown) {
            char** listp;
            if (mapfileusecount.empty()) {
                dbenv->log_archive(&listp, db_arch_remove);
                close();
                if (!fmockdb)
                    boost::filesystem::remove_all(boost::filesystem::path(strpath) / "database");
            }
        }
    }
}
