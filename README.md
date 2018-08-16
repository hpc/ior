# HPC IO Benchmark Repository [![Build Status](https://travis-ci.org/hpc/ior.svg?branch=master)](https://travis-ci.org/hpc/ior)

This repo now contains both IOR and mdtest.
See also NOTES.txt

# Building

0. If "configure" is missing from the top level directory, you
   probably retrieved this code directly from the repository.
   Run "./bootstrap".

   If your versions of the autotools are not new enough to run
   this script, download and official tarball in which the
   configure script is already provided.

1. Run "./configure"

   See "./configure --help" for configuration options.

2. Run "make"

3. Optionally, run "make install".  The installation prefix
   can be changed as an option to the "configure" script.

# Testing

  Run "make check" to invoke the unit test framework of Automake.

  * To run basic functionality tests that we use for continuous integration, see ./testing/
  * There are docker scripts provided to test various distributions at once.
  * See ./testing/docker/
