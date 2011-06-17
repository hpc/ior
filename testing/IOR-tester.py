#!/usr/local/bin/python
#
# Tester for IOR
#
#/*****************************************************************************\
#*                                                                             *
#*       Copyright (c) 2003, The Regents of the University of California       *
#*     See the file COPYRIGHT for a complete copyright notice and license.     *
#*                                                                             *
#\*****************************************************************************/
#
# CVS info:
#   $RCSfile: IOR-tester.py,v $
#   $Revision: 1.1.1.1 $
#   $Date: 2007/10/15 23:36:54 $
#   $Author: rklundt $

import sys
import os.path
import string

# definitions
RETURN_TOTAL_TESTS	= -1
TEST_SETTINGS		= 0
TEST_SUBSET_SIZE	= 5	# number of tests in each job submission
IOR_SIZE_T		= 8
KIBIBYTE		= 1024
MEBIBYTE		= KIBIBYTE * KIBIBYTE
GIBIBYTE		= KIBIBYTE * MEBIBYTE
PASS			= 1
FAIL			= 0
TRUE			= 1
FALSE			= 0
scriptFileBase		= './scriptFile'
executable		= '../src/C/IOR'
#testDir			= './tmp_test_dir'
testDir			= '/p/glocal1/loewe/tmp_test_dir'

debug = FALSE           # debug mode = {FALSE, TRUE}

################################################################################
# class for default test parameters                                            #
################################################################################
class Test:
    ######################
    # default parameters #
    ######################
    def DefaultTest(self):
	return {
	# general
	    'debug':			'debug info',
	    'api':			'POSIX',
	    'testFile':			testDir + '/testFile.1',
	    'hintsFileName':		'',
	    'repetitions':		1,
	    'multiFile':		0,
	    'interTestDelay':		0,
	    'numTasks':			0,
	    'readFile':			1,
	    'writeFile':		1,
	    'filePerProc':		0,
	    'fsync':			0,
	    'checkWrite':		1,
	    'checkRead':		1,
	    'keepFile':			1,
	    'keepFileWithError':	1,
	    'segmentCount':		1,
	    'blockSize':		MEBIBYTE,
	    'transferSize':		(MEBIBYTE / 4),
	    'verbose':			0,
	    'showHelp':			0,
	    'reorderTasks':		1,	# not default in code
	    'quitOnError':		0,
	    'useExistingTestFile':	0,
	    'deadlineForStonewalling':	0,
	    'maxTimeDuration':		0,
	    'setTimeStampSignature':	0,
	    'intraTestBarriers':	0,
	    'storeFileOffset':		0,
	    'randomOffset':		0,
	# POSIX
	    'singleXferAttempt':	0,
	    'useO_DIRECT':		0,
	# MPIIO
	    'useFileView':		0,
	    'preallocate':		0,
	    'useSharedFilePointer':	0,	# not working yet
	    'useStridedDatatype':	0,	# not working yet
	# non-POSIX
	    'showHints':		0,
	    'collective':		0,
	# HDF5
	    'setAlignment':		1,
	    'noFill':			0,	# in hdf5-1.6 or later version
	    'individualDataSets':	0	# not working yet
	}

    ###################
    # tests to be run #
    ###################
    def Tests(self, expectation, testNumber):

################################################################################
################################################################################
#                                                                              #
#                         S T A R T   O F   T E S T S                          #
#                                                                              #
################################################################################
################################################################################
	POSIX_TESTS = [
	    #
	    # pattern independent tests
	    #

            # POSIX, interTestDelay
	    [{'debug':		'POSIX interTestDelay',
	      'interTestDelay':	0}],
	    [{'debug':		'POSIX interTestDelay',
	      'interTestDelay':	1}],

            # POSIX, intraTestBarriers
	    [{'debug':		'POSIX intraTestBarriers',
	      'intraTestBarriers':	1}],

            # POSIX, uniqueDir
	    [{'debug':		'POSIX uniqueDir',
	      'filePerProc':	1,
	      'uniqueDir':	1}],

            # POSIX, uniqueDir random
	    [{'debug':		'POSIX uniqueDir random',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'uniqueDir':	1}],

            # POSIX, fsync
	    [{'debug':		'POSIX fsync',
	      'fsync':	1}],

            # POSIX, repetitions
	    [{'debug':		'POSIX repetitions',
	      'repetitions':	1}],
	    [{'debug':		'POSIX repetitions',
	      'repetitions':	3}],

            # POSIX, repetitions random
	    [{'debug':		'POSIX repetitions random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'repetitions':	3}],

            # POSIX, multiFile
	    [{'debug':		'POSIX multiFile',
	      'repetitions':	3,
	      'multiFile':	1}],

            # POSIX, multiFile random
	    [{'debug':		'POSIX multiFile',
	      'repetitions':	3,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'multiFile':	1}],

            # POSIX, writeFile-only
	    [{'debug':		'POSIX writeFile-only',
	      'writeFile':	1,
	      'readFile':	0,
	      'checkRead':	0}],

            # POSIX, writeFile-only random
	    [{'debug':		'POSIX writeFile-only random',
	      'writeFile':	1,
	      'randomOffset':	1,
	      'readFile':	0,
	      'checkRead':	0}],

            # POSIX, readFile-only
	    [{'debug':		'POSIX readFile-only',
	      'writeFile':	0,
	      'checkWrite':	0,
	      'readFile':	1}],

            # POSIX, readFile-only
	    [{'debug':		'POSIX readFile-only random',
	      'writeFile':	0,
	      'checkWrite':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'readFile':	1}],

            # POSIX, write/read check off
	    [{'debug':		'POSIX write-read check off',
	      'checkWrite':	0,
	      'checkRead':	0}],

            # POSIX, write/read check off random
	    [{'debug':		'POSIX write-read check off random',
	      'checkWrite':	0,
	      'randomOffset':	1,
	      'checkRead':	0}],

            # POSIX, store file offset
	    [{'debug':		'POSIX store file offset',
	      'storeFileOffset':1}],

            # POSIX, remove file
	    [{'debug':		'POSIX remove file',
	      'keepFile':	0}],

            # POSIX, remove file random
	    [{'debug':		'POSIX remove file',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'keepFile':	0}],

            # POSIX, remove file with error
	    [{'debug':		'POSIX remove file with error',
	      'keepFileWithError':	0}],

            # POSIX, remove file with error random
	    [{'debug':		'POSIX remove file with error random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'keepFileWithError':	0}],


            # POSIX, deadline for stonewalling
	    [{'debug':		'POSIX deadlineForStonewalling',
	      'testFile':	test.DefaultTest()['testFile'] + '.stonewall',
	      'blockSize':	GIBIBYTE,
	      'readFile':	0,
	      'checkWrite':	0,
	      'checkRead':	0,
	      'deadlineForStonewalling':1}],

            # POSIX, deadline for stonewalling random
	    [{'debug':		'POSIX deadlineForStonewalling random',
	      'testFile':	test.DefaultTest()['testFile'] + '.stonewall',
	      'blockSize':	GIBIBYTE,
	      'readFile':	0,
	      'checkWrite':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'deadlineForStonewalling':1}],

            # POSIX, max time duration
	    [{'debug':		'POSIX maxTimeDuration',
	      'maxTimeDuration':1}],

            # POSIX, max time duration random
	    [{'debug':		'POSIX maxTimeDuration random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'maxTimeDuration':1}],

            # POSIX, verbose
	    [{'debug':		'POSIX verbose 0',
	      'verbose':	0}],
	    [{'debug':		'POSIX verbose 1',
	      'verbose':	1}],
	    [{'debug':		'POSIX verbose 2',
	      'verbose':	2}],
	    [{'debug':		'POSIX verbose 3',
	      'verbose':	3}],
	    [{'debug':		'POSIX verbose 4',
	      'verbose':	4,
	      'blockSize':      KIBIBYTE,
	      'transferSize':   (KIBIBYTE / 4)}],


            # POSIX, multiple file names, ssf
	    [{'debug':		'POSIX multiple file names ssf',
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # POSIX, multiple file names, ssf random
	    [{'debug':		'POSIX multiple file names ssf random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # POSIX, multiple file names, fpp
	    [{'debug':		'POSIX multiple file names fpp',
	      'filePerProc':	1,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # POSIX, multiple file names, fpp random
	    [{'debug':		'POSIX multiple file names fpp random',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # POSIX, corruptFile
	    [{'debug':		'POSIX corruptFile',
	      'testFile':	test.DefaultTest()['testFile'],
	      'filePerProc':	0,
	      'corruptFile':	1}],

            # POSIX, corruptFile random
	    [{'debug':		'POSIX corruptFile random',
	      'testFile':	test.DefaultTest()['testFile'],
	      'filePerProc':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'corruptFile':	1}],

            # POSIX, corruptFile
	    [{'debug':		'POSIX corruptFile filePerProc',
	      'filePerProc':	1,
	      'corruptFile':	1}],

            # POSIX, corruptFile random
	    [{'debug':		'POSIX corruptFile filePerProc random',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'corruptFile':	1}],

            # POSIX, showHelp
	    [{'debug':		'POSIX showHelp',
	      'showHelp':	1,
	      'filePerProc':	0,
	      'corruptFile':	0}],

            # POSIX, quitOnError
	    [{'debug':		'POSIX quitOnError',
	      'quitOnError':	1}],

            # POSIX, quitOnError random
	    [{'debug':		'POSIX quitOnError random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'quitOnError':	1}],

            # POSIX, singleXferAttempt
	    [{'debug':		'POSIX singleXferAttempt',
	      'singleXferAttempt':	1}],

            # POSIX, singleXferAttempt random
	    [{'debug':		'POSIX singleXferAttempt random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'singleXferAttempt':	1}],

            # POSIX, setTimeStampSignature
	    [{'debug':		'POSIX setTimeStampSignature',
	      'setTimeStampSignature':	123}],

            # POSIX, setTimeStampSignature random
	    [{'debug':		'POSIX setTimeStampSignature random',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'setTimeStampSignature':	123}],

            # POSIX, useExistingTestFile [Note: don't follow HDF5 test]
	    [{'debug':		'POSIX useExistingTestFile',
	      'useExistingTestFile':	1}],

            # POSIX, useExistingTestFile random [Note: don't follow HDF5 test]
	    [{'debug':		'POSIX useExistingTestFile',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'useExistingTestFile':	1}],

	    #
	    # pattern dependent tests
	    #

            # POSIX, filePerProc
	    [{'debug':		'POSIX filePerProc',
	      'filePerProc':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX filePerProc',
	      'filePerProc':	1,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX filePerProc',
	      'filePerProc':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'POSIX filePerProc',
	      'filePerProc':	1,
	      'segmentCount':	3}],

            # POSIX, sharedFile
	    [{'debug':		'POSIX sharedFile',
	      'filePerProc':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX sharedFile',
	      'filePerProc':	0,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX sharedFile',
	      'filePerProc':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'POSIX sharedFile',
	      'filePerProc':	0,
	      'segmentCount':	3}],

	    [{'debug':		'POSIX sharedFile numTasks',
	      'filePerProc':	0,
	      'numTasks':	2,
	      'segmentCount':	3}],

	    #
	    # pattern dependent tests, random
	    #

            # POSIX, filePerProc random
	    [{'debug':		'POSIX filePerProc random',
	      'filePerProc':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX filePerProc random',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX filePerProc random',
	      'filePerProc':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'POSIX filePerProc random',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

            # POSIX, sharedFile random
	    [{'debug':		'POSIX sharedFile random',
	      'filePerProc':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX sharedFile random',
	      'filePerProc':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'POSIX sharedFile random',
	      'filePerProc':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'POSIX sharedFile random',
	      'filePerProc':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'POSIX sharedFile numTasks random',
	      'filePerProc':	0,
	      'numTasks':	2,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}]
	]

	MPIIO_TESTS = [
	    #
	    # pattern independent tests
	    #

            # MPIIO, repetitions
	    [{'debug':		'MPIIO repetitions',
	      'api':		'MPIIO',
	      'repetitions':	1}],
	    [{'debug':		'MPIIO repetitions',
	      'api':		'MPIIO',
	      'repetitions':	3}],
	    [{'debug':		'MPIIO repetitions random',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'repetitions':	3}],

            # MPIIO, multiFile
	    [{'debug':		'MPIIO multiFile',
	      'api':		'MPIIO',
	      'repetitions':	3,
	      'multiFile':	1}],

            # MPIIO, multiFile random
	    [{'debug':		'MPIIO multiFile random',
	      'api':		'MPIIO',
	      'repetitions':	3,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'multiFile':	1}],

            # MPIIO, remove file
	    [{'debug':		'MPIIO remove file',
	      'api':		'MPIIO',
	      'keepFile':	0}],

            # MPIIO, remove file random
	    [{'debug':		'MPIIO remove file',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'keepFile':	0}],

            # MPIIO, remove file with error
	    [{'debug':		'MPIIO remove file with error',
	      'api':		'MPIIO',
	      'keepFileWithError':	0}],

            # MPIIO, remove file with error random
	    [{'debug':		'MPIIO remove file with error random',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'keepFileWithError':	0}],

            # MPIIO, deadline for stonewalling
	    [{'debug':		'MPIIO deadlineForStonewalling',
	      'api':		'MPIIO',
	      'testFile':	test.DefaultTest()['testFile'] + '.stonewall',
	      'blockSize':	GIBIBYTE,
	      'readFile':	0,
	      'checkWrite':	0,
	      'checkRead':	0,
	      'deadlineForStonewalling':1}],

            # MPIIO, deadline for stonewalling random
	    [{'debug':		'MPIIO deadlineForStonewalling random',
	      'api':		'MPIIO',
	      'testFile':	test.DefaultTest()['testFile'] + '.stonewall',
	      'blockSize':	GIBIBYTE,
	      'readFile':	0,
	      'checkWrite':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'deadlineForStonewalling':1}],

            # MPIIO, max time duration
	    [{'debug':		'MPIIO maxTimeDuration',
	      'api':		'MPIIO',
	      'maxTimeDuration':1}],

            # MPIIO, max time duration random
	    [{'debug':		'MPIIO maxTimeDuration random',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'maxTimeDuration':1}],

            # MPIIO, quitOnError
	    [{'debug':		'MPIIO quitOnError',
	      'api':		'MPIIO',
	      'quitOnError':	1}],

            # MPIIO, quitOnError random
	    [{'debug':		'MPIIO quitOnError random',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'quitOnError':	1}],

            # MPIIO, multiple file names, ssf
	    [{'debug':		'MPIIO multiple file names ssf',
	      'api':		'MPIIO',
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # MPIIO, multiple file names, ssf random
	    [{'debug':		'MPIIO multiple file names ssf random',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # MPIIO, multiple file names, fpp
	    [{'debug':		'MPIIO multiple file names fpp',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # MPIIO, multiple file names, fpp random
	    [{'debug':		'MPIIO multiple file names fpp random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # MPIIO, corruptFile
	    [{'debug':		'MPIIO corruptFile',
	      'api':		'MPIIO',
	      'testFile':	test.DefaultTest()['testFile'],
	      'filePerProc':	0,
	      'corruptFile':	1}],

            # MPIIO, corruptFile random
	    [{'debug':		'MPIIO corruptFile random',
	      'api':		'MPIIO',
	      'testFile':	test.DefaultTest()['testFile'],
	      'filePerProc':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'corruptFile':	1}],

            # MPIIO, corruptFile
	    [{'debug':		'MPIIO corruptFile filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'corruptFile':	1}],

            # MPIIO, corruptFile random
	    [{'debug':		'MPIIO corruptFile filePerProc random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'corruptFile':	1}],

            # MPIIO, useExistingTestFile
	    [{'debug':		'MPIIO useExistingTestFile',
	      'api':		'MPIIO',
	      'useExistingTestFile':	0,
	      'filePerProc':	0,
	      'corruptFile':	0}],

            # MPIIO, useExistingTestFile random
	    [{'debug':		'MPIIO useExistingTestFile random',
	      'api':		'MPIIO',
	      'useExistingTestFile':	0,
	      'filePerProc':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'corruptFile':	0}],

            # MPIIO, preallocate
	    [{'debug':		'MPIIO preallocate',
	      'api':		'MPIIO',
	      'preallocate':	1}],

            # MPIIO, showHints
	    [{'debug':		'MPIIO showHints',
	      'api':		'MPIIO',
	      'showHints':	1}],

            # MPIIO, showHints w/hintsFileName
	    [{'debug':		'MPIIO showHints w/hintsFileName',
	      'api':		'MPIIO',
	      'hintsFileName':	'/g/g0/loewe/IOR/test/hintsFile',
	      'showHints':	1}],

            # MPIIO, setTimeStampSignature
	    [{'debug':		'MPIIO setTimeStampSignature',
	      'api':		'MPIIO',
	      'setTimeStampSignature':	123}],

            # MPIIO, setTimeStampSignature random
	    [{'debug':		'MPIIO setTimeStampSignature random',
	      'api':		'MPIIO',
	      'randomOffset':	1,
	      'checkRead':	0,
	      'setTimeStampSignature':	123}],

	    #
	    # pattern dependent tests
	    #

            # MPIIO, independent
	    [{'debug':		'MPIIO independent',
	      'api':		'MPIIO',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent',
	      'api':		'MPIIO',
	      'collective':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent',
	      'api':		'MPIIO',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent',
	      'api':		'MPIIO',
	      'collective':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent numTasks',
	      'api':		'MPIIO',
	      'numTasks':	2,
	      'collective':	0,
	      'segmentCount':	3}],

            # MPIIO, independent random
	    [{'debug':		'MPIIO independent random',
	      'api':		'MPIIO',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent random',
	      'api':		'MPIIO',
	      'collective':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent random',
	      'api':		'MPIIO',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent random',
	      'api':		'MPIIO',
	      'collective':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent numTasks random',
	      'api':		'MPIIO',
	      'numTasks':	2,
	      'collective':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

            # MPIIO, collective
	    [{'debug':		'MPIIO collective',
	      'api':		'MPIIO',
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective',
	      'api':		'MPIIO',
	      'collective':	1,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective',
	      'api':		'MPIIO',
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective',
	      'api':		'MPIIO',
	      'collective':	1,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective numTasks',
	      'api':		'MPIIO',
	      'numTasks':	2,
	      'collective':	1,
	      'segmentCount':	3}],

            # MPIIO, independent, useFileView
	    [{'debug':		'MPIIO independent useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent useFileView numTasks',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'numTasks':	2,
	      'collective':	0,
	      'segmentCount':	3}],

            # MPIIO, collective, useFileView
	    [{'debug':		'MPIIO collective useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	1,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective useFileView',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'collective':	1,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective useFileView numTasks',
	      'api':		'MPIIO',
	      'useFileView':	1,
	      'numTasks':	2,
	      'collective':	1,
	      'segmentCount':	3}],

            # MPIIO, independent, filePerProc
	    [{'debug':		'MPIIO independent filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent filePerProc numTasks',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'numTasks':	2,
	      'collective':	0,
	      'segmentCount':	3}],

            # MPIIO, independent, filePerProc random
	    [{'debug':		'MPIIO independent filePerProc random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent filePerProc random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent filePerProc random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent filePerProc random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent filePerProc numTasks random',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'numTasks':	2,
	      'collective':	0,
	      'randomOffset':	1,
	      'checkRead':	0,
	      'segmentCount':	3}],

            # MPIIO, collective, filePerProc
	    [{'debug':		'MPIIO collective filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	1,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective filePerProc',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'collective':	1,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective filePerProc numTasks',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'numTasks':	2,
	      'collective':	1,
	      'segmentCount':	3}],

            # MPIIO, independent, filePerProc, useFileView
	    [{'debug':		'MPIIO independent filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	0,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO independent filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	0,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO independent filePerProc useFileView numTasks',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'numTasks':	2,
	      'useFileView':	1,
	      'collective':	0,
	      'segmentCount':	3}],

            # MPIIO, collective, filePerProc, useFileView
	    [{'debug':		'MPIIO collective filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	1,
	      'segmentCount':	1}],

	    [{'debug':		'MPIIO collective filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective filePerProc useFileView',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'useFileView':	1,
	      'collective':	1,
	      'segmentCount':	3}],

	    [{'debug':		'MPIIO collective filePerProc useFileView numTasks',
	      'api':		'MPIIO',
	      'filePerProc':	1,
	      'numTasks':	2,
	      'useFileView':	1,
	      'collective':	1,
	      'segmentCount':	3}]
	]

	HDF5_TESTS = [
	    #
	    # pattern independent tests
	    #

            # HDF5, repetitions
	    [{'debug':		'HDF5 repetitions',
	      'api':		'HDF5',
	      'repetitions':	1}],
	    [{'debug':		'HDF5 repetitions',
	      'api':		'HDF5',
	      'repetitions':	3}],

            # HDF5, multiFile
	    [{'debug':		'HDF5 multiFile',
	      'api':		'HDF5',
	      'repetitions':	3,
	      'multiFile':	1}],

            # HDF5, useExistingTestFile [Note: this must follow HDF5 test]
	    [{'debug':		'HDF5 useExistingTestFile',
	      'api':		'HDF5',
	      'useExistingTestFile':	1}],

            # HDF5, remove file
	    [{'debug':		'HDF5 remove file',
	      'api':		'HDF5',
	      'keepFile':	0}],

            # HDF5, remove file with error
	    [{'debug':		'HDF5 remove file with error',
	      'api':		'HDF5',
	      'keepFileWithError':	0}],

            # HDF5, deadline for stonewalling
	    [{'debug':		'HDF5 deadlineForStonewalling',
	      'api':		'HDF5',
	      'testFile':	test.DefaultTest()['testFile'] + '.stonewall',
	      'blockSize':	GIBIBYTE/4,
	      'readFile':	0,
	      'checkWrite':	0,
	      'checkRead':	0,
	      'deadlineForStonewalling':1}],

            # HDF5, max time duration
	    [{'debug':		'HDF5 maxTimeDuration',
	      'api':		'HDF5',
	      'maxTimeDuration':1}],

            # HDF5, quitOnError
	    [{'debug':		'HDF5 quitOnError',
	      'api':		'HDF5',
	      'quitOnError':	1}],

            # HDF5, multiple file names, ssf
	    [{'debug':		'HDF5 multiple file names ssf',
	      'api':		'HDF5',
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # HDF5, multiple file names, fpp
	    [{'debug':		'HDF5 multiple file names fpp',
	      'api':		'HDF5',
	      'filePerProc':	1,
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # HDF5, corruptFile
	    [{'debug':		'HDF5 corruptFile',
	      'api':		'HDF5',
	      'testFile':	test.DefaultTest()['testFile'],
	      'filePerProc':	0,
	      'corruptFile':	1}],

            # HDF5, corruptFile
	    [{'debug':		'HDF5 corruptFile filePerProc',
	      'api':		'HDF5',
	      'filePerProc':	1,
	      'corruptFile':	1}],

            # HDF5, setTimeStampSignature
	    [{'debug':		'HDF5 setTimeStampSignature',
	      'api':		'HDF5',
	      'setTimeStampSignature':	123,
	      'filePerProc':	0,
	      'corruptFile':	0}],

            # HDF5, setAlignment
	    [{'debug':	'HDF5 setAlignment',
	      'api':		'HDF5',
	      'setAlignment':	'4m'}],

            # HDF5, showHints
	    [{'debug':		'HDF5 showHints',
	      'api':		'HDF5',
	      'showHints':	0}], # WEL: omit this test until
                                     #      showHints works

            # HDF5, showHints w/hintsFileName
	    [{'debug':		'HDF5 showHints w/hintsFileName',
	      'api':		'HDF5',
	      'hintsFileName':	'/g/g0/loewe/IOR/test/hintsFile',
	      'showHints':	0}], # WEL: omit this test until
                                     #      showHints works

            # HDF5, noFill
	    [{'debug':	'HDF5 noFill',
	      'api':		'HDF5',
	      'noFill':		0}], # WEL: omit this test until
                                     #      noFill is standard

	    #
	    # pattern dependent tests
	    #

            # HDF5, independent
	    [{'debug':		'HDF5 independent',
	      'api':		'HDF5',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'HDF5 independent',
	      'api':		'HDF5',
	      'collective':	0,
	      'segmentCount':	1}],

	    [{'debug':		'HDF5 independent',
	      'api':		'HDF5',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'HDF5 independent',
	      'api':		'HDF5',
	      'collective':	0,
	      'segmentCount':	3}],

	    [{'debug':		'HDF5 independent numTasks',
	      'api':		'HDF5',
	      'numTasks':	2,
	      'collective':	0,
	      'segmentCount':	3}],

            # HDF5, collective
	    [{'debug':		'HDF5 collective',
	      'api':		'HDF5',
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'HDF5 collective',
	      'api':		'HDF5',
	      'collective':	1,
	      'segmentCount':	1}],

	    [{'debug':		'HDF5 collective',
	      'api':		'HDF5',
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'HDF5 collective',
	      'api':		'HDF5',
	      'collective':	1,
	      'segmentCount':	3}],

	    [{'debug':		'HDF5 collective numTasks',
	      'api':		'HDF5',
	      'numTasks':	2,
	      'collective':	1,
	      'segmentCount':	3}]
	]

	NCMPI_TESTS = [
	    #
	    # pattern independent tests
	    #

            # NCMPI, repetitions
	    [{'debug':		'NCMPI repetitions',
	      'api':		'NCMPI',
	      'repetitions':	1}],
	    [{'debug':		'NCMPI repetitions',
	      'api':		'NCMPI',
	      'repetitions':	3}],

            # NCMPI, multiFile
	    [{'debug':		'NCMPI multiFile',
	      'api':		'NCMPI',
	      'repetitions':	3,
	      'multiFile':	1}],

            # NCMPI, deadline for stonewalling
	    [{'debug':		'NCMPI deadlineForStonewalling',
	      'api':		'NCMPI',
	      'testFile':	test.DefaultTest()['testFile'] + '.stonewall',
	      'blockSize':	GIBIBYTE/4,
	      'readFile':	0,
	      'checkWrite':	0,
	      'checkRead':	0,
	      'deadlineForStonewalling':1}],

            # NCMPI, max time duration
	    [{'debug':		'NCMPI maxTimeDuration',
	      'api':		'NCMPI',
	      'maxTimeDuration':1}],

            # NCMPI, remove file
	    [{'debug':		'NCMPI remove file',
	      'api':		'NCMPI',
	      'keepFile':	0}],

            # NCMPI, remove file with error
	    [{'debug':		'NCMPI remove file with error',
	      'api':		'NCMPI',
	      'keepFileWithError':	0}],

            # NCMPI, quitOnError
	    [{'debug':		'NCMPI quitOnError',
	      'api':		'NCMPI',
	      'quitOnError':	1}],

            # NCMPI, multiple file names, ssf
	    [{'debug':		'NCMPI multiple file names ssf',
	      'api':		'NCMPI',
	      'testFile':	testDir + '/f1@' + testDir + '/f2'}],

            # NCMPI, corruptFile
	    [{'debug':		'NCMPI corruptFile',
	      'api':		'NCMPI',
	      'testFile':	test.DefaultTest()['testFile'],
	      'corruptFile':	1}],

            # NCMPI, setTimeStampSignature
	    [{'debug':		'NCMPI setTimeStampSignature',
	      'api':		'NCMPI',
	      'setTimeStampSignature':	123,
	      'corruptFile':	0}],

            # NCMPI, useExistingTestFile [Note: this must follow NCMPI test]
	    [{'debug':		'NCMPI useExistingTestFile',
	      'api':		'NCMPI',
	      'useExistingTestFile':	1}],

	    #
	    # pattern dependent tests
	    #

            # NCMPI, independent
	    [{'debug':		'NCMPI independent',
	      'api':		'NCMPI',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'NCMPI independent',
	      'api':		'NCMPI',
	      'collective':	0,
	      'segmentCount':	1}],

	    [{'debug':		'NCMPI independent',
	      'api':		'NCMPI',
	      'collective':	0,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'NCMPI independent',
	      'api':		'NCMPI',
	      'collective':	0,
	      'segmentCount':	3}],

	    [{'debug':		'NCMPI independent numTasks',
	      'api':		'NCMPI',
	      'numTasks':	2,
	      'collective':	0,
	      'segmentCount':	3}],

            # NCMPI, collective
	    [{'debug':		'NCMPI collective',
	      'api':		'NCMPI',
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	1}],

	    [{'debug':		'NCMPI collective',
	      'api':		'NCMPI',
	      'collective':	1,
	      'segmentCount':	1}],

	    [{'debug':		'NCMPI collective',
	      'api':		'NCMPI',
	      'collective':	1,
	      'blockSize':	IOR_SIZE_T,
	      'transferSize':	IOR_SIZE_T,
	      'segmentCount':	3}],

	    [{'debug':		'NCMPI collective',
	      'api':		'NCMPI',
	      'collective':	1,
	      'segmentCount':	3}],

	    [{'debug':		'NCMPI collective numTasks',
	      'api':		'NCMPI',
	      'numTasks':	2,
	      'collective':	1,
	      'segmentCount':	3}]
	]

	PassTests = []
	if OS == "AIX":
	    PassTests = PassTests + POSIX_TESTS
	    PassTests = PassTests + MPIIO_TESTS
	    PassTests = PassTests + HDF5_TESTS
	    PassTests = PassTests + NCMPI_TESTS
	elif OS == "Linux":
	    PassTests = PassTests + POSIX_TESTS
	    PassTests = PassTests + MPIIO_TESTS
	    #PassTests = PassTests + HDF5_TESTS
	    #PassTests = PassTests + NCMPI_TESTS
	else:
	    PassTests = [
		[{'debug':		'failure to determine OS'}]
	    ]

	FailTests = [
	    [{'debug':		'no tests should fail'}]
	]

################################################################################
################################################################################
#                                                                              #
#                           E N D   O F   T E S T S                            #
#                                                                              #
################################################################################
################################################################################
	# WEL debugging: if 0 == 0: PassTests = [ [{'debug': 'debug test'}] ]

	if expectation == PASS:
	    if testNumber == RETURN_TOTAL_TESTS:
		return len(PassTests)
	    else:
		return PassTests[testNumber]
	else:	# expectation == FAIL
	    if testNumber == RETURN_TOTAL_TESTS:
		return len(FailTests)
	    else:
		return FailTests[testNumber]


    ###################
    # run test script #
    ###################
    def RunScript(self, nodes, procs):
	if OS == "AIX":
	    command = "poe " + executable + " -f " + scriptFile + \
			" -nodes " + str(nodes) + " -procs " + str(procs) + \
			" -rmpool systest -labelio no -retry wait"
	elif OS == "Linux":
	    command = "srun -N " + str(procs) + " -n " + str(procs) + \
			" -ppdebug " + executable + " -f " + scriptFile
	else:
	    command = "unable to run " + executable + " -f " + scriptFile
	if debug == TRUE:
	    Flush2File(command)
	else:
	    childIn, childOut = os.popen4(command)
	    childIn.close()
	    while 1:
		line = childOut.readline()
		if line == '': break
		Flush2File(line[:-1])
	    childOut.close()
	return


##########################
# create subsets of list #
##########################
def ListSubsets(tests, size):
    listOfSubsets = []
    start = end = 0
    totalTestsSize = len(tests)
    for i in range(0, (totalTestsSize / size) + 1):
        if end >= totalTestsSize:
            break
        end = end + size
        listOfSubsets.append(tests[start:end])
        start = end
    return listOfSubsets


#################
# flush to file #
#################
def Flush2File(string):
    resultsFile.write(string + '\n')
    resultsFile.flush()


##################################
# replace blanks with underscore #
##################################
def UnderScore(string):
    uString = string
    for i in range(0, len(uString)):
	if uString[i] == ' ':
	    uString = uString[:i] + '_' + uString[i+1:]
    return(uString)


#############################
# grep for keywords in file #
#############################
def grepForKeywords(keywords, resultsFileName):
	# create pattern for grep
	pattern = "\""
	for i in range(len(keywords)):
		pattern = pattern + keywords[i]
		if i < len(keywords)-1:
			pattern = pattern + '|'
	pattern = pattern + "\""

	# grep for pattern in file
	resultsFileNameTmp = resultsFileName + ".tmp"
	cmd = "grep -i -E " + pattern + " " + resultsFileName \
		+ " >> " + resultsFileNameTmp + " 2>&1"
	os.system(cmd)
	cmd = "cat " + resultsFileNameTmp + " >> " + resultsFileName
	os.system(cmd)
	cmd = "rm -f " + resultsFileNameTmp
	os.system(cmd)


################################################################################
# main                                                                         #
################################################################################
resultsFileName = "./test-results.txt-" + \
                  os.popen("date +%m.%d.%y").read()[:-1]
resultsFile = open(resultsFileName, "w")
OS = os.popen("uname -s").read()[:-1]
test = Test()
testNumber = 0

#environment variables
nodes     = 1
proccnt   = [1, 3]

Flush2File("TESTING IOR C CODE")

# loop through different processors counts
for proc in proccnt:

    # first run all expected-PASS test, then the FAILs
    for testType in (PASS, FAIL):

        # test type info
	if (testType == PASS):
	    Flush2File("\n*** STARTING EXPECTED  P A S S  TESTS (PROC=" \
		+ str(proc) + ") ***")
        else:
	    Flush2File("\n*** STARTING EXPECTED  F A I L  TESTS (PROC=" \
		+ str(proc) + ") ***")

        # loop through all tests for test type
        totalTests = range(test.Tests(testType, RETURN_TOTAL_TESTS))
        firstTest = TRUE
        for testSubset in ListSubsets(totalTests, TEST_SUBSET_SIZE):
            for i in testSubset:
                if (firstTest == TRUE):
                    Flush2File("\n\n*** Setting up tests ***")
		    firstTest = FALSE
	        if (i % 10 == 0 and i != 0):
                    Flush2File("finished " + str(i) + " tests")
		    sys.stdout.flush()
                # unless an expected fail test, only open a single script
	        # create script file name
	        if (testType == PASS):
		    scriptFile = scriptFileBase + '.' + str(proc) + '.PASS'
	        else:
		    scriptFile = scriptFileBase + '.' + str(proc) + '.FAIL'

		scriptFile = scriptFile + '-TESTS_' + str(testSubset[0:1][0]) \
			+ '-' + str(testSubset[len(testSubset)-1:][0])
    	        if ((i % TEST_SUBSET_SIZE == 0) or (testType == FAIL)):
                    os.system("rm -f " + scriptFile)
                    script = open(scriptFile, "a")
                    script.write("IOR START" + "\n")

                # start with a default test, then modify
                testValues = test.DefaultTest()
        
                # loop through all changes to the default script
                for j in test.Tests(testType, i)[TEST_SETTINGS].keys():
                    testValues[j] = test.Tests(testType, i)[TEST_SETTINGS][j]
        
                testNumber = testNumber + 1
	        testValues['debug'] = UnderScore("Test No. " + \
				      str(testNumber) + ":  " + \
				      testValues['debug'])
	        # write test information to script file
                for entry in testValues.keys():
		    if (str(testValues[entry]) != ''):
                        script.write("\t" + entry + "=" + 
				     str(testValues[entry]) + "\n")
                script.write("RUN" + "\n")

                # unless an expected fail test, only close a single script
    	        if ((i == testSubset[len(testSubset)-1:][0]) \
		    or (testType == FAIL)):
                    # create tail, close file
                    script.write("IOR STOP" + "\n")
                    script.close()
                    if (testType == FAIL):
                        Flush2File("finished 1 test")
		    else:
                        Flush2File("finished %d test%s" % ((i + 1), "s"[i==1:]))
		    firstTest = TRUE
                
                    # display test info for failing test (note that the
		    # test fails before a description is displayed)
                    if testType == FAIL:
                        Flush2File("\n\t================================\n\n")
                        Flush2File("*** DEBUG MODE ***")
                        Flush2File("*** " + str(testValues['debug']) + \
			           " ***")
                        Flush2File("")
            
                    # run
		    os.system ("rm -rf " + testDir)
		    os.system("mkdir " + testDir)
                    test.RunScript(nodes, proc)
                    os.system ("rm -rf " + testDir)
            if 0 == 0: os.system("rm -f " + scriptFile) # run scripts

Flush2File("\nFINISHED TESTING IOR C CODE")
Flush2File("\nRESULTS:")
resultsFile.close()
grepForKeywords(["warn", "fail", "error", "Test_No"], resultsFileName)
