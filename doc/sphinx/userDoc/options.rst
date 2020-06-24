.. _options:

Options
=======

IOR provides many options, in fact there are now more than there are one letter
flags in the alphabet.
For this and to run IOR by a config script, there are some options which are
only available via directives. When both script and command line options are in
use, command line options set in front of -f are the defaults which may be
overridden by the script.
Directives can also be set from the command line via "-O" option. In combination
with a script they behave like the normal command line options. But directives and
normal parameters override each other, so the last one executed.


Command line options
--------------------

These options are to be used on the command line (e.g., ``./ior -a POSIX -b 4K``).

  -a S  api --  API for I/O [POSIX|MPIIO|HDF5|HDFS|S3|S3_EMC|NCMPI|RADOS]
  -A N  refNum -- user reference number to include in long summary
  -b N  blockSize -- contiguous bytes to write per task  (e.g.: 8, 4k, 2m, 1g)
  -c    collective -- collective I/O
  -C    reorderTasksConstant -- changes task ordering to n+1 ordering for readback
  -d N  interTestDelay -- delay between reps in seconds
  -D N  deadlineForStonewalling -- seconds before stopping write or read phase
  -e    fsync -- perform fsync upon POSIX write close
  -E    useExistingTestFile -- do not remove test file before write access
  -f S  scriptFile -- test script name
  -F    filePerProc -- file-per-process
  -g    intraTestBarriers -- use barriers between open, write/read, and close
  -G N  setTimeStampSignature -- set value for time stamp signature
  -h    showHelp -- displays options and help
  -H    showHints -- show hints
  -i N  repetitions -- number of repetitions of test
  -I    individualDataSets -- datasets not shared by all procs [not working]
  -j N  outlierThreshold -- warn on outlier N seconds from mean
  -J N  setAlignment -- HDF5 alignment in bytes (e.g.: 8, 4k, 2m, 1g)
  -k    keepFile -- don't remove the test file(s) on program exit
  -K    keepFileWithError  -- keep error-filled file(s) after data-checking
  -l    data packet type-- type of packet that will be created [offset|incompressible|timestamp|o|i|t]
  -m    multiFile -- use number of reps (-i) for multiple file count
  -M N  memoryPerNode -- hog memory on the node (e.g.: 2g, 75%)
  -n    noFill -- no fill in HDF5 file creation
  -N N  numTasks -- number of tasks that should participate in the test
  -o S  testFile -- full name for test
  -O S  string of IOR directives (e.g. -O checkRead=1,lustreStripeCount=32)
  -p    preallocate -- preallocate file size
  -P    useSharedFilePointer -- use shared file pointer [not working]
  -q    quitOnError -- during file error-checking, abort on error
  -Q N  taskPerNodeOffset for read tests use with -C & -Z options (-C constant N, -Z at least N) [!HDF5]
  -r    readFile -- read existing file
  -R    checkRead -- check read after read
  -s N  segmentCount -- number of segments
  -S    useStridedDatatype -- put strided access into datatype [not working]
  -t N  transferSize -- size of transfer in bytes (e.g.: 8, 4k, 2m, 1g)
  -T N  maxTimeDuration -- max time in minutes to run tests
  -u    uniqueDir -- use unique directory name for each file-per-process
  -U S  hintsFileName -- full name for hints file
  -v    verbose -- output information (repeating flag increases level)
  -V    useFileView -- use MPI_File_set_view
  -w    writeFile -- write file
  -W    checkWrite -- check read after write
  -x    singleXferAttempt -- do not retry transfer if incomplete
  -X N  reorderTasksRandomSeed -- random seed for -Z option
  -Y    fsyncPerWrite -- perform fsync after each POSIX write
  -z    randomOffset -- access is to random, not sequential, offsets within a file
  -Z    reorderTasksRandom -- changes task ordering to random ordering for readback


* S is a string, N is an integer number.

* For transfer and block sizes, the case-insensitive K, M, and G
  suffices are recognized.  I.e., '4k' or '4K' is accepted as 4096.

Various options are only valid for specific modules, you can see details when running $ ./ior -h
These options are typically prefixed with the module name, an example is: --posix.odirect


Directive Options
------------------

For all true/false options below, [1]=true, [0]=false.  All options are case-insensitive.

GENERAL
^^^^^^^^^^^^^^

  * ``refNum`` - user supplied reference number, included in long summary
    (default: 0)

  * ``api`` - must be set to one of POSIX, MPIIO, HDF5, HDFS, S3, S3_EMC, NCMPI,
    IME, MMAP, or RAODS depending on test (default: ``POSIX``)

  * ``testFile`` - name of the output file [testFile].  With ``filePerProc`` set,
    the tasks can round robin across multiple file names via ``-o S@S@S``.
    If only a single file name is specified in this case, IOR appends the MPI
    rank to the end of each file generated (e.g., ``testFile.00000059``)
    (default: ``testFile``)

  * ``hintsFileName`` - name of the hints file (default: none)

  * ``repetitions`` - number of times to run each test (default: 1)

  * ``multiFile`` - creates multiple files for single-shared-file or
    file-per-process modes for each iteration (default: 0)

  * ``reorderTasksConstant`` - reorders tasks by a constant node offset for
    writing/reading neighbor's data from different nodes (default: 0)

  * ``taskPerNodeOffset`` - for read tests. Use with ``-C`` and ``-Z`` options.
    With ``reorderTasks``, constant N. With ``reordertasksrandom``, >= N
    (default: 1)

  * ``reorderTasksRandom`` - reorders tasks to random ordering for read tests
    (default: 0)

  * ``reorderTasksRandomSeed`` - random seed for ``reordertasksrandom`` option. (default: 0)
        * When > 0, use the same seed for all iterations
        * When < 0, different seed for each iteration

  * ``quitOnError`` - upon error encountered on ``checkWrite`` or ``checkRead``,
    display current error and then stop execution.  Otherwise, count errors and
    continue (default: 0)

  * ``numTasks`` - number of tasks that should participate in the test.  0
    denotes all tasks.  (default: 0)

  * ``interTestDelay`` - time (in seconds) to delay before beginning a write or
    read phase in a series of tests This does not delay before check-write or
    check-read phases.  (default: 0)

  * ``outlierThreshold`` - gives warning if any task is more than this number of
    seconds from the mean of all participating tasks.  The warning includes the
    offending task, its timers (start, elapsed create, elapsed transfer, elapsed
    close, end), and the mean and standard deviation for all tasks.  When zero,
    disable this feature. (default: 0)

  * ``intraTestBarriers`` - use barrier between open, write/read, and close
    phases (default: 0)

  * ``uniqueDir`` - create and use unique directory for each file-per-process
    (default: 0)

  * ``writeFile`` - write file(s), first deleting any existing file.
    The defaults for ``writeFile`` and ``readFile`` are set such that if there
    is not at least one of ``-w``, ``-r``, ``-W``, or ``-R``, ``-w`` and ``-r``
    are enabled.  If either ``writeFile`` or ``readFile`` are explicitly
    enabled, though, its complement is *not* also implicitly enabled.

  * ``readFile`` - reads existing file(s) as specified by the ``testFile``
    option.  The defaults for ``writeFile`` and ``readFile`` are set such that
    if there is not at least one of ``-w``, ``-r``, ``-W``, or ``-R``, ``-w``
    and ``-r`` are enabled.  If either ``writeFile`` or ``readFile`` are
    explicitly enabled, though, its complement is *not* also implicitly enabled.

  * ``filePerProc`` - have each MPI process perform I/O to a unique file
    (default: 0)

  * ``checkWrite`` - read data back and check for errors against known pattern.
    Can be used independently of ``writeFile``.  Data checking is not timed and
    does not affect other performance timings.  All errors detected are tallied
    and returned as the program exit code unless ``quitOnError`` is set.
    (default: 0)

  * ``checkRead`` - re-read data and check for errors between reads.  Can be
    used independently of ``readFile``.  Data checking is not timed and does not
    affect other performance timings.  All errors detected are tallied and
    returned as the program exit code unless ``quitOnError`` is set.
    (default: 0)

  * ``keepFile`` - do not remove test file(s) on program exit (default: 0)

  * ``keepFileWithError`` - do not delete any files containing errors if
    detected during read-check or write-check phases. (default: 0)

  * ``useExistingTestFile`` - do not remove test file(s) before write phase
    (default: 0)

  * ``segmentCount`` - number of segments in file, where a segment is a
    contiguous chunk of data accessed by multiple clients each writing/reading
    their own contiguous data (blocks).  The exact semantics of segments
    depend on the API used; for example, HDF5 repeats the pattern of an entire
    shared dataset. (default: 1)

  * ``blockSize`` - size (in bytes) of a contiguous chunk of data accessed by a
    single client.  It is comprised of one or more transfers (default: 1048576)

  * ``transferSize`` - size (in bytes) of a single data buffer to be transferred
    in a single I/O call (default: 262144)

  * ``verbose`` - output more information about what IOR is doing.  Can be set
    to levels 0-5; repeating the -v flag will increase verbosity level.
    (default: 0)

  * ``setTimeStampSignature`` - Value to use for the time stamp signature.  Used
    to rerun tests with the exact data pattern by setting data signature to
    contain positive integer value as timestamp to be written in data file; if
    set to 0, is disabled (default: 0)

  * ``showHelp`` - display options and help (default: 0)

  * ``storeFileOffset`` - use file offset as stored signature when writing file.
    This will affect performance measurements (default: 0)

  * ``memoryPerNode`` - allocate memory on each node to simulate real
    application memory usage or restrict page cache size.  Accepts a percentage
    of node memory (e.g. ``50%``) on systems that support
    ``sysconf(_SC_PHYS_PAGES)`` or a size.  Allocation will be split between
    tasks that share the node. (default: 0)

  * ``memoryPerTask`` - allocate specified amount of memory (in bytes) per task
    to simulate real application memory usage. (default: 0)

  * ``maxTimeDuration`` - max time (in minutes) to run all tests.  Any current
    read/write phase is not interrupted; only future I/O phases are cancelled
    once this time is exceeded.  Value of zero unsets disables. (default: 0)

  * ``deadlineForStonewalling`` - seconds before stopping write or read phase.
    Used for measuring the amount of data moved in a fixed time.  After the
    barrier, each task starts its own timer, begins moving data, and the stops
    moving data at a pre-arranged time.  Instead of measuring the amount of time
    to move a fixed amount of data, this option measures the amount of data
    moved in a fixed amount of time.  The objective is to prevent straggling
    tasks slow from skewing the performance.  This option is incompatible with
    read-check and write-check modes.  Value of zero unsets this option.
    (default: 0)

  * ``randomOffset`` - randomize access offsets within test file(s).  Currently
    incompatible with ``checkRead``, ``storeFileOffset``, MPIIO ``collective``
    and ``useFileView``, and HDF5 and NCMPI APIs. (default: 0)

  * ``summaryAlways`` - Always print the long summary for each test even if the job is interrupted. (default: 0)

POSIX-ONLY
^^^^^^^^^^

  * ``useO_DIRECT`` - use direct I/ for POSIX, bypassing I/O buffers (default: 0)

  * ``singleXferAttempt``    - do not continue to retry transfer entire buffer
    until it is transferred.  When performing a write() or read() in POSIX,
    there is no guarantee that the entire requested size of the buffer will be
    transferred; this flag keeps the retrying a single transfer until it
    completes or returns an error (default: 0)

  * ``fsyncPerWrite`` - perform fsync after each POSIX write (default: 0)

  * ``fsync`` - perform fsync after POSIX file close (default: 0)

MPIIO-ONLY
^^^^^^^^^^

  * ``preallocate`` - preallocate the entire file before writing (default: 0)

  * ``useFileView`` - use an MPI datatype for setting the file view option to
    use individual file pointer.  Default IOR uses explicit file pointers.
    (default: 0)

  * ``useSharedFilePointer`` - use a shared file pointer.  Default IOR uses
    explicit file pointers. (default: 0)

  * ``useStridedDatatype`` - create a datatype (max=2GB) for strided access;
    akin to ``MULTIBLOCK_REGION_SIZE`` (default: 0)

HDF5-ONLY
^^^^^^^^^

  * ``individualDataSets`` - within a single file, each task will access its own
    dataset.  Default IOR creates a dataset the size of ``numTasks * blockSize``
    to be accessed by all tasks (default: 0)

  * ``noFill`` - do not pre-fill data in HDF5 file creation (default: 0)

  * ``setAlignment`` - set the HDF5 alignment in bytes (e.g.: 8, 4k, 2m, 1g) (default: 1)

  * hdf5.collectiveMetadata   - enable HDF5 collective metadata (available since HDF5-1.10.0)

MPIIO-, HDF5-, AND NCMPI-ONLY
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  * ``collective`` - uses collective operations for access (default: 0)

  * ``showHints`` - show hint/value pairs attached to open file.  Not available
    for NCMPI. (default: 0)

LUSTRE-SPECIFIC
^^^^^^^^^^^^^^^^^

  * ``lustreStripeCount`` - set the Lustre stripe count for the test file(s) (default: 0)

  * ``lustreStripeSize`` - set the Lustre stripe size for the test file(s) (default: 0)

  * ``lustreStartOST`` - set the starting OST for the test file(s) (default: -1)

  * ``lustreIgnoreLocks`` - disable Lustre range locking (default: 0)

GPFS-SPECIFIC
^^^^^^^^^^^^^^

  * ``gpfsHintAccess`` - use ``gpfs_fcntl`` hints to pre-declare accesses (default: 0)

  * ``gpfsReleaseToken`` - release all locks immediately after opening or
    creating file.  Might help mitigate lock-revocation traffic when many
    proceses write/read to same file. (default: 0)

Verbosity levels
----------------

The verbosity of output for IOR can be set with ``-v``.  Increasing the number
of ``-v`` instances on a command line sets the verbosity higher.

Here is an overview of the information shown for different verbosity levels:

======  ===================================
Level   Behavior
======  ===================================
  0     default; only bare essentials shown
  1     max clock deviation, participating tasks, free space, access pattern, commence/verify access notification with time
  2     rank/hostname, machine name, timer used, individual repetition performance results, timestamp used for data signature
  3     full test details, transfer block/offset compared, individual data checking errors, environment variables, task writing/reading file name, all test operation times
  4     task id and offset for each transfer
  5     each 8-byte data signature comparison (WARNING: more data to STDOUT than stored in file, use carefully)
======  ===================================


Incompressible notes
--------------------
Please note that incompressibility is a factor of how large a block compression
algorithm uses.  The incompressible buffer is filled only once before write
times, so if the compression algorithm takes in blocks larger than the transfer
size, there will be compression.  Below are some baselines for zip, gzip, and
bzip.

1)  zip:  For zipped files, a transfer size of 1k is sufficient.

2)  gzip: For gzipped files, a transfer size of 1k is sufficient.

3)  bzip2: For bziped files a transfer size of 1k is insufficient (~50% compressed).
    To avoid compression a transfer size of greater than the bzip block size is required
    (default = 900KB). I suggest a transfer size of greather than 1MB to avoid bzip2 compression.

Be aware of the block size your compression algorithm will look at, and adjust
the transfer size accordingly.
