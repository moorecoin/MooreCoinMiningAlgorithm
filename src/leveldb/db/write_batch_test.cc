// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/db.h"

#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "leveldb/env.h"
#include "util/logging.h"
#include "util/testharness.h"

namespace leveldb {

static std::string printcontents(writebatch* b) {
  internalkeycomparator cmp(bytewisecomparator());
  memtable* mem = new memtable(cmp);
  mem->ref();
  std::string state;
  status s = writebatchinternal::insertinto(b, mem);
  int count = 0;
  iterator* iter = mem->newiterator();
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    parsedinternalkey ikey;
    assert_true(parseinternalkey(iter->key(), &ikey));
    switch (ikey.type) {
      case ktypevalue:
        state.append("put(");
        state.append(ikey.user_key.tostring());
        state.append(", ");
        state.append(iter->value().tostring());
        state.append(")");
        count++;
        break;
      case ktypedeletion:
        state.append("delete(");
        state.append(ikey.user_key.tostring());
        state.append(")");
        count++;
        break;
    }
    state.append("@");
    state.append(numbertostring(ikey.sequence));
  }
  delete iter;
  if (!s.ok()) {
    state.append("parseerror()");
  } else if (count != writebatchinternal::count(b)) {
    state.append("countmismatch()");
  }
  mem->unref();
  return state;
}

class writebatchtest { };

test(writebatchtest, empty) {
  writebatch batch;
  assert_eq("", printcontents(&batch));
  assert_eq(0, writebatchinternal::count(&batch));
}

test(writebatchtest, multiple) {
  writebatch batch;
  batch.put(slice("foo"), slice("bar"));
  batch.delete(slice("box"));
  batch.put(slice("baz"), slice("boo"));
  writebatchinternal::setsequence(&batch, 100);
  assert_eq(100, writebatchinternal::sequence(&batch));
  assert_eq(3, writebatchinternal::count(&batch));
  assert_eq("put(baz, boo)@102"
            "delete(box)@101"
            "put(foo, bar)@100",
            printcontents(&batch));
}

test(writebatchtest, corruption) {
  writebatch batch;
  batch.put(slice("foo"), slice("bar"));
  batch.delete(slice("box"));
  writebatchinternal::setsequence(&batch, 200);
  slice contents = writebatchinternal::contents(&batch);
  writebatchinternal::setcontents(&batch,
                                  slice(contents.data(),contents.size()-1));
  assert_eq("put(foo, bar)@200"
            "parseerror()",
            printcontents(&batch));
}

test(writebatchtest, append) {
  writebatch b1, b2;
  writebatchinternal::setsequence(&b1, 200);
  writebatchinternal::setsequence(&b2, 300);
  writebatchinternal::append(&b1, &b2);
  assert_eq("",
            printcontents(&b1));
  b2.put("a", "va");
  writebatchinternal::append(&b1, &b2);
  assert_eq("put(a, va)@200",
            printcontents(&b1));
  b2.clear();
  b2.put("b", "vb");
  writebatchinternal::append(&b1, &b2);
  assert_eq("put(a, va)@200"
            "put(b, vb)@201",
            printcontents(&b1));
  b2.delete("foo");
  writebatchinternal::append(&b1, &b2);
  assert_eq("put(a, va)@200"
            "put(b, vb)@202"
            "put(b, vb)@201"
            "delete(foo)@203",
            printcontents(&b1));
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
