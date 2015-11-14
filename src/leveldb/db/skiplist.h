#ifndef storage_leveldb_db_skiplist_h_
#define storage_leveldb_db_skiplist_h_

// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// thread safety
// -------------
//
// writes require external synchronization, most likely a mutex.
// reads require a guarantee that the skiplist will not be destroyed
// while the read is in progress.  apart from that, reads progress
// without any internal locking or synchronization.
//
// invariants:
//
// (1) allocated nodes are never deleted until the skiplist is
// destroyed.  this is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) the contents of a node except for the next/prev pointers are
// immutable after the node has been linked into the skiplist.
// only insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...

#include <assert.h>
#include <stdlib.h>
#include "port/port.h"
#include "util/arena.h"
#include "util/random.h"

namespace leveldb {

class arena;

template<typename key, class comparator>
class skiplist {
 private:
  struct node;

 public:
  // create a new skiplist object that will use "cmp" for comparing keys,
  // and will allocate memory using "*arena".  objects allocated in the arena
  // must remain allocated for the lifetime of the skiplist object.
  explicit skiplist(comparator cmp, arena* arena);

  // insert key into the list.
  // requires: nothing that compares equal to key is currently in the list.
  void insert(const key& key);

  // returns true iff an entry that compares equal to key is in the list.
  bool contains(const key& key) const;

  // iteration over the contents of a skip list
  class iterator {
   public:
    // initialize an iterator over the specified list.
    // the returned iterator is not valid.
    explicit iterator(const skiplist* list);

    // returns true iff the iterator is positioned at a valid node.
    bool valid() const;

    // returns the key at the current position.
    // requires: valid()
    const key& key() const;

    // advances to the next position.
    // requires: valid()
    void next();

    // advances to the previous position.
    // requires: valid()
    void prev();

    // advance to the first entry with a key >= target
    void seek(const key& target);

    // position at the first entry in list.
    // final state of iterator is valid() iff list is not empty.
    void seektofirst();

    // position at the last entry in list.
    // final state of iterator is valid() iff list is not empty.
    void seektolast();

   private:
    const skiplist* list_;
    node* node_;
    // intentionally copyable
  };

 private:
  enum { kmaxheight = 12 };

  // immutable after construction
  comparator const compare_;
  arena* const arena_;    // arena used for allocations of nodes

  node* const head_;

  // modified only by insert().  read racily by readers, but stale
  // values are ok.
  port::atomicpointer max_height_;   // height of the entire list

  inline int getmaxheight() const {
    return static_cast<int>(
        reinterpret_cast<intptr_t>(max_height_.nobarrier_load()));
  }

  // read/written only by insert().
  random rnd_;

  node* newnode(const key& key, int height);
  int randomheight();
  bool equal(const key& a, const key& b) const { return (compare_(a, b) == 0); }

  // return true if key is greater than the data stored in "n"
  bool keyisafternode(const key& key, node* n) const;

  // return the earliest node that comes at or after key.
  // return null if there is no such node.
  //
  // if prev is non-null, fills prev[level] with pointer to previous
  // node at "level" for every level in [0..max_height_-1].
  node* findgreaterorequal(const key& key, node** prev) const;

  // return the latest node with a key < key.
  // return head_ if there is no such node.
  node* findlessthan(const key& key) const;

  // return the last node in the list.
  // return head_ if list is empty.
  node* findlast() const;

  // no copying allowed
  skiplist(const skiplist&);
  void operator=(const skiplist&);
};

// implementation details follow
template<typename key, class comparator>
struct skiplist<key,comparator>::node {
  explicit node(const key& k) : key(k) { }

  key const key;

  // accessors/mutators for links.  wrapped in methods so we can
  // add the appropriate barriers as necessary.
  node* next(int n) {
    assert(n >= 0);
    // use an 'acquire load' so that we observe a fully initialized
    // version of the returned node.
    return reinterpret_cast<node*>(next_[n].acquire_load());
  }
  void setnext(int n, node* x) {
    assert(n >= 0);
    // use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    next_[n].release_store(x);
  }

  // no-barrier variants that can be safely used in a few locations.
  node* nobarrier_next(int n) {
    assert(n >= 0);
    return reinterpret_cast<node*>(next_[n].nobarrier_load());
  }
  void nobarrier_setnext(int n, node* x) {
    assert(n >= 0);
    next_[n].nobarrier_store(x);
  }

 private:
  // array of length equal to the node height.  next_[0] is lowest level link.
  port::atomicpointer next_[1];
};

template<typename key, class comparator>
typename skiplist<key,comparator>::node*
skiplist<key,comparator>::newnode(const key& key, int height) {
  char* mem = arena_->allocatealigned(
      sizeof(node) + sizeof(port::atomicpointer) * (height - 1));
  return new (mem) node(key);
}

template<typename key, class comparator>
inline skiplist<key,comparator>::iterator::iterator(const skiplist* list) {
  list_ = list;
  node_ = null;
}

template<typename key, class comparator>
inline bool skiplist<key,comparator>::iterator::valid() const {
  return node_ != null;
}

template<typename key, class comparator>
inline const key& skiplist<key,comparator>::iterator::key() const {
  assert(valid());
  return node_->key;
}

template<typename key, class comparator>
inline void skiplist<key,comparator>::iterator::next() {
  assert(valid());
  node_ = node_->next(0);
}

template<typename key, class comparator>
inline void skiplist<key,comparator>::iterator::prev() {
  // instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(valid());
  node_ = list_->findlessthan(node_->key);
  if (node_ == list_->head_) {
    node_ = null;
  }
}

template<typename key, class comparator>
inline void skiplist<key,comparator>::iterator::seek(const key& target) {
  node_ = list_->findgreaterorequal(target, null);
}

template<typename key, class comparator>
inline void skiplist<key,comparator>::iterator::seektofirst() {
  node_ = list_->head_->next(0);
}

template<typename key, class comparator>
inline void skiplist<key,comparator>::iterator::seektolast() {
  node_ = list_->findlast();
  if (node_ == list_->head_) {
    node_ = null;
  }
}

template<typename key, class comparator>
int skiplist<key,comparator>::randomheight() {
  // increase height with probability 1 in kbranching
  static const unsigned int kbranching = 4;
  int height = 1;
  while (height < kmaxheight && ((rnd_.next() % kbranching) == 0)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kmaxheight);
  return height;
}

template<typename key, class comparator>
bool skiplist<key,comparator>::keyisafternode(const key& key, node* n) const {
  // null n is considered infinite
  return (n != null) && (compare_(n->key, key) < 0);
}

template<typename key, class comparator>
typename skiplist<key,comparator>::node* skiplist<key,comparator>::findgreaterorequal(const key& key, node** prev)
    const {
  node* x = head_;
  int level = getmaxheight() - 1;
  while (true) {
    node* next = x->next(level);
    if (keyisafternode(key, next)) {
      // keep searching in this list
      x = next;
    } else {
      if (prev != null) prev[level] = x;
      if (level == 0) {
        return next;
      } else {
        // switch to next list
        level--;
      }
    }
  }
}

template<typename key, class comparator>
typename skiplist<key,comparator>::node*
skiplist<key,comparator>::findlessthan(const key& key) const {
  node* x = head_;
  int level = getmaxheight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    node* next = x->next(level);
    if (next == null || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;
      } else {
        // switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename key, class comparator>
typename skiplist<key,comparator>::node* skiplist<key,comparator>::findlast()
    const {
  node* x = head_;
  int level = getmaxheight() - 1;
  while (true) {
    node* next = x->next(level);
    if (next == null) {
      if (level == 0) {
        return x;
      } else {
        // switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename key, class comparator>
skiplist<key,comparator>::skiplist(comparator cmp, arena* arena)
    : compare_(cmp),
      arena_(arena),
      head_(newnode(0 /* any key will do */, kmaxheight)),
      max_height_(reinterpret_cast<void*>(1)),
      rnd_(0xdeadbeef) {
  for (int i = 0; i < kmaxheight; i++) {
    head_->setnext(i, null);
  }
}

template<typename key, class comparator>
void skiplist<key,comparator>::insert(const key& key) {
  // todo(opt): we can use a barrier-free variant of findgreaterorequal()
  // here since insert() is externally synchronized.
  node* prev[kmaxheight];
  node* x = findgreaterorequal(key, prev);

  // our data structure does not allow duplicate insertion
  assert(x == null || !equal(key, x->key));

  int height = randomheight();
  if (height > getmaxheight()) {
    for (int i = getmaxheight(); i < height; i++) {
      prev[i] = head_;
    }
    //fprintf(stderr, "change height from %d to %d\n", max_height_, height);

    // it is ok to mutate max_height_ without any synchronization
    // with concurrent readers.  a concurrent reader that observes
    // the new value of max_height_ will see either the old value of
    // new level pointers from head_ (null), or a new value set in
    // the loop below.  in the former case the reader will
    // immediately drop to the next level since null sorts after all
    // keys.  in the latter case the reader will use the new node.
    max_height_.nobarrier_store(reinterpret_cast<void*>(height));
  }

  x = newnode(key, height);
  for (int i = 0; i < height; i++) {
    // nobarrier_setnext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->nobarrier_setnext(i, prev[i]->nobarrier_next(i));
    prev[i]->setnext(i, x);
  }
}

template<typename key, class comparator>
bool skiplist<key,comparator>::contains(const key& key) const {
  node* x = findgreaterorequal(key, null);
  if (x != null && equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}

}  // namespace leveldb

#endif  // storage_leveldb_db_skiplist_h_
