// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_db_log_reader_h_
#define storage_leveldb_db_log_reader_h_

#include <stdint.h>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class sequentialfile;

namespace log {

class reader {
 public:
  // interface for reporting errors.
  class reporter {
   public:
    virtual ~reporter();

    // some corruption was detected.  "size" is the approximate number
    // of bytes dropped due to the corruption.
    virtual void corruption(size_t bytes, const status& status) = 0;
  };

  // create a reader that will return log records from "*file".
  // "*file" must remain live while this reader is in use.
  //
  // if "reporter" is non-null, it is notified whenever some data is
  // dropped due to a detected corruption.  "*reporter" must remain
  // live while this reader is in use.
  //
  // if "checksum" is true, verify checksums if available.
  //
  // the reader will start reading at the first record located at physical
  // position >= initial_offset within the file.
  reader(sequentialfile* file, reporter* reporter, bool checksum,
         uint64_t initial_offset);

  ~reader();

  // read the next record into *record.  returns true if read
  // successfully, false if we hit end of the input.  may use
  // "*scratch" as temporary storage.  the contents filled in *record
  // will only be valid until the next mutating operation on this
  // reader or the next mutation to *scratch.
  bool readrecord(slice* record, std::string* scratch);

  // returns the physical offset of the last record returned by readrecord.
  //
  // undefined before the first call to readrecord.
  uint64_t lastrecordoffset();

 private:
  sequentialfile* const file_;
  reporter* const reporter_;
  bool const checksum_;
  char* const backing_store_;
  slice buffer_;
  bool eof_;   // last read() indicated eof by returning < kblocksize

  // offset of the last record returned by readrecord.
  uint64_t last_record_offset_;
  // offset of the first location past the end of buffer_.
  uint64_t end_of_buffer_offset_;

  // offset at which to start looking for the first record to return
  uint64_t const initial_offset_;

  // extend record types with the following special values
  enum {
    keof = kmaxrecordtype + 1,
    // returned whenever we find an invalid physical record.
    // currently there are three situations in which this happens:
    // * the record has an invalid crc (readphysicalrecord reports a drop)
    // * the record is a 0-length record (no drop is reported)
    // * the record is below constructor's initial_offset (no drop is reported)
    kbadrecord = kmaxrecordtype + 2
  };

  // skips all blocks that are completely before "initial_offset_".
  //
  // returns true on success. handles reporting.
  bool skiptoinitialblock();

  // return type, or one of the preceding special values
  unsigned int readphysicalrecord(slice* result);

  // reports dropped bytes to the reporter.
  // buffer_ must be updated to remove the dropped bytes prior to invocation.
  void reportcorruption(uint64_t bytes, const char* reason);
  void reportdrop(uint64_t bytes, const status& reason);

  // no copying allowed
  reader(const reader&);
  void operator=(const reader&);
};

}  // namespace log
}  // namespace leveldb

#endif  // storage_leveldb_db_log_reader_h_
