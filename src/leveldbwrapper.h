// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_leveldbwrapper_h
#define moorecoin_leveldbwrapper_h

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "version.h"

#include <boost/filesystem/path.hpp>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

class leveldb_error : public std::runtime_error
{
public:
    leveldb_error(const std::string& msg) : std::runtime_error(msg) {}
};

void handleerror(const leveldb::status& status) throw(leveldb_error);

/** batch of changes queued to be written to a cleveldbwrapper */
class cleveldbbatch
{
    friend class cleveldbwrapper;

private:
    leveldb::writebatch batch;

public:
    template <typename k, typename v>
    void write(const k& key, const v& value)
    {
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(sskey.getserializesize(key));
        sskey << key;
        leveldb::slice slkey(&sskey[0], sskey.size());

        cdatastream ssvalue(ser_disk, client_version);
        ssvalue.reserve(ssvalue.getserializesize(value));
        ssvalue << value;
        leveldb::slice slvalue(&ssvalue[0], ssvalue.size());

        batch.put(slkey, slvalue);
    }

    template <typename k>
    void erase(const k& key)
    {
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(sskey.getserializesize(key));
        sskey << key;
        leveldb::slice slkey(&sskey[0], sskey.size());

        batch.delete(slkey);
    }
};

class cleveldbwrapper
{
private:
    //! custom environment this database is using (may be null in case of default environment)
    leveldb::env* penv;

    //! database options used
    leveldb::options options;

    //! options used when reading from the database
    leveldb::readoptions readoptions;

    //! options used when iterating over values of the database
    leveldb::readoptions iteroptions;

    //! options used when writing to the database
    leveldb::writeoptions writeoptions;

    //! options used when sync writing to the database
    leveldb::writeoptions syncoptions;

    //! the database itself
    leveldb::db* pdb;

public:
    cleveldbwrapper(const boost::filesystem::path& path, size_t ncachesize, bool fmemory = false, bool fwipe = false);
    ~cleveldbwrapper();

    template <typename k, typename v>
    bool read(const k& key, v& value) const throw(leveldb_error)
    {
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(sskey.getserializesize(key));
        sskey << key;
        leveldb::slice slkey(&sskey[0], sskey.size());

        std::string strvalue;
        leveldb::status status = pdb->get(readoptions, slkey, &strvalue);
        if (!status.ok()) {
            if (status.isnotfound())
                return false;
            logprintf("leveldb read failure: %s\n", status.tostring());
            handleerror(status);
        }
        try {
            cdatastream ssvalue(strvalue.data(), strvalue.data() + strvalue.size(), ser_disk, client_version);
            ssvalue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename k, typename v>
    bool write(const k& key, const v& value, bool fsync = false) throw(leveldb_error)
    {
        cleveldbbatch batch;
        batch.write(key, value);
        return writebatch(batch, fsync);
    }

    template <typename k>
    bool exists(const k& key) const throw(leveldb_error)
    {
        cdatastream sskey(ser_disk, client_version);
        sskey.reserve(sskey.getserializesize(key));
        sskey << key;
        leveldb::slice slkey(&sskey[0], sskey.size());

        std::string strvalue;
        leveldb::status status = pdb->get(readoptions, slkey, &strvalue);
        if (!status.ok()) {
            if (status.isnotfound())
                return false;
            logprintf("leveldb read failure: %s\n", status.tostring());
            handleerror(status);
        }
        return true;
    }

    template <typename k>
    bool erase(const k& key, bool fsync = false) throw(leveldb_error)
    {
        cleveldbbatch batch;
        batch.erase(key);
        return writebatch(batch, fsync);
    }

    bool writebatch(cleveldbbatch& batch, bool fsync = false) throw(leveldb_error);

    // not available for leveldb; provide for compatibility with bdb
    bool flush()
    {
        return true;
    }

    bool sync() throw(leveldb_error)
    {
        cleveldbbatch batch;
        return writebatch(batch, true);
    }

    // not exactly clean encapsulation, but it's easiest for now
    leveldb::iterator* newiterator()
    {
        return pdb->newiterator(iteroptions);
    }
};

#endif // moorecoin_leveldbwrapper_h
