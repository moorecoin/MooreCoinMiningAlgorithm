# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_boost_thread.html
# ===========================================================================
#
# synopsis
#
#   ax_boost_thread
#
# description
#
#   test for thread library from the boost c++ libraries. the macro requires
#   a preceding call to ax_boost_base. further documentation is available at
#   <http://randspringer.de/boost/index.html>.
#
#   this macro calls:
#
#     ac_subst(boost_thread_lib)
#
#   and sets:
#
#     have_boost_thread
#
# license
#
#   copyright (c) 2009 thomas porschberg <thomas@randspringer.de>
#   copyright (c) 2009 michael tindal
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. this file is offered as-is, without any
#   warranty.

#serial 27

ac_defun([ax_boost_thread],
[
	ac_arg_with([boost-thread],
	as_help_string([--with-boost-thread@<:@=special-lib@:>@],
                   [use the thread library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-thread=boost_thread-gcc-mt ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_thread_lib=""
        else
		    want_boost="yes"
		ax_boost_user_thread_lib="$withval"
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

        ac_cache_check(whether the boost::thread library is available,
					   ax_cv_boost_thread,
        [ac_lang_push([c++])
			 cxxflags_save=$cxxflags

			 if test "x$host_os" = "xsolaris" ; then
				 cxxflags="-pthreads $cxxflags"
			 elif test "x$host_os" = "xmingw32" ; then
				 cxxflags="-mthreads $cxxflags"
			 else
				cxxflags="-pthread $cxxflags"
			 fi
			 ac_compile_ifelse([ac_lang_program([[@%:@include <boost/thread/thread.hpp>]],
                                   [[boost::thread_group thrds;
                                   return 0;]])],
                   ax_cv_boost_thread=yes, ax_cv_boost_thread=no)
			 cxxflags=$cxxflags_save
             ac_lang_pop([c++])
		])
		if test "x$ax_cv_boost_thread" = "xyes"; then
           if test "x$host_os" = "xsolaris" ; then
			  boost_cppflags="-pthreads $boost_cppflags"
		   elif test "x$host_os" = "xmingw32" ; then
			  boost_cppflags="-mthreads $boost_cppflags"
		   else
			  boost_cppflags="-pthread $boost_cppflags"
		   fi

			ac_subst(boost_cppflags)

			ac_define(have_boost_thread,,[define if the boost::thread library is available])
            boostlibdir=`echo $boost_ldflags | sed -e 's/@<:@^\/@:>@*//'`

			ldflags_save=$ldflags
                        case "x$host_os" in
                          *bsd* )
                               ldflags="-pthread $ldflags"
                          break;
                          ;;
                        esac
            if test "x$ax_boost_user_thread_lib" = "x"; then
                ax_lib=
                for libextension in `ls -r $boostlibdir/libboost_thread* 2>/dev/null | sed 's,.*/lib,,' | sed 's,\..*,,'`; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_thread_lib="-l$ax_lib"; ac_subst(boost_thread_lib) link_thread="yes"; break],
                                 [link_thread="no"])
				done
                if test "x$link_thread" != "xyes"; then
                for libextension in `ls -r $boostlibdir/boost_thread* 2>/dev/null | sed 's,.*/,,' | sed 's,\..*,,'`; do
                     ax_lib=${libextension}
				    ac_check_lib($ax_lib, exit,
                                 [boost_thread_lib="-l$ax_lib"; ac_subst(boost_thread_lib) link_thread="yes"; break],
                                 [link_thread="no"])
				done
                fi

            else
               for ax_lib in $ax_boost_user_thread_lib boost_thread-$ax_boost_user_thread_lib; do
				      ac_check_lib($ax_lib, exit,
                                   [boost_thread_lib="-l$ax_lib"; ac_subst(boost_thread_lib) link_thread="yes"; break],
                                   [link_thread="no"])
                  done

            fi
            if test "x$ax_lib" = "x"; then
                ac_msg_error(could not find a version of the boost_thread library!)
            fi
			if test "x$link_thread" = "xno"; then
				ac_msg_error(could not link against $ax_lib !)
                        else
                           case "x$host_os" in
                              *bsd* )
				boost_ldflags="-pthread $boost_ldflags"
                              break;
                              ;;
                           esac

			fi
		fi

		cppflags="$cppflags_saved"
	ldflags="$ldflags_saved"
	fi
])
