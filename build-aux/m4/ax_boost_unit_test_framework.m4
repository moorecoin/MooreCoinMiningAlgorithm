# ================================================================================
#  http://www.gnu.org/software/autoconf-archive/ax_boost_unit_test_framework.html
# ================================================================================
#
# synopsis
#
#   ax_boost_unit_test_framework
#
# description
#
#   test for unit_test_framework library from the boost c++ libraries. the
#   macro requires a preceding call to ax_boost_base. further documentation
#   is available at <http://randspringer.de/boost/index.html>.
#
#   this macro calls:
#
#     ac_subst(boost_unit_test_framework_lib)
#
#   and sets:
#
#     have_boost_unit_test_framework
#
# license
#
#   copyright (c) 2008 thomas porschberg <thomas@randspringer.de>
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. this file is offered as-is, without any
#   warranty.

#serial 19

ac_defun([ax_boost_unit_test_framework],
[
	ac_arg_with([boost-unit-test-framework],
	as_help_string([--with-boost-unit-test-framework@<:@=special-lib@:>@],
                   [use the unit_test_framework library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-unit-test-framework=boost_unit_test_framework-gcc ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_unit_test_framework_lib=""
        else
		    want_boost="yes"
		ax_boost_user_unit_test_framework_lib="$withval"
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

        ac_cache_check(whether the boost::unit_test_framework library is available,
					   ax_cv_boost_unit_test_framework,
        [ac_lang_push([c++])
			 ac_compile_ifelse([ac_lang_program([[@%:@include <boost/test/unit_test.hpp>]],
                                    [[using boost::unit_test::test_suite;
							 test_suite* test= boost_test_suite( "unit test example 1" ); return 0;]])],
                   ax_cv_boost_unit_test_framework=yes, ax_cv_boost_unit_test_framework=no)
         ac_lang_pop([c++])
		])
		if test "x$ax_cv_boost_unit_test_framework" = "xyes"; then
			ac_define(have_boost_unit_test_framework,,[define if the boost::unit_test_framework library is available])
            boostlibdir=`echo $boost_ldflags | sed -e 's/@<:@^\/@:>@*//'`

            if test "x$ax_boost_user_unit_test_framework_lib" = "x"; then
			saved_ldflags="${ldflags}"
                ax_lib=
                for monitor_library in `ls $boostlibdir/libboost_unit_test_framework*.so* $boostlibdir/libboost_unit_test_framework*.dylib* $boostlibdir/libboost_unit_test_framework*.a* 2>/dev/null` ; do
                    if test -r $monitor_library ; then
                       libextension=`echo $monitor_library | sed 's,.*/,,' | sed -e 's;^lib\(boost_unit_test_framework.*\)\.so.*$;\1;' -e 's;^lib\(boost_unit_test_framework.*\)\.dylib.*$;\1;' -e 's;^lib\(boost_unit_test_framework.*\)\.a.*$;\1;'`
                       ax_lib=${libextension}
                       link_unit_test_framework="yes"
                    else
                       link_unit_test_framework="no"
                    fi

			    if test "x$link_unit_test_framework" = "xyes"; then
                      boost_unit_test_framework_lib="-l$ax_lib"
                      ac_subst(boost_unit_test_framework_lib)
					  break
				    fi
                done
                if test "x$link_unit_test_framework" != "xyes"; then
                for libextension in `ls $boostlibdir/boost_unit_test_framework*.dll* $boostlibdir/boost_unit_test_framework*.a* 2>/dev/null  | sed 's,.*/,,' | sed -e 's;^\(boost_unit_test_framework.*\)\.dll.*$;\1;' -e 's;^\(boost_unit_test_framework.*\)\.a.*$;\1;'` ; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_unit_test_framework_lib="-l$ax_lib"; ac_subst(boost_unit_test_framework_lib) link_unit_test_framework="yes"; break],
                                 [link_unit_test_framework="no"])
				done
                fi
            else
                link_unit_test_framework="no"
			saved_ldflags="${ldflags}"
                for ax_lib in boost_unit_test_framework-$ax_boost_user_unit_test_framework_lib $ax_boost_user_unit_test_framework_lib ; do
                   if test "x$link_unit_test_framework" = "xyes"; then
                      break;
                   fi
                   for unittest_library in `ls $boostlibdir/lib${ax_lib}.so* $boostlibdir/lib${ax_lib}.a* 2>/dev/null` ; do
                   if test -r $unittest_library ; then
                       libextension=`echo $unittest_library | sed 's,.*/,,' | sed -e 's;^lib\(boost_unit_test_framework.*\)\.so.*$;\1;' -e 's;^lib\(boost_unit_test_framework.*\)\.a*$;\1;'`
                       ax_lib=${libextension}
                       link_unit_test_framework="yes"
                    else
                       link_unit_test_framework="no"
                    fi

				if test "x$link_unit_test_framework" = "xyes"; then
                        boost_unit_test_framework_lib="-l$ax_lib"
                        ac_subst(boost_unit_test_framework_lib)
					    break
				    fi
                  done
               done
            fi
            if test "x$ax_lib" = "x"; then
                ac_msg_error(could not find a version of the boost_unit_test_framework library!)
            fi
			if test "x$link_unit_test_framework" != "xyes"; then
				ac_msg_error(could not link against $ax_lib !)
			fi
		fi

		cppflags="$cppflags_saved"
	ldflags="$ldflags_saved"
	fi
])
