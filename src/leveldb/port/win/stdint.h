// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

// msvc didn't ship with this file until the 2010 version.

#ifndef storage_leveldb_port_win_stdint_h_
#define storage_leveldb_port_win_stdint_h_

#if !defined(_msc_ver)
#error this file should only be included when compiling with msvc.
#endif

// define c99 equivalent types.
typedef signed char           int8_t;
typedef signed short          int16_t;
typedef signed int            int32_t;
typedef signed long long      int64_t;
typedef unsigned char         uint8_t;
typedef unsigned short        uint16_t;
typedef unsigned int          uint32_t;
typedef unsigned long long    uint64_t;

#endif  // storage_leveldb_port_win_stdint_h_
