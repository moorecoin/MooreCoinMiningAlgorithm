package=qt
$(package)_version=5.2.1
$(package)_download_path=http://download.qt-project.org/official_releases/qt/5.2/$($(package)_version)/single
$(package)_file_name=$(package)-everywhere-opensource-src-$($(package)_version).tar.gz
$(package)_sha256_hash=84e924181d4ad6db00239d87250cc89868484a14841f77fb85ab1f1dbdcd7da1
$(package)_dependencies=openssl
$(package)_linux_dependencies=freetype fontconfig dbus libxcb libx11 xproto libxext
$(package)_build_subdir=qtbase
$(package)_qt_libs=corelib network widgets gui plugins testlib
$(package)_patches=mac-qmake.conf fix-xcb-include-order.patch qt5-tablet-osx.patch qt5-yosemite.patch

define $(package)_set_vars
$(package)_config_opts_release = -release
$(package)_config_opts_debug   = -debug
$(package)_config_opts += -opensource -confirm-license -no-audio-backend -no-sql-tds -no-glib -no-icu
$(package)_config_opts += -no-cups -no-iconv -no-gif -no-audio-backend -no-freetype
$(package)_config_opts += -no-sql-sqlite -no-nis -no-cups -no-iconv -no-pch
$(package)_config_opts += -no-gif -no-feature-style-plastique
$(package)_config_opts += -no-qml-debug -no-pch -no-nis -nomake examples -nomake tests
$(package)_config_opts += -no-feature-style-cde -no-feature-style-s60 -no-feature-style-motif
$(package)_config_opts += -no-feature-style-windowsmobile -no-feature-style-windowsce
$(package)_config_opts += -no-feature-style-cleanlooks
$(package)_config_opts += -no-sql-db2 -no-sql-ibase -no-sql-oci -no-sql-tds -no-sql-mysql
$(package)_config_opts += -no-sql-odbc -no-sql-psql -no-sql-sqlite -no-sql-sqlite2
$(package)_config_opts += -skip qtsvg -skip qtwebkit -skip qtwebkit-examples -skip qtserialport
$(package)_config_opts += -skip qtdeclarative -skip qtmultimedia -skip qtimageformats -skip qtx11extras
$(package)_config_opts += -skip qtlocation -skip qtsensors -skip qtquick1 -skip qtxmlpatterns
$(package)_config_opts += -skip qtquickcontrols -skip qtactiveqt -skip qtconnectivity -skip qtmacextras
$(package)_config_opts += -skip qtwinextras -skip qtxmlpatterns -skip qtscript -skip qtdoc

$(package)_config_opts += -prefix $(host_prefix) -bindir $(build_prefix)/bin
$(package)_config_opts += -no-c++11 -openssl-linked  -v -static -silent -pkg-config
$(package)_config_opts += -qt-libpng -qt-libjpeg -qt-zlib -qt-pcre

ifneq ($(build_os),darwin)
$(package)_config_opts_darwin = -xplatform macx-clang-linux -device-option mac_sdk_path=$(osx_sdk) -device-option cross_compile="$(host)-"
$(package)_config_opts_darwin += -device-option mac_min_version=$(osx_min_version) -device-option mac_target=$(host) -device-option mac_ld64_version=$(ld64_version)
endif

$(package)_config_opts_linux  = -qt-xkbcommon -qt-xcb  -no-eglfs -no-linuxfb -system-freetype -no-sm -fontconfig -no-xinput2 -no-libudev -no-egl -no-opengl
$(package)_config_opts_arm_linux  = -platform linux-g++ -xplatform $(host)
$(package)_config_opts_i686_linux  = -xplatform linux-g++-32
$(package)_config_opts_mingw32  = -no-opengl -xplatform win32-g++ -device-option cross_compile="$(host)-"
$(package)_build_env  = qt_rcc_test=1
endef

define $(package)_preprocess_cmds
  sed -i.old "s|updateqm.commands = \$$$$\$$$$lrelease|updateqm.commands = $($(package)_extract_dir)/qttools/bin/lrelease|" qttranslations/translations/translations.pro && \
  sed -i.old "s/src_plugins.depends = src_sql src_xml src_network/src_plugins.depends = src_xml src_network/" qtbase/src/src.pro && \
  sed -i.old "/xiproto.h/d" qtbase/src/plugins/platforms/xcb/qxcbxsettings.cpp && \
  sed -i.old 's/if \[ "$$$$xplatform_mac" = "yes" \]; then xspecvals=$$$$(macsdkify/if \[ "$$$$build_on_mac" = "yes" \]; then xspecvals=$$$$(macsdkify/' qtbase/configure && \
  mkdir -p qtbase/mkspecs/macx-clang-linux &&\
  cp -f qtbase/mkspecs/macx-clang/info.plist.lib qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f qtbase/mkspecs/macx-clang/info.plist.app qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f qtbase/mkspecs/macx-clang/qplatformdefs.h qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f $($(package)_patch_dir)/mac-qmake.conf qtbase/mkspecs/macx-clang-linux/qmake.conf && \
  patch -p1 < $($(package)_patch_dir)/fix-xcb-include-order.patch && \
  patch -p1 < $($(package)_patch_dir)/qt5-tablet-osx.patch && \
  patch -d qtbase -p1 < $($(package)_patch_dir)/qt5-yosemite.patch && \
  echo "qmake_cflags     += $($(package)_cflags) $($(package)_cppflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  echo "qmake_cxxflags   += $($(package)_cxxflags) $($(package)_cppflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  echo "qmake_lflags     += $($(package)_ldflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  sed -i.old "s|qmake_cflags            = |qmake_cflags            = $($(package)_cflags) $($(package)_cppflags) |" qtbase/mkspecs/win32-g++/qmake.conf && \
  sed -i.old "s|qmake_lflags            = |qmake_lflags            = $($(package)_ldflags) |" qtbase/mkspecs/win32-g++/qmake.conf && \
  sed -i.old "s|qmake_cxxflags          = |qmake_cxxflags            = $($(package)_cxxflags) $($(package)_cppflags) |" qtbase/mkspecs/win32-g++/qmake.conf
endef

define $(package)_config_cmds
  export pkg_config_sysroot_dir=/ && \
  export pkg_config_libdir=$(host_prefix)/lib/pkgconfig && \
  export pkg_config_path=$(host_prefix)/share/pkgconfig  && \
  export cpath=$(host_prefix)/include && \
  ./configure $($(package)_config_opts) && \
  $(make) sub-src-clean && \
  cd ../qttranslations && ../qtbase/bin/qmake qttranslations.pro -o makefile && \
  cd translations && ../../qtbase/bin/qmake translations.pro -o makefile && cd ../.. &&\
  cd qttools/src/linguist/lrelease/ && ../../../../qtbase/bin/qmake lrelease.pro -o makefile
endef

define $(package)_build_cmds
  export cpath=$(host_prefix)/include && \
  $(make) -c src $(addprefix sub-,$($(package)_qt_libs)) && \
  $(make) -c ../qttools/src/linguist/lrelease && \
  $(make) -c ../qttranslations
endef

define $(package)_stage_cmds
  $(make) -c src install_root=$($(package)_staging_dir) $(addsuffix -install_subtargets,$(addprefix sub-,$($(package)_qt_libs))) && cd .. &&\
  $(make) -c qttools/src/linguist/lrelease install_root=$($(package)_staging_dir) install_target && \
  $(make) -c qttranslations install_root=$($(package)_staging_dir) install_subtargets && \
  if `test -f qtbase/src/plugins/platforms/xcb/xcb-static/libxcb-static.a`; then \
    cp qtbase/src/plugins/platforms/xcb/xcb-static/libxcb-static.a $($(package)_staging_prefix_dir)/lib; \
  fi
endef

define $(package)_postprocess_cmds
  rm -rf mkspecs/ lib/cmake/ && \
  rm lib/libqt5bootstrap.a lib/lib*.la lib/*.prl plugins/*/*.prl
endef
