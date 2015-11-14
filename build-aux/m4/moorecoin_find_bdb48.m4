ac_defun([bitcoin_find_bdb48],[
  ac_msg_checking([for berkeley db c++ headers])
  bdb_cppflags=
  bdb_libs=
  bdbpath=x
  bdb48path=x
  bdbdirlist=
  for _vn in 4.8 48 4 5 ''; do
    for _pfx in b lib ''; do
      bdbdirlist="$bdbdirlist ${_pfx}db${_vn}"
    done
  done
  for searchpath in $bdbdirlist ''; do
    test -n "${searchpath}" && searchpath="${searchpath}/"
    ac_compile_ifelse([ac_lang_program([[
      #include <${searchpath}db_cxx.h>
    ]],[[
      #if !((db_version_major == 4 && db_version_minor >= 8) || db_version_major > 4)
        #error "failed to find bdb 4.8+"
      #endif
    ]])],[
      if test "x$bdbpath" = "xx"; then
        bdbpath="${searchpath}"
      fi
    ],[
      continue
    ])
    ac_compile_ifelse([ac_lang_program([[
      #include <${searchpath}db_cxx.h>
    ]],[[
      #if !(db_version_major == 4 && db_version_minor == 8)
        #error "failed to find bdb 4.8"
      #endif
    ]])],[
      bdb48path="${searchpath}"
      break
    ],[])
  done
  if test "x$bdbpath" = "xx"; then
    ac_msg_result([no])
    ac_msg_error([libdb_cxx headers missing, bitcoin core requires this library for wallet functionality (--disable-wallet to disable wallet functionality)])
  elif test "x$bdb48path" = "xx"; then
    bitcoin_subdir_to_include(bdb_cppflags,[${bdbpath}],db_cxx)
    ac_arg_with([incompatible-bdb],[as_help_string([--with-incompatible-bdb], [allow using a bdb version other than 4.8])],[
      ac_msg_warn([found berkeley db other than 4.8; wallets opened by this build will not be portable!])
    ],[
      ac_msg_error([found berkeley db other than 4.8, required for portable wallets (--with-incompatible-bdb to ignore or --disable-wallet to disable wallet functionality)])
    ])
  else
    bitcoin_subdir_to_include(bdb_cppflags,[${bdb48path}],db_cxx)
    bdbpath="${bdb48path}"
  fi
  ac_subst(bdb_cppflags)
  
  # todo: ideally this could find the library version and make sure it matches the headers being used
  for searchlib in db_cxx-4.8 db_cxx; do
    ac_check_lib([$searchlib],[main],[
      bdb_libs="-l${searchlib}"
      break
    ])
  done
  if test "x$bdb_libs" = "x"; then
      ac_msg_error([libdb_cxx missing, bitcoin core requires this library for wallet functionality (--disable-wallet to disable wallet functionality)])
  fi
  ac_subst(bdb_libs)
])
