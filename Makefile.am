MAKEFLAGS = --no-print-directory
SUBDIRS = src doc contrib
EXTRA_DIST = META COPYRIGHT README.md NEWS testing

# ACLOCAL_AMFLAGS needed for autoconf < 2.69
ACLOCAL_AMFLAGS = -I config

# The basic-tests.sh scripts run MPI versions of IOR/mdtest and are therefore
# too complicated to run in the context of distclean.  As such we reserve
# `make dist` and `make test` for simple test binaries that do not require any
# special environment.
#TESTS = testing/basic-tests.sh
#DISTCLEANFILES = -r test test_out
