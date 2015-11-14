// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "leveldb/table.h"

#include <map>
#include <string>
#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "leveldb/table_builder.h"
#include "table/block.h"
#include "table/block_builder.h"
#include "table/format.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

// return reverse of "key".
// used to test non-lexicographic comparators.
static std::string reverse(const slice& key) {
  std::string str(key.tostring());
  std::string rev("");
  for (std::string::reverse_iterator rit = str.rbegin();
       rit != str.rend(); ++rit) {
    rev.push_back(*rit);
  }
  return rev;
}

namespace {
class reversekeycomparator : public comparator {
 public:
  virtual const char* name() const {
    return "leveldb.reversebytewisecomparator";
  }

  virtual int compare(const slice& a, const slice& b) const {
    return bytewisecomparator()->compare(reverse(a), reverse(b));
  }

  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const {
    std::string s = reverse(*start);
    std::string l = reverse(limit);
    bytewisecomparator()->findshortestseparator(&s, l);
    *start = reverse(s);
  }

  virtual void findshortsuccessor(std::string* key) const {
    std::string s = reverse(*key);
    bytewisecomparator()->findshortsuccessor(&s);
    *key = reverse(s);
  }
};
}  // namespace
static reversekeycomparator reverse_key_comparator;

static void increment(const comparator* cmp, std::string* key) {
  if (cmp == bytewisecomparator()) {
    key->push_back('\0');
  } else {
    assert(cmp == &reverse_key_comparator);
    std::string rev = reverse(*key);
    rev.push_back('\0');
    *key = reverse(rev);
  }
}

// an stl comparator that uses a comparator
namespace {
struct stllessthan {
  const comparator* cmp;

  stllessthan() : cmp(bytewisecomparator()) { }
  stllessthan(const comparator* c) : cmp(c) { }
  bool operator()(const std::string& a, const std::string& b) const {
    return cmp->compare(slice(a), slice(b)) < 0;
  }
};
}  // namespace

class stringsink: public writablefile {
 public:
  ~stringsink() { }

  const std::string& contents() const { return contents_; }

  virtual status close() { return status::ok(); }
  virtual status flush() { return status::ok(); }
  virtual status sync() { return status::ok(); }

  virtual status append(const slice& data) {
    contents_.append(data.data(), data.size());
    return status::ok();
  }

 private:
  std::string contents_;
};


class stringsource: public randomaccessfile {
 public:
  stringsource(const slice& contents)
      : contents_(contents.data(), contents.size()) {
  }

  virtual ~stringsource() { }

  uint64_t size() const { return contents_.size(); }

  virtual status read(uint64_t offset, size_t n, slice* result,
                       char* scratch) const {
    if (offset > contents_.size()) {
      return status::invalidargument("invalid read offset");
    }
    if (offset + n > contents_.size()) {
      n = contents_.size() - offset;
    }
    memcpy(scratch, &contents_[offset], n);
    *result = slice(scratch, n);
    return status::ok();
  }

 private:
  std::string contents_;
};

typedef std::map<std::string, std::string, stllessthan> kvmap;

// helper class for tests to unify the interface between
// blockbuilder/tablebuilder and block/table.
class constructor {
 public:
  explicit constructor(const comparator* cmp) : data_(stllessthan(cmp)) { }
  virtual ~constructor() { }

  void add(const std::string& key, const slice& value) {
    data_[key] = value.tostring();
  }

  // finish constructing the data structure with all the keys that have
  // been added so far.  returns the keys in sorted order in "*keys"
  // and stores the key/value pairs in "*kvmap"
  void finish(const options& options,
              std::vector<std::string>* keys,
              kvmap* kvmap) {
    *kvmap = data_;
    keys->clear();
    for (kvmap::const_iterator it = data_.begin();
         it != data_.end();
         ++it) {
      keys->push_back(it->first);
    }
    data_.clear();
    status s = finishimpl(options, *kvmap);
    assert_true(s.ok()) << s.tostring();
  }

  // construct the data structure from the data in "data"
  virtual status finishimpl(const options& options, const kvmap& data) = 0;

  virtual iterator* newiterator() const = 0;

  virtual const kvmap& data() { return data_; }

  virtual db* db() const { return null; }  // overridden in dbconstructor

 private:
  kvmap data_;
};

class blockconstructor: public constructor {
 public:
  explicit blockconstructor(const comparator* cmp)
      : constructor(cmp),
        comparator_(cmp),
        block_(null) { }
  ~blockconstructor() {
    delete block_;
  }
  virtual status finishimpl(const options& options, const kvmap& data) {
    delete block_;
    block_ = null;
    blockbuilder builder(&options);

    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.add(it->first, it->second);
    }
    // open the block
    data_ = builder.finish().tostring();
    blockcontents contents;
    contents.data = data_;
    contents.cachable = false;
    contents.heap_allocated = false;
    block_ = new block(contents);
    return status::ok();
  }
  virtual iterator* newiterator() const {
    return block_->newiterator(comparator_);
  }

 private:
  const comparator* comparator_;
  std::string data_;
  block* block_;

  blockconstructor();
};

class tableconstructor: public constructor {
 public:
  tableconstructor(const comparator* cmp)
      : constructor(cmp),
        source_(null), table_(null) {
  }
  ~tableconstructor() {
    reset();
  }
  virtual status finishimpl(const options& options, const kvmap& data) {
    reset();
    stringsink sink;
    tablebuilder builder(options, &sink);

    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      builder.add(it->first, it->second);
      assert_true(builder.status().ok());
    }
    status s = builder.finish();
    assert_true(s.ok()) << s.tostring();

    assert_eq(sink.contents().size(), builder.filesize());

    // open the table
    source_ = new stringsource(sink.contents());
    options table_options;
    table_options.comparator = options.comparator;
    return table::open(table_options, source_, sink.contents().size(), &table_);
  }

  virtual iterator* newiterator() const {
    return table_->newiterator(readoptions());
  }

  uint64_t approximateoffsetof(const slice& key) const {
    return table_->approximateoffsetof(key);
  }

 private:
  void reset() {
    delete table_;
    delete source_;
    table_ = null;
    source_ = null;
  }

  stringsource* source_;
  table* table_;

  tableconstructor();
};

// a helper class that converts internal format keys into user keys
class keyconvertingiterator: public iterator {
 public:
  explicit keyconvertingiterator(iterator* iter) : iter_(iter) { }
  virtual ~keyconvertingiterator() { delete iter_; }
  virtual bool valid() const { return iter_->valid(); }
  virtual void seek(const slice& target) {
    parsedinternalkey ikey(target, kmaxsequencenumber, ktypevalue);
    std::string encoded;
    appendinternalkey(&encoded, ikey);
    iter_->seek(encoded);
  }
  virtual void seektofirst() { iter_->seektofirst(); }
  virtual void seektolast() { iter_->seektolast(); }
  virtual void next() { iter_->next(); }
  virtual void prev() { iter_->prev(); }

  virtual slice key() const {
    assert(valid());
    parsedinternalkey key;
    if (!parseinternalkey(iter_->key(), &key)) {
      status_ = status::corruption("malformed internal key");
      return slice("corrupted key");
    }
    return key.user_key;
  }

  virtual slice value() const { return iter_->value(); }
  virtual status status() const {
    return status_.ok() ? iter_->status() : status_;
  }

 private:
  mutable status status_;
  iterator* iter_;

  // no copying allowed
  keyconvertingiterator(const keyconvertingiterator&);
  void operator=(const keyconvertingiterator&);
};

class memtableconstructor: public constructor {
 public:
  explicit memtableconstructor(const comparator* cmp)
      : constructor(cmp),
        internal_comparator_(cmp) {
    memtable_ = new memtable(internal_comparator_);
    memtable_->ref();
  }
  ~memtableconstructor() {
    memtable_->unref();
  }
  virtual status finishimpl(const options& options, const kvmap& data) {
    memtable_->unref();
    memtable_ = new memtable(internal_comparator_);
    memtable_->ref();
    int seq = 1;
    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      memtable_->add(seq, ktypevalue, it->first, it->second);
      seq++;
    }
    return status::ok();
  }
  virtual iterator* newiterator() const {
    return new keyconvertingiterator(memtable_->newiterator());
  }

 private:
  internalkeycomparator internal_comparator_;
  memtable* memtable_;
};

class dbconstructor: public constructor {
 public:
  explicit dbconstructor(const comparator* cmp)
      : constructor(cmp),
        comparator_(cmp) {
    db_ = null;
    newdb();
  }
  ~dbconstructor() {
    delete db_;
  }
  virtual status finishimpl(const options& options, const kvmap& data) {
    delete db_;
    db_ = null;
    newdb();
    for (kvmap::const_iterator it = data.begin();
         it != data.end();
         ++it) {
      writebatch batch;
      batch.put(it->first, it->second);
      assert_true(db_->write(writeoptions(), &batch).ok());
    }
    return status::ok();
  }
  virtual iterator* newiterator() const {
    return db_->newiterator(readoptions());
  }

  virtual db* db() const { return db_; }

 private:
  void newdb() {
    std::string name = test::tmpdir() + "/table_testdb";

    options options;
    options.comparator = comparator_;
    status status = destroydb(name, options);
    assert_true(status.ok()) << status.tostring();

    options.create_if_missing = true;
    options.error_if_exists = true;
    options.write_buffer_size = 10000;  // something small to force merging
    status = db::open(options, name, &db_);
    assert_true(status.ok()) << status.tostring();
  }

  const comparator* comparator_;
  db* db_;
};

enum testtype {
  table_test,
  block_test,
  memtable_test,
  db_test
};

struct testargs {
  testtype type;
  bool reverse_compare;
  int restart_interval;
};

static const testargs ktestarglist[] = {
  { table_test, false, 16 },
  { table_test, false, 1 },
  { table_test, false, 1024 },
  { table_test, true, 16 },
  { table_test, true, 1 },
  { table_test, true, 1024 },

  { block_test, false, 16 },
  { block_test, false, 1 },
  { block_test, false, 1024 },
  { block_test, true, 16 },
  { block_test, true, 1 },
  { block_test, true, 1024 },

  // restart interval does not matter for memtables
  { memtable_test, false, 16 },
  { memtable_test, true, 16 },

  // do not bother with restart interval variations for db
  { db_test, false, 16 },
  { db_test, true, 16 },
};
static const int knumtestargs = sizeof(ktestarglist) / sizeof(ktestarglist[0]);

class harness {
 public:
  harness() : constructor_(null) { }

  void init(const testargs& args) {
    delete constructor_;
    constructor_ = null;
    options_ = options();

    options_.block_restart_interval = args.restart_interval;
    // use shorter block size for tests to exercise block boundary
    // conditions more.
    options_.block_size = 256;
    if (args.reverse_compare) {
      options_.comparator = &reverse_key_comparator;
    }
    switch (args.type) {
      case table_test:
        constructor_ = new tableconstructor(options_.comparator);
        break;
      case block_test:
        constructor_ = new blockconstructor(options_.comparator);
        break;
      case memtable_test:
        constructor_ = new memtableconstructor(options_.comparator);
        break;
      case db_test:
        constructor_ = new dbconstructor(options_.comparator);
        break;
    }
  }

  ~harness() {
    delete constructor_;
  }

  void add(const std::string& key, const std::string& value) {
    constructor_->add(key, value);
  }

  void test(random* rnd) {
    std::vector<std::string> keys;
    kvmap data;
    constructor_->finish(options_, &keys, &data);

    testforwardscan(keys, data);
    testbackwardscan(keys, data);
    testrandomaccess(rnd, keys, data);
  }

  void testforwardscan(const std::vector<std::string>& keys,
                       const kvmap& data) {
    iterator* iter = constructor_->newiterator();
    assert_true(!iter->valid());
    iter->seektofirst();
    for (kvmap::const_iterator model_iter = data.begin();
         model_iter != data.end();
         ++model_iter) {
      assert_eq(tostring(data, model_iter), tostring(iter));
      iter->next();
    }
    assert_true(!iter->valid());
    delete iter;
  }

  void testbackwardscan(const std::vector<std::string>& keys,
                        const kvmap& data) {
    iterator* iter = constructor_->newiterator();
    assert_true(!iter->valid());
    iter->seektolast();
    for (kvmap::const_reverse_iterator model_iter = data.rbegin();
         model_iter != data.rend();
         ++model_iter) {
      assert_eq(tostring(data, model_iter), tostring(iter));
      iter->prev();
    }
    assert_true(!iter->valid());
    delete iter;
  }

  void testrandomaccess(random* rnd,
                        const std::vector<std::string>& keys,
                        const kvmap& data) {
    static const bool kverbose = false;
    iterator* iter = constructor_->newiterator();
    assert_true(!iter->valid());
    kvmap::const_iterator model_iter = data.begin();
    if (kverbose) fprintf(stderr, "---\n");
    for (int i = 0; i < 200; i++) {
      const int toss = rnd->uniform(5);
      switch (toss) {
        case 0: {
          if (iter->valid()) {
            if (kverbose) fprintf(stderr, "next\n");
            iter->next();
            ++model_iter;
            assert_eq(tostring(data, model_iter), tostring(iter));
          }
          break;
        }

        case 1: {
          if (kverbose) fprintf(stderr, "seektofirst\n");
          iter->seektofirst();
          model_iter = data.begin();
          assert_eq(tostring(data, model_iter), tostring(iter));
          break;
        }

        case 2: {
          std::string key = pickrandomkey(rnd, keys);
          model_iter = data.lower_bound(key);
          if (kverbose) fprintf(stderr, "seek '%s'\n",
                                escapestring(key).c_str());
          iter->seek(slice(key));
          assert_eq(tostring(data, model_iter), tostring(iter));
          break;
        }

        case 3: {
          if (iter->valid()) {
            if (kverbose) fprintf(stderr, "prev\n");
            iter->prev();
            if (model_iter == data.begin()) {
              model_iter = data.end();   // wrap around to invalid value
            } else {
              --model_iter;
            }
            assert_eq(tostring(data, model_iter), tostring(iter));
          }
          break;
        }

        case 4: {
          if (kverbose) fprintf(stderr, "seektolast\n");
          iter->seektolast();
          if (keys.empty()) {
            model_iter = data.end();
          } else {
            std::string last = data.rbegin()->first;
            model_iter = data.lower_bound(last);
          }
          assert_eq(tostring(data, model_iter), tostring(iter));
          break;
        }
      }
    }
    delete iter;
  }

  std::string tostring(const kvmap& data, const kvmap::const_iterator& it) {
    if (it == data.end()) {
      return "end";
    } else {
      return "'" + it->first + "->" + it->second + "'";
    }
  }

  std::string tostring(const kvmap& data,
                       const kvmap::const_reverse_iterator& it) {
    if (it == data.rend()) {
      return "end";
    } else {
      return "'" + it->first + "->" + it->second + "'";
    }
  }

  std::string tostring(const iterator* it) {
    if (!it->valid()) {
      return "end";
    } else {
      return "'" + it->key().tostring() + "->" + it->value().tostring() + "'";
    }
  }

  std::string pickrandomkey(random* rnd, const std::vector<std::string>& keys) {
    if (keys.empty()) {
      return "foo";
    } else {
      const int index = rnd->uniform(keys.size());
      std::string result = keys[index];
      switch (rnd->uniform(3)) {
        case 0:
          // return an existing key
          break;
        case 1: {
          // attempt to return something smaller than an existing key
          if (result.size() > 0 && result[result.size()-1] > '\0') {
            result[result.size()-1]--;
          }
          break;
        }
        case 2: {
          // return something larger than an existing key
          increment(options_.comparator, &result);
          break;
        }
      }
      return result;
    }
  }

  // returns null if not running against a db
  db* db() const { return constructor_->db(); }

 private:
  options options_;
  constructor* constructor_;
};

// test empty table/block.
test(harness, empty) {
  for (int i = 0; i < knumtestargs; i++) {
    init(ktestarglist[i]);
    random rnd(test::randomseed() + 1);
    test(&rnd);
  }
}

// special test for a block with no restart entries.  the c++ leveldb
// code never generates such blocks, but the java version of leveldb
// seems to.
test(harness, zerorestartpointsinblock) {
  char data[sizeof(uint32_t)];
  memset(data, 0, sizeof(data));
  blockcontents contents;
  contents.data = slice(data, sizeof(data));
  contents.cachable = false;
  contents.heap_allocated = false;
  block block(contents);
  iterator* iter = block.newiterator(bytewisecomparator());
  iter->seektofirst();
  assert_true(!iter->valid());
  iter->seektolast();
  assert_true(!iter->valid());
  iter->seek("foo");
  assert_true(!iter->valid());
  delete iter;
}

// test the empty key
test(harness, simpleemptykey) {
  for (int i = 0; i < knumtestargs; i++) {
    init(ktestarglist[i]);
    random rnd(test::randomseed() + 1);
    add("", "v");
    test(&rnd);
  }
}

test(harness, simplesingle) {
  for (int i = 0; i < knumtestargs; i++) {
    init(ktestarglist[i]);
    random rnd(test::randomseed() + 2);
    add("abc", "v");
    test(&rnd);
  }
}

test(harness, simplemulti) {
  for (int i = 0; i < knumtestargs; i++) {
    init(ktestarglist[i]);
    random rnd(test::randomseed() + 3);
    add("abc", "v");
    add("abcd", "v");
    add("ac", "v2");
    test(&rnd);
  }
}

test(harness, simplespecialkey) {
  for (int i = 0; i < knumtestargs; i++) {
    init(ktestarglist[i]);
    random rnd(test::randomseed() + 4);
    add("\xff\xff", "v3");
    test(&rnd);
  }
}

test(harness, randomized) {
  for (int i = 0; i < knumtestargs; i++) {
    init(ktestarglist[i]);
    random rnd(test::randomseed() + 5);
    for (int num_entries = 0; num_entries < 2000;
         num_entries += (num_entries < 50 ? 1 : 200)) {
      if ((num_entries % 10) == 0) {
        fprintf(stderr, "case %d of %d: num_entries = %d\n",
                (i + 1), int(knumtestargs), num_entries);
      }
      for (int e = 0; e < num_entries; e++) {
        std::string v;
        add(test::randomkey(&rnd, rnd.skewed(4)),
            test::randomstring(&rnd, rnd.skewed(5), &v).tostring());
      }
      test(&rnd);
    }
  }
}

test(harness, randomizedlongdb) {
  random rnd(test::randomseed());
  testargs args = { db_test, false, 16 };
  init(args);
  int num_entries = 100000;
  for (int e = 0; e < num_entries; e++) {
    std::string v;
    add(test::randomkey(&rnd, rnd.skewed(4)),
        test::randomstring(&rnd, rnd.skewed(5), &v).tostring());
  }
  test(&rnd);

  // we must have created enough data to force merging
  int files = 0;
  for (int level = 0; level < config::knumlevels; level++) {
    std::string value;
    char name[100];
    snprintf(name, sizeof(name), "leveldb.num-files-at-level%d", level);
    assert_true(db()->getproperty(name, &value));
    files += atoi(value.c_str());
  }
  assert_gt(files, 0);
}

class memtabletest { };

test(memtabletest, simple) {
  internalkeycomparator cmp(bytewisecomparator());
  memtable* memtable = new memtable(cmp);
  memtable->ref();
  writebatch batch;
  writebatchinternal::setsequence(&batch, 100);
  batch.put(std::string("k1"), std::string("v1"));
  batch.put(std::string("k2"), std::string("v2"));
  batch.put(std::string("k3"), std::string("v3"));
  batch.put(std::string("largekey"), std::string("vlarge"));
  assert_true(writebatchinternal::insertinto(&batch, memtable).ok());

  iterator* iter = memtable->newiterator();
  iter->seektofirst();
  while (iter->valid()) {
    fprintf(stderr, "key: '%s' -> '%s'\n",
            iter->key().tostring().c_str(),
            iter->value().tostring().c_str());
    iter->next();
  }

  delete iter;
  memtable->unref();
}

static bool between(uint64_t val, uint64_t low, uint64_t high) {
  bool result = (val >= low) && (val <= high);
  if (!result) {
    fprintf(stderr, "value %llu is not in range [%llu, %llu]\n",
            (unsigned long long)(val),
            (unsigned long long)(low),
            (unsigned long long)(high));
  }
  return result;
}

class tabletest { };

test(tabletest, approximateoffsetofplain) {
  tableconstructor c(bytewisecomparator());
  c.add("k01", "hello");
  c.add("k02", "hello2");
  c.add("k03", std::string(10000, 'x'));
  c.add("k04", std::string(200000, 'x'));
  c.add("k05", std::string(300000, 'x'));
  c.add("k06", "hello3");
  c.add("k07", std::string(100000, 'x'));
  std::vector<std::string> keys;
  kvmap kvmap;
  options options;
  options.block_size = 1024;
  options.compression = knocompression;
  c.finish(options, &keys, &kvmap);

  assert_true(between(c.approximateoffsetof("abc"),       0,      0));
  assert_true(between(c.approximateoffsetof("k01"),       0,      0));
  assert_true(between(c.approximateoffsetof("k01a"),      0,      0));
  assert_true(between(c.approximateoffsetof("k02"),       0,      0));
  assert_true(between(c.approximateoffsetof("k03"),       0,      0));
  assert_true(between(c.approximateoffsetof("k04"),   10000,  11000));
  assert_true(between(c.approximateoffsetof("k04a"), 210000, 211000));
  assert_true(between(c.approximateoffsetof("k05"),  210000, 211000));
  assert_true(between(c.approximateoffsetof("k06"),  510000, 511000));
  assert_true(between(c.approximateoffsetof("k07"),  510000, 511000));
  assert_true(between(c.approximateoffsetof("xyz"),  610000, 612000));

}

static bool snappycompressionsupported() {
  std::string out;
  slice in = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  return port::snappy_compress(in.data(), in.size(), &out);
}

test(tabletest, approximateoffsetofcompressed) {
  if (!snappycompressionsupported()) {
    fprintf(stderr, "skipping compression tests\n");
    return;
  }

  random rnd(301);
  tableconstructor c(bytewisecomparator());
  std::string tmp;
  c.add("k01", "hello");
  c.add("k02", test::compressiblestring(&rnd, 0.25, 10000, &tmp));
  c.add("k03", "hello3");
  c.add("k04", test::compressiblestring(&rnd, 0.25, 10000, &tmp));
  std::vector<std::string> keys;
  kvmap kvmap;
  options options;
  options.block_size = 1024;
  options.compression = ksnappycompression;
  c.finish(options, &keys, &kvmap);

  assert_true(between(c.approximateoffsetof("abc"),       0,      0));
  assert_true(between(c.approximateoffsetof("k01"),       0,      0));
  assert_true(between(c.approximateoffsetof("k02"),       0,      0));
  assert_true(between(c.approximateoffsetof("k03"),    2000,   3000));
  assert_true(between(c.approximateoffsetof("k04"),    2000,   3000));
  assert_true(between(c.approximateoffsetof("xyz"),    4000,   6000));
}

}  // namespace leveldb

int main(int argc, char** argv) {
  return leveldb::test::runalltests();
}
