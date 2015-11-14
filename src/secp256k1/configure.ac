ac_prereq([2.60])
ac_init([libsecp256k1],[0.1])
ac_config_aux_dir([build-aux])
ac_config_macro_dir([build-aux/m4])
ac_canonical_host
ah_top([#ifndef libsecp256k1_config_h])
ah_top([#define libsecp256k1_config_h])
ah_bottom([#endif /*libsecp256k1_config_h*/])
am_init_automake([foreign subdir-objects])
lt_init

dnl make the compilation flags quiet unless v=1 is used
m4_ifdef([am_silent_rules], [am_silent_rules([yes])])

pkg_prog_pkg_config

ac_path_tool(ar, ar)
ac_path_tool(ranlib, ranlib)
ac_path_tool(strip, strip)

if test "x$cflags" = "x"; then
  cflags="-o3 -g"
fi

ac_prog_cc_c89
if test x"$ac_cv_prog_cc_c89" = x"no"; then
  ac_msg_error([c89 compiler support required])
fi

case $host in
  *mingw*)
     use_pkgconfig=no
     ;;
   *)
     use_pkgconfig=yes
     ;;
esac

case $host_os in
  *darwin*)
     if  test x$cross_compiling != xyes; then
       ac_path_prog([brew],brew,)
       if test x$brew != x; then
         dnl these homebrew packages may be keg-only, meaning that they won't be found
         dnl in expected paths because they may conflict with system files. ask
         dnl homebrew where each one is located, then adjust paths accordingly.

         openssl_prefix=`$brew --prefix openssl 2>/dev/null`
         gmp_prefix=`$brew --prefix gmp 2>/dev/null`
         if test x$openssl_prefix != x; then
           pkg_config_path="$openssl_prefix/lib/pkgconfig:$pkg_config_path"
           export pkg_config_path
         fi
         if test x$gmp_prefix != x; then
           gmp_cppflags="-i$gmp_prefix/include"
           gmp_libs="-l$gmp_prefix/lib"
         fi
       else
         ac_path_prog([port],port,)
         dnl if homebrew isn't installed and macports is, add the macports default paths
         dnl as a last resort.
         if test x$port != x; then
           cppflags="$cppflags -isystem /opt/local/include"
           ldflags="$ldflags -l/opt/local/lib"
         fi
       fi
     fi
   ;;
esac

cflags="$cflags -w"

warn_cflags="-std=c89 -pedantic -wall -wextra -wcast-align -wnested-externs -wshadow -wstrict-prototypes -wno-unused-function -wno-long-long -wno-overlength-strings"
saved_cflags="$cflags"
cflags="$cflags $warn_cflags"
ac_msg_checking([if ${cc} supports ${warn_cflags}])
ac_compile_ifelse([ac_lang_source([[char foo;]])],
    [ ac_msg_result([yes]) ],
    [ ac_msg_result([no])
      cflags="$saved_cflags"
    ])


ac_arg_enable(benchmark,
    as_help_string([--enable-benchmark],[compile benchmark (default is no)]),
    [use_benchmark=$enableval],
    [use_benchmark=no])

ac_arg_enable(tests,
    as_help_string([--enable-tests],[compile tests (default is yes)]),
    [use_tests=$enableval],
    [use_tests=yes])

ac_arg_enable(endomorphism,
    as_help_string([--enable-endomorphism],[enable endomorphism (default is no)]),
    [use_endomorphism=$enableval],
    [use_endomorphism=no])

ac_arg_with([field], [as_help_string([--with-field=64bit|32bit|auto],
[specify field implementation. default is auto])],[req_field=$withval], [req_field=auto])

ac_arg_with([bignum], [as_help_string([--with-bignum=gmp|no|auto],
[specify bignum implementation. default is auto])],[req_bignum=$withval], [req_bignum=auto])

ac_arg_with([scalar], [as_help_string([--with-scalar=64bit|32bit|auto],
[specify scalar implementation. default is auto])],[req_scalar=$withval], [req_scalar=auto])

ac_arg_with([asm], [as_help_string([--with-asm=x86_64|no|auto]
[specify assembly optimizations to use. default is auto])],[req_asm=$withval], [req_asm=auto])

ac_check_types([__int128])

ac_msg_checking([for __builtin_expect])
ac_compile_ifelse([ac_lang_source([[void myfunc() {__builtin_expect(0,0);}]])],
    [ ac_msg_result([yes]);ac_define(have_builtin_expect,1,[define this symbol if __builtin_expect is available]) ],
    [ ac_msg_result([no])
    ])

if test x"$req_asm" = x"auto"; then
  secp_64bit_asm_check
  if test x"$has_64bit_asm" = x"yes"; then
    set_asm=x86_64
  fi
  if test x"$set_asm" = x; then
    set_asm=no
  fi
else
  set_asm=$req_asm
  case $set_asm in
  x86_64)
    secp_64bit_asm_check
    if test x"$has_64bit_asm" != x"yes"; then
      ac_msg_error([x86_64 assembly optimization requested but not available])
    fi
    ;;
  no)
    ;;
  *)
    ac_msg_error([invalid assembly optimization selection])
    ;;
  esac
fi

if test x"$req_field" = x"auto"; then
  if test x"set_asm" = x"x86_64"; then
    set_field=64bit
  fi
  if test x"$set_field" = x; then
    secp_int128_check
    if test x"$has_int128" = x"yes"; then
      set_field=64bit
    fi
  fi
  if test x"$set_field" = x; then
    set_field=32bit
  fi
else
  set_field=$req_field
  case $set_field in
  64bit)
    if test x"$set_asm" != x"x86_64"; then
      secp_int128_check
      if test x"$has_int128" != x"yes"; then
        ac_msg_error([64bit field explicitly requested but neither __int128 support or x86_64 assembly available])
      fi
    fi
    ;;
  32bit)
    ;;
  *)
    ac_msg_error([invalid field implementation selection])
    ;;
  esac
fi

if test x"$req_scalar" = x"auto"; then
  secp_int128_check
  if test x"$has_int128" = x"yes"; then
    set_scalar=64bit
  fi
  if test x"$set_scalar" = x; then
    set_scalar=32bit
  fi
else
  set_scalar=$req_scalar
  case $set_scalar in
  64bit)
    secp_int128_check
    if test x"$has_int128" != x"yes"; then
      ac_msg_error([64bit scalar explicitly requested but __int128 support not available])
    fi
    ;;
  32bit)
    ;;
  *)
    ac_msg_error([invalid scalar implementation selected])
    ;;
  esac
fi

if test x"$req_bignum" = x"auto"; then
  secp_gmp_check
  if test x"$has_gmp" = x"yes"; then
    set_bignum=gmp
  fi

  if test x"$set_bignum" = x; then
    set_bignum=no
  fi
else
  set_bignum=$req_bignum
  case $set_bignum in
  gmp)
    secp_gmp_check
    if test x"$has_gmp" != x"yes"; then
      ac_msg_error([gmp bignum explicitly requested but libgmp not available])
    fi
    ;;
  no)
    ;;
  *)
    ac_msg_error([invalid bignum implementation selection])
    ;;
  esac
fi

# select assembly optimization
case $set_asm in
x86_64)
  ac_define(use_asm_x86_64, 1, [define this symbol to enable x86_64 assembly optimizations])
  ;;
no)
  ;;
*)
  ac_msg_error([invalid assembly optimizations])
  ;;
esac

# select field implementation
case $set_field in
64bit)
  ac_define(use_field_5x52, 1, [define this symbol to use the field_5x52 implementation])
  ;;
32bit)
  ac_define(use_field_10x26, 1, [define this symbol to use the field_10x26 implementation])
  ;;
*)
  ac_msg_error([invalid field implementation])
  ;;
esac

# select bignum implementation
case $set_bignum in
gmp)
  ac_define(have_libgmp, 1, [define this symbol if libgmp is installed])
  ac_define(use_num_gmp, 1, [define this symbol to use the gmp implementation for num])
  ac_define(use_field_inv_num, 1, [define this symbol to use the num-based field inverse implementation])
  ac_define(use_scalar_inv_num, 1, [define this symbol to use the num-based scalar inverse implementation])
  ;;
no)
  ac_define(use_num_none, 1, [define this symbol to use no num implementation])
  ac_define(use_field_inv_builtin, 1, [define this symbol to use the native field inverse implementation])
  ac_define(use_scalar_inv_builtin, 1, [define this symbol to use the native scalar inverse implementation])
  ;;
*)
  ac_msg_error([invalid bignum implementation])
  ;;
esac

#select scalar implementation
case $set_scalar in
64bit)
  ac_define(use_scalar_4x64, 1, [define this symbol to use the 4x64 scalar implementation])
  ;;
32bit)
  ac_define(use_scalar_8x32, 1, [define this symbol to use the 8x32 scalar implementation])
  ;;
*)
  ac_msg_error([invalid scalar implementation])
  ;;
esac

if test x"$use_tests" = x"yes"; then
  secp_openssl_check
  if test x"$has_openssl_ec" = x"yes"; then
    ac_define(enable_openssl_tests, 1, [define this symbol if openssl ec functions are available])
    secp_test_includes="$ssl_cflags $crypto_cflags"
    secp_test_libs="$crypto_libs"

    case $host in
    *mingw*)
      secp_test_libs="$secp_test_libs -lgdi32"
      ;;
    esac

  fi
fi

if test x"$set_bignum" = x"gmp"; then
  secp_libs="$secp_libs $gmp_libs"
  secp_includes="$secp_includes $gmp_cppflags"
fi

if test x"$use_endomorphism" = x"yes"; then
  ac_define(use_endomorphism, 1, [define this symbol to use endomorphism optimization])
fi

ac_c_bigendian()

ac_msg_notice([using assembly optimizations: $set_asm])
ac_msg_notice([using field implementation: $set_field])
ac_msg_notice([using bignum implementation: $set_bignum])
ac_msg_notice([using scalar implementation: $set_scalar])
ac_msg_notice([using endomorphism optimizations: $use_endomorphism])

ac_config_headers([src/libsecp256k1-config.h])
ac_config_files([makefile libsecp256k1.pc])
ac_subst(secp_includes)
ac_subst(secp_libs)
ac_subst(secp_test_libs)
ac_subst(secp_test_includes)
am_conditional([use_tests], [test x"$use_tests" != x"no"])
am_conditional([use_benchmark], [test x"$use_benchmark" = x"yes"])

dnl make sure nothing new is exported so that we don't break the cache
pkgconfig_path_temp="$pkg_config_path"
unset pkg_config_path
pkg_config_path="$pkgconfig_path_temp"

ac_output
