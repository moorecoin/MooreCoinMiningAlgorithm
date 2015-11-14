# ===========================================================================
#       http://www.gnu.org/software/autoconf-archive/ax_boost_base.html
# ===========================================================================
#
# synopsis
#
#   ax_boost_base([minimum-version], [action-if-found], [action-if-not-found])
#
# description
#
#   test for the boost c++ libraries of a particular version (or newer)
#
#   if no path to the installed boost library is given the macro searchs
#   under /usr, /usr/local, /opt and /opt/local and evaluates the
#   $boost_root environment variable. further documentation is available at
#   <http://randspringer.de/boost/index.html>.
#
#   this macro calls:
#
#     ac_subst(boost_cppflags) / ac_subst(boost_ldflags)
#
#   and sets:
#
#     have_boost
#
# license
#
#   copyright (c) 2008 thomas porschberg <thomas@randspringer.de>
#   copyright (c) 2009 peter adolphs
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. this file is offered as-is, without any
#   warranty.

#serial 23

ac_defun([ax_boost_base],
[
ac_arg_with([boost],
  [as_help_string([--with-boost@<:@=arg@:>@],
    [use boost library from a standard location (arg=yes),
     from the specified location (arg=<path>),
     or disable it (arg=no)
     @<:@arg=yes@:>@ ])],
    [
    if test "$withval" = "no"; then
        want_boost="no"
    elif test "$withval" = "yes"; then
        want_boost="yes"
        ac_boost_path=""
    else
        want_boost="yes"
        ac_boost_path="$withval"
    fi
    ],
    [want_boost="yes"])


ac_arg_with([boost-libdir],
        as_help_string([--with-boost-libdir=lib_dir],
        [force given directory for boost libraries. note that this will override library path detection, so use this parameter only if default library detection fails and you know exactly where your boost libraries are located.]),
        [
        if test -d "$withval"
        then
                ac_boost_lib_path="$withval"
        else
                ac_msg_error(--with-boost-libdir expected directory name)
        fi
        ],
        [ac_boost_lib_path=""]
)

if test "x$want_boost" = "xyes"; then
    boost_lib_version_req=ifelse([$1], ,1.20.0,$1)
    boost_lib_version_req_shorten=`expr $boost_lib_version_req : '\([[0-9]]*\.[[0-9]]*\)'`
    boost_lib_version_req_major=`expr $boost_lib_version_req : '\([[0-9]]*\)'`
    boost_lib_version_req_minor=`expr $boost_lib_version_req : '[[0-9]]*\.\([[0-9]]*\)'`
    boost_lib_version_req_sub_minor=`expr $boost_lib_version_req : '[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)'`
    if test "x$boost_lib_version_req_sub_minor" = "x" ; then
        boost_lib_version_req_sub_minor="0"
        fi
    want_boost_version=`expr $boost_lib_version_req_major \* 100000 \+  $boost_lib_version_req_minor \* 100 \+ $boost_lib_version_req_sub_minor`
    ac_msg_checking(for boostlib >= $boost_lib_version_req)
    succeeded=no

    dnl on 64-bit systems check for system libraries in both lib64 and lib.
    dnl the former is specified by fhs, but e.g. debian does not adhere to
    dnl this (as it rises problems for generic multi-arch support).
    dnl the last entry in the list is chosen by default when no libraries
    dnl are found, e.g. when only header-only libraries are installed!
    libsubdirs="lib"
    ax_arch=`uname -m`
    case $ax_arch in
      x86_64)
        libsubdirs="lib64 libx32 lib lib64"
        ;;
      ppc64|s390x|sparc64|aarch64)
        libsubdirs="lib64 lib lib64"
        ;;
    esac

    dnl allow for real multi-arch paths e.g. /usr/lib/x86_64-linux-gnu. give
    dnl them priority over the other paths since, if libs are found there, they
    dnl are almost assuredly the ones desired.
    ac_require([ac_canonical_host])
    libsubdirs="lib/${host_cpu}-${host_os} $libsubdirs"

    case ${host_cpu} in
      i?86)
        libsubdirs="lib/i386-${host_os} $libsubdirs"
        ;;
    esac

    dnl some arches may advertise a cpu type that doesn't line up with their
    dnl prefix's cpu type. for example, uname may report armv7l while libs are
    dnl installed to /usr/lib/arm-linux-gnueabihf. try getting the compiler's
    dnl value for an extra chance of finding the correct path.
    libsubdirs="lib/`$cxx -dumpmachine 2>/dev/null` $libsubdirs"

    dnl first we check the system location for boost libraries
    dnl this location ist chosen if boost libraries are installed with the --layout=system option
    dnl or if you install boost with rpm
    if test "$ac_boost_path" != ""; then
        boost_cppflags="-i$ac_boost_path/include"
        for ac_boost_path_tmp in $libsubdirs; do
                if test -d "$ac_boost_path"/"$ac_boost_path_tmp" ; then
                        boost_ldflags="-l$ac_boost_path/$ac_boost_path_tmp"
                        break
                fi
        done
    elif test "$cross_compiling" != yes; then
        for ac_boost_path_tmp in /usr /usr/local /opt /opt/local ; do
            if test -d "$ac_boost_path_tmp/include/boost" && test -r "$ac_boost_path_tmp/include/boost"; then
                for libsubdir in $libsubdirs ; do
                    if ls "$ac_boost_path_tmp/$libsubdir/libboost_"* >/dev/null 2>&1 ; then break; fi
                done
                boost_ldflags="-l$ac_boost_path_tmp/$libsubdir"
                boost_cppflags="-i$ac_boost_path_tmp/include"
                break;
            fi
        done
    fi

    dnl overwrite ld flags if we have required special directory with
    dnl --with-boost-libdir parameter
    if test "$ac_boost_lib_path" != ""; then
       boost_ldflags="-l$ac_boost_lib_path"
    fi

    cppflags_saved="$cppflags"
    cppflags="$cppflags $boost_cppflags"
    export cppflags

    ldflags_saved="$ldflags"
    ldflags="$ldflags $boost_ldflags"
    export ldflags

    ac_require([ac_prog_cxx])
    ac_lang_push(c++)
        ac_compile_ifelse([ac_lang_program([[
    @%:@include <boost/version.hpp>
    ]], [[
    #if boost_version >= $want_boost_version
    // everything is okay
    #else
    #  error boost version is too old
    #endif
    ]])],[
        ac_msg_result(yes)
    succeeded=yes
    found_system=yes
        ],[:
        ])
    ac_lang_pop([c++])



    dnl if we found no boost with system layout we search for boost libraries
    dnl built and installed without the --layout=system option or for a staged(not installed) version
    if test "x$succeeded" != "xyes"; then
        _version=0
        if test "$ac_boost_path" != ""; then
            if test -d "$ac_boost_path" && test -r "$ac_boost_path"; then
                for i in `ls -d $ac_boost_path/include/boost-* 2>/dev/null`; do
                    _version_tmp=`echo $i | sed "s#$ac_boost_path##" | sed 's/\/include\/boost-//' | sed 's/_/./'`
                    v_check=`expr $_version_tmp \> $_version`
                    if test "$v_check" = "1" ; then
                        _version=$_version_tmp
                    fi
                    version_underscore=`echo $_version | sed 's/\./_/'`
                    boost_cppflags="-i$ac_boost_path/include/boost-$version_underscore"
                done
            fi
        else
            if test "$cross_compiling" != yes; then
                for ac_boost_path in /usr /usr/local /opt /opt/local ; do
                    if test -d "$ac_boost_path" && test -r "$ac_boost_path"; then
                        for i in `ls -d $ac_boost_path/include/boost-* 2>/dev/null`; do
                            _version_tmp=`echo $i | sed "s#$ac_boost_path##" | sed 's/\/include\/boost-//' | sed 's/_/./'`
                            v_check=`expr $_version_tmp \> $_version`
                            if test "$v_check" = "1" ; then
                                _version=$_version_tmp
                                best_path=$ac_boost_path
                            fi
                        done
                    fi
                done

                version_underscore=`echo $_version | sed 's/\./_/'`
                boost_cppflags="-i$best_path/include/boost-$version_underscore"
                if test "$ac_boost_lib_path" = ""; then
                    for libsubdir in $libsubdirs ; do
                        if ls "$best_path/$libsubdir/libboost_"* >/dev/null 2>&1 ; then break; fi
                    done
                    boost_ldflags="-l$best_path/$libsubdir"
                fi
            fi

            if test "x$boost_root" != "x"; then
                for libsubdir in $libsubdirs ; do
                    if ls "$boost_root/stage/$libsubdir/libboost_"* >/dev/null 2>&1 ; then break; fi
                done
                if test -d "$boost_root" && test -r "$boost_root" && test -d "$boost_root/stage/$libsubdir" && test -r "$boost_root/stage/$libsubdir"; then
                    version_dir=`expr //$boost_root : '.*/\(.*\)'`
                    stage_version=`echo $version_dir | sed 's/boost_//' | sed 's/_/./g'`
                        stage_version_shorten=`expr $stage_version : '\([[0-9]]*\.[[0-9]]*\)'`
                    v_check=`expr $stage_version_shorten \>\= $_version`
                    if test "$v_check" = "1" -a "$ac_boost_lib_path" = "" ; then
                        ac_msg_notice(we will use a staged boost library from $boost_root)
                        boost_cppflags="-i$boost_root"
                        boost_ldflags="-l$boost_root/stage/$libsubdir"
                    fi
                fi
            fi
        fi

        cppflags="$cppflags $boost_cppflags"
        export cppflags
        ldflags="$ldflags $boost_ldflags"
        export ldflags

        ac_lang_push(c++)
            ac_compile_ifelse([ac_lang_program([[
        @%:@include <boost/version.hpp>
        ]], [[
        #if boost_version >= $want_boost_version
        // everything is okay
        #else
        #  error boost version is too old
        #endif
        ]])],[
            ac_msg_result(yes)
        succeeded=yes
        found_system=yes
            ],[:
            ])
        ac_lang_pop([c++])
    fi

    if test "$succeeded" != "yes" ; then
        if test "$_version" = "0" ; then
            ac_msg_notice([[we could not detect the boost libraries (version $boost_lib_version_req_shorten or higher). if you have a staged boost library (still not installed) please specify \$boost_root in your environment and do not give a path to --with-boost option.  if you are sure you have boost installed, then check your version number looking in <boost/version.hpp>. see http://randspringer.de/boost for more documentation.]])
        else
            ac_msg_notice([your boost libraries seems to old (version $_version).])
        fi
        # execute action-if-not-found (if present):
        ifelse([$3], , :, [$3])
    else
        ac_subst(boost_cppflags)
        ac_subst(boost_ldflags)
        ac_define(have_boost,,[define if the boost library is available])
        # execute action-if-found (if present):
        ifelse([$2], , :, [$2])
    fi

    cppflags="$cppflags_saved"
    ldflags="$ldflags_saved"
fi

])
