// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_table_format_h_
#define storage_leveldb_table_format_h_

#include <string>
#include <stdint.h>
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/table_builder.h"

namespace leveldb {

class block;
class randomaccessfile;
struct readoptions;

// blockhandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class blockhandle {
 public:
  blockhandle();

  // the offset of the block in the file.
  uint64_t offset() const { return offset_; }
  void set_offset(uint64_t offset) { offset_ = offset; }

  // the size of the stored block
  uint64_t size() const { return size_; }
  void set_size(uint64_t size) { size_ = size; }

  void encodeto(std::string* dst) const;
  status decodefrom(slice* input);

  // maximum encoding length of a blockhandle
  enum { kmaxencodedlength = 10 + 10 };

 private:
  uint64_t offset_;
  uint64_t size_;
};

// footer encapsulates the fixed information stored at the tail
// end of every table file.
class footer {
 public:
  footer() { }

  // the block handle for the metaindex block of the table
  const blockhandle& metaindex_handle() const { return metaindex_handle_; }
  void set_metaindex_handle(const blockhandle& h) { metaindex_handle_ = h; }

  // the block handle for the index block of the table
  const blockhandle& index_handle() const {
    return index_handle_;
  }
  void set_index_handle(const blockhandle& h) {
    index_handle_ = h;
  }

  void encodeto(std::string* dst) const;
  status decodefrom(slice* input);

  // encoded length of a footer.  note that the serialization of a
  // footer will always occupy exactly this many bytes.  it consists
  // of two block handles and a magic number.
  enum {
    kencodedlength = 2*blockhandle::kmaxencodedlength + 8
  };

 private:
  blockhandle metaindex_handle_;
  blockhandle index_handle_;
};

// ktablemagicnumber was picked by running
//    echo http://code.google.com/p/leveldb/ | sha1sum
// and taking the leading 64 bits.
static const uint64_t ktablemagicnumber = 0xdb4775248b80fb57ull;

// 1-byte type + 32-bit crc
static const size_t kblocktrailersize = 5;

struct blockcontents {
  slice data;           // actual contents of data
  bool cachable;        // true iff data can be cached
  bool heap_allocated;  // true iff caller should delete[] data.data()
};

// read the block identified by "handle" from "file".  on failure
// return non-ok.  on success fill *result and return ok.
extern status readblock(randomaccessfile* file,
                        const readoptions& options,
                        const blockhandle& handle,
                        blockcontents* result);

// implementation details follow.  clients should ignore,

inline blockhandle::blockhandle()
    : offset_(~static_cast<uint64_t>(0)),
      size_(~static_cast<uint64_t>(0)) {
}

}  // namespace leveldb

#endif  // storage_leveldb_table_format_h_
