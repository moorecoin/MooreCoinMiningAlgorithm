# ===========================================================================
#    http://www.gnu.org/software/autoconf-archive/ax_boost_filesystem.html
# ===========================================================================
#
# synopsis
#
#   ax_boost_filesystem
#
# description
#
#   test for filesystem library from the boost c++ libraries. the macro
#   requires a preceding call to ax_boost_base. further documentation is
#   available at <http://randspringer.de/boost/index.html>.
#
#   this macro calls:
#
#     ac_subst(boost_filesystem_lib)
#
#   and sets:
#
#     have_boost_filesystem
#
# license
#
#   copyright (c) 2009 thomas porschberg <thomas@randspringer.de>
#   copyright (c) 2009 michael tindal
#   copyright (c) 2009 roman rybalko <libtorrent@romanr.info>
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. this file is offered as-is, without any
#   warranty.

#serial 26

ac_defun([ax_boost_filesystem],
[
	ac_arg_with([boost-filesystem],
	as_help_string([--with-boost-filesystem@<:@=special-lib@:>@],
                   [use the filesystem library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-filesystem=boost_filesystem-gcc-mt ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_filesystem_lib=""
        else
		    want_boost="yes"
		ax_boost_user_filesystem_lib="$withval"
		fi
        ],
        [want_boost="yes"]
	)

	if test "x$want_boost" = "xyes"; then
        ac_require([ac_prog_cc])
		cppflags_saved="$cppflags"
		cppflags="$cppflags $boost_cppflags"
		export cppflags

		ldflags_saved="$ldflags"
		ldflags="$ldflags $boost_ldflags"
		export ldflags

		libs_saved=$libs
		libs="$libs $boost_system_lib"
		export libs

        ac_cache_check(whether the boost::filesystem library is available,
					   ax_cv_boost_filesystem,
        [ac_lang_push([c++])
         ac_compile_ifelse([ac_lang_program([[@%:@include <boost/filesystem/path.hpp>]],
                                   [[using namespace boost::filesystem;
                                   path my_path( "foo/bar/data.txt" );
                                   return 0;]])],
					       ax_cv_boost_filesystem=yes, ax_cv_boost_filesystem=no)
         ac_lang_pop([c++])
		])
		if test "x$ax_cv_boost_filesystem" = "xyes"; then
			ac_define(have_boost_filesystem,,[define if the boost::filesystem library is available])
            boostlibdir=`echo $boost_ldflags | sed -e 's/@<:@^\/@:>@*//'`
            ax_lib=
            if test "x$ax_boost_user_filesystem_lib" = "x"; then
                for libextension in `ls -r $boostlibdir/libboost_filesystem* 2>/dev/null | sed 's,.*/lib,,' | sed 's,\..*,,'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_filesystem_lib="-l$ax_lib"; ac_subst(boost_filesystem_lib) link_filesystem="yes"; break],
                                 [link_filesystem="no"])
				done
                if test "x$link_filesystem" != "xyes"; then
                for libextension in `ls -r $boostlibdir/boost_filesystem* 2>/dev/null | sed 's,.*/,,' | sed -e 's,\..*,,'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_filesystem_lib="-l$ax_lib"; ac_subst(boost_filesystem_lib) link_filesystem="yes"; break],
                                 [link_filesystem="no"])
				done
		    fi
            else
               for ax_lib in $ax_boost_user_filesystem_lib boost_filesystem-$ax_boost_user_filesystem_lib; do
				      ac_check_lib($ax_lib, exit,
                                   [boost_filesystem_lib="-l$ax_lib"; ac_subst(boost_filesystem_lib) link_filesystem="yes"; break],
                                   [link_filesystem="no"])
                  done

            fi
            if test "x$ax_lib" = "x"; then
                ac_msg_error(could not find a version of the boost_filesystem library!)
            fi
			if test "x$link_filesystem" != "xyes"; then
				ac_msg_error(could not link against $ax_lib !)
			fi
		fi

		cppflags="$cppflags_saved"
		ldflags="$ldflags_saved"
		libs="$libs_saved"
	fi
])
