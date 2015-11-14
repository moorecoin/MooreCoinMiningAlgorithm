// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/log_reader.h"
#include "db/log_writer.h"
#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/random.h"
#include "util/testharness.h"

namespace leveldb {
namespace log {

// construct a string of the specified length made out of the supplied
// partial string.
static std::string bigstring(const std::string& partial_string, size_t n) {
  std::string result;
  while (result.size() < n) {
    result.append(partial_string);
  }
  result.resize(n);
  return result;
}

// construct a string from a number
static std::string numberstring(int n) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d.", n);
  return std::string(buf);
}

// return a skewed potentially long string
static std::string randomskewedstring(int i, random* rnd) {
  return bigstring(numberstring(i), rnd->skewed(17));
}

class logtest {
 private:
  class stringdest : public writablefile {
   public:
    std::string contents_;

    virtual status close() { return status::ok(); }
    virtual status flush() { return status::ok(); }
    virtual status sync() { return status::ok(); }
    virtual status append(const slice& slice) {
      contents_.append(slice.data(), slice.size());
      return status::ok();
    }
  };

  class stringsource : public sequentialfile {
   public:
    slice contents_;
    bool force_error_;
    bool returned_partial_;
    stringsource() : force_error_(false), returned_partial_(false) { }

    virtual status read(size_t n, slice* result, char* scratch) {
      assert_true(!returned_partial_) << "must not read() after eof/error";

      if (force_error_) {
        force_error_ = false;
        returned_partial_ = true;
        return status::corruption("read error");
      }

      if (contents_.size() < n) {
        n = contents_.size();
        returned_partial_ = true;
      }
      *result = slice(contents_.data(), n);
      contents_.remove_prefix(n);
      return status::ok();
    }

    virtual status skip(uint64_t n) {
      if (n > contents_.size()) {
        contents_.clear();
        return status::notfound("in-memory file skipepd past end");
      }

      contents_.remove_prefix(n);

      return status::ok();
    }
  };

  class reportcollector : public reader::reporter {
   public:
    size_t dropped_bytes_;
    std::string message_;

    reportcollector() : dropped_bytes_(0) { }
    virtual void corruption(size_t bytes, const status& status) {
      dropped_bytes_ += bytes;
      message_.append(status.tostring());
    }
  };

  stringdest dest_;
  stringsource source_;
  reportcollector report_;
  bool reading_;
  writer writer_;
  reader reader_;

  // record metadata for testing initial offset functionality
  static size_t initial_offset_record_sizes_[];
  static uint64_t initial_offset_last_record_offsets_[];

 public:
  logtest() : reading_(false),
              writer_(&dest_),
              reader_(&source_, &report_, true/*checksum*/,
                      0/*initial_offset*/) {
  }

  void write(const std::string& msg) {
    assert_true(!reading_) << "write() after starting to read";
    writer_.addrecord(slice(msg));
  }

  size_t writtenbytes() const {
    return dest_.contents_.size();
  }

  std::string read() {
    if (!reading_) {
      reading_ = true;
      source_.contents_ = slice(dest_.contents_);
    }
    std::string scratch;
    slice record;
    if (reader_.readrecord(&record, &scratch)) {
      return record.tostring();
    } else {
      return "eof";
    }
  }

  void incrementbyte(int offset, int delta) {
    dest_.contents_[offset] += delta;
  }

  void setbyte(int offset, char new_byte) {
    dest_.contents_[offset] = new_byte;
  }

  void shrinksize(int bytes) {
    dest_.contents_.resize(dest_.contents_.size() - bytes);
  }

  void fixchecksum(int header_offset, int len) {
    // compute crc of type/len/data
    uint32_t crc = crc32c::value(&dest_.contents_[header_offset+6], 1 + len);
    crc = crc32c::mask(crc);
    encodefixed32(&dest_.contents_[header_offset], crc);
  }

  void forceerror() {
    source_.force_error_ = true;
  }

  size_t droppedbytes() const {
    return report_.dropped_bytes_;
  }

  std::string reportmessage() const {
    return report_.message_;
  }

  // returns ok iff recorded error message contains "msg"
  std::string matcherror(const std::string& msg) const {
    if (report_.message_.find(msg) == std::string::npos) {
      return report_.message_;
    } else {
      return "ok";
    }
  }

  void writeinitialoffsetlog() {
    for (int i = 0; i < 4; i++) {
      std::string record(initial_offset_record_sizes_[i],
                         static_cast<char>('a' + i));
      write(record);
    }
  }

  void checkoffsetpastendreturnsnorecords(uint64_t offset_past_end) {
    writeinitialoffsetlog();
    reading_ = true;
    source_.contents_ = slice(dest_.contents_);
    reader* offset_reader = new reader(&source_, &report_, true/*checksum*/,
                                       writtenbytes() + offset_past_end);
    slice record;
    std::string scratch;
    assert_true(!offset_reader->readrecord(&record, &scratch));
    delete offset_reader;
  }

  void checkinitialoffsetrecord(uint64_t initial_offset,
                                int expected_record_offset) {
    writeinitialoffsetlog();
    reading_ = true;
    source_.contents_ = slice(dest_.contents_);
    reader* offset_reader = new reader(&source_, &report_, true/*checksum*/,
                                       initial_offset);
    slice record;
    std::string scratch;
    assert_true(offset_reader->readrecord(&record, &scratch));
    assert_eq(initial_offset_record_sizes_[expected_record_offset],
              record.size());
    assert_eq(initial_offset_last_record_offsets_[expected_record_offset],
              offset_reader->lastrecordoffset());
    assert_eq((char)('a' + expected_record_offset), record.data()[0]);
    delete offset_reader;
  }

};

size_t logtest::initial_offset_record_sizes_[] =
    {10000,  // two sizable records in first block
     10000,
     2 * log::kblocksize - 1000,  // span three blocks
     1};

uint64_t logtest::initial_offset_last_record_offsets_[] =
    {0,
     kheadersize + 10000,
     2 * (kheadersize + 10000),
     2 * (kheadersize + 10000) +
         (2 * log::kblocksize - 1000) + 3 * kheadersize};


test(logtest, empty) {
  assert_eq("eof", read());
}

test(logtest, readwrite) {
  write("foo");
  write("bar");
  write("");
  write("xxxx");
  assert_eq("foo", read());
  assert_eq("bar", read());
  assert_eq("", read());
  assert_eq("xxxx", read());
  assert_eq("eof", read());
  assert_eq("eof", read());  // make sure reads at eof work
}

test(logtest, manyblocks) {
  for (int i = 0; i < 100000; i++) {
    write(numberstring(i));
  }
  for (int i = 0; i < 100000; i++) {
    assert_eq(numberstring(i), read());
  }
  assert_eq("eof", read());
}

test(logtest, fragmentation) {
  write("small");
  write(bigstring("medium", 50000));
  write(bigstring("large", 100000));
  assert_eq("small", read());
  assert_eq(bigstring("medium", 50000), read());
  assert_eq(bigstring("large", 100000), read());
  assert_eq("eof", read());
}

test(logtest, marginaltrailer) {
  // make a trailer that is exactly the same length as an empty record.
  const int n = kblocksize - 2*kheadersize;
  write(bigstring("foo", n));
  assert_eq(kblocksize - kheadersize, writtenbytes());
  write("");
  write("bar");
  assert_eq(bigstring("foo", n), read());
  assert_eq("", read());
  assert_eq("bar", read());
  assert_eq("eof", read());
}

test(logtest, marginaltrailer2) {
  // make a trailer that is exactly the same length as an empty record.
  const int n = kblocksize - 2*kheadersize;
  write(bigstring("foo", n));
  assert_eq(kblocksize - kheadersize, writtenbytes());
  write("bar");
  assert_eq(bigstring("foo", n), read());
  assert_eq("bar", read());
  assert_eq("eof", read());
  assert_eq(0, droppedbytes());
  assert_eq("", reportmessage());
}

test(logtest, shorttrailer) {
  const int n = kblocksize - 2*kheadersize + 4;
  write(bigstring("foo", n));
  assert_eq(kblocksize - kheadersize + 4, writtenbytes());
  write("");
  write("bar");
  assert_eq(bigstring("foo", n), read());
  assert_eq("", read());
  assert_eq("bar", read());
  assert_eq("eof", read());
}

test(logtest, alignedeof) {
  const int n = kblocksize - 2*kheadersize + 4;
  write(bigstring("foo", n));
  assert_eq(kblocksize - kheadersize + 4, writtenbytes());
  assert_eq(bigstring("foo", n), read());
  assert_eq("eof", read());
}

test(logtest, randomread) {
  const int n = 500;
  random write_rnd(301);
  for (int i = 0; i < n; i++) {
    write(randomskewedstring(i, &write_rnd));
  }
  random read_rnd(301);
  for (int i = 0; i < n; i++) {
    assert_eq(randomskewedstring(i, &read_rnd), read());
  }
  assert_eq("eof", read());
}

// tests of all the error paths in log_reader.cc follow:

test(logtest, readerror) {
  write("foo");
  forceerror();
  assert_eq("eof", read());
  assert_eq(kblocksize, droppedbytes());
  assert_eq("ok", matcherror("read error"));
}

test(logtest, badrecordtype) {
  write("foo");
  // type is stored in header[6]
  incrementbyte(6, 100);
  fixchecksum(0, 3);
  assert_eq("eof", read());
  assert_eq(3, droppedbytes());
  assert_eq("ok", matcherror("unknown record type"));
}

test(logtest, truncatedtrailingrecordisignored) {
  write("foo");
  shrinksize(4);   // drop all payload as well as a header byte
  assert_eq("eof", read());
  // truncated last record is ignored, not treated as an error.
  assert_eq(0, droppedbytes());
  assert_eq("", reportmessage());
}

test(logtest, badlength) {
  const int kpayloadsize = kblocksize - kheadersize;
  write(bigstring("bar", kpayloadsize));
  write("foo");
  // least significant size byte is stored in header[4].
  incrementbyte(4, 1);
  assert_eq("foo", read());
  assert_eq(kblocksize, droppedbytes());
  assert_eq("ok", matcherror("bad record length"));
}

test(logtest, badlengthatendisignored) {
  write("foo");
  shrinksize(1);
  assert_eq("eof", read());
  assert_eq(0, droppedbytes());
  assert_eq("", reportmessage());
}

test(logtest, checksummismatch) {
  write("foo");
  incrementbyte(0, 10);
  assert_eq("eof", read());
  assert_eq(10, droppedbytes());
  assert_eq("ok", matcherror("checksum mismatch"));
}

test(logtest, unexpectedmiddletype) {
  write("foo");
  setbyte(6, kmiddletype);
  fixchecksum(0, 3);
  assert_eq("eof", read());
  assert_eq(3, droppedbytes());
  assert_eq("ok", matcherror("missing start"));
}

test(logtest, unexpectedlasttype) {
  write("foo");
  setbyte(6, klasttype);
  fixchecksum(0, 3);
  assert_eq("eof", read());
  assert_eq(3, droppedbytes());
  assert_eq("ok", matcherror("missing start"));
}

test(logtest, unexpectedfulltype) {
  write("foo");
  write("bar");
  setbyte(6, kfirsttype);
  fixchecksum(0, 3);
  assert_eq("bar", read());
  assert_eq("eof", read());
  assert_eq(3, droppedbytes());
  assert_eq("ok", matcherror("partial record without end"));
}

test(logtest, unexpectedfirsttype) {
  write("foo");
  write(bigstring("bar", 100000));
  setbyte(6, kfirsttype);
  fixchecksum(0, 3);
  assert_eq(bigstring("bar", 100000), read());
  assert_eq("eof", read());
  assert_eq(3, droppedbytes());
  assert_eq("ok", matcherror("partial record without end"));
}

test(logtest, missinglastisignored) {
  write(bigstring("bar", kblocksize));
  // remove the last block, including header.
  shrinksize(14);
  assert_eq("eof", read());
  assert_eq("", reportmessage());
  assert_eq(0, droppedbytes());
}

test(logtest, partiallastisignored) {
  write(bigstring("bar", kblocksize));
  // cause a bad record length in the last block.
  shrinksize(1);
  assert_eq("eof", read());
  assert_eq("", reportmessage());
  assert_eq(0, droppedbytes());
}

test(logtest, errorjoinsrecords) {
  // consider two fragmented records:
  //    first(r1) last(r1) first(r2) last(r2)
  // where the middle two fragments disappear.  we do not want
  // first(r1),last(r2) to get joined and returned as a valid record.

  // write records that span two blocks
  write(bigstring("foo", kblocksize));
  write(bigstring("bar", kblocksize));
  write("correct");

  // wipe the middle block
  for (int offset = kblocksize; offset < 2*kblocksize; offset++) {
    setbyte(offset, 'x');
  }

  assert_eq("correct", read());
  assert_eq("eof", read());
  const size_t dropped = droppedbytes();
  assert_le(dropped, 2*kblocksize + 100);
  assert_ge(dropped, 2*kblocksize);
}

test(logtest, readstart) {
  checkinitialoffsetrecord(0, 0);
}

test(logtest, readsecondoneoff) {
  checkinitialoffsetrecord(1, 1);
}

test(logtest, readsecondtenthousand) {
  checkinitialoffsetrecord(10000, 1);
}

test(logtest, readsecondstart) {
  checkinitialoffsetrecord(10007, 1);
}

test(logtest, readthirdoneoff) {
  checkinitialoffsetrecord(10008, 2);
}

test(logtest, readthirdstart) {
  checkinitialoffsetrecord(20014, 2);
}

test(logtest, readfourthoneoff) {
  checkinitialoffsetrecord(20015, 3);
}

test(logtest, readfourthfirstblocktrailer) {
  checkinitialoffsetrecord(log::kblocksize - 4, 3);
}

test(logtest, readfourthmiddleblock) {
  checkinitialoffsetrecord(log::kblocksize + 1, 3);
}

test(logtest, readfourthlastblock) {
  checkinitialoffsetrecord(2 * log::kblocksize + 1, 3);
}

test(logtest, readfourthstart) {
  checkinitialoffsetrecord(
      2 * (kheadersize + 1000) + (2 * log::kblocksize - 1000) + 3 * kheadersize,
      3);
}

test(logtest, readend) {
  checkoffsetpastendreturnsnorecords(0);
}

test(logtest, readpastend) {
  checkoffsetpastendreturnsnorecords(5);
}

}  // namespace log
}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
