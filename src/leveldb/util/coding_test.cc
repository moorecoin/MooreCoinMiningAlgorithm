// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/coding.h"

#include "util/testharness.h"

namespace leveldb {

class coding { };

test(coding, fixed32) {
  std::string s;
  for (uint32_t v = 0; v < 100000; v++) {
    putfixed32(&s, v);
  }

  const char* p = s.data();
  for (uint32_t v = 0; v < 100000; v++) {
    uint32_t actual = decodefixed32(p);
    assert_eq(v, actual);
    p += sizeof(uint32_t);
  }
}

test(coding, fixed64) {
  std::string s;
  for (int power = 0; power <= 63; power++) {
    uint64_t v = static_cast<uint64_t>(1) << power;
    putfixed64(&s, v - 1);
    putfixed64(&s, v + 0);
    putfixed64(&s, v + 1);
  }

  const char* p = s.data();
  for (int power = 0; power <= 63; power++) {
    uint64_t v = static_cast<uint64_t>(1) << power;
    uint64_t actual;
    actual = decodefixed64(p);
    assert_eq(v-1, actual);
    p += sizeof(uint64_t);

    actual = decodefixed64(p);
    assert_eq(v+0, actual);
    p += sizeof(uint64_t);

    actual = decodefixed64(p);
    assert_eq(v+1, actual);
    p += sizeof(uint64_t);
  }
}

// test that encoding routines generate little-endian encodings
test(coding, encodingoutput) {
  std::string dst;
  putfixed32(&dst, 0x04030201);
  assert_eq(4, dst.size());
  assert_eq(0x01, static_cast<int>(dst[0]));
  assert_eq(0x02, static_cast<int>(dst[1]));
  assert_eq(0x03, static_cast<int>(dst[2]));
  assert_eq(0x04, static_cast<int>(dst[3]));

  dst.clear();
  putfixed64(&dst, 0x0807060504030201ull);
  assert_eq(8, dst.size());
  assert_eq(0x01, static_cast<int>(dst[0]));
  assert_eq(0x02, static_cast<int>(dst[1]));
  assert_eq(0x03, static_cast<int>(dst[2]));
  assert_eq(0x04, static_cast<int>(dst[3]));
  assert_eq(0x05, static_cast<int>(dst[4]));
  assert_eq(0x06, static_cast<int>(dst[5]));
  assert_eq(0x07, static_cast<int>(dst[6]));
  assert_eq(0x08, static_cast<int>(dst[7]));
}

test(coding, varint32) {
  std::string s;
  for (uint32_t i = 0; i < (32 * 32); i++) {
    uint32_t v = (i / 32) << (i % 32);
    putvarint32(&s, v);
  }

  const char* p = s.data();
  const char* limit = p + s.size();
  for (uint32_t i = 0; i < (32 * 32); i++) {
    uint32_t expected = (i / 32) << (i % 32);
    uint32_t actual;
    const char* start = p;
    p = getvarint32ptr(p, limit, &actual);
    assert_true(p != null);
    assert_eq(expected, actual);
    assert_eq(varintlength(actual), p - start);
  }
  assert_eq(p, s.data() + s.size());
}

test(coding, varint64) {
  // construct the list of values to check
  std::vector<uint64_t> values;
  // some special values
  values.push_back(0);
  values.push_back(100);
  values.push_back(~static_cast<uint64_t>(0));
  values.push_back(~static_cast<uint64_t>(0) - 1);
  for (uint32_t k = 0; k < 64; k++) {
    // test values near powers of two
    const uint64_t power = 1ull << k;
    values.push_back(power);
    values.push_back(power-1);
    values.push_back(power+1);
  }

  std::string s;
  for (size_t i = 0; i < values.size(); i++) {
    putvarint64(&s, values[i]);
  }

  const char* p = s.data();
  const char* limit = p + s.size();
  for (size_t i = 0; i < values.size(); i++) {
    assert_true(p < limit);
    uint64_t actual;
    const char* start = p;
    p = getvarint64ptr(p, limit, &actual);
    assert_true(p != null);
    assert_eq(values[i], actual);
    assert_eq(varintlength(actual), p - start);
  }
  assert_eq(p, limit);

}

test(coding, varint32overflow) {
  uint32_t result;
  std::string input("\x81\x82\x83\x84\x85\x11");
  assert_true(getvarint32ptr(input.data(), input.data() + input.size(), &result)
              == null);
}

test(coding, varint32truncation) {
  uint32_t large_value = (1u << 31) + 100;
  std::string s;
  putvarint32(&s, large_value);
  uint32_t result;
  for (size_t len = 0; len < s.size() - 1; len++) {
    assert_true(getvarint32ptr(s.data(), s.data() + len, &result) == null);
  }
  assert_true(getvarint32ptr(s.data(), s.data() + s.size(), &result) != null);
  assert_eq(large_value, result);
}

test(coding, varint64overflow) {
  uint64_t result;
  std::string input("\x81\x82\x83\x84\x85\x81\x82\x83\x84\x85\x11");
  assert_true(getvarint64ptr(input.data(), input.data() + input.size(), &result)
              == null);
}

test(coding, varint64truncation) {
  uint64_t large_value = (1ull << 63) + 100ull;
  std::string s;
  putvarint64(&s, large_value);
  uint64_t result;
  for (size_t len = 0; len < s.size() - 1; len++) {
    assert_true(getvarint64ptr(s.data(), s.data() + len, &result) == null);
  }
  assert_true(getvarint64ptr(s.data(), s.data() + s.size(), &result) != null);
  assert_eq(large_value, result);
}

test(coding, strings) {
  std::string s;
  putlengthprefixedslice(&s, slice(""));
  putlengthprefixedslice(&s, slice("foo"));
  putlengthprefixedslice(&s, slice("bar"));
  putlengthprefixedslice(&s, slice(std::string(200, 'x')));

  slice input(s);
  slice v;
  assert_true(getlengthprefixedslice(&input, &v));
  assert_eq("", v.tostring());
  assert_true(getlengthprefixedslice(&input, &v));
  assert_eq("foo", v.tostring());
  assert_true(getlengthprefixedslice(&input, &v));
  assert_eq("bar", v.tostring());
  assert_true(getlengthprefixedslice(&input, &v));
  assert_eq(std::string(200, 'x'), v.tostring());
  assert_eq("", input.tostring());
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
