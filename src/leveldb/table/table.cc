// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/table.h"

#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"

namespace leveldb {

struct table::rep {
  ~rep() {
    delete filter;
    delete [] filter_data;
    delete index_block;
  }

  options options;
  status status;
  randomaccessfile* file;
  uint64_t cache_id;
  filterblockreader* filter;
  const char* filter_data;

  blockhandle metaindex_handle;  // handle to metaindex_block: saved from footer
  block* index_block;
};

status table::open(const options& options,
                   randomaccessfile* file,
                   uint64_t size,
                   table** table) {
  *table = null;
  if (size < footer::kencodedlength) {
    return status::corruption("file is too short to be an sstable");
  }

  char footer_space[footer::kencodedlength];
  slice footer_input;
  status s = file->read(size - footer::kencodedlength, footer::kencodedlength,
                        &footer_input, footer_space);
  if (!s.ok()) return s;

  footer footer;
  s = footer.decodefrom(&footer_input);
  if (!s.ok()) return s;

  // read the index block
  blockcontents contents;
  block* index_block = null;
  if (s.ok()) {
    readoptions opt;
    if (options.paranoid_checks) {
      opt.verify_checksums = true;
    }
    s = readblock(file, opt, footer.index_handle(), &contents);
    if (s.ok()) {
      index_block = new block(contents);
    }
  }

  if (s.ok()) {
    // we've successfully read the footer and the index block: we're
    // ready to serve requests.
    rep* rep = new table::rep;
    rep->options = options;
    rep->file = file;
    rep->metaindex_handle = footer.metaindex_handle();
    rep->index_block = index_block;
    rep->cache_id = (options.block_cache ? options.block_cache->newid() : 0);
    rep->filter_data = null;
    rep->filter = null;
    *table = new table(rep);
    (*table)->readmeta(footer);
  } else {
    if (index_block) delete index_block;
  }

  return s;
}

void table::readmeta(const footer& footer) {
  if (rep_->options.filter_policy == null) {
    return;  // do not need any metadata
  }

  // todo(sanjay): skip this if footer.metaindex_handle() size indicates
  // it is an empty block.
  readoptions opt;
  if (rep_->options.paranoid_checks) {
    opt.verify_checksums = true;
  }
  blockcontents contents;
  if (!readblock(rep_->file, opt, footer.metaindex_handle(), &contents).ok()) {
    // do not propagate errors since meta info is not needed for operation
    return;
  }
  block* meta = new block(contents);

  iterator* iter = meta->newiterator(bytewisecomparator());
  std::string key = "filter.";
  key.append(rep_->options.filter_policy->name());
  iter->seek(key);
  if (iter->valid() && iter->key() == slice(key)) {
    readfilter(iter->value());
  }
  delete iter;
  delete meta;
}

void table::readfilter(const slice& filter_handle_value) {
  slice v = filter_handle_value;
  blockhandle filter_handle;
  if (!filter_handle.decodefrom(&v).ok()) {
    return;
  }

  // we might want to unify with readblock() if we start
  // requiring checksum verification in table::open.
  readoptions opt;
  if (rep_->options.paranoid_checks) {
    opt.verify_checksums = true;
  }
  blockcontents block;
  if (!readblock(rep_->file, opt, filter_handle, &block).ok()) {
    return;
  }
  if (block.heap_allocated) {
    rep_->filter_data = block.data.data();     // will need to delete later
  }
  rep_->filter = new filterblockreader(rep_->options.filter_policy, block.data);
}

table::~table() {
  delete rep_;
}

static void deleteblock(void* arg, void* ignored) {
  delete reinterpret_cast<block*>(arg);
}

static void deletecachedblock(const slice& key, void* value) {
  block* block = reinterpret_cast<block*>(value);
  delete block;
}

static void releaseblock(void* arg, void* h) {
  cache* cache = reinterpret_cast<cache*>(arg);
  cache::handle* handle = reinterpret_cast<cache::handle*>(h);
  cache->release(handle);
}

// convert an index iterator value (i.e., an encoded blockhandle)
// into an iterator over the contents of the corresponding block.
iterator* table::blockreader(void* arg,
                             const readoptions& options,
                             const slice& index_value) {
  table* table = reinterpret_cast<table*>(arg);
  cache* block_cache = table->rep_->options.block_cache;
  block* block = null;
  cache::handle* cache_handle = null;

  blockhandle handle;
  slice input = index_value;
  status s = handle.decodefrom(&input);
  // we intentionally allow extra stuff in index_value so that we
  // can add more features in the future.

  if (s.ok()) {
    blockcontents contents;
    if (block_cache != null) {
      char cache_key_buffer[16];
      encodefixed64(cache_key_buffer, table->rep_->cache_id);
      encodefixed64(cache_key_buffer+8, handle.offset());
      slice key(cache_key_buffer, sizeof(cache_key_buffer));
      cache_handle = block_cache->lookup(key);
      if (cache_handle != null) {
        block = reinterpret_cast<block*>(block_cache->value(cache_handle));
      } else {
        s = readblock(table->rep_->file, options, handle, &contents);
        if (s.ok()) {
          block = new block(contents);
          if (contents.cachable && options.fill_cache) {
            cache_handle = block_cache->insert(
                key, block, block->size(), &deletecachedblock);
          }
        }
      }
    } else {
      s = readblock(table->rep_->file, options, handle, &contents);
      if (s.ok()) {
        block = new block(contents);
      }
    }
  }

  iterator* iter;
  if (block != null) {
    iter = block->newiterator(table->rep_->options.comparator);
    if (cache_handle == null) {
      iter->registercleanup(&deleteblock, block, null);
    } else {
      iter->registercleanup(&releaseblock, block_cache, cache_handle);
    }
  } else {
    iter = newerroriterator(s);
  }
  return iter;
}

iterator* table::newiterator(const readoptions& options) const {
  return newtwoleveliterator(
      rep_->index_block->newiterator(rep_->options.comparator),
      &table::blockreader, const_cast<table*>(this), options);
}

status table::internalget(const readoptions& options, const slice& k,
                          void* arg,
                          void (*saver)(void*, const slice&, const slice&)) {
  status s;
  iterator* iiter = rep_->index_block->newiterator(rep_->options.comparator);
  iiter->seek(k);
  if (iiter->valid()) {
    slice handle_value = iiter->value();
    filterblockreader* filter = rep_->filter;
    blockhandle handle;
    if (filter != null &&
        handle.decodefrom(&handle_value).ok() &&
        !filter->keymaymatch(handle.offset(), k)) {
      // not found
    } else {
      iterator* block_iter = blockreader(this, options, iiter->value());
      block_iter->seek(k);
      if (block_iter->valid()) {
        (*saver)(arg, block_iter->key(), block_iter->value());
      }
      s = block_iter->status();
      delete block_iter;
    }
  }
  if (s.ok()) {
    s = iiter->status();
  }
  delete iiter;
  return s;
}


uint64_t table::approximateoffsetof(const slice& key) const {
  iterator* index_iter =
      rep_->index_block->newiterator(rep_->options.comparator);
  index_iter->seek(key);
  uint64_t result;
  if (index_iter->valid()) {
    blockhandle handle;
    slice input = index_iter->value();
    status s = handle.decodefrom(&input);
    if (s.ok()) {
      result = handle.offset();
    } else {
      // strange: we can't decode the block handle in the index block.
      // we'll just return the offset of the metaindex block, which is
      // close to the whole file size for this case.
      result = rep_->metaindex_handle.offset();
    }
  } else {
    // key is past the last key in the file.  approximate the offset
    // by returning the offset of the metaindex block (which is
    // right near the end of the file).
    result = rep_->metaindex_handle.offset();
  }
  delete index_iter;
  return result;
}

}  // namespace leveldb
