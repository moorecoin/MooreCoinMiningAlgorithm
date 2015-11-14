osx_min_version=10.7
osx_sdk_version=10.9
osx_sdk=$(sdk_path)/macosx$(osx_sdk_version).sdk
ld64_version=241.9
darwin_cc=clang -target $(host) -mmacosx-version-min=$(osx_min_version) --sysroot $(osx_sdk) -mlinker-version=$(ld64_version)
darwin_cxx=clang++ -target $(host) -mmacosx-version-min=$(osx_min_version) --sysroot $(osx_sdk) -mlinker-version=$(ld64_version)

darwin_cflags=-pipe
darwin_cxxflags=$(darwin_cflags)

darwin_release_cflags=-o2
darwin_release_cxxflags=$(darwin_release_cflags)

darwin_debug_cflags=-o1
darwin_debug_cxxflags=$(darwin_debug_cflags)

darwin_native_toolchain=native_cctools
