**leveldb is a fast key-value storage library written at google that provides an ordered mapping from string keys to string values.**

authors: sanjay ghemawat (sanjay@google.com) and jeff dean (jeff@google.com)

# features
  * keys and values are arbitrary byte arrays.
  * data is stored sorted by key.
  * callers can provide a custom comparison function to override the sort order.
  * the basic operations are `put(key,value)`, `get(key)`, `delete(key)`.
  * multiple changes can be made in one atomic batch.
  * users can create a transient snapshot to get a consistent view of data.
  * forward and backward iteration is supported over the data.
  * data is automatically compressed using the [snappy compression library](http://code.google.com/p/snappy).
  * external activity (file system operations etc.) is relayed through a virtual interface so users can customize the operating system interactions.
  * [detailed documentation](http://htmlpreview.github.io/?https://github.com/google/leveldb/blob/master/doc/index.html) about how to use the library is included with the source code.


# limitations
  * this is not a sql database.  it does not have a relational data model, it does not support sql queries, and it has no support for indexes.
  * only a single process (possibly multi-threaded) can access a particular database at a time.
  * there is no client-server support builtin to the library.  an application that needs such support will have to wrap their own server around the library.

# performance

here is a performance report (with explanations) from the run of the
included db_bench program.  the results are somewhat noisy, but should
be enough to get a ballpark performance estimate.

## setup

we use a database with a million entries.  each entry has a 16 byte
key, and a 100 byte value.  values used by the benchmark compress to
about half their original size.

    leveldb:    version 1.1
    date:       sun may  1 12:11:26 2011
    cpu:        4 x intel(r) core(tm)2 quad cpu    q6600  @ 2.40ghz
    cpucache:   4096 kb
    keys:       16 bytes each
    values:     100 bytes each (50 bytes after compression)
    entries:    1000000
    raw size:   110.6 mb (estimated)
    file size:  62.9 mb (estimated)

## write performance

the "fill" benchmarks create a brand new database, in either
sequential, or random order.  the "fillsync" benchmark flushes data
from the operating system to the disk after every operation; the other
write operations leave the data sitting in the operating system buffer
cache for a while.  the "overwrite" benchmark does random writes that
update existing keys in the database.

    fillseq      :       1.765 micros/op;   62.7 mb/s
    fillsync     :     268.409 micros/op;    0.4 mb/s (10000 ops)
    fillrandom   :       2.460 micros/op;   45.0 mb/s
    overwrite    :       2.380 micros/op;   46.5 mb/s

each "op" above corresponds to a write of a single key/value pair.
i.e., a random write benchmark goes at approximately 400,000 writes per second.

each "fillsync" operation costs much less (0.3 millisecond)
than a disk seek (typically 10 milliseconds).  we suspect that this is
because the hard disk itself is buffering the update in its memory and
responding before the data has been written to the platter.  this may
or may not be safe based on whether or not the hard disk has enough
power to save its memory in the event of a power failure.

## read performance

we list the performance of reading sequentially in both the forward
and reverse direction, and also the performance of a random lookup.
note that the database created by the benchmark is quite small.
therefore the report characterizes the performance of leveldb when the
working set fits in memory.  the cost of reading a piece of data that
is not present in the operating system buffer cache will be dominated
by the one or two disk seeks needed to fetch the data from disk.
write performance will be mostly unaffected by whether or not the
working set fits in memory.

    readrandom   :      16.677 micros/op;  (approximately 60,000 reads per second)
    readseq      :       0.476 micros/op;  232.3 mb/s
    readreverse  :       0.724 micros/op;  152.9 mb/s

leveldb compacts its underlying storage data in the background to
improve read performance.  the results listed above were done
immediately after a lot of random writes.  the results after
compactions (which are usually triggered automatically) are better.

    readrandom   :      11.602 micros/op;  (approximately 85,000 reads per second)
    readseq      :       0.423 micros/op;  261.8 mb/s
    readreverse  :       0.663 micros/op;  166.9 mb/s

some of the high cost of reads comes from repeated decompression of blocks
read from disk.  if we supply enough cache to the leveldb so it can hold the
uncompressed blocks in memory, the read performance improves again:

    readrandom   :       9.775 micros/op;  (approximately 100,000 reads per second before compaction)
    readrandom   :       5.215 micros/op;  (approximately 190,000 reads per second after compaction)

## repository contents

see doc/index.html for more explanation. see doc/impl.html for a brief overview of the implementation.

the public interface is in include/*.h.  callers should not include or
rely on the details of any other header files in this package.  those
internal apis may be changed without warning.

guide to header files:

* **include/db.h**: main interface to the db: start here

* **include/options.h**: control over the behavior of an entire database,
and also control over the behavior of individual reads and writes.

* **include/comparator.h**: abstraction for user-specified comparison function. 
if you want just bytewise comparison of keys, you can use the default
comparator, but clients can write their own comparator implementations if they
want custom ordering (e.g. to handle different character encodings, etc.)

* **include/iterator.h**: interface for iterating over data. you can get
an iterator from a db object.

* **include/write_batch.h**: interface for atomically applying multiple
updates to a database.

* **include/slice.h**: a simple module for maintaining a pointer and a
length into some other byte array.

* **include/status.h**: status is returned from many of the public interfaces
and is used to report success and various kinds of errors.

* **include/env.h**: 
abstraction of the os environment.  a posix implementation of this interface is
in util/env_posix.cc

* **include/table.h, include/table_builder.h**: lower-level modules that most
clients probably won't use directly
