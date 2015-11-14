build_darwin_cc: = $(shell xcrun -f clang)
build_darwin_cxx: = $(shell xcrun -f clang++)
build_darwin_ar: = $(shell xcrun -f ar)
build_darwin_ranlib: = $(shell xcrun -f ranlib)
build_darwin_strip: = $(shell xcrun -f strip)
build_darwin_otool: = $(shell xcrun -f otool)
build_darwin_nm: = $(shell xcrun -f nm)
build_darwin_install_name_tool:=$(shell xcrun -f install_name_tool)
build_darwin_sha256sum = shasum -a 256
build_darwin_download = curl --connect-timeout $(download_connect_timeout) --retry $(download_retries) -l -o

#darwin host on darwin builder. overrides darwin host preferences.
darwin_cc=$(shell xcrun -f clang) -mmacosx-version-min=$(osx_min_version)
darwin_cxx:=$(shell xcrun -f clang++) -mmacosx-version-min=$(osx_min_version)
darwin_ar:=$(shell xcrun -f ar)
darwin_ranlib:=$(shell xcrun -f ranlib)
darwin_strip:=$(shell xcrun -f strip)
darwin_libtool:=$(shell xcrun -f libtool)
darwin_otool:=$(shell xcrun -f otool)
darwin_nm:=$(shell xcrun -f nm)
darwin_install_name_tool:=$(shell xcrun -f install_name_tool)
darwin_native_toolchain=
