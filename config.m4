dnl config.m4 - Build configuration for the voyager PHP extension

PHP_ARG_ENABLE([voyager],
  [whether to enable voyager support],
  [AS_HELP_STRING([--enable-voyager],
    [Enable voyager runtime debug support])],
  [no])

if test "$PHP_VOYAGER" != "no"; then

  AC_DEFINE(HAVE_VOYAGER, 1, [Have voyager support])
  PHP_NEW_EXTENSION(voyager, voyager.c voyager_functions.c voyager_methods.c voyager_common.c, $ext_shared)

fi
