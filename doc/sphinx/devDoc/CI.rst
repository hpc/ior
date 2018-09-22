Continuous Integration
======================

Continuous Integration is used for basic sanity checking. Travis-CI provides free
CI for open source github projects and is configured via a ``.travis.yml``.

For now this is set up to compile IOR on a ubuntu 14.04 machine with gcc 4.8,
openmpi and hdf5 for the backends. This is a pretty basic check and should be
advance over time. Nevertheless this should detect major errors early as they
are shown in pull requests.
