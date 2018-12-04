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
These options are to be used on the command line. E.g., 'IOR -a POSIX -b 4K'.
  -a S  api --  API for I/O [POSIX|MPIIO|HDF5|HDFS|S3|S3_EMC|NCMPI|RADOS]
  -A N  refNum -- user reference number to include in long summary
  -b N  blockSize -- contiguous bytes to write per task  (e.g.: 8, 4k, 2m, 1g)
  -B    useO_DIRECT -- uses O_DIRECT for POSIX, bypassing I/O buffers
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


NOTES: * S is a string, N is an integer number.
       * For transfer and block sizes, the case-insensitive K, M, and G
         suffices are recognized.  I.e., '4k' or '4K' is accepted as 4096.


Directive Options
------------------
For each of the general settings, note the default is shown in brackets.
IMPORTANT NOTE: For all true/false options below [1]=true, [0]=false
IMPORTANT NOTE: Contrary to appearance, the script options below are NOT case sensitive


GENERAL:
^^^^^^^^^^^^^^
  * refNum               - user supplied reference number, included in
                           long summary [0]

  * api                  - must be set to one of POSIX, MPIIO, HDF5, HDFS, S3,
                           S3_EMC, or NCMPI, depending on test [POSIX]

  * testFile             - name of the output file [testFile]
                           NOTE: with filePerProc set, the tasks can round
                                 robin across multiple file names '-o S@S@S'

  * hintsFileName        - name of the hints file []

  * repetitions          - number of times to run each test [1]

  * multiFile            - creates multiple files for single-shared-file or
                           file-per-process modes; i.e. each iteration creates
                           a new file [0=FALSE]

  * reorderTasksConstant - reorders tasks by a constant node offset for writing/reading neighbor's
                           data from different nodes [0=FALSE]

  * taskPerNodeOffset    - for read tests. Use with -C & -Z options. [1]
                           With reorderTasks, constant N. With reordertasksrandom, >= N

  * reorderTasksRandom   - reorders tasks to random ordering for readback [0=FALSE]

  * reorderTasksRandomSeed - random seed for reordertasksrandom option. [0]
                              >0, same seed for all iterations. <0, different seed for each iteration

  * quitOnError          - upon error encountered on checkWrite or checkRead,
                           display current error and then stop execution;
                           if not set, count errors and continue [0=FALSE]

  * numTasks             - number of tasks that should participate in the test
                           [0]
                           NOTE: 0 denotes all tasks

  * interTestDelay       - this is the time in seconds to delay before
                           beginning a write or read in a series of tests [0]
                           NOTE: it does not delay before a check write or
                           check read

  * outlierThreshold     - gives warning if any task is more than this number
                           of seconds from the mean of all participating tasks.
                           If so, the task is identified, its time (start,
                           elapsed create, elapsed transfer, elapsed close, or
                           end) is reported, as is the mean and standard
                           deviation for all tasks.  The default for this is 0,
                           which turns it off.  If set to a positive value, for
                           example 3, any task not within 3 seconds of the mean
                           displays its times. [0]

  * intraTestBarriers    - use barrier between open, write/read, and close [0=FALSE]

  * uniqueDir            - create and use unique directory for each
                           file-per-process [0=FALSE]

  * writeFile            - writes file(s), first deleting any existing file [1=TRUE]
                           NOTE: the defaults for writeFile and readFile are
                                 set such that if there is not at least one of
                                 the following -w, -r, -W, or -R, it is assumed
                                 that -w and -r are expected and are
                                 consequently used -- this is only true with
                                 the command line, and may be overridden in
                                 a script

  * readFile             - reads existing file(s) (from current or previous
                           run) [1=TRUE]
                           NOTE: see writeFile notes

  * filePerProc          - accesses a single file for each processor; default
                           is a single file accessed by all processors [0=FALSE]

  * checkWrite           - read data back and check for errors against known
                           pattern; can be used independently of writeFile [0=FALSE]
                           NOTES: - data checking is not timed and does not
                                    affect other performance timings
                                  - all errors tallied and returned as program
                                    exit code, unless quitOnError set

  * checkRead            - reread data and check for errors between reads; can
                           be used independently of readFile [0=FALSE]
                           NOTE: see checkWrite notes

  * keepFile             - stops removal of test file(s) on program exit [0=FALSE]

  * keepFileWithError    - ensures that with any error found in data-checking,
                           the error-filled file(s) will not be deleted [0=FALSE]

  * useExistingTestFile  - do not remove test file before write access [0=FALSE]

  * segmentCount         - number of segments in file [1]
                           NOTES: - a segment is a contiguous chunk of data
                                    accessed by multiple clients each writing/
                                    reading their own contiguous data;
                                    comprised of blocks accessed by multiple
                                    clients
                                  - with HDF5 this repeats the pattern of an
                                    entire shared dataset

  * blockSize            - size (in bytes) of a contiguous chunk of data
                           accessed by a single client; it is comprised of one
                           or more transfers [1048576]

  * transferSize         - size (in bytes) of a single data buffer to be
                           transferred in a single I/O call [262144]

  * verbose              - output information [0]
                           NOTE: this can be set to levels 0-5 on the command
                                 line; repeating the -v flag will increase
                                 verbosity level

  * setTimeStampSignature - set value for time stamp signature [0]
                            NOTE: used to rerun tests with the exact data
                                  pattern by setting data signature to contain
                                  positive integer value as timestamp to be
                                  written in data file; if set to 0, is
                                  disabled

  * showHelp             - display options and help [0=FALSE]

  * storeFileOffset      - use file offset as stored signature when writing
                           file [0=FALSE]
                           NOTE: this will affect performance measurements

  * memoryPerNode        - Allocate memory on each node to simulate real
                           application memory usage.  Accepts a percentage of
                           node memory (e.g. "50%") on machines that support
                           sysconf(_SC_PHYS_PAGES) or a size.  Allocation will
                           be split between tasks that share the node.

  * memoryPerTask        - Allocate secified amount of memory per task to
                           simulate real application memory usage.

  * maxTimeDuration      - max time in minutes to run tests [0]
                           NOTES: * setting this to zero (0) unsets this option
                                  * this option allows the current read/write
                                    to complete without interruption

  * deadlineForStonewalling - seconds before stopping write or read phase [0]
                           NOTES: - used for measuring the amount of data moved
                                    in a fixed time.  After the barrier, each
                                    task starts its own timer, begins moving
                                    data, and the stops moving data at a pre-
                                    arranged time.  Instead of measuring the
                                    amount of time to move a fixed amount of
                                    data, this option measures the amount of
                                    data moved in a fixed amount of time.  The
                                    objective is to prevent tasks slow to
                                    complete from skewing the performance.
                                  - setting this to zero (0) unsets this option
                                  - this option is incompatible w/data checking

  * randomOffset         - access is to random, not sequential, offsets within a file [0=FALSE]
                           NOTES: - this option is currently incompatible with:
                                    -checkRead
                                    -storeFileOffset
                                    -MPIIO collective or useFileView
                                    -HDF5 or NCMPI
  * summaryAlways        - Always print the long summary for each test.
                           Useful for long runs that may be interrupted, preventing
                           the final long summary for ALL tests to be printed.


POSIX-ONLY
^^^^^^^^^^
  * useO_DIRECT          - use O_DIRECT for POSIX, bypassing I/O buffers [0]

  * singleXferAttempt    - will not continue to retry transfer entire buffer
                           until it is transferred [0=FALSE]
                           NOTE: when performing a write() or read() in POSIX,
                                 there is no guarantee that the entire
                                 requested size of the buffer will be
                                 transferred; this flag keeps the retrying a
                                 single transfer until it completes or returns
                                 an error

  * fsyncPerWrite        - perform fsync after each POSIX write  [0=FALSE]
  * fsync                - perform fsync after POSIX write close [0=FALSE]

MPIIO-ONLY
^^^^^^^^^^
  * preallocate          - preallocate the entire file before writing [0=FALSE]

  * useFileView          - use an MPI datatype for setting the file view option
                           to use individual file pointer [0=FALSE]
                           NOTE: default IOR uses explicit file pointers

  * useSharedFilePointer - use a shared file pointer [0=FALSE] (not working)
                           NOTE: default IOR uses explicit file pointers

  * useStridedDatatype   - create a datatype (max=2GB) for strided access; akin
                           to MULTIBLOCK_REGION_SIZE [0] (not working)

HDF5-ONLY
^^^^^^^^^
  * individualDataSets   - within a single file each task will access its own
                           dataset [0=FALSE] (not working)
                           NOTE: default IOR creates a dataset the size of
                                 numTasks * blockSize to be accessed by all
                                 tasks

  * noFill               - no pre-filling of data in HDF5 file creation [0=FALSE]

  * setAlignment         - HDF5 alignment in bytes (e.g.: 8, 4k, 2m, 1g) [1]

  * collectiveMetadata   - enable HDF5 collective metadata (available since
                           HDF5-1.10.0)

MPIIO-, HDF5-, AND NCMPI-ONLY
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  * collective           - uses collective operations for access [0=FALSE]

  * showHints            - show hint/value pairs attached to open file [0=FALSE]
                           NOTE: not available in NCMPI

LUSTRE-SPECIFIC
^^^^^^^^^^^^^^^^^
  * lustreStripeCount    - set the lustre stripe count for the test file(s) [0]

  * lustreStripeSize     - set the lustre stripe size for the test file(s) [0]

  * lustreStartOST       - set the starting OST for the test file(s) [-1]

  * lustreIgnoreLocks    - disable lustre range locking [0]

GPFS-SPECIFIC
^^^^^^^^^^^^^^
  * gpfsHintAccess       - use gpfs_fcntl hints to pre-declare accesses

  * gpfsReleaseToken     - immediately after opening or creating file, release
			   all locks.  Might help mitigate lock-revocation
			   traffic when many proceses write/read to same file.



Verbosity levels
---------------------
The verbosity of output for IOR can be set with -v.  Increasing the number of
-v instances on a command line sets the verbosity higher.

Here is an overview of the information shown for different verbosity levels:

0)  default; only bare essentials shown
1)  max clock deviation, participating tasks, free space, access pattern,
    commence/verify access notification w/time
2)  rank/hostname, machine name, timer used, individual repetition
    performance results, timestamp used for data signature
3)  full test details, transfer block/offset compared, individual data
    checking errors, environment variables, task writing/reading file name,
    all test operation times
4)  task id and offset for each transfer
5)  each 8-byte data signature comparison (WARNING: more data to STDOUT
    than stored in file, use carefully)


Incompressible notes
-------------------------
Please note that incompressibility is a factor of how large a block compression
algorithm uses.  The incompressible buffer is filled only once before write times,
so if the compression algorithm takes in blocks larger than the transfer size,
there will be compression.  Below are some baselines that I established for
zip, gzip, and bzip.

1)  zip:  For zipped files, a transfer size of 1k is sufficient.

2)  gzip: For gzipped files, a transfer size of 1k is sufficient.

3)  bzip2: For bziped files a transfer size of 1k is insufficient (~50% compressed).
    To avoid compression a transfer size of greater than the bzip block size is required
    (default = 900KB). I suggest a transfer size of greather than 1MB to avoid bzip2 compression.

Be aware of the block size your compression algorithm will look at, and adjust the transfer size
accordingly.
