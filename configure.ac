dnl require autoconf 2.60 (as_echo/as_echo_n)
ac_prereq([2.60])
define(_client_version_major, 0)
define(_client_version_minor, 11)
define(_client_version_revision, 99)
define(_client_version_build, 0)
define(_client_version_is_release, false)
define(_copyright_year, 2015)
ac_init([bitcoin core],[_client_version_major._client_version_minor._client_version_revision],[https://github.com/bitcoin/bitcoin/issues],[bitcoin])
ac_config_srcdir([src/main.cpp])
ac_config_headers([src/config/bitcoin-config.h])
ac_config_aux_dir([build-aux])
ac_config_macro_dir([build-aux/m4])

ac_canonical_host

ah_top([#ifndef bitcoin_config_h])
ah_top([#define bitcoin_config_h])
ah_bottom([#endif //bitcoin_config_h])

dnl faketime breaks configure and is only needed for make. disable it here.
unset faketime

dnl automake init set-up and checks
am_init_automake([no-define subdir-objects foreign])

dnl faketime messes with timestamps and causes configure to be re-run.
dnl --disable-maintainer-mode can be used to bypass this.
am_maintainer_mode([enable])

dnl make the compilation flags quiet unless v=1 is used
m4_ifdef([am_silent_rules], [am_silent_rules([yes])])

dnl compiler checks (here before libtool).
if test "x${cxxflags+set}" = "xset"; then
  cxxflags_overridden=yes
else
  cxxflags_overridden=no
fi
ac_prog_cxx
m4_ifdef([ac_prog_objcxx],[ac_prog_objcxx])

dnl by default, libtool for mingw refuses to link static libs into a dll for
dnl fear of mixing pic/non-pic objects, and import/export complications. since
dnl we have those under control, re-enable that functionality.
case $host in
  *mingw*)
     lt_cv_deplibs_check_method="pass_all"
  ;;
esac
dnl libtool init checks.
lt_init([pic-only])

dnl check/return path for base programs.
ac_path_tool(ar, ar)
ac_path_tool(ranlib, ranlib)
ac_path_tool(strip, strip)
ac_path_tool(gcov, gcov)
ac_path_prog(lcov, lcov)
ac_path_prog(java, java)
ac_path_prog(genhtml, genhtml)
ac_path_prog([git], [git])
ac_path_prog(ccache,ccache)
ac_path_prog(xgettext,xgettext)
ac_path_prog(hexdump,hexdump)

dnl pkg-config check.
pkg_prog_pkg_config

# enable wallet
ac_arg_enable([wallet],
  [as_help_string([--enable-wallet],
  [enable wallet (default is yes)])],
  [enable_wallet=$enableval],
  [enable_wallet=yes])

ac_arg_with([miniupnpc],
  [as_help_string([--with-miniupnpc],
  [enable upnp (default is yes if libminiupnpc is found)])],
  [use_upnp=$withval],
  [use_upnp=auto])

ac_arg_enable([upnp-default],
  [as_help_string([--enable-upnp-default],
  [if upnp is enabled, turn it on at startup (default is no)])],
  [use_upnp_default=$enableval],
  [use_upnp_default=no])

ac_arg_enable(tests,
    as_help_string([--enable-tests],[compile tests (default is yes)]),
    [use_tests=$enableval],
    [use_tests=yes])

ac_arg_with([comparison-tool],
    as_help_string([--with-comparison-tool],[path to java comparison tool (requires --enable-tests)]),
    [use_comparison_tool=$withval],
    [use_comparison_tool=no])

ac_arg_enable([comparison-tool-reorg-tests],
    as_help_string([--enable-comparison-tool-reorg-tests],[enable expensive reorg tests in the comparison tool (default no)]),
    [use_comparison_tool_reorg_tests=$enableval],
    [use_comparison_tool_reorg_tests=no])

ac_arg_with([qrencode],
  [as_help_string([--with-qrencode],
  [enable qr code support (default is yes if qt is enabled and libqrencode is found)])],
  [use_qr=$withval],
  [use_qr=auto])

ac_arg_enable([hardening],
  [as_help_string([--enable-hardening],
  [attempt to harden the resulting executables (default is yes)])],
  [use_hardening=$enableval],
  [use_hardening=yes])

ac_arg_enable([reduce-exports],
  [as_help_string([--enable-reduce-exports],
  [attempt to reduce exported symbols in the resulting executables (default is no)])],
  [use_reduce_exports=$enableval],
  [use_reduce_exports=no])

ac_arg_enable([ccache],
  [as_help_string([--enable-ccache],
  [use ccache for building (default is yes if ccache is found)])],
  [use_ccache=$enableval],
  [use_ccache=auto])

ac_arg_enable([lcov],
  [as_help_string([--enable-lcov],
  [enable lcov testing (default is no)])],
  [use_lcov=yes],
  [use_lcov=no])

ac_arg_enable([glibc-back-compat],
  [as_help_string([--enable-glibc-back-compat],
  [enable backwards compatibility with glibc])],
  [use_glibc_compat=$enableval],
  [use_glibc_compat=no])

ac_arg_with([protoc-bindir],[as_help_string([--with-protoc-bindir=bin_dir],[specify protoc bin path])], [protoc_bin_path=$withval], [])

# enable debug 
ac_arg_enable([debug],
    [as_help_string([--enable-debug],
                    [use debug compiler flags and macros (default is no)])],
    [enable_debug=$enableval],
    [enable_debug=no])

if test "x$enable_debug" = xyes; then
    if test "x$gcc" = xyes; then
        cflags="-g3 -o0 -ddebug"
    fi
    
    if test "x$gxx" = xyes; then
        cxxflags="-g3 -o0 -ddebug"
    fi
fi 

## todo: remove these hard-coded paths and flags. they are here for the sake of
##       compatibility with the legacy buildsystem.
##
if test "x$cxxflags_overridden" = "xno"; then
  cxxflags="$cxxflags -wall -wextra -wformat -wformat-security -wno-unused-parameter"
fi
cppflags="$cppflags -dhave_build_info -d__stdc_format_macros"

ac_arg_with([utils],
  [as_help_string([--with-utils],
  [build bitcoin-cli bitcoin-tx (default=yes)])],
  [build_bitcoin_utils=$withval],
  [build_bitcoin_utils=yes])

ac_arg_with([libs],
  [as_help_string([--with-libs],
  [build libraries (default=yes)])],
  [build_bitcoin_libs=$withval],
  [build_bitcoin_libs=yes])

ac_arg_with([daemon],
  [as_help_string([--with-daemon],
  [build bitcoind daemon (default=yes)])],
  [build_bitcoind=$withval],
  [build_bitcoind=yes])

ac_lang_push([c++])

use_pkgconfig=yes
case $host in
  *mingw*)

     #pkgconfig does more harm than good with mingw
     use_pkgconfig=no

     target_os=windows
     ac_check_lib([mingwthrd],      [main],, ac_msg_error(lib missing))
     ac_check_lib([kernel32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([user32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([gdi32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([comdlg32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([winspool],      [main],, ac_msg_error(lib missing))
     ac_check_lib([winmm],      [main],, ac_msg_error(lib missing))
     ac_check_lib([shell32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([comctl32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([ole32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([oleaut32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([uuid],      [main],, ac_msg_error(lib missing))
     ac_check_lib([rpcrt4],      [main],, ac_msg_error(lib missing))
     ac_check_lib([advapi32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([ws2_32],      [main],, ac_msg_error(lib missing))
     ac_check_lib([mswsock],      [main],, ac_msg_error(lib missing))
     ac_check_lib([shlwapi],      [main],, ac_msg_error(lib missing))
     ac_check_lib([iphlpapi],      [main],, ac_msg_error(lib missing))
     ac_check_lib([crypt32],      [main],, ac_msg_error(lib missing))

     # -static is interpreted by libtool, where it has a different meaning.
     # in libtool-speak, it's -all-static.
     ax_check_link_flag([[-static]],[libtool_app_ldflags="$libtool_app_ldflags -all-static"])

     ac_path_prog([makensis], [makensis], none)
     if test x$makensis = xnone; then
       ac_msg_warn("makensis not found. cannot create installer.")
     fi

     ac_path_tool(windres, windres, none)
     if test x$windres = xnone; then
       ac_msg_error("windres not found")
     fi

     cppflags="$cppflags -d_mt -dwin32 -d_windows -dboost_thread_use_lib"
     leveldb_target_flags="target_os=os_windows_crosscompile"
     if test "x$cxxflags_overridden" = "xno"; then
       cxxflags="$cxxflags -w"
     fi
     case $host in
       i?86-*) windows_bits=32 ;;
       x86_64-*) windows_bits=64 ;;
       *) ac_msg_error("could not determine win32/win64 for installer") ;;
     esac
     ac_subst(windows_bits)

     dnl libtool insists upon adding -nostdlib and a list of objects/libs to link against.
     dnl that breaks our ability to build dll's with static libgcc/libstdc++/libssp. override
     dnl its command here, with the predeps/postdeps removed, and -static inserted. postdeps are
     dnl also overridden to prevent their insertion later.
     dnl this should only affect dll's.
     archive_cmds_cxx="\$cc -shared \$libobjs \$deplibs \$compiler_flags -static -o \$output_objdir/\$soname \${wl}--enable-auto-image-base -xlinker --out-implib -xlinker \$lib"
     postdeps_cxx=

     ;;
  *darwin*)
     target_os=darwin
     leveldb_target_flags="target_os=darwin"
     if  test x$cross_compiling != xyes; then
       build_os=darwin
       ac_check_prog([port],port, port)
       if test x$port = xport; then
         dnl add default macports paths
         cppflags="$cppflags -isystem /opt/local/include"
         libs="$libs -l/opt/local/lib"
         if test -d /opt/local/include/db48; then
           cppflags="$cppflags -i/opt/local/include/db48"
           libs="$libs -l/opt/local/lib/db48"
         fi
       fi

       ac_check_prog([brew],brew, brew)
       if test x$brew = xbrew; then
         dnl these homebrew packages may be keg-only, meaning that they won't be found
         dnl in expected paths because they may conflict with system files. ask
         dnl homebrew where each one is located, then adjust paths accordingly.
         dnl it's safe to add these paths even if the functionality is disabled by
         dnl the user (--without-wallet or --without-gui for example).

         openssl_prefix=`$brew --prefix openssl 2>/dev/null`
         bdb_prefix=`$brew --prefix berkeley-db4 2>/dev/null`
         qt5_prefix=`$brew --prefix qt5 2>/dev/null`
         if test x$openssl_prefix != x; then
           pkg_config_path="$openssl_prefix/lib/pkgconfig:$pkg_config_path"
           export pkg_config_path
         fi
         if test x$bdb_prefix != x; then
           cppflags="$cppflags -i$bdb_prefix/include"
           libs="$libs -l$bdb_prefix/lib"
         fi
         if test x$qt5_prefix != x; then
           pkg_config_path="$qt5_prefix/lib/pkgconfig:$pkg_config_path"
           export pkg_config_path
         fi
       fi
     else
       case $build_os in
         *darwin*)
           build_os=darwin
           ;;
         *)
           ac_path_tool([installnametool], [install_name_tool], install_name_tool)
           ac_path_tool([otool], [otool], otool)
           ac_path_progs([genisoimage], [genisoimage mkisofs],genisoimage)

           dnl libtool will try to strip the static lib, which is a problem for
           dnl cross-builds because strip attempts to call a hard-coded ld,
           dnl which may not exist in the path. stripping the .a is not
           dnl necessary, so just disable it.
           old_striplib=
           ;;
       esac
     fi

     ax_check_link_flag([[-wl,-headerpad_max_install_names]], [ldflags="$ldflags -wl,-headerpad_max_install_names"])
     cppflags="$cppflags -dmac_osx"
     ;;
   *linux*)
     target_os=linux
     ;;
   *)
     ;;
esac

if test x$use_comparison_tool != xno; then
  ac_subst(java_comparison_tool, $use_comparison_tool)
fi

if test x$use_comparison_tool_reorg_tests != xno; then
  if test x$use_comparison_tool = x; then
    ac_msg_error("comparison tool reorg tests but comparison tool was not specified")
  fi
  ac_subst(comparison_tool_reorg_tests, 1)
else
  ac_subst(comparison_tool_reorg_tests, 0)
fi

if test x$use_lcov = xyes; then
  if test x$lcov = x; then
    ac_msg_error("lcov testing requested but lcov not found")
  fi
  if test x$gcov = x; then
    ac_msg_error("lcov testing requested but gcov not found")
  fi
  if test x$java = x; then
    ac_msg_error("lcov testing requested but java not found")
  fi
  if test x$genhtml = x; then
    ac_msg_error("lcov testing requested but genhtml not found")
  fi
  if test x$use_comparison_tool = x; then
    ac_msg_error("lcov testing requested but comparison tool was not specified")
  fi
  lcov="$lcov --gcov-tool=$gcov"
  ax_check_compile_flag([--coverage],[cxxflags="$cxxflags --coverage"],
    [ac_msg_error("lcov testing requested but --coverage flag does not work")])
fi

dnl check for endianness
ac_c_bigendian

dnl check for pthread compile/link requirements
ax_pthread

# the following macro will add the necessary defines to bitcoin-config.h, but
# they also need to be passed down to any subprojects. pull the results out of
# the cache and add them to cppflags.
ac_sys_largefile
# detect posix or gnu variant of strerror_r
ac_func_strerror_r

if test x$ac_cv_sys_file_offset_bits != x &&
   test x$ac_cv_sys_file_offset_bits != xno &&
   test x$ac_cv_sys_file_offset_bits != xunknown; then
  cppflags="$cppflags -d_file_offset_bits=$ac_cv_sys_file_offset_bits"
fi

if test x$ac_cv_sys_large_files != x &&
   test x$ac_cv_sys_large_files != xno &&
   test x$ac_cv_sys_large_files != xunknown; then
  cppflags="$cppflags -d_large_files=$ac_cv_sys_large_files"
fi

ax_check_link_flag([[-wl,--large-address-aware]], [ldflags="$ldflags -wl,--large-address-aware"])

ax_gcc_func_attribute([visibility])
ax_gcc_func_attribute([dllexport])
ax_gcc_func_attribute([dllimport])

if test x$use_glibc_compat != xno; then

  #__fdelt_chk's params and return type have changed from long unsigned int to long int.
  # see which one is present here.
  ac_msg_checking(__fdelt_chk type)
  ac_compile_ifelse([ac_lang_program([[#ifdef _fortify_source
                    #undef _fortify_source
                  #endif
                  #define _fortify_source 2
                  #include <sys/select.h>
     extern "c" long unsigned int __fdelt_warn(long unsigned int);]],[[]])],
    [ fdelt_type="long unsigned int"],
    [ fdelt_type="long int"])
  ac_msg_result($fdelt_type)
  ac_define_unquoted(fdelt_type, $fdelt_type,[parameter and return value type for __fdelt_chk])

fi

if test x$use_hardening != xno; then
  ax_check_compile_flag([-wstack-protector],[hardened_cxxflags="$hardened_cxxflags -wstack-protector"])
  ax_check_compile_flag([-fstack-protector-all],[hardened_cxxflags="$hardened_cxxflags -fstack-protector-all"])

  ax_check_preproc_flag([-d_fortify_source=2],[
    ax_check_preproc_flag([-u_fortify_source],[
      hardened_cppflags="$hardened_cppflags -u_fortify_source"
    ])
    hardened_cppflags="$hardened_cppflags -d_fortify_source=2"
  ])

  ax_check_link_flag([[-wl,--dynamicbase]], [hardened_ldflags="$hardened_ldflags -wl,--dynamicbase"])
  ax_check_link_flag([[-wl,--nxcompat]], [hardened_ldflags="$hardened_ldflags -wl,--nxcompat"])
  ax_check_link_flag([[-wl,-z,relro]], [hardened_ldflags="$hardened_ldflags -wl,-z,relro"])
  ax_check_link_flag([[-wl,-z,now]], [hardened_ldflags="$hardened_ldflags -wl,-z,now"])

  if test x$target_os != xwindows; then
    # all windows code is pic, forcing it on just adds useless compile warnings
    ax_check_compile_flag([-fpie],[hardened_cxxflags="$hardened_cxxflags -fpie"])
    ax_check_link_flag([[-pie]], [hardened_ldflags="$hardened_ldflags -pie"])
  fi

  case $host in
    *mingw*)
       ac_check_lib([ssp],      [main],, ac_msg_error(lib missing))
    ;;
  esac

  cxxflags="$cxxflags $hardened_cxxflags"
  cppflags="$cppflags $hardened_cppflags"
  ldflags="$ldflags $hardened_ldflags"
  objcxxflags="$cxxflags"
fi

dnl this flag screws up non-darwin gcc even when the check fails. special-case it.
if test x$target_os = xdarwin; then
  ax_check_link_flag([[-wl,-dead_strip]], [ldflags="$ldflags -wl,-dead_strip"])
fi

ac_check_headers([endian.h sys/endian.h byteswap.h stdio.h stdlib.h unistd.h strings.h sys/types.h sys/stat.h sys/select.h sys/prctl.h])
ac_search_libs([getaddrinfo_a], [anl], [ac_define(have_getaddrinfo_a, 1, [define this symbol if you have getaddrinfo_a])])
ac_search_libs([inet_pton], [nsl resolv], [ac_define(have_inet_pton, 1, [define this symbol if you have inet_pton])])

ac_check_decls([strnlen])

ac_check_decls([le16toh, le32toh, le64toh, htole16, htole32, htole64, be16toh, be32toh, be64toh, htobe16, htobe32, htobe64],,,
		[#if have_endian_h
                 #include <endian.h>
                 #elif have_sys_endian_h
                 #include <sys/endian.h>
                 #endif])

ac_check_decls([bswap_16, bswap_32, bswap_64],,,
		[#if have_byteswap_h
                 #include <byteswap.h>
                 #endif])

dnl check for msg_nosignal
ac_msg_checking(for msg_nosignal)
ac_compile_ifelse([ac_lang_program([[#include <sys/socket.h>]],
 [[ int f = msg_nosignal; ]])],
 [ ac_msg_result(yes); ac_define(have_msg_nosignal, 1,[define this symbol if you have msg_nosignal]) ],
 [ ac_msg_result(no)]
)

ac_search_libs([clock_gettime],[rt])

ac_msg_checking([for visibility attribute])
ac_link_ifelse([ac_lang_source([
  int foo_def( void ) __attribute__((visibility("default")));
  int main(){}
  ])],
  [
    ac_define(have_visibility_attribute,1,[define if the visibility attribute is supported.])
    ac_msg_result(yes)
  ],
  [
    ac_msg_result(no)
    if test x$use_reduce_exports = xyes; then
      ac_msg_error([cannot find a working visibility attribute. use --disable-reduce-exports.])
    fi
  ]
)

if test x$use_reduce_exports = xyes; then
  ax_check_compile_flag([-fvisibility=hidden],[re_cxxflags="-fvisibility=hidden"],
  [ac_msg_error([cannot set default symbol visibility. use --disable-reduce-exports.])])
fi

leveldb_cppflags=
libleveldb=
libmemenv=
am_conditional([embedded_leveldb],[true])
ac_subst(leveldb_cppflags)
ac_subst(libleveldb)
ac_subst(libmemenv)

if test x$enable_wallet != xno; then
    dnl check for libdb_cxx only if wallet enabled
    bitcoin_find_bdb48
fi

dnl check for libminiupnpc (optional)
if test x$use_upnp != xno; then
  ac_check_headers(
    [miniupnpc/miniwget.h miniupnpc/miniupnpc.h miniupnpc/upnpcommands.h miniupnpc/upnperrors.h],
    [ac_check_lib([miniupnpc], [main],[miniupnpc_libs=-lminiupnpc], [have_miniupnpc=no])],
    [have_miniupnpc=no]
  )
fi

bitcoin_qt_init

dnl sets $bitcoin_enable_qt, $bitcoin_enable_qt_test, $bitcoin_enable_qt_dbus
bitcoin_qt_configure([$use_pkgconfig], [qt4])

if test x$build_bitcoin_utils$build_bitcoind$bitcoin_enable_qt$use_tests = xnononono; then
    use_boost=no
else
    use_boost=yes
fi

if test x$use_boost = xyes; then

dnl check for boost libs
ax_boost_base
ax_boost_system
ax_boost_filesystem
ax_boost_program_options
ax_boost_thread
ax_boost_chrono


if test x$use_reduce_exports = xyes; then
  ac_msg_checking([for working boost reduced exports])
  temp_cppflags="$cppflags"
  cppflags="$boost_cppflags $cppflags"
  ac_preproc_ifelse([ac_lang_program([[
      @%:@include <boost/version.hpp>
    ]], [[
      #if boost_version >= 104900
      // everything is okay
      #else
      #  error boost version is too old
      #endif
    ]])],[
      ac_msg_result(yes)
    ],[
    ac_msg_error([boost versions < 1.49 are known to be broken with reduced exports. use --disable-reduce-exports.])
  ])
  cppflags="$temp_cppflags"
fi
fi

if test x$use_reduce_exports = xyes; then
    cxxflags="$cxxflags $re_cxxflags"
    ax_check_link_flag([[-wl,--exclude-libs,all]], [reldflags="-wl,--exclude-libs,all"])
fi

if test x$use_tests = xyes; then

  if test x$hexdump = x; then
    ac_msg_error(hexdump is required for tests)
  fi


  if test x$use_boost = xyes; then

  ax_boost_unit_test_framework

  dnl determine if -dboost_test_dyn_link is needed
  ac_msg_checking([for dynamic linked boost test])
  temp_libs="$libs"
  libs="$libs $boost_ldflags $boost_unit_test_framework_lib"
  temp_cppflags="$cppflags"
  cppflags="$cppflags $boost_cppflags"
  ac_link_ifelse([ac_lang_source([
       #define boost_test_dyn_link
       #define boost_test_main
        #include <boost/test/unit_test.hpp>

       ])],
    [ac_msg_result(yes)]
    [testdefs="$testdefs -dboost_test_dyn_link"],
    [ac_msg_result(no)])
  libs="$temp_libs"
  cppflags="$temp_cppflags"

  fi
fi

if test x$use_boost = xyes; then

boost_libs="$boost_ldflags $boost_system_lib $boost_filesystem_lib $boost_program_options_lib $boost_thread_lib $boost_chrono_lib"

dnl boost >= 1.50 uses sleep_for rather than the now-deprecated sleep, however
dnl it was broken from 1.50 to 1.52 when backed by nanosleep. use sleep_for if
dnl a working version is available, else fall back to sleep. sleep was removed
dnl after 1.56.
dnl if neither is available, abort.
temp_libs="$libs"
libs="$boost_libs $libs"
temp_cppflags="$cppflags"
cppflags="$cppflags $boost_cppflags"
ac_link_ifelse([ac_lang_program([[
  #include <boost/thread/thread.hpp>
  #include <boost/version.hpp>
  ]],[[
  #if boost_version >= 105000 && (!defined(boost_has_nanosleep) || boost_version >= 105200)
      boost::this_thread::sleep_for(boost::chrono::milliseconds(0));
  #else
   choke me
  #endif
  ]])],
  [boost_sleep=yes;
     ac_define(have_working_boost_sleep_for, 1, [define this symbol if boost sleep_for works])],
  [boost_sleep=no])
libs="$temp_libs"
cppflags="$temp_cppflags"

if test x$boost_sleep != xyes; then
temp_libs="$libs"
libs="$boost_libs $libs"
temp_cppflags="$cppflags"
cppflags="$cppflags $boost_cppflags"
ac_link_ifelse([ac_lang_program([[
  #include <boost/version.hpp>
  #include <boost/thread.hpp>
  #include <boost/date_time/posix_time/posix_time_types.hpp>
  ]],[[
  #if boost_version <= 105600
      boost::this_thread::sleep(boost::posix_time::milliseconds(0));
  #else
   choke me
  #endif
  ]])],
  [boost_sleep=yes; ac_define(have_working_boost_sleep, 1, [define this symbol if boost sleep works])],
  [boost_sleep=no])
libs="$temp_libs"
cppflags="$temp_cppflags"
fi

if test x$boost_sleep != xyes; then
  ac_msg_error(no working boost sleep implementation found.)
fi

fi

if test x$use_pkgconfig = xyes; then

  if test x"$pkg_config" = "x"; then
    ac_msg_error(pkg-config not found.)
  fi

  : #nop
  m4_ifdef(
    [pkg_check_modules],
    [
      pkg_check_modules([ssl], [libssl],, [ac_msg_error(openssl  not found.)])
      pkg_check_modules([crypto], [libcrypto],,[ac_msg_error(libcrypto  not found.)])
      bitcoin_qt_check([pkg_check_modules([protobuf], [protobuf], [have_protobuf=yes], [bitcoin_qt_fail(libprotobuf not found)])])
      if test x$use_qr != xno; then
        bitcoin_qt_check([pkg_check_modules([qr], [libqrencode], [have_qrencode=yes], [have_qrencode=no])])
      fi
    ]
  )
else
  ac_check_header([openssl/crypto.h],,ac_msg_error(libcrypto headers missing))
  ac_check_lib([crypto],      [main],crypto_libs=-lcrypto, ac_msg_error(libcrypto missing))

  ac_check_header([openssl/ssl.h],, ac_msg_error(libssl headers missing),)
  ac_check_lib([ssl],         [main],ssl_libs=-lssl, ac_msg_error(libssl missing))

  bitcoin_qt_check(ac_check_lib([protobuf] ,[main],[protobuf_libs=-lprotobuf], bitcoin_qt_fail(libprotobuf not found)))
  if test x$use_qr != xno; then
    bitcoin_qt_check([ac_check_lib([qrencode], [main],[qr_libs=-lqrencode], [have_qrencode=no])])
    bitcoin_qt_check([ac_check_header([qrencode.h],, have_qrencode=no)])
  fi
fi

ac_check_lib([crypto],[rand_egd],[],[
  ac_arg_with([libressl],
    [as_help_string([--with-libressl],[build with system libressl (default is no; dangerous; not supported)])],
    [ac_msg_warn([detected libressl: this is not supported, and may break consensus compatibility!])],
    [ac_msg_error([detected libressl: this is not supported, and may break consensus compatibility!])]
  )
])

cflags_temp="$cflags"
libs_temp="$libs"
cflags="$cflags $ssl_cflags $crypto_cflags"
libs="$libs $ssl_libs $crypto_libs"
ac_check_header([openssl/ec.h],, ac_msg_error(openssl ec header missing),)
cflags="$cflags_temp"
libs="$libs_temp"

bitcoin_qt_path_progs([protoc], [protoc],$protoc_bin_path)

ac_msg_checking([whether to build bitcoind])
am_conditional([build_bitcoind], [test x$build_bitcoind = xyes])
ac_msg_result($build_bitcoind)

ac_msg_checking([whether to build utils (bitcoin-cli bitcoin-tx)])
am_conditional([build_bitcoin_utils], [test x$build_bitcoin_utils = xyes])
ac_msg_result($build_bitcoin_utils)

ac_msg_checking([whether to build libraries])
am_conditional([build_bitcoin_libs], [test x$build_bitcoin_libs = xyes])
if test x$build_bitcoin_libs = xyes; then
  ac_define(have_consensus_lib, 1, [define this symbol if the consensus lib has been built])
  ac_config_files([libbitcoinconsensus.pc:libbitcoinconsensus.pc.in])
fi
ac_msg_result($build_bitcoin_libs)

ac_lang_pop

if test "x$use_ccache" != "xno"; then
  ac_msg_checking(if ccache should be used)
  if test x$ccache = x; then
    if test "x$use_ccache" = "xyes"; then
      ac_msg_error([ccache not found.]);
    else
      use_ccache=no
    fi
  else
    use_ccache=yes
    cc="$ac_cv_path_ccache $cc"
    cxx="$ac_cv_path_ccache $cxx"
  fi
  ac_msg_result($use_ccache)
fi
if test "x$use_ccache" = "xyes"; then
    ax_check_preproc_flag([-qunused-arguments],[cppflags="-qunused-arguments $cppflags"])
fi

dnl enable wallet
ac_msg_checking([if wallet should be enabled])
if test x$enable_wallet != xno; then
  ac_msg_result(yes)
  ac_define_unquoted([enable_wallet],[1],[define to 1 to enable wallet functions])

else
  ac_msg_result(no)
fi

dnl enable upnp support
ac_msg_checking([whether to build with support for upnp])
if test x$have_miniupnpc = xno; then
  if test x$use_upnp = xyes; then
     ac_msg_error("upnp requested but cannot be built. use --without-miniupnpc")
  fi
  ac_msg_result(no)
else
  if test x$use_upnp != xno; then
    ac_msg_result(yes)
    ac_msg_checking([whether to build with upnp enabled by default])
    use_upnp=yes
    upnp_setting=0
    if test x$use_upnp_default != xno; then
      use_upnp_default=yes
      upnp_setting=1
    fi
    ac_msg_result($use_upnp_default)
    ac_define_unquoted([use_upnp],[$upnp_setting],[upnp support not compiled if undefined, otherwise value (0 or 1) determines default state])
    if test x$target_os = xwindows; then
      miniupnpc_cppflags="-dstaticlib -dminiupnp_staticlib"
    fi
  else
    ac_msg_result(no)
  fi
fi

dnl these are only used when qt is enabled
if test x$bitcoin_enable_qt != xno; then
  build_qt=qt
  dnl enable dbus support
  ac_msg_checking([whether to build gui with support for d-bus])
  if test x$bitcoin_enable_qt_dbus != xno; then
    ac_define([use_dbus],[1],[define if dbus support should be compiled in])
  fi
  ac_msg_result($bitcoin_enable_qt_dbus)

  dnl enable qr support
  ac_msg_checking([whether to build gui with support for qr codes])
  if test x$have_qrencode = xno; then
    if test x$use_qr = xyes; then
     ac_msg_error("qr support requested but cannot be built. use --without-qrencode")
    fi
    ac_msg_result(no)
  else
    if test x$use_qr != xno; then
      ac_msg_result(yes)
      ac_define([use_qrcode],[1],[define if qr support should be compiled in])
      use_qr=yes
    else
      ac_msg_result(no)
    fi
  fi

  if test x$xgettext = x; then
    ac_msg_warn("xgettext is required to update qt translations")
  fi

  ac_msg_checking([whether to build test_bitcoin-qt])
  if test x$use_tests$bitcoin_enable_qt_test = xyesyes; then
    ac_msg_result([yes])
    build_test_qt="test"
  else
    ac_msg_result([no])
  fi
fi

ac_msg_checking([whether to build test_bitcoin])
if test x$use_tests = xyes; then
  ac_msg_result([yes])
  build_test="test"
else
  ac_msg_result([no])
fi

ac_msg_checking([whether to reduce exports])
if test x$use_reduce_exports = xyes; then
  ac_msg_result([yes])
else
  ac_msg_result([no])
fi

if test x$build_bitcoin_utils$build_bitcoin_libs$build_bitcoind$bitcoin_enable_qt$use_tests = xnonononono; then
  ac_msg_error([no targets! please specify at least one of: --with-utils --with-libs --with-daemon --with-gui or --enable-tests])
fi

am_conditional([target_darwin], [test x$target_os = xdarwin])
am_conditional([build_darwin], [test x$build_os = xdarwin])
am_conditional([target_windows], [test x$target_os = xwindows])
am_conditional([enable_wallet],[test x$enable_wallet = xyes])
am_conditional([enable_tests],[test x$use_tests = xyes])
am_conditional([enable_qt],[test x$bitcoin_enable_qt = xyes])
am_conditional([enable_qt_tests],[test x$use_tests$bitcoin_enable_qt_test = xyesyes])
am_conditional([use_qrcode], [test x$use_qr = xyes])
am_conditional([use_lcov],[test x$use_lcov = xyes])
am_conditional([use_comparison_tool],[test x$use_comparison_tool != xno])
am_conditional([use_comparison_tool_reorg_tests],[test x$use_comparison_tool_reorg_test != xno])
am_conditional([glibc_back_compat],[test x$use_glibc_compat = xyes])

ac_define(client_version_major, _client_version_major, [major version])
ac_define(client_version_minor, _client_version_minor, [minor version])
ac_define(client_version_revision, _client_version_revision, [build revision])
ac_define(client_version_build, _client_version_build, [version build])
ac_define(client_version_is_release, _client_version_is_release, [version is release])
ac_define(copyright_year, _copyright_year, [version is release])
ac_subst(client_version_major, _client_version_major)
ac_subst(client_version_minor, _client_version_minor)
ac_subst(client_version_revision, _client_version_revision)
ac_subst(client_version_build, _client_version_build)
ac_subst(client_version_is_release, _client_version_is_release)
ac_subst(copyright_year, _copyright_year)

ac_subst(reldflags)
ac_subst(libtool_app_ldflags)
ac_subst(use_upnp)
ac_subst(use_qrcode)
ac_subst(boost_libs)
ac_subst(testdefs)
ac_subst(leveldb_target_flags)
ac_subst(build_test)
ac_subst(build_qt)
ac_subst(build_test_qt)
ac_subst(miniupnpc_cppflags)
ac_subst(miniupnpc_libs)
ac_config_files([makefile src/makefile share/setup.nsi share/qt/info.plist src/test/buildenv.py])
ac_config_files([qa/pull-tester/run-bitcoind-for-test.sh],[chmod +x qa/pull-tester/run-bitcoind-for-test.sh])
ac_config_files([qa/pull-tester/tests-config.sh],[chmod +x qa/pull-tester/tests-config.sh])

dnl boost's m4 checks do something really nasty: they export these vars. as a
dnl result, they leak into secp256k1's configure and crazy things happen.
dnl until this is fixed upstream and we've synced, we'll just un-export them.
cppflags_temp="$cppflags"
unset cppflags
cppflags="$cppflags_temp"

ldflags_temp="$ldflags"
unset ldflags
ldflags="$ldflags_temp"

libs_temp="$libs"
unset libs
libs="$libs_temp"

pkgconfig_path_temp="$pkg_config_path"
unset pkg_config_path
pkg_config_path="$pkgconfig_path_temp"

pkgconfig_libdir_temp="$pkg_config_libdir"
unset pkg_config_libdir
pkg_config_libdir="$pkgconfig_libdir_temp"

ac_configure_args="${ac_configure_args} --disable-shared --with-pic --with-bignum=no"
ac_config_subdirs([src/secp256k1])

ac_output

dnl taken from https://wiki.debian.org/rpathissue
case $host in
   *-*-linux-gnu)
     ac_msg_result([fixing libtool for -rpath problems.])
     sed < libtool > libtool-2 \
     's/^hardcode_libdir_flag_spec.*$'/'hardcode_libdir_flag_spec=" -d__libtool_is_a_fool__ "/'
     mv libtool-2 libtool
     chmod 755 libtool
   ;;
esac
