#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT
X_AC_META
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/ior.c])
AC_CONFIG_HEADER([src/config.h])

AC_CANONICAL_HOST

# Automake support
AM_INIT_AUTOMAKE([$META_NAME], [$META_VERSION], [check-news dist-bzip2 gnu no-define])
AM_MAINTAINER_MODE

# Checks for programs.
AX_PROG_CC_MPI

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h libintl.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/statfs.h sys/statvfs.h sys/time.h unistd.h wchar.h gpfs.h gpfs_fcntl.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([getpagesize gettimeofday memset mkdir pow putenv realpath regcomp sqrt strcasecmp strchr strerror strncasecmp strstr uname])
AC_SEARCH_LIBS([sqrt], [m], [],
        [AC_MSG_ERROR([Math library not found])])

# Check for system capabilities
AC_SYS_LARGEFILE

# Check for lustre availability
AC_ARG_WITH([lustre],
        [AS_HELP_STRING([--with-lustre],
                [support configurable Lustre striping values @<:@default=check@:>@])],
        [], [with_lustre=check])
AS_IF([test "x$with_lustre" != xno], [
        AC_CHECK_HEADERS([lustre/lustre_user.h], [], [
                if test "x$with_lustre" != xcheck; then
                        AC_MSG_FAILURE([--with-lustre was given, <lustre/lustre_user.h> not found])
                fi
        ])
])

# POSIX IO support
AC_ARG_WITH([posix],
        [AS_HELP_STRING([--with-posix],
           [support IO with POSIX backend @<:@default=yes@:>@])],
        [],
        [with_posix=yes])
AM_CONDITIONAL([USE_POSIX_AIORI], [test x$with_posix = xyes])
AM_COND_IF([USE_POSIX_AIORI],[
        AC_DEFINE([USE_POSIX_AIORI], [], [Build POSIX backend AIORI])
])

# MPIIO support
AC_ARG_WITH([mpiio],
        [AS_HELP_STRING([--with-mpiio],
           [support IO with MPI-IO backend @<:@default=yes@:>@])],
        [],
        [with_mpiio=yes])
AM_CONDITIONAL([USE_MPIIO_AIORI], [test x$with_mpiio = xyes])
AM_COND_IF([USE_MPIIO_AIORI],[
        AC_DEFINE([USE_MPIIO_AIORI], [], [Build MPIIO backend AIORI])
])

# HDF5 support
AC_ARG_WITH([hdf5],
        [AS_HELP_STRING([--with-hdf5],
           [support IO with HDF5 backend @<:@default=no@:>@])],
        [],
        [with_hdf5=no])
AM_CONDITIONAL([USE_HDF5_AIORI], [test x$with_hdf5 = xyes])
AM_COND_IF([USE_HDF5_AIORI],[
        AC_DEFINE([USE_HDF5_AIORI], [], [Build HDF5 backend AIORI])
])

# NCMPI (Parallel netcdf) support
AC_ARG_WITH([ncmpi],
        [AS_HELP_STRING([--with-ncmpi],
           [support IO with NCMPI backend @<:@default=no@:>@])],
        [],
        [with_ncmpi=no])
AM_CONDITIONAL([USE_NCMPI_AIORI], [test x$with_ncmpi = xyes])
AM_COND_IF([USE_NCMPI_AIORI],[
        AC_DEFINE([USE_NCMPI_AIORI], [], [Build NCMPI backend AIORI])
])

# Enable building "IOR", in all capitals
AC_ARG_ENABLE([caps],
        [AS_HELP_STRING([--enable-caps],
          [build "IOR" binary (name is in all-caps) @<:@default=no@:>@])],
        [], [enable_caps=no])
AM_CONDITIONAL([USE_CAPS], [test x$enable_caps = xyes])


AC_CONFIG_FILES([Makefile
                 src/Makefile
                 contrib/Makefile
                 doc/Makefile])
AC_OUTPUT
