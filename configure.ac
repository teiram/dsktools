#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dsktools], [0.3],[manuel.teira@gmail.com])
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
LT_INIT
AC_CONFIG_SRCDIR([src/dskmgmt.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/ioctl.h sys/param.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([atexit localtime_r memset strdup strerror strstr strtol])

AM_CONDITIONAL([LINUX], false)
case "$host" in
  *-linux-*)
  AM_CONDITIONAL([LINUX], true)
  ;;
  *)
  AC_MSG_NOTICE(["No supported fd driver on: $host"])
  ;;
esac

PKG_CHECK_MODULES(GTK, gtk+-3.0 gmodule-2.0,
		       AM_CONDITIONAL([ENABLE_GUI], [true]),
		       AM_CONDITIONAL([ENABLE_GUI], [false]))

AM_COND_IF([ENABLE_GUI], AC_CONFIG_FILES([src/gui/Makefile]))		       		      
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
