mingw32_cflags=-pipe
mingw32_cxxflags=$(mingw32_cflags)

mingw32_release_cflags=-o2
mingw32_release_cxxflags=$(mingw32_release_cflags)

mingw32_debug_cflags=-o1
mingw32_debug_cxxflags=$(mingw32_debug_cflags)

mingw32_debug_cppflags=-d_glibcxx_debug -d_glibcxx_debug_pedantic
