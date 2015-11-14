linux_cflags=-pipe
linux_cxxflags=$(linux_cflags)

linux_release_cflags=-o2
linux_release_cxxflags=$(linux_release_cflags)

linux_debug_cflags=-o1
linux_debug_cxxflags=$(linux_debug_cflags)

linux_debug_cppflags=-d_glibcxx_debug -d_glibcxx_debug_pedantic

ifeq (86,$(findstring 86,$(build_arch)))
i686_linux_cc=gcc -m32
i686_linux_cxx=g++ -m32
i686_linux_ar=ar
i686_linux_ranlib=ranlib
i686_linux_nm=nm
i686_linux_strip=strip

x86_64_linux_cc=gcc -m64
x86_64_linux_cxx=g++ -m64
x86_64_linux_ar=ar
x86_64_linux_ranlib=ranlib
x86_64_linux_nm=nm
x86_64_linux_strip=strip
else
i686_linux_cc=$(default_host_cc) -m32
i686_linux_cxx=$(default_host_cxx) -m32
x86_64_linux_cc=$(default_host_cc) -m64
x86_64_linux_cxx=$(default_host_cxx) -m64
endif
