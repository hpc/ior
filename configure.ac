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
AC_CHECK_HEADERS([fcntl.h libintl.h stdlib.h string.h strings.h sys/ioctl.h sys/param.h sys/statfs.h sys/statvfs.h sys/time.h unistd.h wchar.h plfs.h hdfs.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([getpagesize gettimeofday memset mkdir pow putenv realpath regcomp sqrt strcasecmp strchr strerror strncasecmp strstr uname])
AC_SEARCH_LIBS([sqrt], [m], [],
        [AC_MSG_ERROR([Math library not found])])

# Check for gpfs availability
AC_ARG_WITH([gpfs],
        [AS_HELP_STRING([--with-gpfs],
                [support configurable GPFS @<:@default=check@:>@])],
        [], [with_gpfs=check])

AS_IF([test "x$with_gpfs" != xno], [
        AC_CHECK_HEADERS([gpfs.h gpfs_fcntl.h], [], [
                if test "x$with_gpfs" != xcheck; then
                        AC_MSG_FAILURE([--with-gpfs was given, <gpfs.h> and <gpfs_fcntl.h> not found])
                fi
        ])
AS_IF([test "$ac_cv_header_gpfs_h" = "yes" -o "$ac_cv_header_gpfs_fcntl_h" = "yes"], [
        AC_SEARCH_LIBS([gpfs_fcntl], [gpfs], [],
        [AC_MSG_ERROR([Library containing gpfs_fcntl symbols not found])
        ])
    ])
])

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

# HDFS support
AC_ARG_WITH([hdfs],
        [AS_HELP_STRING([--with-hdfs],
           [support IO with HDFS backend @<:@default=no@:>@])],
        [],
        [with_hdfs=no])
AM_CONDITIONAL([USE_HDFS_AIORI], [test x$with_hdfs = xyes])
AM_COND_IF([USE_HDFS_AIORI],[
        AC_DEFINE([USE_HDFS_AIORI], [], [Build HDFS backend AIORI])
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

# PLFS IO support
AC_ARG_WITH([plfs],
        [AS_HELP_STRING([--with-plfs],
           [support IO with PLFS backend @<:@default=no@:>@])],
        [],
        [with_plfs=no])
AM_CONDITIONAL([USE_PLFS_AIORI], [test x$with_plfs = xyes])
AM_COND_IF([USE_PLFS_AIORI],[
        AC_DEFINE([USE_PLFS_AIORI], [], [Build PLFS backend AIORI])
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




# aws4c is needed for the S3 backend (see --with-S3, below).
# Version 0.5.2 of aws4c is available at https://github.com/jti-lanl/aws4c.git
# Install it something like this:
#
#   cd $my_install_dir
#   git clone https://github.com/jti-lanl/aws4c.git
#   cd aws4c
#   make
#
# Then:
#   --with-S3 --with-aws4c=$my_install_dir/aws4c

aws4c_dir=
AC_ARG_WITH([aws4c],
        [AS_HELP_STRING([--with-aws4c=DIR],
           [aws4c library is needed for Amazon S3 backend])],
        [aws4c_dir="$withval"])
AM_CONDITIONAL([AWS4C_DIR], [test x$aws4c_dir != x])

# AC_SUBST([AWS4C_DIR],[$aws4c_dir])
AM_COND_IF([AWS4C_DIR],[
   AC_SUBST([AWS4C_CPPFLAGS],[-I$aws4c_dir])
   AC_SUBST([AWS4C_LDFLAGS], [-L$aws4c_dir])
])


# Amazon S3 support  [see also:  --with-aws4c]
AC_ARG_WITH([S3],
        [AS_HELP_STRING([--with-S3],
           [support IO with Amazon S3 backend @<:@default=no@:>@])],
        [],
        [with_S3=no])
AM_CONDITIONAL([USE_S3_AIORI], [test x$with_S3 = xyes])
AM_COND_IF([USE_S3_AIORI],[
        AC_DEFINE([USE_S3_AIORI], [], [Build Amazon-S3 backend AIORI])
])

err=0
AS_IF([test "x$with_S3" != xno], [
            AC_MSG_NOTICE([beginning of S3-related checks])

            # save user's values, while we use AC_CHECK_HEADERS with $AWS4C_DIR
            ORIG_CPPFLAGS=$CPPFLAGS
            ORIG_LDFLAGS=$LDFLAGS

            CPPFLAGS="$CPPFLAGS $AWS4C_CPPFLAGS"
            LDFLAGS=" $LDFLAGS  $AWS4C_LDFLAGS"

            AC_CHECK_HEADERS([aws4c.h], [], [err=1])
            AC_CHECK_HEADERS([libxml/parser.h], [], [err=1])

            # Autotools thinks searching for a library means I want it added to LIBS
            ORIG_LIBS=$LIBS
            AC_CHECK_LIB([curl], [curl_easy_init], [], [err=1])
            AC_CHECK_LIB([xml2], [xmlDocGetRootElement], [], [err=1])
            AC_CHECK_LIB([aws4c], [s3_get], [], [err=1], [-lcurl -lxml2 -lcrypto])
            LIBS=$ORIG_LIBS

            AC_MSG_NOTICE([end of S3-related checks])
            if test "$err" == 1; then
                AC_MSG_FAILURE([S3 support is missing.  dnl
Make sure you have access to libaws4c, libcurl, libxml2, and libcrypto.  dnl
Consider --with-aws4c=, CPPFLAGS, LDFLAGS, etc])
            fi

            # restore user's values
            CPPFLAGS=$ORIG_CPPFLAGS
            LDFLAGS=$ORIG_LDFLAGS
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
