# HPC IO Benchmark Repository [![Build Status](https://travis-ci.org/hpc/ior.svg?branch=main)](https://travis-ci.org/hpc/ior)

This repository contains the IOR and mdtest parallel I/O benchmarks.  The
[official IOR/mdtest documentation][] can be found in the `docs/` subdirectory
or on Read the Docs.

## Building

1. If `configure` is missing from the top level directory, you probably
   retrieved this code directly from the repository.  Run `./bootstrap`
   to generate the configure script.  Alternatively, download an
   [official IOR release][] which includes the configure script.

1. Run `./configure`.  For a full list of configuration options, use
   `./configure --help`.

2. Run `make`

3. Optionally, run `make install`.  The installation prefix
   can be changed via `./configure --prefix=...`.

The HDF5, PnetCDF, and Linux AIO backends can be enabled with
`--with-hdf5`, `--with-ncmpi`, and `--with-aio`, respectively.  Each option
also accepts an installation prefix, for example
`--with-hdf5=/opt/parallel-hdf5`; the prefix's `include`, `lib`, and `lib64`
directories are searched.  HDF5 must provide its parallel MPI interface.
Prefix arguments containing whitespace are rejected explicitly; use a prefix
path without whitespace.
The normal `CPPFLAGS`, `LDFLAGS`, and `LIBS` settings remain available for
other dependency layouts.  At runtime, shared libraries must also be
discoverable by the system loader or the environment.

## Testing

* Run `make check` to invoke the unit tests.
* More comprehensive functionality tests are included in `testing/`.  These
  scripts will launch IOR and mdtest via MPI.
* Docker scripts are also provided in `testing/docker/` to test various
  distributions at once.  

[official IOR release]: https://github.com/hpc/ior/releases
[official IOR/mdtest documentation]: http://ior.readthedocs.org/
