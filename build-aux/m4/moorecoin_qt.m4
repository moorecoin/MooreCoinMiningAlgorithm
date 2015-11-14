dnl helper for cases where a qt dependency is not met.
dnl output: if qt version is auto, set bitcoin_enable_qt to false. else, exit.
ac_defun([bitcoin_qt_fail],[
  if test "x$bitcoin_qt_want_version" = "xauto" && test x$bitcoin_qt_force != xyes; then
    if test x$bitcoin_enable_qt != xno; then
      ac_msg_warn([$1; bitcoin-qt frontend will not be built])
    fi
    bitcoin_enable_qt=no
    bitcoin_enable_qt_test=no
  else
    ac_msg_error([$1])
  fi
])

ac_defun([bitcoin_qt_check],[
  if test "x$bitcoin_enable_qt" != "xno" && test x$bitcoin_qt_want_version != xno; then
    true
    $1
  else
    true
    $2
  fi
])

dnl bitcoin_qt_path_progs([foo], [foo foo2], [/path/to/search/first], [continue if missing])
dnl helper for finding the path of programs needed for qt.
dnl inputs: $1: variable to be set
dnl inputs: $2: list of programs to search for
dnl inputs: $3: look for $2 here before $path
dnl inputs: $4: if "yes", don't fail if $2 is not found.
dnl output: $1 is set to the path of $2 if found. $2 are searched in order.
ac_defun([bitcoin_qt_path_progs],[
  bitcoin_qt_check([
    if test "x$3" != "x"; then
      ac_path_progs($1,$2,,$3)
    else
      ac_path_progs($1,$2)
    fi
    if test "x$$1" = "x" && test "x$4" != "xyes"; then
      bitcoin_qt_fail([$1 not found])
    fi
  ])
])

dnl initialize qt input.
dnl this must be called before any other bitcoin_qt* macros to ensure that
dnl input variables are set correctly.
dnl caution: do not use this inside of a conditional.
ac_defun([bitcoin_qt_init],[
  dnl enable qt support
  ac_arg_with([gui],
    [as_help_string([--with-gui@<:@=no|qt4|qt5|auto@:>@],
    [build bitcoin-qt gui (default=auto, qt4 tried first)])],
    [
     bitcoin_qt_want_version=$withval
     if test x$bitcoin_qt_want_version = xyes; then
       bitcoin_qt_force=yes
       bitcoin_qt_want_version=auto
     fi
    ],
    [bitcoin_qt_want_version=auto])

  ac_arg_with([qt-incdir],[as_help_string([--with-qt-incdir=inc_dir],[specify qt include path (overridden by pkgconfig)])], [qt_include_path=$withval], [])
  ac_arg_with([qt-libdir],[as_help_string([--with-qt-libdir=lib_dir],[specify qt lib path (overridden by pkgconfig)])], [qt_lib_path=$withval], [])
  ac_arg_with([qt-plugindir],[as_help_string([--with-qt-plugindir=plugin_dir],[specify qt plugin path (overridden by pkgconfig)])], [qt_plugin_path=$withval], [])
  ac_arg_with([qt-translationdir],[as_help_string([--with-qt-translationdir=plugin_dir],[specify qt translation path (overridden by pkgconfig)])], [qt_translation_path=$withval], [])
  ac_arg_with([qt-bindir],[as_help_string([--with-qt-bindir=bin_dir],[specify qt bin path])], [qt_bin_path=$withval], [])

  ac_arg_with([qtdbus],
    [as_help_string([--with-qtdbus],
    [enable dbus support (default is yes if qt is enabled and qtdbus is found)])],
    [use_dbus=$withval],
    [use_dbus=auto])

  ac_subst(qt_translation_dir,$qt_translation_path)
])

dnl find the appropriate version of qt libraries and includes.
dnl inputs: $1: whether or not pkg-config should be used. yes|no. default: yes.
dnl inputs: $2: if $1 is "yes" and --with-gui=auto, which qt version should be
dnl         tried first.
dnl outputs: see _bitcoin_qt_find_libs_*
dnl outputs: sets variables for all qt-related tools.
dnl outputs: bitcoin_enable_qt, bitcoin_enable_qt_dbus, bitcoin_enable_qt_test
ac_defun([bitcoin_qt_configure],[
  use_pkgconfig=$1

  if test x$use_pkgconfig = x; then
    use_pkgconfig=yes
  fi

  if test x$use_pkgconfig = xyes; then
    bitcoin_qt_check([_bitcoin_qt_find_libs_with_pkgconfig([$2])])
  else
    bitcoin_qt_check([_bitcoin_qt_find_libs_without_pkgconfig])
  fi

  dnl this is ugly and complicated. yuck. works as follows:
  dnl we can't discern whether qt4 builds are static or not. for qt5, we can
  dnl check a header to find out. when qt is built statically, some plugins must
  dnl be linked into the final binary as well. these plugins have changed between
  dnl qt4 and qt5. with qt5, languages moved into core and the windowsintegration
  dnl plugin was added. since we can't tell if qt4 is static or not, it is
  dnl assumed for windows builds.
  dnl _bitcoin_qt_check_static_plugins does a quick link-check and appends the
  dnl results to qt_libs.
  bitcoin_qt_check([
  temp_cppflags=$cppflags
  cppflags=$qt_includes
  if test x$bitcoin_qt_got_major_vers = x5; then
    _bitcoin_qt_is_static
    if test x$bitcoin_cv_static_qt = xyes; then
      ac_define(qt_staticplugin, 1, [define this symbol if qt plugins are static])
      if test x$qt_plugin_path != x; then
        qt_libs="$qt_libs -l$qt_plugin_path/accessible"
        qt_libs="$qt_libs -l$qt_plugin_path/platforms"
      fi
      if test x$use_pkgconfig = xyes; then
        pkg_check_modules([qtplatform], [qt5platformsupport], [qt_libs="$qtplatform_libs $qt_libs"])
      fi
      _bitcoin_qt_check_static_plugins([q_import_plugin(accessiblefactory)], [-lqtaccessiblewidgets])
      if test x$target_os = xwindows; then
        _bitcoin_qt_check_static_plugins([q_import_plugin(qwindowsintegrationplugin)],[-lqwindows])
        ac_define(qt_qpa_platform_windows, 1, [define this symbol if the qt platform is windows])
      elif test x$target_os = xlinux; then
        pkg_check_modules([x11xcb], [x11-xcb], [qt_libs="$x11xcb_libs $qt_libs"])
        _bitcoin_qt_check_static_plugins([q_import_plugin(qxcbintegrationplugin)],[-lqxcb -lxcb-static])
        ac_define(qt_qpa_platform_xcb, 1, [define this symbol if the qt platform is xcb])
      elif test x$target_os = xdarwin; then
        if test x$use_pkgconfig = xyes; then
          pkg_check_modules([qtprint], [qt5printsupport], [qt_libs="$qtprint_libs $qt_libs"])
        fi
        ax_check_link_flag([[-framework iokit]],[qt_libs="$qt_libs -framework iokit"],[ac_msg_error(could not iokit framework)])
        _bitcoin_qt_check_static_plugins([q_import_plugin(qcocoaintegrationplugin)],[-lqcocoa])
        ac_define(qt_qpa_platform_cocoa, 1, [define this symbol if the qt platform is cocoa])
      fi
    fi
  else
    if test x$target_os = xwindows; then
      ac_define(qt_staticplugin, 1, [define this symbol if qt plugins are static])
      if test x$qt_plugin_path != x; then
        qt_libs="$qt_libs -l$qt_plugin_path/accessible"
        qt_libs="$qt_libs -l$qt_plugin_path/codecs"
      fi
      _bitcoin_qt_check_static_plugins([
         q_import_plugin(qcncodecs)
         q_import_plugin(qjpcodecs)
         q_import_plugin(qtwcodecs)
         q_import_plugin(qkrcodecs)
         q_import_plugin(accessiblefactory)],
         [-lqcncodecs -lqjpcodecs -lqtwcodecs -lqkrcodecs -lqtaccessiblewidgets])
    fi
  fi
  cppflags=$temp_cppflags
  ])

  if test x$use_pkgconfig$qt_bin_path = xyes; then
    if test x$bitcoin_qt_got_major_vers = x5; then
      qt_bin_path="`$pkg_config --variable=host_bins qt5core 2>/dev/null`"
    fi
  fi

  bitcoin_qt_path_progs([moc], [moc-qt${bitcoin_qt_got_major_vers} moc${bitcoin_qt_got_major_vers} moc], $qt_bin_path)
  bitcoin_qt_path_progs([uic], [uic-qt${bitcoin_qt_got_major_vers} uic${bitcoin_qt_got_major_vers} uic], $qt_bin_path)
  bitcoin_qt_path_progs([rcc], [rcc-qt${bitcoin_qt_got_major_vers} rcc${bitcoin_qt_got_major_vers} rcc], $qt_bin_path)
  bitcoin_qt_path_progs([lrelease], [lrelease-qt${bitcoin_qt_got_major_vers} lrelease${bitcoin_qt_got_major_vers} lrelease], $qt_bin_path)
  bitcoin_qt_path_progs([lupdate], [lupdate-qt${bitcoin_qt_got_major_vers} lupdate${bitcoin_qt_got_major_vers} lupdate],$qt_bin_path, yes)

  moc_defs='-dhave_config_h -i$(srcdir)'
  case $host in
    *darwin*)
     bitcoin_qt_check([
       moc_defs="${moc_defs} -dq_os_mac"
       base_frameworks="-framework foundation -framework applicationservices -framework appkit"
       ax_check_link_flag([[$base_frameworks]],[qt_libs="$qt_libs $base_frameworks"],[ac_msg_error(could not find base frameworks)])
     ])
    ;;
    *mingw*)
       bitcoin_qt_check([
         ax_check_link_flag([[-mwindows]],[qt_ldflags="$qt_ldflags -mwindows"],[ac_msg_warn(-mwindows linker support not detected)])
       ])
  esac


  dnl enable qt support
  ac_msg_checking(whether to build bitcoin core gui)
  bitcoin_qt_check([
    bitcoin_enable_qt=yes
    bitcoin_enable_qt_test=yes
    if test x$have_qt_test = xno; then
      bitcoin_enable_qt_test=no
    fi
    bitcoin_enable_qt_dbus=no
    if test x$use_dbus != xno && test x$have_qt_dbus = xyes; then
      bitcoin_enable_qt_dbus=yes
    fi
    if test x$use_dbus = xyes && test x$have_qt_dbus = xno; then
      ac_msg_error("libqtdbus not found. install libqtdbus or remove --with-qtdbus.")
    fi
    if test x$lupdate = x; then
      ac_msg_warn("lupdate is required to update qt translations")
    fi
  ],[
    bitcoin_enable_qt=no
  ])
  ac_msg_result([$bitcoin_enable_qt (qt${bitcoin_qt_got_major_vers})])

  ac_subst(qt_includes)
  ac_subst(qt_libs)
  ac_subst(qt_ldflags)
  ac_subst(qt_dbus_includes)
  ac_subst(qt_dbus_libs)
  ac_subst(qt_test_includes)
  ac_subst(qt_test_libs)
  ac_subst(qt_select, qt${bitcoin_qt_got_major_vers})
  ac_subst(moc_defs)
])

dnl all macros below are internal and should _not_ be used from the main
dnl configure.ac.
dnl ----

dnl internal. check if the included version of qt is qt5.
dnl requires: includes must be populated as necessary.
dnl output: bitcoin_cv_qt5=yes|no
ac_defun([_bitcoin_qt_check_qt5],[
  ac_cache_check(for qt 5, bitcoin_cv_qt5,[
  ac_compile_ifelse([ac_lang_program(
    [[#include <qtcore>]],
    [[
      #if qt_version < 0x050000
      choke me
      #else
      return 0;
      #endif
    ]])],
    [bitcoin_cv_qt5=yes],
    [bitcoin_cv_qt5=no])
])])

dnl internal. check if the linked version of qt was built as static libs.
dnl requires: qt5. this check cannot determine if qt4 is static.
dnl requires: includes and libs must be populated as necessary.
dnl output: bitcoin_cv_static_qt=yes|no
dnl output: defines qt_staticplugin if plugins are static.
ac_defun([_bitcoin_qt_is_static],[
  ac_cache_check(for static qt, bitcoin_cv_static_qt,[
  ac_compile_ifelse([ac_lang_program(
    [[#include <qtcore>]],
    [[
      #if defined(qt_static)
      return 0;
      #else
      choke me
      #endif
    ]])],
    [bitcoin_cv_static_qt=yes],
    [bitcoin_cv_static_qt=no])
  ])
  if test xbitcoin_cv_static_qt = xyes; then
    ac_define(qt_staticplugin, 1, [define this symbol for static qt plugins])
  fi
])

dnl internal. check if the link-requirements for static plugins are met.
dnl requires: includes and libs must be populated as necessary.
dnl inputs: $1: a series of q_import_plugin().
dnl inputs: $2: the libraries that resolve $1.
dnl output: qt_libs is prepended or configure exits.
ac_defun([_bitcoin_qt_check_static_plugins],[
  ac_msg_checking(for static qt plugins: $2)
  check_static_plugins_temp_libs="$libs"
  libs="$2 $qt_libs $libs"
  ac_link_ifelse([ac_lang_program([[
    #define qt_staticplugin
    #include <qtplugin>
    $1]],
    [[return 0;]])],
    [ac_msg_result(yes); qt_libs="$2 $qt_libs"],
    [ac_msg_result(no); bitcoin_qt_fail(could not resolve: $2)])
  libs="$check_static_plugins_temp_libs"
])

dnl internal. find qt libraries using pkg-config.
dnl inputs: bitcoin_qt_want_version (from --with-gui=). the version to check
dnl         first.
dnl inputs: $1: if bitcoin_qt_want_version is "auto", check for this version
dnl         first.
dnl outputs: all necessary qt_* variables are set.
dnl outputs: bitcoin_qt_got_major_vers is set to "4" or "5".
dnl outputs: have_qt_test and have_qt_dbus are set (if applicable) to yes|no.
ac_defun([_bitcoin_qt_find_libs_with_pkgconfig],[
  m4_ifdef([pkg_check_modules],[
  auto_priority_version=$1
  if test x$auto_priority_version = x; then
    auto_priority_version=qt5
  fi
    if test x$bitcoin_qt_want_version = xqt5 ||  ( test x$bitcoin_qt_want_version = xauto && test x$auto_priority_version = xqt5 ); then
      qt_lib_prefix=qt5
      bitcoin_qt_got_major_vers=5
    else
      qt_lib_prefix=qt
      bitcoin_qt_got_major_vers=4
    fi
    qt5_modules="qt5core qt5gui qt5network qt5widgets"
    qt4_modules="qtcore qtgui qtnetwork"
    bitcoin_qt_check([
      if test x$bitcoin_qt_want_version = xqt5 || ( test x$bitcoin_qt_want_version = xauto && test x$auto_priority_version = xqt5 ); then
        pkg_check_modules([qt], [$qt5_modules], [qt_includes="$qt_cflags"; have_qt=yes],[have_qt=no])
      elif test x$bitcoin_qt_want_version = xqt4 || ( test x$bitcoin_qt_want_version = xauto && test x$auto_priority_version = xqt4 ); then
        pkg_check_modules([qt], [$qt4_modules], [qt_includes="$qt_cflags"; have_qt=yes], [have_qt=no])
      fi

      dnl qt version is set to 'auto' and the preferred version wasn't found. now try the other.
      if test x$have_qt = xno && test x$bitcoin_qt_want_version = xauto; then
        if test x$auto_priority_version = x$qt5; then
          pkg_check_modules([qt], [$qt4_modules], [qt_includes="$qt_cflags"; have_qt=yes; qt_lib_prefix=qt; bitcoin_qt_got_major_vers=4], [have_qt=no])
        else
          pkg_check_modules([qt], [$qt5_modules], [qt_includes="$qt_cflags"; have_qt=yes; qt_lib_prefix=qt5; bitcoin_qt_got_major_vers=5], [have_qt=no])
        fi
      fi
      if test x$have_qt != xyes; then
        have_qt=no
        bitcoin_qt_fail([qt dependencies not found])
      fi
    ])
    bitcoin_qt_check([
      pkg_check_modules([qt_test], [${qt_lib_prefix}test], [qt_test_includes="$qt_test_cflags"; have_qt_test=yes], [have_qt_test=no])
      if test x$use_dbus != xno; then
        pkg_check_modules([qt_dbus], [${qt_lib_prefix}dbus], [qt_dbus_includes="$qt_dbus_cflags"; have_qt_dbus=yes], [have_qt_dbus=no])
      fi
    ])
  ])
  true; dnl
])

dnl internal. find qt libraries without using pkg-config. version is deduced
dnl from the discovered headers.
dnl inputs: bitcoin_qt_want_version (from --with-gui=). the version to use.
dnl         if "auto", the version will be discovered by _bitcoin_qt_check_qt5.
dnl outputs: all necessary qt_* variables are set.
dnl outputs: bitcoin_qt_got_major_vers is set to "4" or "5".
dnl outputs: have_qt_test and have_qt_dbus are set (if applicable) to yes|no.
ac_defun([_bitcoin_qt_find_libs_without_pkgconfig],[
  temp_cppflags="$cppflags"
  temp_libs="$libs"
  bitcoin_qt_check([
    if test x$qt_include_path != x; then
      qt_includes="-i$qt_include_path -i$qt_include_path/qtcore -i$qt_include_path/qtgui -i$qt_include_path/qtwidgets -i$qt_include_path/qtnetwork -i$qt_include_path/qttest -i$qt_include_path/qtdbus"
      cppflags="$qt_includes $cppflags"
    fi
  ])

  bitcoin_qt_check([ac_check_header([qtplugin],,bitcoin_qt_fail(qtcore headers missing))])
  bitcoin_qt_check([ac_check_header([qapplication],, bitcoin_qt_fail(qtgui headers missing))])
  bitcoin_qt_check([ac_check_header([qlocalsocket],, bitcoin_qt_fail(qtnetwork headers missing))])

  bitcoin_qt_check([
    if test x$bitcoin_qt_want_version = xauto; then
      _bitcoin_qt_check_qt5
    fi
    if test x$bitcoin_cv_qt5 = xyes || test x$bitcoin_qt_want_version = xqt5; then
      qt_lib_prefix=qt5
      bitcoin_qt_got_major_vers=5
    else
      qt_lib_prefix=qt
      bitcoin_qt_got_major_vers=4
    fi
  ])

  bitcoin_qt_check([
    libs=
    if test x$qt_lib_path != x; then
      libs="$libs -l$qt_lib_path"
    fi

    if test x$target_os = xwindows; then
      ac_check_lib([imm32],      [main],, bitcoin_qt_fail(libimm32 not found))
    fi
  ])

  bitcoin_qt_check(ac_check_lib([z] ,[main],,ac_msg_warn([zlib not found. assuming qt has it built-in])))
  bitcoin_qt_check(ac_check_lib([png] ,[main],,ac_msg_warn([libpng not found. assuming qt has it built-in])))
  bitcoin_qt_check(ac_check_lib([jpeg] ,[main],,ac_msg_warn([libjpeg not found. assuming qt has it built-in])))
  bitcoin_qt_check(ac_check_lib([pcre16] ,[main],,ac_msg_warn([libpcre16 not found. assuming qt has it built-in])))
  bitcoin_qt_check(ac_check_lib([${qt_lib_prefix}core]   ,[main],,bitcoin_qt_fail(lib$qt_lib_prefixcore not found)))
  bitcoin_qt_check(ac_check_lib([${qt_lib_prefix}gui]    ,[main],,bitcoin_qt_fail(lib$qt_lib_prefixgui not found)))
  bitcoin_qt_check(ac_check_lib([${qt_lib_prefix}network],[main],,bitcoin_qt_fail(lib$qt_lib_prefixnetwork not found)))
  if test x$bitcoin_qt_got_major_vers = x5; then
    bitcoin_qt_check(ac_check_lib([${qt_lib_prefix}widgets],[main],,bitcoin_qt_fail(lib$qt_lib_prefixwidgets not found)))
  fi
  qt_libs="$libs"
  libs="$temp_libs"

  bitcoin_qt_check([
    libs=
    if test x$qt_lib_path != x; then
      libs="-l$qt_lib_path"
    fi
    ac_check_lib([${qt_lib_prefix}test],      [main],, have_qt_test=no)
    ac_check_header([qtest],, have_qt_test=no)
    qt_test_libs="$libs"
    if test x$use_dbus != xno; then
      libs=
      if test x$qt_lib_path != x; then
        libs="-l$qt_lib_path"
      fi
      ac_check_lib([${qt_lib_prefix}dbus],      [main],, have_qt_dbus=no)
      ac_check_header([qtdbus],, have_qt_dbus=no)
      qt_dbus_libs="$libs"
    fi
  ])
  cppflags="$temp_cppflags"
  libs="$temp_libs"
])

