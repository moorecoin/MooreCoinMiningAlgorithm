# ===========================================================================
#   http://www.gnu.org/software/autoconf-archive/ax_gcc_func_attribute.html
# ===========================================================================
#
# synopsis
#
#   ax_gcc_func_attribute(attribute)
#
# description
#
#   this macro checks if the compiler supports one of gcc's function
#   attributes; many other compilers also provide function attributes with
#   the same syntax. compiler warnings are used to detect supported
#   attributes as unsupported ones are ignored by default so quieting
#   warnings when using this macro will yield false positives.
#
#   the attribute parameter holds the name of the attribute to be checked.
#
#   if attribute is supported define have_func_attribute_<attribute>.
#
#   the macro caches its result in the ax_cv_have_func_attribute_<attribute>
#   variable.
#
#   the macro currently supports the following function attributes:
#
#    alias
#    aligned
#    alloc_size
#    always_inline
#    artificial
#    cold
#    const
#    constructor
#    deprecated
#    destructor
#    dllexport
#    dllimport
#    error
#    externally_visible
#    flatten
#    format
#    format_arg
#    gnu_inline
#    hot
#    ifunc
#    leaf
#    malloc
#    noclone
#    noinline
#    nonnull
#    noreturn
#    nothrow
#    optimize
#    pure
#    unused
#    used
#    visibility
#    warning
#    warn_unused_result
#    weak
#    weakref
#
#   unsuppored function attributes will be tested with a prototype returning
#   an int and not accepting any arguments and the result of the check might
#   be wrong or meaningless so use with care.
#
# license
#
#   copyright (c) 2013 gabriele svelto <gabriele.svelto@gmail.com>
#
#   copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  this file is offered as-is, without any
#   warranty.

#serial 2

ac_defun([ax_gcc_func_attribute], [
    as_var_pushdef([ac_var], [ax_cv_have_func_attribute_$1])

    ac_cache_check([for __attribute__(($1))], [ac_var], [
        ac_link_ifelse([ac_lang_program([
            m4_case([$1],
                [alias], [
                    int foo( void ) { return 0; }
                    int bar( void ) __attribute__(($1("foo")));
                ],
                [aligned], [
                    int foo( void ) __attribute__(($1(32)));
                ],
                [alloc_size], [
                    void *foo(int a) __attribute__(($1(1)));
                ],
                [always_inline], [
                    inline __attribute__(($1)) int foo( void ) { return 0; }
                ],
                [artificial], [
                    inline __attribute__(($1)) int foo( void ) { return 0; }
                ],
                [cold], [
                    int foo( void ) __attribute__(($1));
                ],
                [const], [
                    int foo( void ) __attribute__(($1));
                ],
                [constructor], [
                    int foo( void ) __attribute__(($1));
                ],
                [deprecated], [
                    int foo( void ) __attribute__(($1("")));
                ],
                [destructor], [
                    int foo( void ) __attribute__(($1));
                ],
                [dllexport], [
                    __attribute__(($1)) int foo( void ) { return 0; }
                ],
                [dllimport], [
                    int foo( void ) __attribute__(($1));
                ],
                [error], [
                    int foo( void ) __attribute__(($1("")));
                ],
                [externally_visible], [
                    int foo( void ) __attribute__(($1));
                ],
                [flatten], [
                    int foo( void ) __attribute__(($1));
                ],
                [format], [
                    int foo(const char *p, ...) __attribute__(($1(printf, 1, 2)));
                ],
                [format_arg], [
                    char *foo(const char *p) __attribute__(($1(1)));
                ],
                [gnu_inline], [
                    inline __attribute__(($1)) int foo( void ) { return 0; }
                ],
                [hot], [
                    int foo( void ) __attribute__(($1));
                ],
                [ifunc], [
                    int my_foo( void ) { return 0; }
                    static int (*resolve_foo(void))(void) { return my_foo; }
                    int foo( void ) __attribute__(($1("resolve_foo")));
                ],
                [leaf], [
                    __attribute__(($1)) int foo( void ) { return 0; }
                ],
                [malloc], [
                    void *foo( void ) __attribute__(($1));
                ],
                [noclone], [
                    int foo( void ) __attribute__(($1));
                ],
                [noinline], [
                    __attribute__(($1)) int foo( void ) { return 0; }
                ],
                [nonnull], [
                    int foo(char *p) __attribute__(($1(1)));
                ],
                [noreturn], [
                    void foo( void ) __attribute__(($1));
                ],
                [nothrow], [
                    int foo( void ) __attribute__(($1));
                ],
                [optimize], [
                    __attribute__(($1(3))) int foo( void ) { return 0; }
                ],
                [pure], [
                    int foo( void ) __attribute__(($1));
                ],
                [unused], [
                    int foo( void ) __attribute__(($1));
                ],
                [used], [
                    int foo( void ) __attribute__(($1));
                ],
                [visibility], [
                    int foo_def( void ) __attribute__(($1("default")));
                    int foo_hid( void ) __attribute__(($1("hidden")));
                ],
                [warning], [
                    int foo( void ) __attribute__(($1("")));
                ],
                [warn_unused_result], [
                    int foo( void ) __attribute__(($1));
                ],
                [weak], [
                    int foo( void ) __attribute__(($1));
                ],
                [weakref], [
                    static int foo( void ) { return 0; }
                    static int bar( void ) __attribute__(($1("foo")));
                ],
                [
                 m4_warn([syntax], [unsupported attribute $1, the test may fail])
                 int foo( void ) __attribute__(($1));
                ]
            )], [])
            ],
            dnl gcc doesn't exit with an error if an unknown attribute is
            dnl provided but only outputs a warning, so accept the attribute
            dnl only if no warning were issued.
            [as_if([test -s conftest.err],
                [as_var_set([ac_var], [no])],
                [as_var_set([ac_var], [yes])])],
            [as_var_set([ac_var], [no])])
    ])

    as_if([test yes = as_var_get([ac_var])],
        [ac_define_unquoted(as_tr_cpp(have_func_attribute_$1), 1,
            [define to 1 if the system has the `$1' function attribute])], [])

    as_var_popdef([ac_var])
])
