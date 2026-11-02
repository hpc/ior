Scripting
=========

IOR can use an input script with the command line using the ``-f`` option.
Command-line options set the default settings for the script. For example, ::

    mpirun ./ior -W -f script

will run all tests in the script with an implicit ``-W``. The script itself can
override these settings and may run many different tests in a single execution.
Options after ``-f`` also set defaults; for example, ::

    mpirun ./ior -f script -W

also uses ``-W`` as a default for the script's tests.

Input scripts are specified using the long-form option names that correspond to
each command-line option.  In addition to long-form options,

    * ``IOR START`` and ``IOR STOP`` (case-insensitive) mark the beginning and
      end of the script.
    * ``RUN`` (case-insensitive) dispatches the test using all of the options
      specified before it.
    * All previous set parameter stay set for the next test. They are not reset
      to the default! For default the must be rest manually.
    * White space is ignored in script, as are comments starting with ``#``.
    * Not all test parameters need be set.

An example of a script: ::

  IOR START
      api=POSIX
      testFile=testFile
      writeFile=1
      readFile=1
      blockSize=32k
      transferSize=4k
      segmentCount=1
  RUN
      # Change settings for another test.
      blockSize=64k
      transferSize=8k
  RUN
  IOR STOP

Backend-specific options use their prefixed names in scripts. For example,
with the MPIIO backend, ``--mpiio.hintsFileName=hints.txt`` reads a hints file
and ``--mpiio.showHints=1`` displays hints. See :ref:`options` for details.
