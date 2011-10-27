#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/IOR.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

# Automake support
AM_INIT_AUTOMAKE([$PACKAGE], [$VERSION], [check-news dist-bzip2 gnu no-define])
AM_MAINTAINER_MODE

# Checks for programs.
AX_PROG_CC_MPI

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h libintl.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/statfs.h sys/statvfs.h sys/time.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([getpagesize gettimeofday memset mkdir pow putenv realpath regcomp sqrt strcasecmp strchr strerror strncasecmp strstr uname])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/cbif/Makefile])
AC_OUTPUT
