// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_port_port_h_
#define storage_leveldb_port_port_h_

#include <string.h>

// include the appropriate platform specific file below.  if you are
// porting to a new platform, see "port_example.h" for documentation
// of what the new port_<platform>.h file must provide.
#if defined(leveldb_platform_posix)
#  include "port/port_posix.h"
#elif defined(leveldb_platform_chromium)
#  include "port/port_chromium.h"
#elif defined(leveldb_platform_windows)
#  include "port/port_win.h"
#endif

#endif  // storage_leveldb_port_port_h_
