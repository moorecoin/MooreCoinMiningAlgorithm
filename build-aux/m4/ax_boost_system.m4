# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_boost_system.html
# ===========================================================================
#
# synopsis
#
#   ax_boost_system
#
# description
#
#   test for system library from the boost c++ libraries. the macro requires
#   a preceding call to ax_boost_base. further documentation is available at
#   <http://randspringer.de/boost/index.html>.
#
#   this macro calls:
#
#     ac_subst(boost_system_lib)
#
#   and sets:
#
#     have_boost_system
#
# license
#
#   copyright (c) 2008 thomas porschberg <thomas@randspringer.de>
#   copyright (c) 2008 michael tindal
#   copyright (c) 2008 daniel casimiro <dan.casimiro@gmail.com>
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. this file is offered as-is, without any
#   warranty.

#serial 17

ac_defun([ax_boost_system],
[
	ac_arg_with([boost-system],
	as_help_string([--with-boost-system@<:@=special-lib@:>@],
                   [use the system library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-system=boost_system-gcc-mt ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_system_lib=""
        else
		    want_boost="yes"
		ax_boost_user_system_lib="$withval"
		fi
        ],
        [want_boost="yes"]
	)

	if test "x$want_boost" = "xyes"; then
        ac_require([ac_prog_cc])
        ac_require([ac_canonical_build])
		cppflags_saved="$cppflags"
		cppflags="$cppflags $boost_cppflags"
		export cppflags

		ldflags_saved="$ldflags"
		ldflags="$ldflags $boost_ldflags"
		export ldflags

        ac_cache_check(whether the boost::system library is available,
					   ax_cv_boost_system,
        [ac_lang_push([c++])
			 cxxflags_save=$cxxflags

			 ac_compile_ifelse([ac_lang_program([[@%:@include <boost/system/error_code.hpp>]],
                                   [[boost::system::system_category]])],
                   ax_cv_boost_system=yes, ax_cv_boost_system=no)
			 cxxflags=$cxxflags_save
             ac_lang_pop([c++])
		])
		if test "x$ax_cv_boost_system" = "xyes"; then
			ac_subst(boost_cppflags)

			ac_define(have_boost_system,,[define if the boost::system library is available])
            boostlibdir=`echo $boost_ldflags | sed -e 's/@<:@^\/@:>@*//'`

			ldflags_save=$ldflags
            if test "x$ax_boost_user_system_lib" = "x"; then
                ax_lib=
                for libextension in `ls -r $boostlibdir/libboost_system* 2>/dev/null | sed 's,.*/lib,,' | sed 's,\..*,,'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_system_lib="-l$ax_lib"; ac_subst(boost_system_lib) link_system="yes"; break],
                                 [link_system="no"])
				done
                if test "x$link_system" != "xyes"; then
                for libextension in `ls -r $boostlibdir/boost_system* 2>/dev/null | sed 's,.*/,,' | sed -e 's,\..*,,'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_system_lib="-l$ax_lib"; ac_subst(boost_system_lib) link_system="yes"; break],
                                 [link_system="no"])
				done
                fi

            else
               for ax_lib in $ax_boost_user_system_lib boost_system-$ax_boost_user_system_lib; do
				      ac_check_lib($ax_lib, exit,
                                   [boost_system_lib="-l$ax_lib"; ac_subst(boost_system_lib) link_system="yes"; break],
                                   [link_system="no"])
                  done

            fi
            if test "x$ax_lib" = "x"; then
                ac_msg_error(could not find a version of the boost_system library!)
            fi
			if test "x$link_system" = "xno"; then
				ac_msg_error(could not link against $ax_lib !)
			fi
		fi

		cppflags="$cppflags_saved"
	ldflags="$ldflags_saved"
	fi
])
