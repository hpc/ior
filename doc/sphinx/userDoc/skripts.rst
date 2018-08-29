Scripting
=========

IOR can use a script with the command line.  Any options on the command line set
before the script will be considered the default settings for running the script.
(I.e.,'$ ./IOR -W -f script' will have all tests in the script run with the -W
option as default.)
The script itself can override these settings and may be set to run
run many different tests of IOR under a single execution.
The command line is: ::

  ./IOR -f script

In IOR/scripts, there are scripts of test cases for simulating I/O behavior of
various application codes.  Details are included in each script as necessary.

Syntax:
    * IOR START / IOR END: marks the beginning and end of the script
    * RUN: Delimiter for next Test
    * All previous set parameter stay set for the next test. They are not reset
      to the default! For default the musst be rest manually.
    * White space is ignored in script, as are comments starting with '#'.
    * Not all test parameters need be set.

An example of a script: ::

  IOR START
    api=[POSIX|MPIIO|HDF5|HDFS|S3|S3_EMC|NCMPI|RADOS]
    testFile=testFile
    hintsFileName=hintsFile
    repetitions=8
    multiFile=0
    interTestDelay=5
    readFile=1
    writeFile=1
    filePerProc=0
    checkWrite=0
    checkRead=0
    keepFile=1
    quitOnError=0
    segmentCount=1
    blockSize=32k
    outlierThreshold=0
    setAlignment=1
    transferSize=32
    singleXferAttempt=0
    individualDataSets=0
    verbose=0
    numTasks=32
    collective=1
    preallocate=0
    useFileView=0
    keepFileWithError=0
    setTimeStampSignature=0
    useSharedFilePointer=0
    useStridedDatatype=0
    uniqueDir=0
    fsync=0
    storeFileOffset=0
    maxTimeDuration=60
    deadlineForStonewalling=0
    useExistingTestFile=0
    useO_DIRECT=0
    showHints=0
    showHelp=0
  RUN
    # additional tests are optional
    <snip>
  RUN
    <snip>
  RUN
  IOR STOP
