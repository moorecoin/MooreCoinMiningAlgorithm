// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/format.h"

#include "leveldb/env.h"
#include "port/port.h"
#include "table/block.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {

void blockhandle::encodeto(std::string* dst) const {
  // sanity check that all fields have been set
  assert(offset_ != ~static_cast<uint64_t>(0));
  assert(size_ != ~static_cast<uint64_t>(0));
  putvarint64(dst, offset_);
  putvarint64(dst, size_);
}

status blockhandle::decodefrom(slice* input) {
  if (getvarint64(input, &offset_) &&
      getvarint64(input, &size_)) {
    return status::ok();
  } else {
    return status::corruption("bad block handle");
  }
}

void footer::encodeto(std::string* dst) const {
#ifndef ndebug
  const size_t original_size = dst->size();
#endif
  metaindex_handle_.encodeto(dst);
  index_handle_.encodeto(dst);
  dst->resize(2 * blockhandle::kmaxencodedlength);  // padding
  putfixed32(dst, static_cast<uint32_t>(ktablemagicnumber & 0xffffffffu));
  putfixed32(dst, static_cast<uint32_t>(ktablemagicnumber >> 32));
  assert(dst->size() == original_size + kencodedlength);
}

status footer::decodefrom(slice* input) {
  const char* magic_ptr = input->data() + kencodedlength - 8;
  const uint32_t magic_lo = decodefixed32(magic_ptr);
  const uint32_t magic_hi = decodefixed32(magic_ptr + 4);
  const uint64_t magic = ((static_cast<uint64_t>(magic_hi) << 32) |
                          (static_cast<uint64_t>(magic_lo)));
  if (magic != ktablemagicnumber) {
    return status::corruption("not an sstable (bad magic number)");
  }

  status result = metaindex_handle_.decodefrom(input);
  if (result.ok()) {
    result = index_handle_.decodefrom(input);
  }
  if (result.ok()) {
    // we skip over any leftover data (just padding for now) in "input"
    const char* end = magic_ptr + 8;
    *input = slice(end, input->data() + input->size() - end);
  }
  return result;
}

status readblock(randomaccessfile* file,
                 const readoptions& options,
                 const blockhandle& handle,
                 blockcontents* result) {
  result->data = slice();
  result->cachable = false;
  result->heap_allocated = false;

  // read the block contents as well as the type/crc footer.
  // see table_builder.cc for the code that built this structure.
  size_t n = static_cast<size_t>(handle.size());
  char* buf = new char[n + kblocktrailersize];
  slice contents;
  status s = file->read(handle.offset(), n + kblocktrailersize, &contents, buf);
  if (!s.ok()) {
    delete[] buf;
    return s;
  }
  if (contents.size() != n + kblocktrailersize) {
    delete[] buf;
    return status::corruption("truncated block read");
  }

  // check the crc of the type and the block contents
  const char* data = contents.data();    // pointer to where read put the data
  if (options.verify_checksums) {
    const uint32_t crc = crc32c::unmask(decodefixed32(data + n + 1));
    const uint32_t actual = crc32c::value(data, n + 1);
    if (actual != crc) {
      delete[] buf;
      s = status::corruption("block checksum mismatch");
      return s;
    }
  }

  switch (data[n]) {
    case knocompression:
      if (data != buf) {
        // file implementation gave us pointer to some other data.
        // use it directly under the assumption that it will be live
        // while the file is open.
        delete[] buf;
        result->data = slice(data, n);
        result->heap_allocated = false;
        result->cachable = false;  // do not double-cache
      } else {
        result->data = slice(buf, n);
        result->heap_allocated = true;
        result->cachable = true;
      }

      // ok
      break;
    case ksnappycompression: {
      size_t ulength = 0;
      if (!port::snappy_getuncompressedlength(data, n, &ulength)) {
        delete[] buf;
        return status::corruption("corrupted compressed block contents");
      }
      char* ubuf = new char[ulength];
      if (!port::snappy_uncompress(data, n, ubuf)) {
        delete[] buf;
        delete[] ubuf;
        return status::corruption("corrupted compressed block contents");
      }
      delete[] buf;
      result->data = slice(ubuf, ulength);
      result->heap_allocated = true;
      result->cachable = true;
      break;
    }
    default:
      delete[] buf;
      return status::corruption("bad block type");
  }

  return status::ok();
}

}  // namespace leveldb
