// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/table_builder.h"

#include <assert.h>
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block_builder.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {

struct tablebuilder::rep {
  options options;
  options index_block_options;
  writablefile* file;
  uint64_t offset;
  status status;
  blockbuilder data_block;
  blockbuilder index_block;
  std::string last_key;
  int64_t num_entries;
  bool closed;          // either finish() or abandon() has been called.
  filterblockbuilder* filter_block;

  // we do not emit the index entry for a block until we have seen the
  // first key for the next data block.  this allows us to use shorter
  // keys in the index block.  for example, consider a block boundary
  // between the keys "the quick brown fox" and "the who".  we can use
  // "the r" as the key for the index block entry since it is >= all
  // entries in the first block and < all entries in subsequent
  // blocks.
  //
  // invariant: r->pending_index_entry is true only if data_block is empty.
  bool pending_index_entry;
  blockhandle pending_handle;  // handle to add to index block

  std::string compressed_output;

  rep(const options& opt, writablefile* f)
      : options(opt),
        index_block_options(opt),
        file(f),
        offset(0),
        data_block(&options),
        index_block(&index_block_options),
        num_entries(0),
        closed(false),
        filter_block(opt.filter_policy == null ? null
                     : new filterblockbuilder(opt.filter_policy)),
        pending_index_entry(false) {
    index_block_options.block_restart_interval = 1;
  }
};

tablebuilder::tablebuilder(const options& options, writablefile* file)
    : rep_(new rep(options, file)) {
  if (rep_->filter_block != null) {
    rep_->filter_block->startblock(0);
  }
}

tablebuilder::~tablebuilder() {
  assert(rep_->closed);  // catch errors where caller forgot to call finish()
  delete rep_->filter_block;
  delete rep_;
}

status tablebuilder::changeoptions(const options& options) {
  // note: if more fields are added to options, update
  // this function to catch changes that should not be allowed to
  // change in the middle of building a table.
  if (options.comparator != rep_->options.comparator) {
    return status::invalidargument("changing comparator while building table");
  }

  // note that any live blockbuilders point to rep_->options and therefore
  // will automatically pick up the updated options.
  rep_->options = options;
  rep_->index_block_options = options;
  rep_->index_block_options.block_restart_interval = 1;
  return status::ok();
}

void tablebuilder::add(const slice& key, const slice& value) {
  rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->num_entries > 0) {
    assert(r->options.comparator->compare(key, slice(r->last_key)) > 0);
  }

  if (r->pending_index_entry) {
    assert(r->data_block.empty());
    r->options.comparator->findshortestseparator(&r->last_key, key);
    std::string handle_encoding;
    r->pending_handle.encodeto(&handle_encoding);
    r->index_block.add(r->last_key, slice(handle_encoding));
    r->pending_index_entry = false;
  }

  if (r->filter_block != null) {
    r->filter_block->addkey(key);
  }

  r->last_key.assign(key.data(), key.size());
  r->num_entries++;
  r->data_block.add(key, value);

  const size_t estimated_block_size = r->data_block.currentsizeestimate();
  if (estimated_block_size >= r->options.block_size) {
    flush();
  }
}

void tablebuilder::flush() {
  rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->data_block.empty()) return;
  assert(!r->pending_index_entry);
  writeblock(&r->data_block, &r->pending_handle);
  if (ok()) {
    r->pending_index_entry = true;
    r->status = r->file->flush();
  }
  if (r->filter_block != null) {
    r->filter_block->startblock(r->offset);
  }
}

void tablebuilder::writeblock(blockbuilder* block, blockhandle* handle) {
  // file format contains a sequence of blocks where each block has:
  //    block_data: uint8[n]
  //    type: uint8
  //    crc: uint32
  assert(ok());
  rep* r = rep_;
  slice raw = block->finish();

  slice block_contents;
  compressiontype type = r->options.compression;
  // todo(postrelease): support more compression options: zlib?
  switch (type) {
    case knocompression:
      block_contents = raw;
      break;

    case ksnappycompression: {
      std::string* compressed = &r->compressed_output;
      if (port::snappy_compress(raw.data(), raw.size(), compressed) &&
          compressed->size() < raw.size() - (raw.size() / 8u)) {
        block_contents = *compressed;
      } else {
        // snappy not supported, or compressed less than 12.5%, so just
        // store uncompressed form
        block_contents = raw;
        type = knocompression;
      }
      break;
    }
  }
  writerawblock(block_contents, type, handle);
  r->compressed_output.clear();
  block->reset();
}

void tablebuilder::writerawblock(const slice& block_contents,
                                 compressiontype type,
                                 blockhandle* handle) {
  rep* r = rep_;
  handle->set_offset(r->offset);
  handle->set_size(block_contents.size());
  r->status = r->file->append(block_contents);
  if (r->status.ok()) {
    char trailer[kblocktrailersize];
    trailer[0] = type;
    uint32_t crc = crc32c::value(block_contents.data(), block_contents.size());
    crc = crc32c::extend(crc, trailer, 1);  // extend crc to cover block type
    encodefixed32(trailer+1, crc32c::mask(crc));
    r->status = r->file->append(slice(trailer, kblocktrailersize));
    if (r->status.ok()) {
      r->offset += block_contents.size() + kblocktrailersize;
    }
  }
}

status tablebuilder::status() const {
  return rep_->status;
}

status tablebuilder::finish() {
  rep* r = rep_;
  flush();
  assert(!r->closed);
  r->closed = true;

  blockhandle filter_block_handle, metaindex_block_handle, index_block_handle;

  // write filter block
  if (ok() && r->filter_block != null) {
    writerawblock(r->filter_block->finish(), knocompression,
                  &filter_block_handle);
  }

  // write metaindex block
  if (ok()) {
    blockbuilder meta_index_block(&r->options);
    if (r->filter_block != null) {
      // add mapping from "filter.name" to location of filter data
      std::string key = "filter.";
      key.append(r->options.filter_policy->name());
      std::string handle_encoding;
      filter_block_handle.encodeto(&handle_encoding);
      meta_index_block.add(key, handle_encoding);
    }

    // todo(postrelease): add stats and other meta blocks
    writeblock(&meta_index_block, &metaindex_block_handle);
  }

  // write index block
  if (ok()) {
    if (r->pending_index_entry) {
      r->options.comparator->findshortsuccessor(&r->last_key);
      std::string handle_encoding;
      r->pending_handle.encodeto(&handle_encoding);
      r->index_block.add(r->last_key, slice(handle_encoding));
      r->pending_index_entry = false;
    }
    writeblock(&r->index_block, &index_block_handle);
  }

  // write footer
  if (ok()) {
    footer footer;
    footer.set_metaindex_handle(metaindex_block_handle);
    footer.set_index_handle(index_block_handle);
    std::string footer_encoding;
    footer.encodeto(&footer_encoding);
    r->status = r->file->append(footer_encoding);
    if (r->status.ok()) {
      r->offset += footer_encoding.size();
    }
  }
  return r->status;
}

void tablebuilder::abandon() {
  rep* r = rep_;
  assert(!r->closed);
  r->closed = true;
}

uint64_t tablebuilder::numentries() const {
  return rep_->num_entries;
}

uint64_t tablebuilder::filesize() const {
  return rep_->offset;
}

}  // namespace leveldb
