/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
\******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>              /* tolower() */
#include <errno.h>
#include <math.h>
#include <mpi.h>
#include <string.h>

#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif

#include <sys/stat.h>           /* struct stat */
#include <time.h>

#ifndef _WIN32
# include <sys/time.h>           /* gettimeofday() */
# include <sys/utsname.h>        /* uname() */
#endif

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif

#include <assert.h>

#include "ior.h"
#include "ior-internal.h"
#include "aiori.h"
#include "utilities.h"
#include "parse_options.h"

enum {
        IOR_TIMER_OPEN_START,
        IOR_TIMER_OPEN_STOP,
        IOR_TIMER_RDWR_START,
        IOR_TIMER_RDWR_STOP,
        IOR_TIMER_CLOSE_START,
        IOR_TIMER_CLOSE_STOP,
        IOR_NB_TIMERS
};

/* file scope globals */
extern char **environ;
static int totalErrorCount;
static const ior_aiori_t *backend;

static void DestroyTests(IOR_test_t *tests_head);
static char *PrependDir(IOR_param_t *, char *);
static char **ParseFileName(char *, int *);
static void InitTests(IOR_test_t *);
static void TestIoSys(IOR_test_t *);
static void ValidateTests(IOR_param_t * params, MPI_Comm com);
static IOR_offset_t WriteOrRead(IOR_param_t *test, int rep, IOR_results_t *results,
                                aiori_fd_t *fd, const int access,
                                IOR_io_buffers *ioBuffers);

static void ior_set_xfer_hints(IOR_param_t * p){
  aiori_xfer_hint_t * hints = & p->hints;
  hints->dryRun = p->dryRun;
  hints->filePerProc = p->filePerProc;
  hints->collective = p->collective;
  hints->numTasks = p->numTasks;
  hints->numNodes = p->numNodes;
  hints->randomOffset = p->randomOffset;
  hints->fsyncPerWrite = p->fsyncPerWrite;
  hints->segmentCount = p->segmentCount;
  hints->blockSize = p->blockSize;
  hints->transferSize = p->transferSize;
  hints->expectedAggFileSize = p->expectedAggFileSize;
  hints->singleXferAttempt = p->singleXferAttempt;

  if(backend->xfer_hints){
    backend->xfer_hints(hints);
  }
}

int aiori_warning_as_errors = 0;

/*
 Returns 1 if the process participates in the test
 */
static int test_initialize(IOR_test_t * test){
  int range[3];
  IOR_param_t *params = &test->params;
  MPI_Group orig_group, new_group;

  /* set up communicator for test */
  MPI_CHECK(MPI_Comm_group(params->mpi_comm_world, &orig_group),
            "MPI_Comm_group() error");
  range[0] = 0;                     /* first rank */
  range[1] = params->numTasks - 1;  /* last rank */
  range[2] = 1;                     /* stride */
  MPI_CHECK(MPI_Group_range_incl(orig_group, 1, &range, &new_group),
            "MPI_Group_range_incl() error");
  MPI_CHECK(MPI_Comm_create(params->mpi_comm_world, new_group, & params->testComm),
            "MPI_Comm_create() error");
  MPI_CHECK(MPI_Group_free(&orig_group), "MPI_Group_Free() error");
  MPI_CHECK(MPI_Group_free(&new_group), "MPI_Group_Free() error");


  if (params->testComm == MPI_COMM_NULL) {
    /* tasks not in the group do not participate in this test, this matches the proceses in test_finalize() that participate */
    MPI_CHECK(MPI_Barrier(params->mpi_comm_world), "barrier error");
    return 0;
  }

  /* Setup global variables */
  testComm = params->testComm;
  verbose = test->params.verbose;
  backend = test->params.backend;

  if(test->params.gpuMemoryFlags != IOR_MEMORY_TYPE_CPU){
    initCUDA(test->params.tasksBlockMapping, rank, test->params.numNodes, test->params.numTasksOnNode0, test->params.gpuID);
  }
  

  if(backend->initialize){
    backend->initialize(test->params.backend_options);
  }
  ior_set_xfer_hints(& test->params);
  aiori_warning_as_errors = test->params.warningAsErrors;

  if (rank == 0 && verbose >= VERBOSE_0) {
    ShowTestStart(& test->params);
  }
  return 1;
}

static void test_finalize(IOR_test_t * test){
  backend = test->params.backend;
  if(backend->finalize){
    backend->finalize(test->params.backend_options);
  }
  MPI_CHECK(MPI_Barrier(test->params.mpi_comm_world), "barrier error");
  MPI_CHECK(MPI_Comm_free(& testComm), "MPI_Comm_free() error");
}


IOR_test_t * ior_run(int argc, char **argv, MPI_Comm world_com, FILE * world_out){
        IOR_test_t *tests_head;
        IOR_test_t *tptr;
        out_logfile = world_out;
        out_resultfile = world_out;

        MPI_CHECK(MPI_Comm_rank(world_com, &rank), "cannot get rank");

        /* setup tests, and validate parameters */
        tests_head = ParseCommandLine(argc, argv, world_com);
        InitTests(tests_head);

        PrintHeader(argc, argv);

        /* perform each test */
        for (tptr = tests_head; tptr != NULL; tptr = tptr->next) {
                int participate = test_initialize(tptr);
                if( ! participate ) continue;
                totalErrorCount = 0;
                TestIoSys(tptr);
                tptr->results->errors = totalErrorCount;
                ShowTestEnd(tptr);
                test_finalize(tptr);
        }

        PrintLongSummaryAllTests(tests_head);

        /* display finish time */
        PrintTestEnds();
        return tests_head;
}



int ior_main(int argc, char **argv)
{
    IOR_test_t *tests_head;
    IOR_test_t *tptr;

    out_logfile = stdout;
    out_resultfile = stdout;

    /* start the MPI code */
    MPI_CHECK(MPI_Init(&argc, &argv), "cannot initialize MPI");

    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank), "cannot get rank");

    /*
     * check -h option from commandline without starting MPI;
     */
    tests_head = ParseCommandLine(argc, argv, MPI_COMM_WORLD);

    /* set error-handling */
    /*MPI_CHECK(MPI_Errhandler_set(mpi_comm_world, MPI_ERRORS_RETURN),
       "cannot set errhandler"); */

    /* setup tests, and validate parameters */
    InitTests(tests_head);

    PrintHeader(argc, argv);

    /* perform each test */
    for (tptr = tests_head; tptr != NULL; tptr = tptr->next) {
            int participate = test_initialize(tptr);
            if( ! participate ) continue;

            // This is useful for trapping a running MPI process.  While
            // this is sleeping, run the script 'testing/hdfs/gdb.attach'
            if (verbose >= VERBOSE_4) {
                    fprintf(out_logfile, "\trank %d: sleeping\n", rank);
                    sleep(5);
                    fprintf(out_logfile, "\trank %d: awake.\n", rank);
            }

            TestIoSys(tptr);
            ShowTestEnd(tptr);
            test_finalize(tptr);
    }

    if (verbose <= VERBOSE_0)
            /* always print final summary */
            verbose = VERBOSE_1;
    PrintLongSummaryAllTests(tests_head);

    /* display finish time */
    PrintTestEnds();

    MPI_CHECK(MPI_Finalize(), "cannot finalize MPI");

    DestroyTests(tests_head);

    return totalErrorCount;
}

/***************************** F U N C T I O N S ******************************/

/*
 * Initialize an IOR_param_t structure to the defaults
 */
void init_IOR_Param_t(IOR_param_t * p, MPI_Comm com)
{
        const char *default_aiori = aiori_default ();
        assert (NULL != default_aiori);

        memset(p, 0, sizeof(IOR_param_t));
        p->api = strdup(default_aiori);
        p->platform = strdup("HOST(OSTYPE)");
        p->testFileName = strdup("testFile");

        p->writeFile = p->readFile = FALSE;
        p->checkWrite = p->checkRead = FALSE;
        
        p->minTimeDuration = 0;
        
        /*
         * These can be overridden from the command-line but otherwise will be
         * set from MPI.
         */
        p->numTasks = -1;
        p->numNodes = -1;
        p->numTasksOnNode0 = -1;
        p->gpuID = -1;

        p->repetitions = 1;
        p->repCounter = -1;
        p->open = WRITE;
        p->taskPerNodeOffset = 1;
        p->segmentCount = 1;
        p->blockSize = 1048576;
        p->transferSize = 262144;
        p->randomSeed = -1;
        p->incompressibleSeed = 573;
        p->testComm = com; // this com might change for smaller tests
        p->mpi_comm_world = com;

        p->URI = NULL;
}

static void
DisplayOutliers(int numTasks,
                double timerVal,
                char *timeString, int access, int outlierThreshold)
{
        char accessString[MAX_STR];
        double sum, mean, sqrDiff, var, sd;

        /* for local timerVal, don't compensate for wall clock delta */
        //timerVal += wall_clock_delta;

        MPI_CHECK(MPI_Allreduce
                  (&timerVal, &sum, 1, MPI_DOUBLE, MPI_SUM, testComm),
                  "MPI_Allreduce()");
        mean = sum / numTasks;
        sqrDiff = pow((mean - timerVal), 2);
        MPI_CHECK(MPI_Allreduce
                  (&sqrDiff, &var, 1, MPI_DOUBLE, MPI_SUM, testComm),
                  "MPI_Allreduce()");
        var = var / numTasks;
        sd = sqrt(var);

        if (access == WRITE) {
                strcpy(accessString, "write");
        } else {                /* READ */
                strcpy(accessString, "read");
        }
        if (fabs(timerVal - mean) > (double)outlierThreshold) {
                char hostname[MAX_STR];
                int ret = gethostname(hostname, MAX_STR);
                if (ret != 0)
                        strcpy(hostname, "unknown");

                WARNF("for %s, task %d, %s %s is %f (mean=%f, stddev=%f)\n",
                        hostname, rank, accessString, timeString, timerVal,  mean, sd);
        }
}

/*
 * Check for outliers in start/end times and elapsed create/xfer/close times.
 */
static void
CheckForOutliers(IOR_param_t *test, const double *timer, const int access)
{
        DisplayOutliers(test->numTasks, timer[IOR_TIMER_OPEN_START],
                        "start time", access, test->outlierThreshold);
        DisplayOutliers(test->numTasks,
                        timer[IOR_TIMER_OPEN_STOP] - timer[IOR_TIMER_OPEN_START],
                        "elapsed create time", access, test->outlierThreshold);
        DisplayOutliers(test->numTasks,
                        timer[IOR_TIMER_RDWR_STOP] - timer[IOR_TIMER_RDWR_START],
                        "elapsed transfer time", access,
                        test->outlierThreshold);
        DisplayOutliers(test->numTasks,
                        timer[IOR_TIMER_CLOSE_STOP] - timer[IOR_TIMER_CLOSE_START],
                        "elapsed close time", access, test->outlierThreshold);
        DisplayOutliers(test->numTasks, timer[IOR_TIMER_CLOSE_STOP], "end time",
                        access, test->outlierThreshold);
}

/*
 * Check if actual file size equals expected size; if not use actual for
 * calculating performance rate.
 */
static void CheckFileSize(IOR_test_t *test, char * testFilename, IOR_offset_t dataMoved, int rep, const int access)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;
        IOR_point_t *point = (access == WRITE) ? &results[rep].write :
                                                 &results[rep].read;

        /* get the size of the file */
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;
        aggFileSizeFromStat = backend->get_file_size(params->backend_options,  testFilename);

        if (params->hints.filePerProc == TRUE) {
            MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpSum, 1,
                                    MPI_LONG_LONG_INT, MPI_SUM, testComm),
                      "cannot reduce total data moved");
            aggFileSizeFromStat = tmpSum;
        } else {
            MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMin, 1,
                                    MPI_LONG_LONG_INT, MPI_MIN, testComm),
                      "cannot reduce total data moved");
            MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMax, 1,
                                    MPI_LONG_LONG_INT, MPI_MAX, testComm),
                      "cannot reduce total data moved");
            if (tmpMin != tmpMax) {
                    if (rank == 0) {
                            WARN("inconsistent file size by different tasks");
                    }
                    /* incorrect, but now consistent across tasks */
                    aggFileSizeFromStat = tmpMin;
            }
        }
        point->aggFileSizeFromStat = aggFileSizeFromStat;

        MPI_CHECK(MPI_Allreduce(&dataMoved, &point->aggFileSizeFromXfer,
                                1, MPI_LONG_LONG_INT, MPI_SUM, testComm),
                  "cannot total data moved");

        if (strcasecmp(params->api, "HDF5") != 0 && strcasecmp(params->api, "NCMPI") != 0) {
                if (verbose >= VERBOSE_0 && rank == 0) {
                        if ((params->expectedAggFileSize
                             != point->aggFileSizeFromXfer)
                            || (point->aggFileSizeFromStat
                                != point->aggFileSizeFromXfer)) {
                                WARNF("Expected aggregate file size       = %lld", (long long) params->expectedAggFileSize);
                                WARNF("Stat() of aggregate file size      = %lld", (long long) point->aggFileSizeFromStat);
                                WARNF("Using actual aggregate bytes moved = %lld", (long long) point->aggFileSizeFromXfer);
                                if(params->deadlineForStonewalling){
                                  WARN("Maybe caused by deadlineForStonewalling");
                                }
                        }
                }
        }

        point->aggFileSizeForBW = point->aggFileSizeFromXfer;
}

/*
 * Compare buffers after reading/writing each transfer.  Displays only first
 * difference in buffers and returns total errors counted.
 */
static size_t
CompareData(void *expectedBuffer, size_t size, IOR_param_t *test, IOR_offset_t offset, int fillrank, int access)
{
        assert(access == WRITECHECK || access == READCHECK);
        return verify_memory_pattern(offset, expectedBuffer, size, test->timeStampSignatureValue, fillrank, test->dataPacketType, test->gpuMemoryFlags);
}

/*
 * Count all errors across all tasks; report errors found.
 */
static int CountErrors(IOR_param_t * test, int access, int errors)
{
        int allErrors = 0;

        if (test->checkWrite || test->checkRead) {
                MPI_CHECK(MPI_Reduce(&errors, &allErrors, 1, MPI_INT, MPI_SUM,
                                     0, testComm), "cannot reduce errors");
                MPI_CHECK(MPI_Bcast(&allErrors, 1, MPI_INT, 0, testComm),
                          "cannot broadcast allErrors value");
                if (allErrors != 0) {
                        totalErrorCount += allErrors;
                        test->errorFound = TRUE;
                }
                if (rank == 0 && allErrors != 0) {
                        if (allErrors < 0) {
                                WARN("overflow in errors counted");
                                allErrors = -1;
                        }
                        WARNF("Incorrect data on %s (%d errors found).\n",
                                access == WRITECHECK ? "write" : "read", allErrors);
                        fprintf(out_logfile,
                                "Used Time Stamp %u (0x%x) for Data Signature\n",
                                test->timeStampSignatureValue,
                                test->timeStampSignatureValue);
                }
        }
        return (allErrors);
}

void AllocResults(IOR_test_t *test)
{
  int reps;
  if (test->results != NULL)
    return;

  reps = test->params.repetitions;
  test->results = (IOR_results_t *) safeMalloc(sizeof(IOR_results_t) * reps);
}

void FreeResults(IOR_test_t *test)
{
  if (test->results != NULL) {
      free(test->results);
  }
}


/**
 * Create new test for list of tests.
 */
IOR_test_t *CreateTest(IOR_param_t *init_params, int test_num)
{
        IOR_test_t *newTest = NULL;

        newTest = (IOR_test_t *) malloc(sizeof(IOR_test_t));
        if (newTest == NULL)
                ERR("malloc() of IOR_test_t failed");
        newTest->params = *init_params;
        newTest->params.platform = GetPlatformName();
        newTest->params.id = test_num;
        newTest->next = NULL;
        newTest->results = NULL;

        return newTest;
}

static void DestroyTest(IOR_test_t *test)
{
        FreeResults(test);
        free(test);
}

static void DestroyTests(IOR_test_t *tests_head)
{
        IOR_test_t *tptr, *next;

        for (tptr = tests_head; tptr != NULL; tptr = next) {
                next = tptr->next;
                DestroyTest(tptr);
        }
}

/*
 * Distribute IOR_HINTs to all tasks' environments.
 */
static void DistributeHints(MPI_Comm com)
{
        char hint[MAX_HINTS][MAX_STR], fullHint[MAX_STR], hintVariable[MAX_STR], hintValue[MAX_STR];
        int hintCount = 0, i;

        if (rank == 0) {
                for (i = 0; environ[i] != NULL; i++) {
                        if (strncmp(environ[i], "IOR_HINT", strlen("IOR_HINT"))
                            == 0) {
                                hintCount++;
                                if (hintCount == MAX_HINTS) {
                                        WARN("exceeded max hints; reset MAX_HINTS and recompile");
                                        hintCount = MAX_HINTS;
                                        break;
                                }
                                /* assume no IOR_HINT is greater than MAX_STR in length */
                                strncpy(hint[hintCount - 1], environ[i],
                                        MAX_STR - 1);
                        }
                }
        }

        MPI_CHECK(MPI_Bcast(&hintCount, sizeof(hintCount), MPI_BYTE, 0, com), "cannot broadcast hints");
        for (i = 0; i < hintCount; i++) {
                MPI_CHECK(MPI_Bcast(&hint[i], MAX_STR, MPI_BYTE, 0, com),
                          "cannot broadcast hints");
                strcpy(fullHint, hint[i]);
                strcpy(hintVariable, strtok(fullHint, "="));
                strcpy(hintValue, strtok(NULL, "="));
                if (getenv(hintVariable) == NULL) {
                        /* doesn't exist in this task's environment; better set it */
                        if (setenv(hintVariable, hintValue, 1) != 0)
                                WARN("cannot set environment variable");
                }
        }
}

/*
 * Return string describing machine name and type.
 */
char * GetPlatformName()
{
        char nodeName[MAX_STR], *p, *start, sysName[MAX_STR];
        char platformName[MAX_STR];
        struct utsname name;

        if (uname(&name) != 0) {
                WARN("cannot get platform name");
                sprintf(sysName, "%s", "Unknown");
                sprintf(nodeName, "%s", "Unknown");
        } else {
                sprintf(sysName, "%s", name.sysname);
                sprintf(nodeName, "%s", name.nodename);
        }

        start = nodeName;
        if (strlen(nodeName) == 0) {
                p = start;
        } else {
                /* point to one character back from '\0' */
                p = start + strlen(nodeName) - 1;
        }
        /*
         * to cut off trailing node number, search backwards
         * for the first non-numeric character
         */
        while (p != start) {
                if (*p < '0' || *p > '9') {
                        *(p + 1) = '\0';
                        break;
                } else {
                        p--;
                }
        }

        sprintf(platformName, "%s(%s)", nodeName, sysName);
        return strdup(platformName);
}



/*
 * Parse file name.
 */
static char **ParseFileName(char *name, int *count)
{
        char **fileNames, *tmp, *token;
        char delimiterString[3] = { FILENAME_DELIMITER, '\n', '\0' };
        int i = 0;

        *count = 0;
        tmp = name;

        /* pass one */
        /* if something there, count the first item */
        if (*tmp != '\0') {
                (*count)++;
        }
        /* count the rest of the filenames */
        while (*tmp != '\0') {
                if (*tmp == FILENAME_DELIMITER) {
                        (*count)++;
                }
                tmp++;
        }

        fileNames = (char **)malloc((*count) * sizeof(char **));
        if (fileNames == NULL)
                ERR("out of memory");

        /* pass two */
        token = strtok(name, delimiterString);
        while (token != NULL) {
                fileNames[i] = token;
                token = strtok(NULL, delimiterString);
                i++;
        }
        return (fileNames);
}


/*
 * Return test file name to access.
 * for single shared file, fileNames[0] is returned in testFileName
 */
void GetTestFileName(char *testFileName, IOR_param_t * test)
{
        char **fileNames;
        char   initialTestFileName[MAX_PATHLEN];
        char   testFileNameRoot[MAX_STR];
        char   tmpString[MAX_STR];
        int    count;
        int    socket, core;

        /* parse filename for multiple file systems */
        strcpy(initialTestFileName, test->testFileName);
        if(test->dualMount){
                GetProcessorAndCore(&socket, &core);
                sprintf(tmpString, "%s%d/%s",initialTestFileName, socket, "data");
                strcpy(initialTestFileName, tmpString);
        }
        fileNames = ParseFileName(initialTestFileName, &count);
        if (count > 1 && test->uniqueDir == TRUE)
                ERR("cannot use multiple file names with unique directories");
        if (test->filePerProc) {
                strcpy(testFileNameRoot,
                       fileNames[((rank +
                                   rankOffset) % test->numTasks) % count]);
        } else {
                strcpy(testFileNameRoot, fileNames[0]);
        }

        /* give unique name if using multiple files */
        if (test->filePerProc) {
                /*
                 * prepend rank subdirectory before filename
                 * e.g., /dir/file => /dir/<rank>/file
                 */
                if (test->uniqueDir == TRUE) {
                        strcpy(testFileNameRoot,
                               PrependDir(test, testFileNameRoot));
                }
                sprintf(testFileName, "%s.%08d", testFileNameRoot,
                        (rank + rankOffset) % test->numTasks);
        } else {
                strcpy(testFileName, testFileNameRoot);
        }

        /* add suffix for multiple files */
        if (test->repCounter > -1) {
                sprintf(tmpString, ".%d", test->repCounter);
                strcat(testFileName, tmpString);
        }
        free (fileNames);
}

/*
 * From absolute directory, insert rank as subdirectory.  Allows each task
 * to write to its own directory.  E.g., /dir/file => /dir/<rank>/file.
 */
static char *PrependDir(IOR_param_t * test, char *rootDir)
{
        char *dir;
        char *fname;
        int i;

        dir = (char *)malloc(MAX_STR + 1);
        if (dir == NULL)
                ERR("out of memory");

        /* get dir name */
        strcpy(dir, rootDir);
        i = strlen(dir) - 1;
        while (i > 0) {
                if (dir[i] == '\0' || dir[i] == '/') {
                        dir[i] = '/';
                        dir[i + 1] = '\0';
                        break;
                }
                i--;
        }

        /* get file name */
        fname = rootDir + i + 1;

        /* create directory with rank as subdirectory */
        sprintf(dir + i + 1, "%d", (rank + rankOffset) % test->numTasks);

        /* dir doesn't exist, so create */
        if (backend->access(dir, F_OK, test->backend_options) != 0) {
                if (backend->mkdir(dir, S_IRWXU, test->backend_options) < 0) {
                        ERRF("cannot create directory: %s", dir);
                }

                /* check if correct permissions */
        } else if (backend->access(dir, R_OK, test->backend_options) != 0 ||
                   backend->access(dir, W_OK, test->backend_options) != 0 ||
                   backend->access(dir, X_OK, test->backend_options) != 0) {
                ERRF("invalid directory permissions: %s", dir);
        }

        /* concatenate dir and file names */
        strcat(dir, "/");
        strcat(dir, fname);

        return dir;
}

/******************************************************************************/
/*
 * Reduce test results, and show if verbose set.
 */
static void
ReduceIterResults(IOR_test_t *test, double *timer, const int rep, const int access)
{
        double reduced[IOR_NB_TIMERS] = { 0 };
        double diff[IOR_NB_TIMERS / 2 + 1];
        double totalTime, accessTime;
        IOR_param_t *params = &test->params;
        double bw, iops, latency, minlatency;
        int i;
        MPI_Op op;

        assert(access == WRITE || access == READ);

        /* Find the minimum start time of the even numbered timers, and the
           maximum finish time for the odd numbered timers */
        for (i = 0; i < IOR_NB_TIMERS; i++) {
                op = i % 2 ? MPI_MAX : MPI_MIN;
                MPI_CHECK(MPI_Reduce(&timer[i], &reduced[i], 1, MPI_DOUBLE,
                                     op, 0, testComm), "MPI_Reduce()");
        }

        /* Calculate elapsed times and throughput numbers */
        for (i = 0; i < IOR_NB_TIMERS / 2; i++)
                diff[i] = reduced[2 * i + 1] - reduced[2 * i];

        totalTime = reduced[IOR_TIMER_CLOSE_STOP] - reduced[IOR_TIMER_OPEN_START];
        accessTime = reduced[IOR_TIMER_RDWR_STOP] - reduced[IOR_TIMER_RDWR_START];

        IOR_point_t *point = (access == WRITE) ? &test->results[rep].write :
                                                 &test->results[rep].read;

        point->time = totalTime;

        if (verbose < VERBOSE_0)
                return;

        bw = (double)point->aggFileSizeForBW / totalTime;

        /* For IOPS in this iteration, we divide the total amount of IOs from
         * all ranks over the entire access time (first start -> last end). */
        iops = (point->aggFileSizeForBW / params->transferSize) / accessTime;

        /* For Latency, we divide the total access time for each task over the
         * number of I/Os issued from that task; then reduce and display the
         * minimum (best) latency achieved. So what is reported is the average
         * latency of all ops from a single task, then taking the minimum of
         * that between all tasks. */
        latency = (timer[IOR_TIMER_RDWR_STOP] - timer[IOR_TIMER_RDWR_START]) / point->pairs_accessed;
        MPI_CHECK(MPI_Reduce(&latency, &minlatency, 1, MPI_DOUBLE, MPI_MIN, 0, testComm), "MPI_Reduce()");

        /* Only rank 0 tallies and prints the results. */
        if (rank != 0)
                return;

        PrintReducedResult(test, access, bw, iops, latency, diff, totalTime, rep);
}

/*
 * Check for file(s), then remove all files if file-per-proc, else single file.
 *
 */
static void RemoveFile(char *testFileName, int filePerProc, IOR_param_t * test)
{
        int tmpRankOffset = 0;
        if (filePerProc) {
                /* in random tasks, delete own file */
                if (test->reorderTasksRandom == TRUE) {
                        tmpRankOffset = rankOffset;
                        rankOffset = 0;
                        GetTestFileName(testFileName, test);
                }
                if (backend->access(testFileName, F_OK, test->backend_options) == 0) {
                        if (verbose >= VERBOSE_3) {
                                fprintf(out_logfile, "task %d removing %s\n", rank,
                                        testFileName);
                        }
                        backend->remove(testFileName, test->backend_options);
                }
                if (test->reorderTasksRandom == TRUE) {
                        rankOffset = tmpRankOffset;
                        GetTestFileName(testFileName, test);
                }
        } else {
                if ((rank == 0) && (backend->access(testFileName, F_OK, test->backend_options) == 0)) {
                        if (verbose >= VERBOSE_3) {
                                fprintf(out_logfile, "task %d removing %s\n", rank,
                                        testFileName);
                        }
                        backend->remove(testFileName, test->backend_options);
                }
        }
}

/*
 * Setup tests by parsing commandline and creating test script.
 * Perform a sanity-check on the configured parameters.
 */
static void InitTests(IOR_test_t *tests)
{
        if(tests == NULL){
          return;
        }
        MPI_Comm com = tests->params.mpi_comm_world;
        int mpiNumNodes = 0;
        int mpiNumTasks = 0;
        int mpiNumTasksOnNode0 = 0;

        verbose = tests->params.verbose;
        aiori_warning_as_errors = tests->params.warningAsErrors;

        /*
         * These default values are the same for every test and expensive to
         * retrieve so just do it once.
         */
        mpiNumNodes = GetNumNodes(com);
        mpiNumTasks = GetNumTasks(com);
        mpiNumTasksOnNode0 = GetNumTasksOnNode0(com);

        /*
         * Since there is no guarantee that anyone other than
         * task 0 has the environment settings for the hints, pass
         * the hint=value pair to everyone else in mpi_comm_world
         */
        DistributeHints(com);

        /* check validity of tests and create test queue */
        while (tests != NULL) {
                IOR_param_t *params = & tests->params;
                params->testComm = com;

                /* use MPI values if not overridden on command-line */
                if (params->numNodes == -1) {
                        params->numNodes = mpiNumNodes;
                }
                if (params->numTasks == -1) {
                        params->numTasks = mpiNumTasks;
                } else if (params->numTasks > mpiNumTasks) {
                        if (rank == 0) {
                                WARNF("More tasks requested (%d) than available (%d),",
                                        params->numTasks, mpiNumTasks);
                                WARNF("         running with %d tasks.\n", mpiNumTasks);
                        }
                        params->numTasks = mpiNumTasks;
                }
                if (params->numTasksOnNode0 == -1) {
                        params->numTasksOnNode0 = mpiNumTasksOnNode0;
                }

                params->tasksBlockMapping = QueryNodeMapping(com,false);
                params->expectedAggFileSize =
                  params->blockSize * params->segmentCount * params->numTasks;

                ValidateTests(&tests->params, com);
                tests = tests->next;
        }

        init_clock(com);
}

/*
 * Setup transfer buffers, creating and filling as needed.
 */
static void XferBuffersSetup(IOR_io_buffers* ioBuffers, IOR_param_t* test,
                             int pretendRank)
{
        ioBuffers->buffer = aligned_buffer_alloc(test->transferSize, test->gpuMemoryFlags);
}

/*
 * Free transfer buffers.
 */
static void XferBuffersFree(IOR_io_buffers* ioBuffers, IOR_param_t* test)

{
        aligned_buffer_free(ioBuffers->buffer, test->gpuMemoryFlags);
}



/*
 * malloc a buffer, touching every page in an attempt to defeat lazy allocation.
 */
static void *malloc_and_touch(size_t size)
{
        size_t page_size;
        char *buf;
        char *ptr;

        if (size == 0)
                return NULL;

        page_size = sysconf(_SC_PAGESIZE);

        buf = (char *)malloc(size);
        if (buf == NULL)
                return NULL;

        for (ptr = buf; ptr < buf+size; ptr += page_size) {
                *ptr = (char)1;
        }

        return (void *)buf;
}

static void file_hits_histogram(IOR_param_t *params)
{
        int *rankoffs = NULL;
        int *filecont = NULL;
        int *filehits = NULL;
        int ifile;
        int jfile;

        if (rank == 0) {
                rankoffs = (int *)malloc(params->numTasks * sizeof(int));
                filecont = (int *)malloc(params->numTasks * sizeof(int));
                filehits = (int *)malloc(params->numTasks * sizeof(int));
        }

        MPI_CHECK(MPI_Gather(&rankOffset, 1, MPI_INT, rankoffs,
                             1, MPI_INT, 0, params->testComm),
                  "MPI_Gather error");

        if (rank != 0)
                return;

        memset((void *)filecont, 0, params->numTasks * sizeof(int));
        for (ifile = 0; ifile < params->numTasks; ifile++) {
                filecont[(ifile + rankoffs[ifile]) % params->numTasks]++;
        }
        memset((void *)filehits, 0, params->numTasks * sizeof(int));
        for (ifile = 0; ifile < params->numTasks; ifile++)
                for (jfile = 0; jfile < params->numTasks; jfile++) {
                        if (ifile == filecont[jfile])
                                filehits[ifile]++;
                }
        fprintf(out_logfile, "#File Hits Dist:");
        jfile = 0;
        ifile = 0;
        while (jfile < params->numTasks && ifile < params->numTasks) {
                fprintf(out_logfile, " %d", filehits[ifile]);
                jfile += filehits[ifile], ifile++;
        }
        fprintf(out_logfile, "\n");
        free(rankoffs);
        free(filecont);
        free(filehits);
}


int test_time_elapsed(IOR_param_t *params, double startTime)
{
        double endTime;

        if (params->maxTimeDuration == 0)
                return 0;

        endTime = startTime + (params->maxTimeDuration * 60);

        return GetTimeStamp() >= endTime;
}

/*
 * hog some memory as a rough simulation of a real application's memory use
 */
static void *HogMemory(IOR_param_t *params)
{
        size_t size;
        void *buf;

        if (params->memoryPerTask != 0) {
                size = params->memoryPerTask;
        } else if (params->memoryPerNode != 0) {
                if (verbose >= VERBOSE_3)
                        fprintf(out_logfile, "This node hogging %ld bytes of memory\n",
                                params->memoryPerNode);
                size = params->memoryPerNode / params->numTasksOnNode0;
        } else {
                return NULL;
        }

        if (verbose >= VERBOSE_3)
                fprintf(out_logfile, "This task hogging %ld bytes of memory\n", size);

        buf = malloc_and_touch(size);
        if (buf == NULL)
                ERR("malloc of simulated applciation buffer failed");

        return buf;
}
/*
 * Write times taken during each iteration of the test.
 */
static void
WriteTimes(IOR_param_t *test, const double *timer, const int iteration,
           const int access)
{
        char timerName[MAX_STR];

        for (int i = 0; i < IOR_NB_TIMERS; i++) {

                if (access == WRITE) {
                        switch (i) {
                        case IOR_TIMER_OPEN_START:
                                strcpy(timerName, "write open start");
                                break;
                        case IOR_TIMER_OPEN_STOP:
                                strcpy(timerName, "write open stop");
                                break;
                        case IOR_TIMER_RDWR_START:
                                strcpy(timerName, "write start");
                                break;
                        case IOR_TIMER_RDWR_STOP:
                                strcpy(timerName, "write stop");
                                break;
                        case IOR_TIMER_CLOSE_START:
                                strcpy(timerName, "write close start");
                                break;
                        case IOR_TIMER_CLOSE_STOP:
                                strcpy(timerName, "write close stop");
                                break;
                        default:
                                strcpy(timerName, "invalid timer");
                                break;
                        }
                }
                else {
                        switch (i) {
                        case IOR_TIMER_OPEN_START:
                                strcpy(timerName, "read open start");
                                break;
                        case IOR_TIMER_OPEN_STOP:
                                strcpy(timerName, "read open stop");
                                break;
                        case IOR_TIMER_RDWR_START:
                                strcpy(timerName, "read start");
                                break;
                        case IOR_TIMER_RDWR_STOP:
                                strcpy(timerName, "read stop");
                                break;
                        case IOR_TIMER_CLOSE_START:
                                strcpy(timerName, "read close start");
                                break;
                        case IOR_TIMER_CLOSE_STOP:
                                strcpy(timerName, "read close stop");
                                break;
                        default:
                                strcpy(timerName, "invalid timer");
                                break;
                        }
                }
                fprintf(out_logfile, "Test %d: Iter=%d, Task=%d, Time=%f, %s\n",
                        test->id, iteration, (int)rank, timer[i],
                        timerName);
        }
}

static void StoreRankInformation(IOR_test_t *test, double *timer, const int rep, const int access){
  IOR_param_t *params = &test->params;
  double totalTime = timer[IOR_TIMER_CLOSE_STOP] - timer[IOR_TIMER_OPEN_START];
  double accessTime = timer[IOR_TIMER_RDWR_STOP] - timer[IOR_TIMER_RDWR_START];
  double times[] = {totalTime, accessTime};

  if(rank == 0){
    FILE* fd = fopen(params->saveRankDetailsCSV, "a");
    if (fd == NULL){
      FAIL("Cannot open saveRankPerformanceDetailsCSV file for writes!");
    }
    int size;
    MPI_Comm_size(params->testComm, & size);
    double *all_times = malloc(2* size * sizeof(double));
    MPI_Gather(times, 2, MPI_DOUBLE, all_times, 2, MPI_DOUBLE, 0, params->testComm);
    IOR_point_t *point = (access == WRITE) ? &test->results[rep].write : &test->results[rep].read;
    double file_size = ((double) point->aggFileSizeForBW) / size;

    for(int i=0; i < size; i++){
      char buff[1024];
      sprintf(buff, "%s,%d,%.10e,%.10e,%.10e,%.10e\n", access==WRITE ? "write" : "read", i, all_times[i*2], all_times[i*2+1], file_size/all_times[i*2], file_size/all_times[i*2+1] );
      int ret = fwrite(buff, strlen(buff), 1, fd);
      if(ret != 1){
        WARN("Couln't append to saveRankPerformanceDetailsCSV file\n");
        break;
      }
    }
    fclose(fd);
  }else{
    MPI_Gather(& times, 2, MPI_DOUBLE, NULL, 2, MPI_DOUBLE, 0, testComm);
  }
}

static void ProcessIterResults(IOR_test_t *test, double *timer, const int rep, const int access){
  IOR_param_t *params = &test->params;

  if (verbose >= VERBOSE_3)
    WriteTimes(params, timer, rep, access);
  ReduceIterResults(test, timer, rep, access);
  if (params->outlierThreshold) {
    CheckForOutliers(params, timer, access);
  }

  if(params->saveRankDetailsCSV){
    StoreRankInformation(test, timer, rep, access);
  }
}

/*
 * Using the test parameters, run iteration(s) of single test.
 */
static void TestIoSys(IOR_test_t *test)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;
        char testFileName[MAX_STR];
        double timer[IOR_NB_TIMERS];
        double startTime;
        int pretendRank;
        int rep;
        aiori_fd_t *fd;
        IOR_offset_t dataMoved; /* for data rate calculation */
        void *hog_buf;
        IOR_io_buffers ioBuffers;

        /* show test setup */
        if (rank == 0 && verbose >= VERBOSE_0)
                ShowSetup(params);

        hog_buf = HogMemory(params);

        pretendRank = (rank + rankOffset) % params->numTasks;

        /* IO Buffer Setup */

        if (params->setTimeStampSignature) { // initialize the buffer properly
                params->timeStampSignatureValue = (unsigned int) params->setTimeStampSignature;
        }

        XferBuffersSetup(&ioBuffers, params, pretendRank);
        
        /* Initial time stamp */
        startTime = GetTimeStamp();

        /* loop over test iterations */
        uint64_t params_saved_wearout = params->stoneWallingWearOutIterations;

        /* Check if the file exists and warn users */
        if((params->writeFile || params->checkWrite) && (params->hints.filePerProc || rank == 0)){
          struct stat sb;
          GetTestFileName(testFileName, params);
          int ret = backend->stat(testFileName, & sb, params->backend_options);
          if(ret == 0) {
            WARNF("The file \"%s\" exists already and will be %s", testFileName,
		  params->useExistingTestFile ? "overwritten" : "deleted");
          }
        }

        for (rep = 0; rep < params->repetitions; rep++) {
                /* Get iteration start time in seconds in task 0 and broadcast to
                   all tasks */
                if (rank == 0) {
                        if (! params->setTimeStampSignature) {
                                time_t currentTime;
                                if ((currentTime = time(NULL)) == -1) {
                                        ERR("cannot get current time");
                                }
                                params->timeStampSignatureValue = (unsigned int)currentTime;
                        }
                        if (verbose >= VERBOSE_2) {
                                fprintf(out_logfile,
                                        "Using Time Stamp %u (0x%x) for Data Signature\n",
                                        params->timeStampSignatureValue,
                                        params->timeStampSignatureValue);
                        }
                        if (rep == 0 && verbose >= VERBOSE_0) {
                                PrintTableHeader();
                        }
                }
                MPI_CHECK(MPI_Bcast
                          (&params->timeStampSignatureValue, 1, MPI_UNSIGNED, 0,
                           testComm), "cannot broadcast start time value");

                generate_memory_pattern((char*) ioBuffers.buffer, params->transferSize, params->timeStampSignatureValue, pretendRank, params->dataPacketType, params->gpuMemoryFlags);

                /* use repetition count for number of multiple files */
                if (params->multiFile)
                        params->repCounter = rep;

                /*
                 * write the file(s), getting timing between I/O calls
                 */

                if (params->writeFile && !test_time_elapsed(params, startTime)) {
                        GetTestFileName(testFileName, params);
                        if (verbose >= VERBOSE_3) {
                                fprintf(out_logfile, "task %d writing %s\n", rank,
                                        testFileName);
                        }
                        DelaySecs(params->interTestDelay);
                        if (params->useExistingTestFile == FALSE) {
                                RemoveFile(testFileName, params->filePerProc,
                                           params);
                        }

                        params->stoneWallingWearOutIterations = params_saved_wearout;
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                        params->open = WRITE;
                        timer[IOR_TIMER_OPEN_START] = GetTimeStamp();
                        fd = backend->create(testFileName, IOR_WRONLY | IOR_CREAT | IOR_TRUNC, params->backend_options);
                        if(fd == NULL) FAIL("Cannot create file");
                        timer[IOR_TIMER_OPEN_STOP] = GetTimeStamp();
                        if (params->intraTestBarriers)
                                MPI_CHECK(MPI_Barrier(testComm),
                                          "barrier error");
                        if (rank == 0 && verbose >= VERBOSE_3) {
                                fprintf(out_logfile,
                                        "Commencing write performance test: %s",
                                        CurrentTimeString());
                        }
                        timer[IOR_TIMER_RDWR_START] = GetTimeStamp();
                        dataMoved = WriteOrRead(params, rep, &results[rep], fd, WRITE, &ioBuffers);
                        if (params->verbose >= VERBOSE_4) {
                          fprintf(out_logfile, "* data moved = %llu\n", dataMoved);
                          fflush(out_logfile);
                        }
                        timer[IOR_TIMER_RDWR_STOP] = GetTimeStamp();
                        if (params->intraTestBarriers)
                                MPI_CHECK(MPI_Barrier(testComm),
                                          "barrier error");
                        timer[IOR_TIMER_CLOSE_START] = GetTimeStamp();
                        backend->close(fd, params->backend_options);

                        timer[IOR_TIMER_CLOSE_STOP] = GetTimeStamp();
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");

                        /* check if stat() of file doesn't equal expected file size,
                           use actual amount of byte moved */
                        CheckFileSize(test, testFileName, dataMoved, rep, WRITE);

                        ProcessIterResults(test, timer, rep, WRITE);

                        /* check if in this round we run write with stonewalling */
                        if(params->deadlineForStonewalling > 0){
                          params->stoneWallingWearOutIterations = results[rep].write.pairs_accessed;
                        }
                }

                /*
                 * perform a check of data, reading back data and comparing
                 * against what was expected to be written
                 */
                if (params->checkWrite && !test_time_elapsed(params, startTime)) {
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                        if (rank == 0 && verbose >= VERBOSE_1) {
                                fprintf(out_logfile,
                                        "Verifying contents of the file(s) just written.\n");
                                fprintf(out_logfile, "%s\n", CurrentTimeString());
                        }
                        if (params->reorderTasks) {
                                /* move two nodes away from writing node */
                                int shift = 1; /* assume a by-node (round-robin) mapping of tasks to nodes */
                                if (params->tasksBlockMapping) {
                                    shift = params->numTasksOnNode0; /* switch to by-slot (contiguous block) mapping */
                                }
                                rankOffset = (2 * shift) % params->numTasks;
                        }
                        
                        GetTestFileName(testFileName, params);
                        params->open = WRITECHECK;
                        fd = backend->open(testFileName, IOR_RDONLY, params->backend_options);
                        if(fd == NULL) FAIL("Cannot open file");
                        dataMoved = WriteOrRead(params, rep, &results[rep], fd, WRITECHECK, &ioBuffers);
                        backend->close(fd, params->backend_options);
                        rankOffset = 0;
                }
                /*
                 * read the file(s), getting timing between I/O calls
                 */
                if ((params->readFile || params->checkRead ) && !test_time_elapsed(params, startTime)) {
                        /* check for stonewall */
                        if(params->stoneWallingStatusFile){
                          params->stoneWallingWearOutIterations = ReadStoneWallingIterations(params->stoneWallingStatusFile, params->testComm);
                          if(params->stoneWallingWearOutIterations == -1 && rank == 0){
                            WARN("Could not read back the stonewalling status from the file!");
                            params->stoneWallingWearOutIterations = 0;
                          }
                        }
                        int operation_flag = READ;
                        if ( params->checkRead ){
                          // actually read and then compare the buffer
                          operation_flag = READCHECK;
                        }
                        /* Get rankOffset [file offset] for this process to read, based on -C,-Z,-Q,-X options */
                        /* Constant process offset reading */
                        if (params->reorderTasks) {
                                /* move one node away from writing node */
                                int shift = 1; /* assume a by-node (round-robin) mapping of tasks to nodes */
                                if (params->tasksBlockMapping) {
                                    shift=params->numTasksOnNode0; /* switch to a by-slot (contiguous block) mapping */
                                }
                                rankOffset = (params->taskPerNodeOffset * shift) % params->numTasks;
                        }
                        /* random process offset reading */
                        if (params->reorderTasksRandom == 1) {
                                /* this should not intefere with randomOffset within a file because GetOffsetArrayRandom */
                                /* seeds every rand() call  */
                                int nodeoffset;
                                unsigned int iseed0;
                                nodeoffset = params->taskPerNodeOffset;
                                nodeoffset = (nodeoffset < params->numNodes) ? nodeoffset : params->numNodes - 1;
                                if (params->reorderTasksRandomSeed < 0)
                                        iseed0 = -1 * params->reorderTasksRandomSeed + rep;
                                else
                                        iseed0 = params->reorderTasksRandomSeed;
                                srand(rank + iseed0);
                                {
                                        rankOffset = rand() % params->numTasks;
                                }
                                while (rankOffset <
                                       (nodeoffset * params->numTasksOnNode0)) {
                                        rankOffset = rand() % params->numTasks;
                                }
                                /* Get more detailed stats if requested by verbose level */
                                if (verbose >= VERBOSE_2) {
                                        file_hits_histogram(params);
                                }
                        }
                        if (params->reorderTasksRandom > 1) { /* Shuffling rank offset */
                                int nodeoffset;
                                unsigned int iseed0;
                                nodeoffset = params->taskPerNodeOffset;
                                nodeoffset = (nodeoffset < params->numNodes) ? nodeoffset : params->numNodes - 1;
                                if (params->reorderTasksRandomSeed < 0)
                                        iseed0 = -1 * params->reorderTasksRandomSeed + rep;
                                else
                                        iseed0 = params->reorderTasksRandomSeed;
                                srand(iseed0);
                                int * rankOffsets = safeMalloc(sizeof(int) * params->numTasks);
                                for(int i=0; i < params->numTasks; i++)
                                {
                                        rankOffsets[i] = i;
                                }
                                for(int i=0; i < params->numTasks; i++)
                                {
                                        int tgt = i + rand() % (params->numTasks - i);
                                        int tmp = rankOffsets[tgt];
                                        rankOffsets[tgt] = rankOffsets[i];
                                        rankOffsets[i] = tmp;
                                }
                                rankOffset = rankOffsets[rank] - rank; // must subtract as we talk about rankOffset and not the actual rank!
                                free(rankOffsets);
                        }
                        /* Using globally passed rankOffset, following function generates testFileName to read */
                        GetTestFileName(testFileName, params);
                        if(params->randomOffset > 1){
                          params->fileSizeForRead = backend->get_file_size(params->backend_options, testFileName);
                        }

                        if (verbose >= VERBOSE_3) {
                                fprintf(out_logfile, "task %d reading %s\n", rank,
                                        testFileName);
                        }
                        DelaySecs(params->interTestDelay);
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                        params->open = READ;
                        timer[IOR_TIMER_OPEN_START] = GetTimeStamp();
                        fd = backend->open(testFileName, IOR_RDONLY, params->backend_options);
                        if(fd == NULL) FAIL("Cannot open file");
                        timer[IOR_TIMER_OPEN_STOP] = GetTimeStamp();
                        if (params->intraTestBarriers)
                                MPI_CHECK(MPI_Barrier(testComm),
                                          "barrier error");
                        if (rank == 0 && verbose >= VERBOSE_3) {
                                fprintf(out_logfile,
                                        "Commencing read performance test: %s\n",
                                        CurrentTimeString());
                        }
                        timer[IOR_TIMER_RDWR_START] = GetTimeStamp();
                        dataMoved = WriteOrRead(params, rep, &results[rep], fd, operation_flag, &ioBuffers);
                        timer[IOR_TIMER_RDWR_STOP] = GetTimeStamp();
                        if (params->intraTestBarriers)
                                MPI_CHECK(MPI_Barrier(testComm),
                                          "barrier error");
                        timer[IOR_TIMER_CLOSE_START] = GetTimeStamp();
                        backend->close(fd, params->backend_options);
                        timer[IOR_TIMER_CLOSE_STOP] = GetTimeStamp();

                        /* check if stat() of file doesn't equal expected file size,
                           use actual amount of byte moved */
                        CheckFileSize(test, testFileName, dataMoved, rep, READ);

                        ProcessIterResults(test, timer, rep, READ);
                }

                if (!params->keepFile
                    && !(params->errorFound && params->keepFileWithError)) {
                        double start, finish;
                        start = GetTimeStamp();
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                        RemoveFile(testFileName, params->filePerProc, params);
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                        finish = GetTimeStamp();
                        PrintRemoveTiming(start, finish, rep);
                } else {
                        MPI_CHECK(MPI_Barrier(testComm), "barrier error");
                }
                params->errorFound = FALSE;
                rankOffset = 0;

        }
        PrintRepeatEnd();

        if (params->summary_every_test) {
                PrintLongSummaryHeader();
                PrintLongSummaryOneTest(test);
        } else {
                PrintShortSummary(test);
        }

        XferBuffersFree(&ioBuffers, params);

        if (hog_buf != NULL)
                free(hog_buf);
}

/*
 * Determine if valid tests from parameters.
 */
static void ValidateTests(IOR_param_t * test, MPI_Comm com)
{
        IOR_param_t defaults;
        init_IOR_Param_t(&defaults, com);

        if (test->gpuDirect && test->gpuMemoryFlags == IOR_MEMORY_TYPE_CPU )
          ERR("GPUDirect requires a non-CPU memory type");
        if (test->gpuMemoryFlags == IOR_MEMORY_TYPE_GPU_DEVICE_ONLY && ! test->gpuDirect )
          ERR("Using GPU Device memory only requires the usage of GPUDirect");
        if (test->stoneWallingStatusFile && test->keepFile == 0)
          ERR("a StoneWallingStatusFile is only sensible when splitting write/read into multiple executions of ior, please use -k");
        if (test->stoneWallingStatusFile && test->stoneWallingWearOut == 0 && test->writeFile)
          ERR("the StoneWallingStatusFile is only sensible for a write test when using  stoneWallingWearOut");
        if (test->deadlineForStonewalling == 0 && test->stoneWallingWearOut > 0)
          ERR("the stoneWallingWearOut is only sensible when setting a stonewall deadline with -D");
        if (test->stoneWallingStatusFile && test->testscripts)
          WARN("the StoneWallingStatusFile only preserves the last experiment, make sure that each run uses a separate status file!");
        if (test->repetitions <= 0)
                WARN_RESET("too few test repetitions",
                           test, &defaults, repetitions);
        if (test->numTasks <= 0)
                ERR("too few tasks for testing");
        if (test->interTestDelay < 0)
                WARN_RESET("inter-test delay must be nonnegative value",
                           test, &defaults, interTestDelay);
        if (test->readFile != TRUE && test->writeFile != TRUE
            && test->checkRead != TRUE && test->checkWrite != TRUE)
                ERR("test must write, read, or check read/write file");
        if(! test->setTimeStampSignature && test->writeFile != TRUE && test->checkRead == TRUE)
                ERR("using readCheck only requires to write a timeStampSignature -- use -G");
        if (test->segmentCount < 0)
                ERR("segment count must be positive value");
        if ((test->blockSize % sizeof(IOR_size_t)) != 0)
                ERR("block size must be a multiple of access size");
        if (test->blockSize < 0)
                ERR("block size must be non-negative integer");
        if ((test->transferSize % sizeof(IOR_size_t)) != 0)
                ERR("transfer size must be a multiple of access size");
        if (test->transferSize < 0)
                ERR("transfer size must be non-negative integer");
        if (test->transferSize == 0) {
                ERR("test will not complete with zero transfer size");
        } else {
                if ((test->blockSize % test->transferSize) != 0)
                        ERR("block size must be a multiple of transfer size");
        }
        if (test->blockSize < test->transferSize)
                ERR("block size must not be smaller than transfer size");
        if (test->randomOffset == 1 && test->blockSize == test->transferSize)
            ERR("IOR will randomize access within a block and repeats the same pattern for all segments, therefore choose blocksize > transferSize");
        if (! test->randomOffset && test->randomPrefillBlocksize)
          ERR("Setting the randomPrefill option without using random is not useful");
        if (test->randomPrefillBlocksize && (test->blockSize % test->randomPrefillBlocksize != 0))
          ERR("The randomPrefill option must divide the blockSize");
        /* specific APIs */
        if ((strcasecmp(test->api, "MPIIO") == 0)
            && (test->blockSize < sizeof(IOR_size_t)
                || test->transferSize < sizeof(IOR_size_t)))
                ERR("block/transfer size may not be smaller than IOR_size_t for MPIIO");
        if ((strcasecmp(test->api, "HDF5") == 0)
            && (test->blockSize < sizeof(IOR_size_t)
                || test->transferSize < sizeof(IOR_size_t)))
                ERR("block/transfer size may not be smaller than IOR_size_t for HDF5");
        if ((strcasecmp(test->api, "NCMPI") == 0)
            && (test->blockSize < sizeof(IOR_size_t)
                || test->transferSize < sizeof(IOR_size_t)))
                ERR("block/transfer size may not be smaller than IOR_size_t for NCMPI");
        if (((strcasecmp(test->api, "POSIX") != 0)
            && (strcasecmp(test->api, "MPIIO") != 0)
            && (strcasecmp(test->api, "HDF5") != 0)
            && (strcasecmp(test->api, "NCMPI") != 0)
            && (strcasecmp(test->api, "DUMMY") != 0)
            && (strcasecmp(test->api, "AIO") != 0)
            && (strcasecmp(test->api, "PMDK") != 0)
            && (strcasecmp(test->api, "MMAP") != 0)
            && (strcasecmp(test->api, "HDFS") != 0)
            && (strcasecmp(test->api, "DFS") != 0)
            && (strcasecmp(test->api, "Gfarm") != 0)
            && (strcasecmp(test->api, "RADOS") != 0)
            && (strcasecmp(test->api, "CEPHFS") != 0)) && test->fsync)
                WARN_RESET("fsync() not supported in selected backend",
                           test, &defaults, fsync);
        /* parameter consistency */
        if (test->reorderTasks == TRUE && test->reorderTasksRandom == TRUE)
                ERR("Both Constant and Random task re-ordering specified. Choose one and resubmit");
        if (test->randomOffset && test->reorderTasksRandom
            && test->filePerProc == FALSE)
                ERR("random offset and random reorder tasks specified with single-shared-file. Choose one and resubmit");
        if (test->randomOffset && test->reorderTasks
            && test->filePerProc == FALSE)
                ERR("random offset and constant reorder tasks specified with single-shared-file. Choose one and resubmit");
        if (test->randomOffset && test->checkRead && test->randomSeed == -1)
                ERR("random offset with read check option requires to set the random seed");
        if ((strcasecmp(test->api, "HDF5") == 0) && test->randomOffset)
                ERR("random offset not available with HDF5");
        if ((strcasecmp(test->api, "NCMPI") == 0) && test->randomOffset)
                ERR("random offset not available with NCMPI");
        if ((strcasecmp(test->api, "NCMPI") == 0) && test->filePerProc)
                ERR("file-per-proc not available in current NCMPI");

        backend = test->backend;
        ior_set_xfer_hints(test);
        /* allow the backend to validate the options */
        if(test->backend->check_params){
          int check = test->backend->check_params(test->backend_options);
          if (check){
            ERR("The backend returned that the test parameters are invalid.");
          }
        }
}

static int init_random_seed(IOR_param_t * test, int pretendRank){
  int seed;
  if (test->filePerProc) {
    /* set up seed, each process can determine which regions to access individually */
    if (test->randomSeed == -1) {
            seed = time(NULL);
            test->randomSeed = seed;
    } else {
        seed = test->randomSeed + pretendRank;
    }
  }else{
    /* Shared file requires that the seed is synchronized */
    if (test->randomSeed == -1) {
      // all processes need to have the same seed.
      if(rank == 0){
        seed = time(NULL);
      }
      MPI_CHECK(MPI_Bcast(& seed, 1, MPI_INT, 0, test->testComm), "cannot broadcast random seed value");
      test->randomSeed = seed;
    }else{
      seed = test->randomSeed;
    }
  }
  srandom(seed);
  return seed;
}

/**
 * Returns a precomputed array of IOR_offset_t for the inner benchmark loop.
 * They get created sequentially and mixed up in the end.
 * It should be noted that as the seeds get synchronised across all processes if not FilePerProcess is set
 * every process computes the same random order.
 * For a shared file all transfers get randomly assigned to ranks. The processes
 * can also have differen't numbers of transfers. This might lead to a bigger
 * diversion in accesse as it dose with filePerProc. This is expected but
 * should be mined.
 * @param test IOR_param_t for getting transferSize, blocksize and SegmentCount
 * @param pretendRank int pretended Rank for shifting the offsets correctly
 * @return IOR_offset_t
 */
IOR_offset_t *GetOffsetArrayRandom(IOR_param_t * test, int pretendRank, IOR_offset_t * out_count)
{
        int seed;
        IOR_offset_t i;
        IOR_offset_t offsets;
        IOR_offset_t offsetCnt = 0;
        IOR_offset_t *offsetArray;

        seed = init_random_seed(test, pretendRank);

        /* count needed offsets (pass 1) */
        if (test->filePerProc) {
          offsets = test->blockSize / test->transferSize;
        }else{
          offsets = 0;
          for (i = 0; i < test->blockSize * test->numTasks; i += test->transferSize) {
            // this counts which process get how many transferes in the shared file
            if ((rand() % test->numTasks) == pretendRank) {
              offsets++;
            }
          }
        }

        /* setup empty array */
        offsetArray = (IOR_offset_t *) safeMalloc(offsets * sizeof(IOR_offset_t));

        *out_count = offsets;

        if (test->filePerProc) {
            /* fill array */
            for (i = 0; i < offsets; i++) {
                offsetArray[i] = i * test->transferSize;
            }
        } else {
            /* fill with offsets (pass 2) */
            srandom(seed);  /* need same seed to get same transfers as counted in the beginning*/
            for (i = 0; i < test->blockSize * test->numTasks; i += test->transferSize) {
                if ((rand() % test->numTasks) == pretendRank) {
                    offsetArray[offsetCnt] = i;
                    offsetCnt++;
                }
            }
        }
        /* reorder array */
        for (i = 0; i < offsets; i++) {
                IOR_offset_t value, tmp;
                value = rand() % offsets;
                tmp = offsetArray[value];
                offsetArray[value] = offsetArray[i];
                offsetArray[i] = tmp;
        }

        return (offsetArray);
}

static IOR_offset_t WriteOrReadSingle(IOR_offset_t offset, int pretendRank, IOR_offset_t transfer, int * errors, IOR_param_t * test, aiori_fd_t * fd, IOR_io_buffers* ioBuffers, int access, OpTimer* ot, double startTime){
  IOR_offset_t amtXferred = 0;

  void *buffer = ioBuffers->buffer;
  if (access == WRITE) {
          /* fills each transfer with a unique pattern
           * containing the offset into the file */
          update_write_memory_pattern(offset, ioBuffers->buffer, transfer, test->setTimeStampSignature, pretendRank, test->dataPacketType, test->gpuMemoryFlags);
          double start = GetTimeStamp();
          amtXferred = backend->xfer(access, fd, buffer, transfer, offset, test->backend_options);
          if(ot) OpTimerValue(ot, start - startTime, GetTimeStamp() - start);
          if (amtXferred != transfer)
                  ERR("cannot write to file");
          if (test->fsyncPerWrite)
                backend->fsync(fd, test->backend_options);
          if (test->interIODelay > 0){
            struct timespec wait = {test->interIODelay / 1000 / 1000, 1000l * (test->interIODelay % 1000000)};
            nanosleep( & wait, NULL);
          }
  } else if (access == READ) {
          double start = GetTimeStamp();
          amtXferred = backend->xfer(access, fd, buffer, transfer, offset, test->backend_options);
          if(ot) OpTimerValue(ot, start - startTime, GetTimeStamp() - start);
          if (amtXferred != transfer)
                  ERR("cannot read from file");
          if (test->interIODelay > 0){
            struct timespec wait = {test->interIODelay / 1000 / 1000, 1000l * (test->interIODelay % 1000000)};
            nanosleep( & wait, NULL);
          }
  } else if (access == WRITECHECK) {
          invalidate_buffer_pattern(buffer, transfer, test->gpuMemoryFlags);
          double start = GetTimeStamp();
          amtXferred = backend->xfer(access, fd, buffer, transfer, offset, test->backend_options);
          if(ot) OpTimerValue(ot, start - startTime, GetTimeStamp() - start);
          if (amtXferred != transfer)
                  ERR("cannot read from file write check");
          *errors += CompareData(buffer, transfer, test, offset, pretendRank, WRITECHECK);
  } else if (access == READCHECK) {
          invalidate_buffer_pattern(buffer, transfer, test->gpuMemoryFlags);          
          double start = GetTimeStamp();
          amtXferred = backend->xfer(access, fd, buffer, transfer, offset, test->backend_options);
          if(ot) OpTimerValue(ot, start - startTime, GetTimeStamp() - start);
          if (amtXferred != transfer){
            ERR("cannot read from file");
          }
          *errors += CompareData(buffer, transfer, test, offset, pretendRank, READCHECK);
  }
  return amtXferred;
}

static void prefillSegment(IOR_param_t *test, void * randomPrefillBuffer, int pretendRank, aiori_fd_t *fd, IOR_io_buffers *ioBuffers, int startSegment, int endSegment){
  // prefill the whole file already with an invalid pattern
  int offsets = test->blockSize / test->randomPrefillBlocksize;
  void * oldBuffer = ioBuffers->buffer;
  int errors;
  ioBuffers->buffer = randomPrefillBuffer;
  for (IOR_offset_t i = startSegment; i < endSegment; i++){
    for (int j = 0; j < offsets; j++) {
      IOR_offset_t offset = j * test->randomPrefillBlocksize;
      if (test->filePerProc) {
        offset += i * test->blockSize;
      } else {
        offset += (i * test->numTasks * test->blockSize) + (pretendRank * test->blockSize);
      }
      WriteOrReadSingle(offset, pretendRank, test->randomPrefillBlocksize, & errors, test, fd, ioBuffers, WRITE, NULL, 0);
    }
  }
  ioBuffers->buffer = oldBuffer;
}

/*
 * Write or Read data to file(s).  This loops through the strides, writing
 * out the data to each block in transfer sizes, until the remainder left is 0.
 */
static IOR_offset_t WriteOrRead(IOR_param_t *test, int rep, IOR_results_t *results,
                                aiori_fd_t *fd, const int access, IOR_io_buffers *ioBuffers)
{
        int errors = 0;
        uint64_t pairCnt = 0;
        int pretendRank;
        IOR_offset_t dataMoved = 0;     /* for data rate calculation */
        double startForStonewall;
        int hitStonewall;
        IOR_offset_t i, j;
        IOR_point_t *point = ((access == WRITE) || (access == WRITECHECK)) ?
                             &results->write : &results->read;

        /* initialize values */
        pretendRank = (rank + rankOffset) % test->numTasks;

        //  offsetArray = GetOffsetArraySequential(test, pretendRank);

        IOR_offset_t offsets;
        IOR_offset_t * offsets_rnd;
        if (test->randomOffset == 1) {
          offsets_rnd = GetOffsetArrayRandom(test, pretendRank, & offsets);
        }else{
          offsets = (test->blockSize / test->transferSize);
        }
        if (test->randomOffset > 1){
          int seed = init_random_seed(test, pretendRank);
          srand(seed + pretendRank);
          srand64(((uint64_t) seed) * (pretendRank + 1));
        }

        void * randomPrefillBuffer = NULL;
        if(test->randomPrefillBlocksize && (access == WRITE || access == WRITECHECK)){
          randomPrefillBuffer = aligned_buffer_alloc(test->randomPrefillBlocksize, test->gpuMemoryFlags);
          // store invalid data into the buffer
          memset(randomPrefillBuffer, -1, test->randomPrefillBlocksize);
        }

        /* Per operation statistics */
        OpTimer * ot = NULL;
        if(test->savePerOpDataCSV != NULL) {
                char fname[FILENAME_MAX];
                sprintf(fname, "%s-%d-%05d.csv", test->savePerOpDataCSV, rep, rank);
                ot = OpTimerInit(fname, test->transferSize);
        }
        // start timer after random offset was generated        
        startForStonewall = GetTimeStamp();
        hitStonewall = 0;

        if(randomPrefillBuffer && test->deadlineForStonewalling == 0){
          double t_start = GetTimeStamp();
          prefillSegment(test, randomPrefillBuffer, pretendRank, fd, ioBuffers, 0, test->segmentCount);
          if(rank == 0 && verbose > VERBOSE_1){
            fprintf(out_logfile, "Random prefill took: %fs\n", GetTimeStamp() - t_start);
          }
          // must synchronize processes to ensure they are not running ahead
          MPI_Barrier(test->testComm);
        }
        do{ // to ensure the benchmark runs a certain time
          for (i = 0; i < test->segmentCount && !hitStonewall; i++) {
            IOR_offset_t offset;
            if(randomPrefillBuffer && test->deadlineForStonewalling != 0){
              // prefill the whole segment with data, this needs to be done collectively
              double t_start = GetTimeStamp();
              prefillSegment(test, randomPrefillBuffer, pretendRank, fd, ioBuffers, i, i+1);
              MPI_Barrier(test->testComm);
              if(rank == 0 && verbose > VERBOSE_1){
                fprintf(out_logfile, "Random: synchronizing segment count with barrier and prefill took: %fs\n", GetTimeStamp() - t_start);
              }
            }
            if (test->randomOffset > 1){
                size_t sizerand = test->fileSizeForRead; 
                //if(test->filePerProc){
                //  sizerand /= test->numTasks;
                //}
                uint64_t rpos;
                rpos = rand64();
                offset = rpos % (sizerand / test->blockSize) * test->blockSize - test->transferSize;
                if(i == 0 && access == WRITE){ // always write the last block first
                  if(test->filePerProc || rank == 0){
                    offset = (sizerand / test->blockSize - 1) * test->blockSize - test->transferSize;
                  }
                }                
            }
            for (j = 0; j < offsets &&  !hitStonewall ; j++) {
              if (test->randomOffset == 1) {
                if(test->filePerProc){
                  offset = offsets_rnd[j] + (i * test->blockSize);
                }else{
                  offset = offsets_rnd[j] + (i * test->numTasks * test->blockSize);
                }
              }else if (test->randomOffset > 1){
                offset += test->transferSize;
              }else{
                offset = j * test->transferSize;
                if (test->filePerProc) {
                  offset += i * test->blockSize;
                } else {
                  offset += (i * test->numTasks * test->blockSize) + (pretendRank * test->blockSize);
                }
              }
              dataMoved += WriteOrReadSingle(offset, pretendRank, test->transferSize, & errors, test, fd, ioBuffers, access, ot, startForStonewall);
              pairCnt++;

              hitStonewall = ((test->deadlineForStonewalling != 0
                  && (GetTimeStamp() - startForStonewall) > test->deadlineForStonewalling))
                  || (test->stoneWallingWearOutIterations != 0 && pairCnt == test->stoneWallingWearOutIterations) ;

              if ( test->collective && test->deadlineForStonewalling ) {
                // if collective-mode, you'll get a HANG, if some rank 'accidentally' leave this loop
                // it absolutely must be an 'all or none':
                MPI_CHECK(MPI_Bcast(&hitStonewall, 1, MPI_INT, 0, testComm), "hitStonewall broadcast failed");
              }
            }
          }
        } while((GetTimeStamp() - startForStonewall) < test->minTimeDuration);
        if (test->stoneWallingWearOut){
          if (verbose >= VERBOSE_1){
            fprintf(out_logfile, "%d: stonewalling pairs accessed: %lld\n", rank, (long long) pairCnt);
          }
          long long data_moved_ll = (long long) dataMoved;
          long long pairs_accessed_min = 0;
          MPI_CHECK(MPI_Allreduce(& pairCnt, &point->pairs_accessed,
                                  1, MPI_LONG_LONG_INT, MPI_MAX, testComm), "cannot reduce pairs moved");
          double stonewall_runtime = GetTimeStamp() - startForStonewall;
          point->stonewall_time = stonewall_runtime;
          MPI_CHECK(MPI_Reduce(& pairCnt, & pairs_accessed_min,
                                  1, MPI_LONG_LONG_INT, MPI_MIN, 0, testComm), "cannot reduce pairs moved");
          MPI_CHECK(MPI_Reduce(& data_moved_ll, &point->stonewall_min_data_accessed,
                                  1, MPI_LONG_LONG_INT, MPI_MIN, 0, testComm), "cannot reduce pairs moved");
          MPI_CHECK(MPI_Reduce(& data_moved_ll, &point->stonewall_total_data_accessed,
                                  1, MPI_LONG_LONG_INT, MPI_SUM, 0, testComm), "cannot reduce pairs moved");

          if(rank == 0){
            point->stonewall_avg_data_accessed = point->stonewall_total_data_accessed / test->numTasks;
            fprintf(out_logfile, "stonewalling pairs accessed min: %lld max: %zu -- min data: %.1f GiB mean data: %.1f GiB time: %.1fs\n",
             pairs_accessed_min, point->pairs_accessed,
             point->stonewall_min_data_accessed /1024.0 / 1024 / 1024, point->stonewall_avg_data_accessed / 1024.0 / 1024 / 1024 , point->stonewall_time);
          }
          if(pairCnt != point->pairs_accessed){
            // some work needs still to be done, complete the current block !
            i--;
            if(j == offsets){
              j = 0; // current block is completed
              i++;
            }
            for ( ; pairCnt < point->pairs_accessed; i++) {
              IOR_offset_t offset;
              if(i == test->segmentCount) i = 0; // wrap over, necessary to deal with minTimeDuration
              if (test->randomOffset > 1){
                  size_t sizerand = test->fileSizeForRead; 
                  if(test->filePerProc){
                    sizerand /= test->numTasks;
                  }
                  offset = rand64() % (sizerand / test->blockSize) * test->blockSize - test->transferSize;
              }
              for ( ; j < offsets && pairCnt < point->pairs_accessed ; j++) {
                if (test->randomOffset == 1) {
                  if(test->filePerProc){
                    offset = offsets_rnd[j] + (i * test->blockSize);
                  }else{
                    offset = offsets_rnd[j] + (i * test->numTasks * test->blockSize);
                  }
                }else if (test->randomOffset > 1){
                  offset += test->transferSize;
                }else{
                  offset = j * test->transferSize;
                  if (test->filePerProc) {
                    offset += i * test->blockSize;
                  } else {
                    offset += (i * test->numTasks * test->blockSize) + (pretendRank * test->blockSize);
                  }
                }
                dataMoved += WriteOrReadSingle(offset, pretendRank, test->transferSize, & errors, test, fd, ioBuffers, access, ot, startForStonewall);
                pairCnt++;
              }
              j = 0;              
            }
          }
        }else{
          point->pairs_accessed = pairCnt;
        }

        OpTimerFree(& ot);
        totalErrorCount += CountErrors(test, access, errors);

        if (access == WRITE && test->fsync == TRUE) {
                backend->fsync(fd, test->backend_options);       /*fsync after all accesses */
        }
        if(randomPrefillBuffer){
          aligned_buffer_free(randomPrefillBuffer, test->gpuMemoryFlags);
        }

        return (dataMoved);
}
