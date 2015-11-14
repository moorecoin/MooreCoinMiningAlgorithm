// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// endian-neutral encoding:
// * fixed-length numbers are encoded with least-significant byte first
// * in addition we support variable length "varint" encoding
// * strings are encoded prefixed by their length in varint format

#ifndef storage_leveldb_util_coding_h_
#define storage_leveldb_util_coding_h_

#include <stdint.h>
#include <string.h>
#include <string>
#include "leveldb/slice.h"
#include "port/port.h"

namespace leveldb {

// standard put... routines append to a string
extern void putfixed32(std::string* dst, uint32_t value);
extern void putfixed64(std::string* dst, uint64_t value);
extern void putvarint32(std::string* dst, uint32_t value);
extern void putvarint64(std::string* dst, uint64_t value);
extern void putlengthprefixedslice(std::string* dst, const slice& value);

// standard get... routines parse a value from the beginning of a slice
// and advance the slice past the parsed value.
extern bool getvarint32(slice* input, uint32_t* value);
extern bool getvarint64(slice* input, uint64_t* value);
extern bool getlengthprefixedslice(slice* input, slice* result);

// pointer-based variants of getvarint...  these either store a value
// in *v and return a pointer just past the parsed value, or return
// null on error.  these routines only look at bytes in the range
// [p..limit-1]
extern const char* getvarint32ptr(const char* p,const char* limit, uint32_t* v);
extern const char* getvarint64ptr(const char* p,const char* limit, uint64_t* v);

// returns the length of the varint32 or varint64 encoding of "v"
extern int varintlength(uint64_t v);

// lower-level versions of put... that write directly into a character buffer
// requires: dst has enough space for the value being written
extern void encodefixed32(char* dst, uint32_t value);
extern void encodefixed64(char* dst, uint64_t value);

// lower-level versions of put... that write directly into a character buffer
// and return a pointer just past the last byte written.
// requires: dst has enough space for the value being written
extern char* encodevarint32(char* dst, uint32_t value);
extern char* encodevarint64(char* dst, uint64_t value);

// lower-level versions of get... that read directly from a character buffer
// without any bounds checking.

inline uint32_t decodefixed32(const char* ptr) {
  if (port::klittleendian) {
    // load the raw bytes
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0])))
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
  }
}

inline uint64_t decodefixed64(const char* ptr) {
  if (port::klittleendian) {
    // load the raw bytes
    uint64_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    uint64_t lo = decodefixed32(ptr);
    uint64_t hi = decodefixed32(ptr + 4);
    return (hi << 32) | lo;
  }
}

// internal routine for use by fallback path of getvarint32ptr
extern const char* getvarint32ptrfallback(const char* p,
                                          const char* limit,
                                          uint32_t* value);
inline const char* getvarint32ptr(const char* p,
                                  const char* limit,
                                  uint32_t* value) {
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
    if ((result & 128) == 0) {
      *value = result;
      return p + 1;
    }
  }
  return getvarint32ptrfallback(p, limit, value);
}

}  // namespace leveldb

#endif  // storage_leveldb_util_coding_h_
