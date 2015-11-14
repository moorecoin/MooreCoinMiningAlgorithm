// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/crc32c.h"
#include "util/testharness.h"

namespace leveldb {
namespace crc32c {

class crc { };

test(crc, standardresults) {
  // from rfc3720 section b.4.
  char buf[32];

  memset(buf, 0, sizeof(buf));
  assert_eq(0x8a9136aa, value(buf, sizeof(buf)));

  memset(buf, 0xff, sizeof(buf));
  assert_eq(0x62a8ab43, value(buf, sizeof(buf)));

  for (int i = 0; i < 32; i++) {
    buf[i] = i;
  }
  assert_eq(0x46dd794e, value(buf, sizeof(buf)));

  for (int i = 0; i < 32; i++) {
    buf[i] = 31 - i;
  }
  assert_eq(0x113fdb5c, value(buf, sizeof(buf)));

  unsigned char data[48] = {
    0x01, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x14,
    0x00, 0x00, 0x00, 0x18,
    0x28, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
  };
  assert_eq(0xd9963a56, value(reinterpret_cast<char*>(data), sizeof(data)));
}

test(crc, values) {
  assert_ne(value("a", 1), value("foo", 3));
}

test(crc, extend) {
  assert_eq(value("hello world", 11),
            extend(value("hello ", 6), "world", 5));
}

test(crc, mask) {
  uint32_t crc = value("foo", 3);
  assert_ne(crc, mask(crc));
  assert_ne(crc, mask(mask(crc)));
  assert_eq(crc, unmask(mask(crc)));
  assert_eq(crc, unmask(unmask(mask(mask(crc)))));
}

}  // namespace crc32c
}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
