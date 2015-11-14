// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/filter_policy.h"

#include "util/coding.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

static const int kverbose = 1;

static slice key(int i, char* buffer) {
  encodefixed32(buffer, i);
  return slice(buffer, sizeof(uint32_t));
}

class bloomtest {
 private:
  const filterpolicy* policy_;
  std::string filter_;
  std::vector<std::string> keys_;

 public:
  bloomtest() : policy_(newbloomfilterpolicy(10)) { }

  ~bloomtest() {
    delete policy_;
  }

  void reset() {
    keys_.clear();
    filter_.clear();
  }

  void add(const slice& s) {
    keys_.push_back(s.tostring());
  }

  void build() {
    std::vector<slice> key_slices;
    for (size_t i = 0; i < keys_.size(); i++) {
      key_slices.push_back(slice(keys_[i]));
    }
    filter_.clear();
    policy_->createfilter(&key_slices[0], key_slices.size(), &filter_);
    keys_.clear();
    if (kverbose >= 2) dumpfilter();
  }

  size_t filtersize() const {
    return filter_.size();
  }

  void dumpfilter() {
    fprintf(stderr, "f(");
    for (size_t i = 0; i+1 < filter_.size(); i++) {
      const unsigned int c = static_cast<unsigned int>(filter_[i]);
      for (int j = 0; j < 8; j++) {
        fprintf(stderr, "%c", (c & (1 <<j)) ? '1' : '.');
      }
    }
    fprintf(stderr, ")\n");
  }

  bool matches(const slice& s) {
    if (!keys_.empty()) {
      build();
    }
    return policy_->keymaymatch(s, filter_);
  }

  double falsepositiverate() {
    char buffer[sizeof(int)];
    int result = 0;
    for (int i = 0; i < 10000; i++) {
      if (matches(key(i + 1000000000, buffer))) {
        result++;
      }
    }
    return result / 10000.0;
  }
};

test(bloomtest, emptyfilter) {
  assert_true(! matches("hello"));
  assert_true(! matches("world"));
}

test(bloomtest, small) {
  add("hello");
  add("world");
  assert_true(matches("hello"));
  assert_true(matches("world"));
  assert_true(! matches("x"));
  assert_true(! matches("foo"));
}

static int nextlength(int length) {
  if (length < 10) {
    length += 1;
  } else if (length < 100) {
    length += 10;
  } else if (length < 1000) {
    length += 100;
  } else {
    length += 1000;
  }
  return length;
}

test(bloomtest, varyinglengths) {
  char buffer[sizeof(int)];

  // count number of filters that significantly exceed the false positive rate
  int mediocre_filters = 0;
  int good_filters = 0;

  for (int length = 1; length <= 10000; length = nextlength(length)) {
    reset();
    for (int i = 0; i < length; i++) {
      add(key(i, buffer));
    }
    build();

    assert_le(filtersize(), static_cast<size_t>((length * 10 / 8) + 40))
        << length;

    // all added keys must match
    for (int i = 0; i < length; i++) {
      assert_true(matches(key(i, buffer)))
          << "length " << length << "; key " << i;
    }

    // check false positive rate
    double rate = falsepositiverate();
    if (kverbose >= 1) {
      fprintf(stderr, "false positives: %5.2f%% @ length = %6d ; bytes = %6d\n",
              rate*100.0, length, static_cast<int>(filtersize()));
    }
    assert_le(rate, 0.02);   // must not be over 2%
    if (rate > 0.0125) mediocre_filters++;  // allowed, but not too often
    else good_filters++;
  }
  if (kverbose >= 1) {
    fprintf(stderr, "filters: %d good, %d mediocre\n",
            good_filters, mediocre_filters);
  }
  assert_le(mediocre_filters, good_filters/5);
}

// different bits-per-byte

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
