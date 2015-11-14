dnl bitcoin_subdir_to_include([cppflags-variable-name],[subdirectory-name],[header-file])
dnl subdirectory-name must end with a path separator
ac_defun([bitcoin_subdir_to_include],[
  if test "x$2" = "x"; then
    ac_msg_result([default])
  else
    echo "#include <$2$3.h>" >conftest.cpp
    newinclpath=`${cxxcpp} ${cppflags} -m conftest.cpp 2>/dev/null | [ tr -d '\\n\\r\\\\' | sed -e 's/^.*[[:space:]:]\(\/[^[:space:]]*\)]$3[\.h[[:space:]].*$/\1/' -e t -e d`]
    ac_msg_result([${newinclpath}])
    if test "x${newinclpath}" != "x"; then
      eval "$1=\"\$$1\"' -i${newinclpath}'"
    fi
  fi
])
