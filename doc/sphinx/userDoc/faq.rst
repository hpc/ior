Frequently Asked Questions
==========================

HOW DO I PERFORM MULTIPLE DATA CHECKS ON AN EXISTING FILE?

  Use this command line:  IOR -k -E -W -i 5 -o file

  -k keeps the file after the access rather than deleting it
  -E uses the existing file rather than truncating it first
  -W performs the writecheck
  -i number of iterations of checking
  -o filename

  On versions of IOR prior to 2.8.8, you need the -r flag also, otherwise
  you'll first overwrite the existing file.  (In earlier versions, omitting -w
  and -r implied using both.  This semantic has been subsequently altered to be
  omitting -w, -r, -W, and -R implied using both -w and -r.)

  If you're running new tests to create a file and want repeat data checking on
  this file multiple times, there is an undocumented option for this.  It's -O
  multiReRead=1, and you'd need to have an IOR version compiled with the
  USE_UNDOC_OPT=1 (in iordef.h).  The command line would look like this:

  IOR -k -E -w -W -i 5 -o file -O multiReRead=1

  For the first iteration, the file would be written (w/o data checking).  Then
  for any additional iterations (four, in this example) the file would be
  reread for whatever data checking option is used.


HOW DOES IOR CALCULATE PERFORMANCE?

  IOR performs get a time stamp START, then has all participating tasks open a
  shared or independent file, transfer data, close the file(s), and then get a
  STOP time.  A stat() or MPI_File_get_size() is performed on the file(s) and
  compared against the aggregate amount of data transferred.  If this value
  does not match, a warning is issued and the amount of data transferred as
  calculated from write(), e.g., return codes is used.  The calculated
  bandwidth is the amount of data transferred divided by the elapsed
  STOP-minus-START time.

  IOR also gets time stamps to report the open, transfer, and close times.
  Each of these times is based on the earliest start time for any task and the
  latest stop time for any task.  Without using barriers between these
  operations (-g), the sum of the open, transfer, and close times may not equal
  the elapsed time from the first open to the last close.


HOW DO I ACCESS MULTIPLE FILE SYSTEMS IN IOR?

  It is possible when using the filePerProc option to have tasks round-robin
  across multiple file names.  Rather than use a single file name '-o file',
  additional names '-o file1@file2@file3' may be used.  In this case, a file
  per process would have three different file names (which may be full path
  names) to access.  The '@' delimiter is arbitrary, and may be set in the
  FILENAME_DELIMITER definition in iordef.h.

  Note that this option of multiple filenames only works with the filePerProc
  -F option.  This will not work for shared files.


HOW DO I BALANCE LOAD ACROSS MULTIPLE FILE SYSTEMS?

  As for the balancing of files per file system where different file systems
  offer different performance, additional instances of the same destination
  path can generally achieve good balance.

  For example, with FS1 getting 50% better performance than FS2, set the '-o'
  flag such that there are additional instances of the FS1 directory.  In this
  case, '-o FS1/file@FS1/file@FS1/file@FS2/file@FS2/file' should adjust for
  the performance difference and balance accordingly.


HOW DO I USE STONEWALLING?

  To use stonewalling (-D), it's generally best to separate write testing from
  read testing.  Start with writing a file with '-D 0' (stonewalling disabled)
  to determine how long the file takes to be written.  If it takes 10 seconds
  for the data transfer, run again with a shorter duration, '-D 7' e.g., to
  stop before the file would be completed without stonewalling.  For reading,
  it's best to create a full file (not an incompletely written file from a
  stonewalling run) and then run with stonewalling set on this preexisting
  file.  If a write and read test are performed in the same run with
  stonewalling, it's likely that the read will encounter an error upon hitting
  the EOF.  Separating the runs can correct for this.  E.g.,

  IOR -w -k -o file -D 10  # write and keep file, stonewall after 10 seconds
  IOR -r -E -o file -D 7   # read existing file, stonewall after 7 seconds

  Also, when running multiple iterations of a read-only stonewall test, it may
  be necessary to set the -D value high enough so that each iteration is not
  reading from cache.  Otherwise, in some cases, the first iteration may show
  100 MB/s, the next 200 MB/s, the third 300 MB/s.  Each of these tests is
  actually reading the same amount from disk in the allotted time, but they
  are also reading the cached data from the previous test each time to get the
  increased performance.  Setting -D high enough so that the cache is
  overfilled will prevent this.


HOW DO I BYPASS CACHING WHEN READING BACK A FILE I'VE JUST WRITTEN?

  One issue with testing file systems is handling cached data.  When a file is
  written, that data may be stored locally on the node writing the file.  When
  the same node attempts to read the data back from the file system either for
  performance or data integrity checking, it may be reading from its own cache
  rather from the file system.

  The reorderTasksConstant '-C' option attempts to address this by having a
  different node read back data than wrote it.  For example, node N writes the
  data to file, node N+1 reads back the data for read performance, node N+2
  reads back the data for write data checking, and node N+3 reads the data for
  read data checking, comparing this with the reread data from node N+4.  The
  objective is to make sure on file access that the data is not being read from
  cached data.

    Node 0: writes data
    Node 1: reads data
    Node 2: reads written data for write checking
    Node 3: reads written data for read checking
    Node 4: reads written data for read checking, comparing with Node 3

  The algorithm for skipping from N to N+1, e.g., expects consecutive task
  numbers on nodes (block assignment), not those assigned round robin (cyclic
  assignment).  For example, a test running 6 tasks on 3 nodes would expect
  tasks 0,1 on node 0; tasks 2,3 on node 1; and tasks 4,5 on node 2.  Were the
  assignment for tasks-to-node in round robin fashion, there would be tasks 0,3
  on node 0; tasks 1,4 on node 1; and tasks 2,5 on node 2.  In this case, there
  would be no expectation that a task would not be reading from data cached on
  a node.


HOW DO I USE HINTS?

  It is possible to pass hints to the I/O library or file system layers
  following this form::
    'setenv IOR_HINT__<layer>__<hint> <value>'

  For example::
    'setenv IOR_HINT__MPI__IBM_largeblock_io true'
    'setenv IOR_HINT__GPFS__important_hint true'

  or, in a file in the form::
    'IOR_HINT__<layer>__<hint>=<value>'

  Note that hints to MPI from the HDF5 or NCMPI layers are of the form::
    'setenv IOR_HINT__MPI__<hint> <value>'


HOW DO I EXPLICITY SET THE FILE DATA SIGNATURE?

  The data signature for a transfer contains the MPI task number, transfer-
  buffer offset, and also timestamp for the start of iteration.  As IOR works
  with 8-byte long long ints, the even-numbered long longs written contain a
  32-bit MPI task number and a 32-bit timestamp.  The odd-numbered long longs
  contain a 64-bit transferbuffer offset (or file offset if the '-l'
  storeFileOffset option is used).  To set the timestamp value, use '-G' or
  setTimeStampSignature.


HOW DO I EASILY CHECK OR CHANGE A BYTE IN AN OUTPUT DATA FILE?

  There is a simple utility IOR/src/C/cbif/cbif.c that may be built.  This is a
  stand-alone, serial application called cbif (Change Byte In File).  The
  utility allows a file offset to be checked, returning the data at that
  location in IOR's data check format.  It also allows a byte at that location
  to be changed.


HOW DO I CORRECT FOR CLOCK SKEW BETWEEN NODES IN A CLUSTER?

  To correct for clock skew between nodes, IOR compares times between nodes,
  then broadcasts the root node's timestamp so all nodes can adjust by the
  difference.  To see an egregious outlier, use the '-j' option.  Be sure
  to set this value high enough to only show a node outside a certain time
  from the mean.
