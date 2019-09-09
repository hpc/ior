.. _compatibility:

Version Compatibility
=====================

IOR has a long history and only IOR version 3 is currently supported.  However,
there are many forks of IOR based on earlier versions, and the following
incompatibilities are known to exist between major versions.

1)  IOR version 1 (c. 1996-2002) and IOR version 2 (c. 2003-present) are
    incompatible.  Input decks from one will not work on the other.  As version
    1 is not included in this release, this shouldn't be case for concern.  All
    subsequent compatibility issues are for IOR version 2.

2)  IOR versions prior to release 2.8 provided data size and rates in powers
    of two.  E.g., 1 MB/sec referred to 1,048,576 bytes per second.  With the
    IOR release 2.8 and later versions, MB is now defined as 1,000,000 bytes
    and MiB is 1,048,576 bytes.

3)  In IOR versions 2.5.3 to 2.8.7, IOR could be run without any command line
    options.  This assumed that if both write and read options (-w -r) were
    omitted, the run with them both set as default.  Later, it became clear
    that in certain cases (data checking, e.g.) this caused difficulties.  In
    IOR versions 2.8.8 and later, if not one of the -w -r -W or -R options is
    set, then -w and -r are set implicitly.

4)  IOR version 3 (Jan 2012-present) has changed the output of IOR somewhat,
    and the "testNum" option was renamed "refNum".
