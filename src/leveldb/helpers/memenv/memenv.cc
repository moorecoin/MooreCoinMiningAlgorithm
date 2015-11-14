// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "helpers/memenv/memenv.h"

#include "leveldb/env.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "util/mutexlock.h"
#include <map>
#include <string.h>
#include <string>
#include <vector>

namespace leveldb {

namespace {

class filestate {
 public:
  // filestates are reference counted. the initial reference count is zero
  // and the caller must call ref() at least once.
  filestate() : refs_(0), size_(0) {}

  // increase the reference count.
  void ref() {
    mutexlock lock(&refs_mutex_);
    ++refs_;
  }

  // decrease the reference count. delete if this is the last reference.
  void unref() {
    bool do_delete = false;

    {
      mutexlock lock(&refs_mutex_);
      --refs_;
      assert(refs_ >= 0);
      if (refs_ <= 0) {
        do_delete = true;
      }
    }

    if (do_delete) {
      delete this;
    }
  }

  uint64_t size() const { return size_; }

  status read(uint64_t offset, size_t n, slice* result, char* scratch) const {
    if (offset > size_) {
      return status::ioerror("offset greater than file size.");
    }
    const uint64_t available = size_ - offset;
    if (n > available) {
      n = static_cast<size_t>(available);
    }
    if (n == 0) {
      *result = slice();
      return status::ok();
    }

    assert(offset / kblocksize <= size_max);
    size_t block = static_cast<size_t>(offset / kblocksize);
    size_t block_offset = offset % kblocksize;

    if (n <= kblocksize - block_offset) {
      // the requested bytes are all in the first block.
      *result = slice(blocks_[block] + block_offset, n);
      return status::ok();
    }

    size_t bytes_to_copy = n;
    char* dst = scratch;

    while (bytes_to_copy > 0) {
      size_t avail = kblocksize - block_offset;
      if (avail > bytes_to_copy) {
        avail = bytes_to_copy;
      }
      memcpy(dst, blocks_[block] + block_offset, avail);

      bytes_to_copy -= avail;
      dst += avail;
      block++;
      block_offset = 0;
    }

    *result = slice(scratch, n);
    return status::ok();
  }

  status append(const slice& data) {
    const char* src = data.data();
    size_t src_len = data.size();

    while (src_len > 0) {
      size_t avail;
      size_t offset = size_ % kblocksize;

      if (offset != 0) {
        // there is some room in the last block.
        avail = kblocksize - offset;
      } else {
        // no room in the last block; push new one.
        blocks_.push_back(new char[kblocksize]);
        avail = kblocksize;
      }

      if (avail > src_len) {
        avail = src_len;
      }
      memcpy(blocks_.back() + offset, src, avail);
      src_len -= avail;
      src += avail;
      size_ += avail;
    }

    return status::ok();
  }

 private:
  // private since only unref() should be used to delete it.
  ~filestate() {
    for (std::vector<char*>::iterator i = blocks_.begin(); i != blocks_.end();
         ++i) {
      delete [] *i;
    }
  }

  // no copying allowed.
  filestate(const filestate&);
  void operator=(const filestate&);

  port::mutex refs_mutex_;
  int refs_;  // protected by refs_mutex_;

  // the following fields are not protected by any mutex. they are only mutable
  // while the file is being written, and concurrent access is not allowed
  // to writable files.
  std::vector<char*> blocks_;
  uint64_t size_;

  enum { kblocksize = 8 * 1024 };
};

class sequentialfileimpl : public sequentialfile {
 public:
  explicit sequentialfileimpl(filestate* file) : file_(file), pos_(0) {
    file_->ref();
  }

  ~sequentialfileimpl() {
    file_->unref();
  }

  virtual status read(size_t n, slice* result, char* scratch) {
    status s = file_->read(pos_, n, result, scratch);
    if (s.ok()) {
      pos_ += result->size();
    }
    return s;
  }

  virtual status skip(uint64_t n) {
    if (pos_ > file_->size()) {
      return status::ioerror("pos_ > file_->size()");
    }
    const uint64_t available = file_->size() - pos_;
    if (n > available) {
      n = available;
    }
    pos_ += n;
    return status::ok();
  }

 private:
  filestate* file_;
  uint64_t pos_;
};

class randomaccessfileimpl : public randomaccessfile {
 public:
  explicit randomaccessfileimpl(filestate* file) : file_(file) {
    file_->ref();
  }

  ~randomaccessfileimpl() {
    file_->unref();
  }

  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const {
    return file_->read(offset, n, result, scratch);
  }

 private:
  filestate* file_;
};

class writablefileimpl : public writablefile {
 public:
  writablefileimpl(filestate* file) : file_(file) {
    file_->ref();
  }

  ~writablefileimpl() {
    file_->unref();
  }

  virtual status append(const slice& data) {
    return file_->append(data);
  }

  virtual status close() { return status::ok(); }
  virtual status flush() { return status::ok(); }
  virtual status sync() { return status::ok(); }

 private:
  filestate* file_;
};

class nooplogger : public logger {
 public:
  virtual void logv(const char* format, va_list ap) { }
};

class inmemoryenv : public envwrapper {
 public:
  explicit inmemoryenv(env* base_env) : envwrapper(base_env) { }

  virtual ~inmemoryenv() {
    for (filesystem::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
      i->second->unref();
    }
  }

  // partial implementation of the env interface.
  virtual status newsequentialfile(const std::string& fname,
                                   sequentialfile** result) {
    mutexlock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = null;
      return status::ioerror(fname, "file not found");
    }

    *result = new sequentialfileimpl(file_map_[fname]);
    return status::ok();
  }

  virtual status newrandomaccessfile(const std::string& fname,
                                     randomaccessfile** result) {
    mutexlock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = null;
      return status::ioerror(fname, "file not found");
    }

    *result = new randomaccessfileimpl(file_map_[fname]);
    return status::ok();
  }

  virtual status newwritablefile(const std::string& fname,
                                 writablefile** result) {
    mutexlock lock(&mutex_);
    if (file_map_.find(fname) != file_map_.end()) {
      deletefileinternal(fname);
    }

    filestate* file = new filestate();
    file->ref();
    file_map_[fname] = file;

    *result = new writablefileimpl(file);
    return status::ok();
  }

  virtual bool fileexists(const std::string& fname) {
    mutexlock lock(&mutex_);
    return file_map_.find(fname) != file_map_.end();
  }

  virtual status getchildren(const std::string& dir,
                             std::vector<std::string>* result) {
    mutexlock lock(&mutex_);
    result->clear();

    for (filesystem::iterator i = file_map_.begin(); i != file_map_.end(); ++i){
      const std::string& filename = i->first;

      if (filename.size() >= dir.size() + 1 && filename[dir.size()] == '/' &&
          slice(filename).starts_with(slice(dir))) {
        result->push_back(filename.substr(dir.size() + 1));
      }
    }

    return status::ok();
  }

  void deletefileinternal(const std::string& fname) {
    if (file_map_.find(fname) == file_map_.end()) {
      return;
    }

    file_map_[fname]->unref();
    file_map_.erase(fname);
  }

  virtual status deletefile(const std::string& fname) {
    mutexlock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return status::ioerror(fname, "file not found");
    }

    deletefileinternal(fname);
    return status::ok();
  }

  virtual status createdir(const std::string& dirname) {
    return status::ok();
  }

  virtual status deletedir(const std::string& dirname) {
    return status::ok();
  }

  virtual status getfilesize(const std::string& fname, uint64_t* file_size) {
    mutexlock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return status::ioerror(fname, "file not found");
    }

    *file_size = file_map_[fname]->size();
    return status::ok();
  }

  virtual status renamefile(const std::string& src,
                            const std::string& target) {
    mutexlock lock(&mutex_);
    if (file_map_.find(src) == file_map_.end()) {
      return status::ioerror(src, "file not found");
    }

    deletefileinternal(target);
    file_map_[target] = file_map_[src];
    file_map_.erase(src);
    return status::ok();
  }

  virtual status lockfile(const std::string& fname, filelock** lock) {
    *lock = new filelock;
    return status::ok();
  }

  virtual status unlockfile(filelock* lock) {
    delete lock;
    return status::ok();
  }

  virtual status gettestdirectory(std::string* path) {
    *path = "/test";
    return status::ok();
  }

  virtual status newlogger(const std::string& fname, logger** result) {
    *result = new nooplogger;
    return status::ok();
  }

 private:
  // map from filenames to filestate objects, representing a simple file system.
  typedef std::map<std::string, filestate*> filesystem;
  port::mutex mutex_;
  filesystem file_map_;  // protected by mutex_.
};

}  // namespace

env* newmemenv(env* base_env) {
  return new inmemoryenv(base_env);
}

}  // namespace leveldb
