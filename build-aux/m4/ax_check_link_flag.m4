# ===========================================================================
#    http://www.gnu.org/software/autoconf-archive/ax_check_link_flag.html
# ===========================================================================
#
# synopsis
#
#   ax_check_link_flag(flag, [action-success], [action-failure], [extra-flags])
#
# description
#
#   check whether the given flag works with the linker or gives an error.
#   (warnings, however, are ignored)
#
#   action-success/action-failure are shell commands to execute on
#   success/failure.
#
#   if extra-flags is defined, it is added to the linker's default flags
#   when the check is done.  the check is thus made with the flags: "ldflags
#   extra-flags flag".  this can for example be used to force the linker to
#   issue an error when a bad flag is given.
#
#   note: implementation based on ax_cflags_gcc_option. please keep this
#   macro in sync with ax_check_{preproc,compile}_flag.
#
# license
#
#   copyright (c) 2008 guido u. draheim <guidod@gmx.de>
#   copyright (c) 2011 maarten bosmans <mkbosmans@gmail.com>
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

#serial 2

ac_defun([ax_check_link_flag],
[as_var_pushdef([cachevar],[ax_cv_check_ldflags_$4_$1])dnl
ac_cache_check([whether the linker accepts $1], cachevar, [
  ax_check_save_flags=$ldflags
  ldflags="$ldflags $4 $1"
  ac_link_ifelse([ac_lang_program()],
    [as_var_set(cachevar,[yes])],
    [as_var_set(cachevar,[no])])
  ldflags=$ax_check_save_flags])
as_if([test x"as_var_get(cachevar)" = xyes],
  [m4_default([$2], :)],
  [m4_default([$3], :)])
as_var_popdef([cachevar])dnl
])dnl ax_check_link_flags
