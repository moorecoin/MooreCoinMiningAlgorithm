# ===========================================================================
#        http://www.gnu.org/software/autoconf-archive/ax_pthread.html
# ===========================================================================
#
# synopsis
#
#   ax_pthread([action-if-found[, action-if-not-found]])
#
# description
#
#   this macro figures out how to build c programs using posix threads. it
#   sets the pthread_libs output variable to the threads library and linker
#   flags, and the pthread_cflags output variable to any special c compiler
#   flags that are needed. (the user can also force certain compiler
#   flags/libs to be tested by setting these environment variables.)
#
#   also sets pthread_cc to any special c compiler that is needed for
#   multi-threaded programs (defaults to the value of cc otherwise). (this
#   is necessary on aix to use the special cc_r compiler alias.)
#
#   note: you are assumed to not only compile your program with these flags,
#   but also link it with them as well. e.g. you should link with
#   $pthread_cc $cflags $pthread_cflags $ldflags ... $pthread_libs $libs
#
#   if you are only building threads programs, you may wish to use these
#   variables in your default libs, cflags, and cc:
#
#     libs="$pthread_libs $libs"
#     cflags="$cflags $pthread_cflags"
#     cc="$pthread_cc"
#
#   in addition, if the pthread_create_joinable thread-attribute constant
#   has a nonstandard name, defines pthread_create_joinable to that name
#   (e.g. pthread_create_undetached on aix).
#
#   also have_pthread_prio_inherit is defined if pthread is found and the
#   pthread_prio_inherit symbol is defined when compiling with
#   pthread_cflags.
#
#   action-if-found is a list of shell commands to run if a threads library
#   is found, and action-if-not-found is a list of commands to run it if it
#   is not found. if action-if-found is not specified, the default action
#   will define have_pthread.
#
#   please let the authors know if this macro fails on any platform, or if
#   you have any other suggestions or comments. this macro was based on work
#   by sgj on autoconf scripts for fftw (http://www.fftw.org/) (with help
#   from m. frigo), as well as ac_pthread and hb_pthread macros posted by
#   alejandro forero cuervo to the autoconf macro repository. we are also
#   grateful for the helpful feedback of numerous users.
#
#   updated for autoconf 2.68 by daniel richard g.
#
# license
#
#   copyright (c) 2008 steven g. johnson <stevenj@alum.mit.edu>
#   copyright (c) 2011 daniel richard g. <skunk@iskunk.org>
#
#   this program is free software: you can redistribute it and/or modify it
#   under the terms of the gnu general public license as published by the
#   free software foundation, either version 3 of the license, or (at your
#   option) any later version.
#
#   this program is distributed in the hope that it will be useful, but
#   without any warranty; without even the implied warranty of
#   merchantability or fitness for a particular purpose. see the gnu general
#   public license for more details.
#
#   you should have received a copy of the gnu general public license along
#   with this program. if not, see <http://www.gnu.org/licenses/>.
#
#   as a special exception, the respective autoconf macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of autoconf when processing the macro. you
#   need not follow the terms of the gnu general public license when using
#   or distributing such scripts, even though portions of the text of the
#   macro appear in them. the gnu general public license (gpl) does govern
#   all other use of the material that constitutes the autoconf macro.
#
#   this special exception to the gpl applies to versions of the autoconf
#   macro released by the autoconf archive. when you make and distribute a
#   modified version of the autoconf macro, you may extend this special
#   exception to the gpl to apply to your modified version as well.

#serial 21

au_alias([acx_pthread], [ax_pthread])
ac_defun([ax_pthread], [
ac_require([ac_canonical_host])
ac_lang_push([c])
ax_pthread_ok=no

# we used to check for pthread.h first, but this fails if pthread.h
# requires special compiler flags (e.g. on true64 or sequent).
# it gets checked for in the link test anyway.

# first of all, check if the user has set any of the pthread_libs,
# etcetera environment variables, and if threads linking works using
# them:
if test x"$pthread_libs$pthread_cflags" != x; then
        save_cflags="$cflags"
        cflags="$cflags $pthread_cflags"
        save_libs="$libs"
        libs="$pthread_libs $libs"
        ac_msg_checking([for pthread_join in libs=$pthread_libs with cflags=$pthread_cflags])
        ac_try_link_func([pthread_join], [ax_pthread_ok=yes])
        ac_msg_result([$ax_pthread_ok])
        if test x"$ax_pthread_ok" = xno; then
                pthread_libs=""
                pthread_cflags=""
        fi
        libs="$save_libs"
        cflags="$save_cflags"
fi

# we must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. dec) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-posix).

# create a list of thread flags to try.  items starting with a "-" are
# c compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all, and "pthread-config"
# which is a program returning the flags for the pth emulation library.

ax_pthread_flags="pthreads none -kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt pthread-config"

# the ordering *is* (sometimes) important.  some notes on the
# individual items follow:

# pthreads: aix (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -kthread and
#       other compiler flags to prevent continual compiler warnings
# -kthread: sequent (threads in libc, but -kthread needed for pthread.h)
# -kthread: freebsd kernel threads (preferred to -pthread since smp-able)
# lthread: linuxthreads port on freebsd (also preferred to -pthread)
# -pthread: linux/gcc (kernel threads), bsd/gcc (userland threads)
# -pthreads: solaris/gcc
# -mthreads: mingw32/gcc, lynx/gcc
# -mt: sun workshop c (may only link sunos threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -d_reentrant)
#      ... -mt is also the pthreads flag for hp/acc
# pthread: linux, etcetera
# --thread-safe: kai c++
# pthread-config: use pthread-config program (for gnu pth library)

case ${host_os} in
        solaris*)

        # on solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (we need to link with -pthreads/-mt/
        # -lpthread.)  (the stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  so,
        # we'll just look for -pthreads and -lpthread first:

        ax_pthread_flags="-pthreads pthread -mt -pthread $ax_pthread_flags"
        ;;

        darwin*)
        ax_pthread_flags="-pthread $ax_pthread_flags"
        ;;
esac

# clang doesn't consider unrecognized options an error unless we specify
# -werror. we throw in some extra clang-specific options to ensure that
# this doesn't happen for gcc, which also accepts -werror.

ac_msg_checking([if compiler needs -werror to reject unknown flags])
save_cflags="$cflags"
ax_pthread_extra_flags="-werror"
cflags="$cflags $ax_pthread_extra_flags -wunknown-warning-option -wsizeof-array-argument"
ac_compile_ifelse([ac_lang_program([int foo(void);],[foo()])],
                  [ac_msg_result([yes])],
                  [ax_pthread_extra_flags=
                   ac_msg_result([no])])
cflags="$save_cflags"

if test x"$ax_pthread_ok" = xno; then
for flag in $ax_pthread_flags; do

        case $flag in
                none)
                ac_msg_checking([whether pthreads work without any flags])
                ;;

                -*)
                ac_msg_checking([whether pthreads work with $flag])
                pthread_cflags="$flag"
                ;;

                pthread-config)
                ac_check_prog([ax_pthread_config], [pthread-config], [yes], [no])
                if test x"$ax_pthread_config" = xno; then continue; fi
                pthread_cflags="`pthread-config --cflags`"
                pthread_libs="`pthread-config --ldflags` `pthread-config --libs`"
                ;;

                *)
                ac_msg_checking([for the pthreads library -l$flag])
                pthread_libs="-l$flag"
                ;;
        esac

        save_libs="$libs"
        save_cflags="$cflags"
        libs="$pthread_libs $libs"
        cflags="$cflags $pthread_cflags $ax_pthread_extra_flags"

        # check for various functions.  we must include pthread.h,
        # since some functions may be macros.  (on the sequent, we
        # need a special flag -kthread to make this header compile.)
        # we check for pthread_join because it is in -lpthread on irix
        # while pthread_create is in libc.  we check for pthread_attr_init
        # due to dec craziness with -lpthreads.  we check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on solaris that doesn't have a non-functional libc stub.
        # we try pthread_create on general principles.
        ac_link_ifelse([ac_lang_program([#include <pthread.h>
                        static void routine(void *a) { a = 0; }
                        static void *start_routine(void *a) { return a; }],
                       [pthread_t th; pthread_attr_t attr;
                        pthread_create(&th, 0, start_routine, 0);
                        pthread_join(th, 0);
                        pthread_attr_init(&attr);
                        pthread_cleanup_push(routine, 0);
                        pthread_cleanup_pop(0) /* ; */])],
                [ax_pthread_ok=yes],
                [])

        libs="$save_libs"
        cflags="$save_cflags"

        ac_msg_result([$ax_pthread_ok])
        if test "x$ax_pthread_ok" = xyes; then
                break;
        fi

        pthread_libs=""
        pthread_cflags=""
done
fi

# various other checks:
if test "x$ax_pthread_ok" = xyes; then
        save_libs="$libs"
        libs="$pthread_libs $libs"
        save_cflags="$cflags"
        cflags="$cflags $pthread_cflags"

        # detect aix lossage: joinable attribute is called undetached.
        ac_msg_checking([for joinable pthread attribute])
        attr_name=unknown
        for attr in pthread_create_joinable pthread_create_undetached; do
            ac_link_ifelse([ac_lang_program([#include <pthread.h>],
                           [int attr = $attr; return attr /* ; */])],
                [attr_name=$attr; break],
                [])
        done
        ac_msg_result([$attr_name])
        if test "$attr_name" != pthread_create_joinable; then
            ac_define_unquoted([pthread_create_joinable], [$attr_name],
                               [define to necessary symbol if this constant
                                uses a non-standard name on your system.])
        fi

        ac_msg_checking([if more special flags are required for pthreads])
        flag=no
        case ${host_os} in
            aix* | freebsd* | darwin*) flag="-d_thread_safe";;
            osf* | hpux*) flag="-d_reentrant";;
            solaris*)
            if test "$gcc" = "yes"; then
                flag="-d_reentrant"
            else
                # todo: what about clang on solaris?
                flag="-mt -d_reentrant"
            fi
            ;;
        esac
        ac_msg_result([$flag])
        if test "x$flag" != xno; then
            pthread_cflags="$flag $pthread_cflags"
        fi

        ac_cache_check([for pthread_prio_inherit],
            [ax_cv_pthread_prio_inherit], [
                ac_link_ifelse([ac_lang_program([[#include <pthread.h>]],
                                                [[int i = pthread_prio_inherit;]])],
                    [ax_cv_pthread_prio_inherit=yes],
                    [ax_cv_pthread_prio_inherit=no])
            ])
        as_if([test "x$ax_cv_pthread_prio_inherit" = "xyes"],
            [ac_define([have_pthread_prio_inherit], [1], [have pthread_prio_inherit.])])

        libs="$save_libs"
        cflags="$save_cflags"

        # more aix lossage: compile with *_r variant
        if test "x$gcc" != xyes; then
            case $host_os in
                aix*)
                as_case(["x/$cc"],
                  [x*/c89|x*/c89_128|x*/c99|x*/c99_128|x*/cc|x*/cc128|x*/xlc|x*/xlc_v6|x*/xlc128|x*/xlc128_v6],
                  [#handle absolute path differently from path based program lookup
                   as_case(["x$cc"],
                     [x/*],
                     [as_if([as_executable_p([${cc}_r])],[pthread_cc="${cc}_r"])],
                     [ac_check_progs([pthread_cc],[${cc}_r],[$cc])])])
                ;;
            esac
        fi
fi

test -n "$pthread_cc" || pthread_cc="$cc"

ac_subst([pthread_libs])
ac_subst([pthread_cflags])
ac_subst([pthread_cc])

# finally, execute action-if-found/action-if-not-found:
if test x"$ax_pthread_ok" = xyes; then
        ifelse([$1],,[ac_define([have_pthread],[1],[define if you have posix threads libraries and header files.])],[$1])
        :
else
        ax_pthread_ok=no
        $2
fi
ac_lang_pop
])dnl ax_pthread
