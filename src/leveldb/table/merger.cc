// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/merger.h"

#include "leveldb/comparator.h"
#include "leveldb/iterator.h"
#include "table/iterator_wrapper.h"

namespace leveldb {

namespace {
class mergingiterator : public iterator {
 public:
  mergingiterator(const comparator* comparator, iterator** children, int n)
      : comparator_(comparator),
        children_(new iteratorwrapper[n]),
        n_(n),
        current_(null),
        direction_(kforward) {
    for (int i = 0; i < n; i++) {
      children_[i].set(children[i]);
    }
  }

  virtual ~mergingiterator() {
    delete[] children_;
  }

  virtual bool valid() const {
    return (current_ != null);
  }

  virtual void seektofirst() {
    for (int i = 0; i < n_; i++) {
      children_[i].seektofirst();
    }
    findsmallest();
    direction_ = kforward;
  }

  virtual void seektolast() {
    for (int i = 0; i < n_; i++) {
      children_[i].seektolast();
    }
    findlargest();
    direction_ = kreverse;
  }

  virtual void seek(const slice& target) {
    for (int i = 0; i < n_; i++) {
      children_[i].seek(target);
    }
    findsmallest();
    direction_ = kforward;
  }

  virtual void next() {
    assert(valid());

    // ensure that all children are positioned after key().
    // if we are moving in the forward direction, it is already
    // true for all of the non-current_ children since current_ is
    // the smallest child and key() == current_->key().  otherwise,
    // we explicitly position the non-current_ children.
    if (direction_ != kforward) {
      for (int i = 0; i < n_; i++) {
        iteratorwrapper* child = &children_[i];
        if (child != current_) {
          child->seek(key());
          if (child->valid() &&
              comparator_->compare(key(), child->key()) == 0) {
            child->next();
          }
        }
      }
      direction_ = kforward;
    }

    current_->next();
    findsmallest();
  }

  virtual void prev() {
    assert(valid());

    // ensure that all children are positioned before key().
    // if we are moving in the reverse direction, it is already
    // true for all of the non-current_ children since current_ is
    // the largest child and key() == current_->key().  otherwise,
    // we explicitly position the non-current_ children.
    if (direction_ != kreverse) {
      for (int i = 0; i < n_; i++) {
        iteratorwrapper* child = &children_[i];
        if (child != current_) {
          child->seek(key());
          if (child->valid()) {
            // child is at first entry >= key().  step back one to be < key()
            child->prev();
          } else {
            // child has no entries >= key().  position at last entry.
            child->seektolast();
          }
        }
      }
      direction_ = kreverse;
    }

    current_->prev();
    findlargest();
  }

  virtual slice key() const {
    assert(valid());
    return current_->key();
  }

  virtual slice value() const {
    assert(valid());
    return current_->value();
  }

  virtual status status() const {
    status status;
    for (int i = 0; i < n_; i++) {
      status = children_[i].status();
      if (!status.ok()) {
        break;
      }
    }
    return status;
  }

 private:
  void findsmallest();
  void findlargest();

  // we might want to use a heap in case there are lots of children.
  // for now we use a simple array since we expect a very small number
  // of children in leveldb.
  const comparator* comparator_;
  iteratorwrapper* children_;
  int n_;
  iteratorwrapper* current_;

  // which direction is the iterator moving?
  enum direction {
    kforward,
    kreverse
  };
  direction direction_;
};

void mergingiterator::findsmallest() {
  iteratorwrapper* smallest = null;
  for (int i = 0; i < n_; i++) {
    iteratorwrapper* child = &children_[i];
    if (child->valid()) {
      if (smallest == null) {
        smallest = child;
      } else if (comparator_->compare(child->key(), smallest->key()) < 0) {
        smallest = child;
      }
    }
  }
  current_ = smallest;
}

void mergingiterator::findlargest() {
  iteratorwrapper* largest = null;
  for (int i = n_-1; i >= 0; i--) {
    iteratorwrapper* child = &children_[i];
    if (child->valid()) {
      if (largest == null) {
        largest = child;
      } else if (comparator_->compare(child->key(), largest->key()) > 0) {
        largest = child;
      }
    }
  }
  current_ = largest;
}
}  // namespace

iterator* newmergingiterator(const comparator* cmp, iterator** list, int n) {
  assert(n >= 0);
  if (n == 0) {
    return newemptyiterator();
  } else if (n == 1) {
    return list[0];
  } else {
    return new mergingiterator(cmp, list, n);
  }
}

}  // namespace leveldb
