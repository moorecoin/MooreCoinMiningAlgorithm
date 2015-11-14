// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_edit.h"
#include "util/testharness.h"

namespace leveldb {

static void testencodedecode(const versionedit& edit) {
  std::string encoded, encoded2;
  edit.encodeto(&encoded);
  versionedit parsed;
  status s = parsed.decodefrom(encoded);
  assert_true(s.ok()) << s.tostring();
  parsed.encodeto(&encoded2);
  assert_eq(encoded, encoded2);
}

class versionedittest { };

test(versionedittest, encodedecode) {
  static const uint64_t kbig = 1ull << 50;

  versionedit edit;
  for (int i = 0; i < 4; i++) {
    testencodedecode(edit);
    edit.addfile(3, kbig + 300 + i, kbig + 400 + i,
                 internalkey("foo", kbig + 500 + i, ktypevalue),
                 internalkey("zoo", kbig + 600 + i, ktypedeletion));
    edit.deletefile(4, kbig + 700 + i);
    edit.setcompactpointer(i, internalkey("x", kbig + 900 + i, ktypevalue));
  }

  edit.setcomparatorname("foo");
  edit.setlognumber(kbig + 100);
  edit.setnextfile(kbig + 200);
  edit.setlastsequence(kbig + 1000);
  testencodedecode(edit);
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
