// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_set.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

class findfiletest {
 public:
  std::vector<filemetadata*> files_;
  bool disjoint_sorted_files_;

  findfiletest() : disjoint_sorted_files_(true) { }

  ~findfiletest() {
    for (int i = 0; i < files_.size(); i++) {
      delete files_[i];
    }
  }

  void add(const char* smallest, const char* largest,
           sequencenumber smallest_seq = 100,
           sequencenumber largest_seq = 100) {
    filemetadata* f = new filemetadata;
    f->number = files_.size() + 1;
    f->smallest = internalkey(smallest, smallest_seq, ktypevalue);
    f->largest = internalkey(largest, largest_seq, ktypevalue);
    files_.push_back(f);
  }

  int find(const char* key) {
    internalkey target(key, 100, ktypevalue);
    internalkeycomparator cmp(bytewisecomparator());
    return findfile(cmp, files_, target.encode());
  }

  bool overlaps(const char* smallest, const char* largest) {
    internalkeycomparator cmp(bytewisecomparator());
    slice s(smallest != null ? smallest : "");
    slice l(largest != null ? largest : "");
    return somefileoverlapsrange(cmp, disjoint_sorted_files_, files_,
                                 (smallest != null ? &s : null),
                                 (largest != null ? &l : null));
  }
};

test(findfiletest, empty) {
  assert_eq(0, find("foo"));
  assert_true(! overlaps("a", "z"));
  assert_true(! overlaps(null, "z"));
  assert_true(! overlaps("a", null));
  assert_true(! overlaps(null, null));
}

test(findfiletest, single) {
  add("p", "q");
  assert_eq(0, find("a"));
  assert_eq(0, find("p"));
  assert_eq(0, find("p1"));
  assert_eq(0, find("q"));
  assert_eq(1, find("q1"));
  assert_eq(1, find("z"));

  assert_true(! overlaps("a", "b"));
  assert_true(! overlaps("z1", "z2"));
  assert_true(overlaps("a", "p"));
  assert_true(overlaps("a", "q"));
  assert_true(overlaps("a", "z"));
  assert_true(overlaps("p", "p1"));
  assert_true(overlaps("p", "q"));
  assert_true(overlaps("p", "z"));
  assert_true(overlaps("p1", "p2"));
  assert_true(overlaps("p1", "z"));
  assert_true(overlaps("q", "q"));
  assert_true(overlaps("q", "q1"));

  assert_true(! overlaps(null, "j"));
  assert_true(! overlaps("r", null));
  assert_true(overlaps(null, "p"));
  assert_true(overlaps(null, "p1"));
  assert_true(overlaps("q", null));
  assert_true(overlaps(null, null));
}


test(findfiletest, multiple) {
  add("150", "200");
  add("200", "250");
  add("300", "350");
  add("400", "450");
  assert_eq(0, find("100"));
  assert_eq(0, find("150"));
  assert_eq(0, find("151"));
  assert_eq(0, find("199"));
  assert_eq(0, find("200"));
  assert_eq(1, find("201"));
  assert_eq(1, find("249"));
  assert_eq(1, find("250"));
  assert_eq(2, find("251"));
  assert_eq(2, find("299"));
  assert_eq(2, find("300"));
  assert_eq(2, find("349"));
  assert_eq(2, find("350"));
  assert_eq(3, find("351"));
  assert_eq(3, find("400"));
  assert_eq(3, find("450"));
  assert_eq(4, find("451"));

  assert_true(! overlaps("100", "149"));
  assert_true(! overlaps("251", "299"));
  assert_true(! overlaps("451", "500"));
  assert_true(! overlaps("351", "399"));

  assert_true(overlaps("100", "150"));
  assert_true(overlaps("100", "200"));
  assert_true(overlaps("100", "300"));
  assert_true(overlaps("100", "400"));
  assert_true(overlaps("100", "500"));
  assert_true(overlaps("375", "400"));
  assert_true(overlaps("450", "450"));
  assert_true(overlaps("450", "500"));
}

test(findfiletest, multiplenullboundaries) {
  add("150", "200");
  add("200", "250");
  add("300", "350");
  add("400", "450");
  assert_true(! overlaps(null, "149"));
  assert_true(! overlaps("451", null));
  assert_true(overlaps(null, null));
  assert_true(overlaps(null, "150"));
  assert_true(overlaps(null, "199"));
  assert_true(overlaps(null, "200"));
  assert_true(overlaps(null, "201"));
  assert_true(overlaps(null, "400"));
  assert_true(overlaps(null, "800"));
  assert_true(overlaps("100", null));
  assert_true(overlaps("200", null));
  assert_true(overlaps("449", null));
  assert_true(overlaps("450", null));
}

test(findfiletest, overlapsequencechecks) {
  add("200", "200", 5000, 3000);
  assert_true(! overlaps("199", "199"));
  assert_true(! overlaps("201", "300"));
  assert_true(overlaps("200", "200"));
  assert_true(overlaps("190", "200"));
  assert_true(overlaps("200", "210"));
}

test(findfiletest, overlappingfiles) {
  add("150", "600");
  add("400", "500");
  disjoint_sorted_files_ = false;
  assert_true(! overlaps("100", "149"));
  assert_true(! overlaps("601", "700"));
  assert_true(overlaps("100", "150"));
  assert_true(overlaps("100", "200"));
  assert_true(overlaps("100", "300"));
  assert_true(overlaps("100", "400"));
  assert_true(overlaps("100", "500"));
  assert_true(overlaps("375", "400"));
  assert_true(overlaps("450", "450"));
  assert_true(overlaps("450", "500"));
  assert_true(overlaps("450", "700"));
  assert_true(overlaps("600", "700"));
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
