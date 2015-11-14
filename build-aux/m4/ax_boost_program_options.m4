# ============================================================================
#  http://www.gnu.org/software/autoconf-archive/ax_boost_program_options.html
# ============================================================================
#
# synopsis
#
#   ax_boost_program_options
#
# description
#
#   test for program options library from the boost c++ libraries. the macro
#   requires a preceding call to ax_boost_base. further documentation is
#   available at <http://randspringer.de/boost/index.html>.
#
#   this macro calls:
#
#     ac_subst(boost_program_options_lib)
#
#   and sets:
#
#     have_boost_program_options
#
# license
#
#   copyright (c) 2009 thomas porschberg <thomas@randspringer.de>
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. this file is offered as-is, without any
#   warranty.

#serial 22

ac_defun([ax_boost_program_options],
[
	ac_arg_with([boost-program-options],
		as_help_string([--with-boost-program-options@<:@=special-lib@:>@],
                       [use the program options library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-program-options=boost_program_options-gcc-mt-1_33_1 ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_program_options_lib=""
        else
		    want_boost="yes"
		ax_boost_user_program_options_lib="$withval"
		fi
        ],
        [want_boost="yes"]
	)

	if test "x$want_boost" = "xyes"; then
        ac_require([ac_prog_cc])
	    export want_boost
		cppflags_saved="$cppflags"
		cppflags="$cppflags $boost_cppflags"
		export cppflags
		ldflags_saved="$ldflags"
		ldflags="$ldflags $boost_ldflags"
		export ldflags
		ac_cache_check([whether the boost::program_options library is available],
					   ax_cv_boost_program_options,
					   [ac_lang_push(c++)
				ac_compile_ifelse([ac_lang_program([[@%:@include <boost/program_options.hpp>
                                                          ]],
                                  [[boost::program_options::options_description generic("generic options");
                                   return 0;]])],
                           ax_cv_boost_program_options=yes, ax_cv_boost_program_options=no)
					ac_lang_pop([c++])
		])
		if test "$ax_cv_boost_program_options" = yes; then
				ac_define(have_boost_program_options,,[define if the boost::program_options library is available])
                  boostlibdir=`echo $boost_ldflags | sed -e 's/@<:@^\/@:>@*//'`
                if test "x$ax_boost_user_program_options_lib" = "x"; then
                ax_lib=
                for libextension in `ls $boostlibdir/libboost_program_options*.so* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^lib\(boost_program_options.*\)\.so.*$;\1;'` `ls $boostlibdir/libboost_program_options*.dylib* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^lib\(boost_program_options.*\)\.dylib.*$;\1;'` `ls $boostlibdir/libboost_program_options*.a* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^lib\(boost_program_options.*\)\.a.*$;\1;'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_program_options_lib="-l$ax_lib"; ac_subst(boost_program_options_lib) link_program_options="yes"; break],
                                 [link_program_options="no"])
				done
                if test "x$link_program_options" != "xyes"; then
                for libextension in `ls $boostlibdir/boost_program_options*.dll* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^\(boost_program_options.*\)\.dll.*$;\1;'` `ls $boostlibdir/boost_program_options*.a* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^\(boost_program_options.*\)\.a.*$;\1;'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_program_options_lib="-l$ax_lib"; ac_subst(boost_program_options_lib) link_program_options="yes"; break],
                                 [link_program_options="no"])
				done
                fi
                else
                  for ax_lib in $ax_boost_user_program_options_lib boost_program_options-$ax_boost_user_program_options_lib; do
				      ac_check_lib($ax_lib, main,
                                   [boost_program_options_lib="-l$ax_lib"; ac_subst(boost_program_options_lib) link_program_options="yes"; break],
                                   [link_program_options="no"])
                  done
                fi
            if test "x$ax_lib" = "x"; then
                ac_msg_error(could not find a version of the boost_program_options library!)
            fi
				if test "x$link_program_options" != "xyes"; then
					ac_msg_error([could not link against [$ax_lib] !])
				fi
		fi
		cppflags="$cppflags_saved"
	ldflags="$ldflags_saved"
	fi
])
