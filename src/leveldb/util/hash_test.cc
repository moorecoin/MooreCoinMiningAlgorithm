// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/hash.h"
#include "util/testharness.h"

namespace leveldb {

class hash { };

test(hash, signedunsignedissue) {
  const unsigned char data1[1] = {0x62};
  const unsigned char data2[2] = {0xc3, 0x97};
  const unsigned char data3[3] = {0xe2, 0x99, 0xa5};
  const unsigned char data4[4] = {0xe1, 0x80, 0xb9, 0x32};
  const unsigned char data5[48] = {
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

  assert_eq(hash(0, 0, 0xbc9f1d34), 0xbc9f1d34);
  assert_eq(
      hash(reinterpret_cast<const char*>(data1), sizeof(data1), 0xbc9f1d34),
      0xef1345c4);
  assert_eq(
      hash(reinterpret_cast<const char*>(data2), sizeof(data2), 0xbc9f1d34),
      0x5b663814);
  assert_eq(
      hash(reinterpret_cast<const char*>(data3), sizeof(data3), 0xbc9f1d34),
      0x323c078f);
  assert_eq(
      hash(reinterpret_cast<const char*>(data4), sizeof(data4), 0xbc9f1d34),
      0xed21633a);
  assert_eq(
      hash(reinterpret_cast<const char*>(data5), sizeof(data5), 0x12345678),
      0xf333dabb);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
