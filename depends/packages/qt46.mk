package=qt46
$(package)_version=4.6.4
$(package)_download_path=http://download.qt-project.org/archive/qt/4.6/
$(package)_file_name=qt-everywhere-opensource-src-$($(package)_version).tar.gz
$(package)_sha256_hash=9ad4d46c721b53a429ed5a2eecfd3c239a9ab566562f183f99d3125f1a234250
$(package)_dependencies=openssl freetype dbus libx11 xproto libxext libice libsm
$(package)_patches=stlfix.patch 

define $(package)_set_vars
$(package)_config_opts  = -prefix $(host_prefix) -headerdir $(host_prefix)/include/qt4 -bindir $(build_prefix)/bin
$(package)_config_opts += -release -no-separate-debug-info -opensource -confirm-license
$(package)_config_opts += -stl -qt-zlib

$(package)_config_opts += -nomake examples -nomake tests -nomake tools -nomake translations -nomake demos -nomake docs
$(package)_config_opts += -no-audio-backend -no-glib -no-nis -no-cups -no-iconv -no-gif -no-pch
$(package)_config_opts += -no-xkb -no-xrender -no-xrandr -no-xfixes -no-xcursor -no-xinerama -no-xsync -no-xinput -no-mitshm -no-xshape
$(package)_config_opts += -no-libtiff -no-fontconfig -openssl-linked
$(package)_config_opts += -no-sql-db2 -no-sql-ibase -no-sql-oci -no-sql-tds -no-sql-mysql
$(package)_config_opts += -no-sql-odbc -no-sql-psql -no-sql-sqlite -no-sql-sqlite2
$(package)_config_opts += -no-xmlpatterns -no-multimedia -no-phonon -no-scripttools -no-declarative
$(package)_config_opts += -no-phonon-backend -no-webkit -no-javascript-jit -no-script
$(package)_config_opts += -no-svg -no-libjpeg -no-libtiff -no-libpng -no-libmng -no-qt3support -no-opengl

$(package)_config_opts_x86_64_linux  += -platform linux-g++-64
$(package)_config_opts_i686_linux  = -platform linux-g++-32
$(package)_build_env  = qt_rcc_test=1
endef

define $(package)_preprocess_cmds
   sed -i.old "s|/include /usr/include||" config.tests/unix/freetype/freetype.pri && \
   sed -i.old "s|src_plugins.depends = src_gui src_sql src_svg|src_plugins.depends = src_gui src_sql|" src/src.pro && \
   sed -i.old "s|\.lower(|\.tolower(|g" src/network/ssl/qsslsocket_openssl.cpp && \
   sed -i.old "s|key_backspace|key_backspace|" src/gui/itemviews/qabstractitemview.cpp && \
   sed -i.old "s|/usr/x11r6/lib64|$(host_prefix)/lib|" mkspecs/*/*.conf && \
   sed -i.old "s|/usr/x11r6/lib|$(host_prefix)/lib|" mkspecs/*/*.conf && \
   sed -i.old "s|/usr/x11r6/include|$(host_prefix)/include|" mkspecs/*/*.conf && \
   sed -i.old "s|qmake_lflags_shlib\t+= -shared|qmake_lflags_shlib\t+= -shared -wl,--exclude-libs,all|" mkspecs/common/g++.conf && \
   sed -i.old "/sslv2_client_method/d" src/network/ssl/qsslsocket_openssl.cpp src/network/ssl/qsslsocket_openssl_symbols.cpp && \
   sed -i.old "/sslv2_server_method/d" src/network/ssl/qsslsocket_openssl.cpp src/network/ssl/qsslsocket_openssl_symbols.cpp && \
   patch -p1 < $($(package)_patch_dir)/stlfix.patch 
endef

define $(package)_config_cmds
  export pkg_config_sysroot_dir=/ && \
  export pkg_config_libdir=$(host_prefix)/lib/pkgconfig && \
  export pkg_config_path=$(host_prefix)/share/pkgconfig  && \
  export cpath=$(host_prefix)/include && \
  openssl_libs='-l$(host_prefix)/lib -lssl -lcrypto' ./configure $($(package)_config_opts) && \
  cd tools/linguist/lrelease; ../../../bin/qmake  -o makefile lrelease.pro
endef

define $(package)_build_cmds
  export cpath=$(host_prefix)/include && \
  $(make) -c src && \
  $(make) -c tools/linguist/lrelease
endef

define $(package)_stage_cmds
  $(make) -c src install_root=$($(package)_staging_dir) install && \
  $(make) -c tools/linguist/lrelease install_root=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf mkspecs/ lib/cmake/ lib/*.prl lib/*.la && \
  find native/bin -type f -exec mv {} {}-qt4 \;
endef
