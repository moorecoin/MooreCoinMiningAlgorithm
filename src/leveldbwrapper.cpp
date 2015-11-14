// copyright (c) 2012-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "leveldbwrapper.h"

#include "util.h"

#include <boost/filesystem.hpp>

#include <leveldb/cache.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <memenv.h>

void handleerror(const leveldb::status& status) throw(leveldb_error)
{
    if (status.ok())
        return;
    logprintf("%s\n", status.tostring());
    if (status.iscorruption())
        throw leveldb_error("database corrupted");
    if (status.isioerror())
        throw leveldb_error("database i/o error");
    if (status.isnotfound())
        throw leveldb_error("database entry missing");
    throw leveldb_error("unknown database error");
}

static leveldb::options getoptions(size_t ncachesize)
{
    leveldb::options options;
    options.block_cache = leveldb::newlrucache(ncachesize / 2);
    options.write_buffer_size = ncachesize / 4; // up to two write buffers may be held in memory simultaneously
    options.filter_policy = leveldb::newbloomfilterpolicy(10);
    options.compression = leveldb::knocompression;
    options.max_open_files = 64;
    if (leveldb::kmajorversion > 1 || (leveldb::kmajorversion == 1 && leveldb::kminorversion >= 16)) {
        // leveldb versions before 1.16 consider short writes to be corruption. only trigger error
        // on corruption in later versions.
        options.paranoid_checks = true;
    }
    return options;
}

cleveldbwrapper::cleveldbwrapper(const boost::filesystem::path& path, size_t ncachesize, bool fmemory, bool fwipe)
{
    penv = null;
    readoptions.verify_checksums = true;
    iteroptions.verify_checksums = true;
    iteroptions.fill_cache = false;
    syncoptions.sync = true;
    options = getoptions(ncachesize);
    options.create_if_missing = true;
    if (fmemory) {
        penv = leveldb::newmemenv(leveldb::env::default());
        options.env = penv;
    } else {
        if (fwipe) {
            logprintf("wiping leveldb in %s\n", path.string());
            leveldb::destroydb(path.string(), options);
        }
        trycreatedirectory(path);
        logprintf("opening leveldb in %s\n", path.string());
    }
    leveldb::status status = leveldb::db::open(options, path.string(), &pdb);
    handleerror(status);
    logprintf("opened leveldb successfully\n");
}

cleveldbwrapper::~cleveldbwrapper()
{
    delete pdb;
    pdb = null;
    delete options.filter_policy;
    options.filter_policy = null;
    delete options.block_cache;
    options.block_cache = null;
    delete penv;
    options.env = null;
}

bool cleveldbwrapper::writebatch(cleveldbbatch& batch, bool fsync) throw(leveldb_error)
{
    leveldb::status status = pdb->write(fsync ? syncoptions : writeoptions, &batch.batch);
    handleerror(status);
    return true;
}
