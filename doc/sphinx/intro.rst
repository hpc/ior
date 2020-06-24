Introduction
============

IOR is a parallel IO benchmark that can be used to test the performance of
parallel storage systems using various interfaces and access patterns.  The
IOR repository also includes the mdtest benchmark which specifically tests
the peak metadata rates of storage systems under different directory
structures.  Both benchmarks use a common parallel I/O abstraction backend
and rely on MPI for synchronization.

This documentation consists of two parts.

**User documentation** includes installation instructions (:ref:`install`), a
beginner's tutorial (:ref:`first-steps`), and information about IOR's
runtime :ref:`options`.

**Developer documentation** consists of code documentation generated with
Doxygen and some notes about the contiguous integration with Travis.

Many aspects of both IOR/mdtest user and developer documentation are incomplete,
and contributors are encouraged to comment the code directly or expand upon this
documentation.
