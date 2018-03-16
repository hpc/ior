Introduction
============

Welcome to the IOR documentation.

**I**\ nterleaved **o**\ r **R**\ andom is a parallel IO benchmark.
IOR can be used for testing performance of parallel file systems using various
interfaces and access patterns. IOR uses MPI for process synchronization.
This documentation provides information for versions 3 and higher, for other
versions check :ref:`compatibility`

This documentation consists of tow parts.

The first part is a user documentation were you find instructions on compilation, a
beginners tutorial (:ref:`first-steps`) as well as information about all
available :ref:`options`.

The second part is the developer documentation. It currently only consists of a
auto generated Doxygen and some notes about the contiguous integration with travis.
As there are quite some people how needs to modify or extend IOR to there needs
it would be great to have documentation on what and how to alter IOR without
breaking other stuff. Currently there is neither a documentation on the overall
concept of the code nor on implementation details. If you are getting your
hands dirty in code anyways or have deeper understanding of IOR, you are more
then welcome to comment the code directly, which will result in better Doxygen
output or add your insight to this sphinx documentation.
