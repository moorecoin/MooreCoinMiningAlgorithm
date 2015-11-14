// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/coding.h"

namespace leveldb {

void encodefixed32(char* buf, uint32_t value) {
  if (port::klittleendian) {
    memcpy(buf, &value, sizeof(value));
  } else {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
  }
}

void encodefixed64(char* buf, uint64_t value) {
  if (port::klittleendian) {
    memcpy(buf, &value, sizeof(value));
  } else {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
    buf[4] = (value >> 32) & 0xff;
    buf[5] = (value >> 40) & 0xff;
    buf[6] = (value >> 48) & 0xff;
    buf[7] = (value >> 56) & 0xff;
  }
}

void putfixed32(std::string* dst, uint32_t value) {
  char buf[sizeof(value)];
  encodefixed32(buf, value);
  dst->append(buf, sizeof(buf));
}

void putfixed64(std::string* dst, uint64_t value) {
  char buf[sizeof(value)];
  encodefixed64(buf, value);
  dst->append(buf, sizeof(buf));
}

char* encodevarint32(char* dst, uint32_t v) {
  // operate on characters as unsigneds
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  static const int b = 128;
  if (v < (1<<7)) {
    *(ptr++) = v;
  } else if (v < (1<<14)) {
    *(ptr++) = v | b;
    *(ptr++) = v>>7;
  } else if (v < (1<<21)) {
    *(ptr++) = v | b;
    *(ptr++) = (v>>7) | b;
    *(ptr++) = v>>14;
  } else if (v < (1<<28)) {
    *(ptr++) = v | b;
    *(ptr++) = (v>>7) | b;
    *(ptr++) = (v>>14) | b;
    *(ptr++) = v>>21;
  } else {
    *(ptr++) = v | b;
    *(ptr++) = (v>>7) | b;
    *(ptr++) = (v>>14) | b;
    *(ptr++) = (v>>21) | b;
    *(ptr++) = v>>28;
  }
  return reinterpret_cast<char*>(ptr);
}

void putvarint32(std::string* dst, uint32_t v) {
  char buf[5];
  char* ptr = encodevarint32(buf, v);
  dst->append(buf, ptr - buf);
}

char* encodevarint64(char* dst, uint64_t v) {
  static const int b = 128;
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  while (v >= b) {
    *(ptr++) = (v & (b-1)) | b;
    v >>= 7;
  }
  *(ptr++) = static_cast<unsigned char>(v);
  return reinterpret_cast<char*>(ptr);
}

void putvarint64(std::string* dst, uint64_t v) {
  char buf[10];
  char* ptr = encodevarint64(buf, v);
  dst->append(buf, ptr - buf);
}

void putlengthprefixedslice(std::string* dst, const slice& value) {
  putvarint32(dst, value.size());
  dst->append(value.data(), value.size());
}

int varintlength(uint64_t v) {
  int len = 1;
  while (v >= 128) {
    v >>= 7;
    len++;
  }
  return len;
}

const char* getvarint32ptrfallback(const char* p,
                                   const char* limit,
                                   uint32_t* value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // more bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return null;
}

bool getvarint32(slice* input, uint32_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = getvarint32ptr(p, limit, value);
  if (q == null) {
    return false;
  } else {
    *input = slice(q, limit - q);
    return true;
  }
}

const char* getvarint64ptr(const char* p, const char* limit, uint64_t* value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *(reinterpret_cast<const unsigned char*>(p));
    p++;
    if (byte & 128) {
      // more bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char*>(p);
    }
  }
  return null;
}

bool getvarint64(slice* input, uint64_t* value) {
  const char* p = input->data();
  const char* limit = p + input->size();
  const char* q = getvarint64ptr(p, limit, value);
  if (q == null) {
    return false;
  } else {
    *input = slice(q, limit - q);
    return true;
  }
}

const char* getlengthprefixedslice(const char* p, const char* limit,
                                   slice* result) {
  uint32_t len;
  p = getvarint32ptr(p, limit, &len);
  if (p == null) return null;
  if (p + len > limit) return null;
  *result = slice(p, len);
  return p + len;
}

bool getlengthprefixedslice(slice* input, slice* result) {
  uint32_t len;
  if (getvarint32(input, &len) &&
      input->size() >= len) {
    *result = slice(input->data(), len);
    input->remove_prefix(len);
    return true;
  } else {
    return false;
  }
}

}  // namespace leveldb
