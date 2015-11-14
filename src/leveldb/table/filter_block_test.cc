// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"
#include "util/hash.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

// for testing: emit an array with one hash value per key
class testhashfilter : public filterpolicy {
 public:
  virtual const char* name() const {
    return "testhashfilter";
  }

  virtual void createfilter(const slice* keys, int n, std::string* dst) const {
    for (int i = 0; i < n; i++) {
      uint32_t h = hash(keys[i].data(), keys[i].size(), 1);
      putfixed32(dst, h);
    }
  }

  virtual bool keymaymatch(const slice& key, const slice& filter) const {
    uint32_t h = hash(key.data(), key.size(), 1);
    for (size_t i = 0; i + 4 <= filter.size(); i += 4) {
      if (h == decodefixed32(filter.data() + i)) {
        return true;
      }
    }
    return false;
  }
};

class filterblocktest {
 public:
  testhashfilter policy_;
};

test(filterblocktest, emptybuilder) {
  filterblockbuilder builder(&policy_);
  slice block = builder.finish();
  assert_eq("\\x00\\x00\\x00\\x00\\x0b", escapestring(block));
  filterblockreader reader(&policy_, block);
  assert_true(reader.keymaymatch(0, "foo"));
  assert_true(reader.keymaymatch(100000, "foo"));
}

test(filterblocktest, singlechunk) {
  filterblockbuilder builder(&policy_);
  builder.startblock(100);
  builder.addkey("foo");
  builder.addkey("bar");
  builder.addkey("box");
  builder.startblock(200);
  builder.addkey("box");
  builder.startblock(300);
  builder.addkey("hello");
  slice block = builder.finish();
  filterblockreader reader(&policy_, block);
  assert_true(reader.keymaymatch(100, "foo"));
  assert_true(reader.keymaymatch(100, "bar"));
  assert_true(reader.keymaymatch(100, "box"));
  assert_true(reader.keymaymatch(100, "hello"));
  assert_true(reader.keymaymatch(100, "foo"));
  assert_true(! reader.keymaymatch(100, "missing"));
  assert_true(! reader.keymaymatch(100, "other"));
}

test(filterblocktest, multichunk) {
  filterblockbuilder builder(&policy_);

  // first filter
  builder.startblock(0);
  builder.addkey("foo");
  builder.startblock(2000);
  builder.addkey("bar");

  // second filter
  builder.startblock(3100);
  builder.addkey("box");

  // third filter is empty

  // last filter
  builder.startblock(9000);
  builder.addkey("box");
  builder.addkey("hello");

  slice block = builder.finish();
  filterblockreader reader(&policy_, block);

  // check first filter
  assert_true(reader.keymaymatch(0, "foo"));
  assert_true(reader.keymaymatch(2000, "bar"));
  assert_true(! reader.keymaymatch(0, "box"));
  assert_true(! reader.keymaymatch(0, "hello"));

  // check second filter
  assert_true(reader.keymaymatch(3100, "box"));
  assert_true(! reader.keymaymatch(3100, "foo"));
  assert_true(! reader.keymaymatch(3100, "bar"));
  assert_true(! reader.keymaymatch(3100, "hello"));

  // check third filter (empty)
  assert_true(! reader.keymaymatch(4100, "foo"));
  assert_true(! reader.keymaymatch(4100, "bar"));
  assert_true(! reader.keymaymatch(4100, "box"));
  assert_true(! reader.keymaymatch(4100, "hello"));

  // check last filter
  assert_true(reader.keymaymatch(9000, "box"));
  assert_true(reader.keymaymatch(9000, "hello"));
  assert_true(! reader.keymaymatch(9000, "foo"));
  assert_true(! reader.keymaymatch(9000, "bar"));
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
