#ifndef _WIN32
# include <sys/utsname.h>        /* uname() */
#endif

#include <math.h>

#include "ior.h"
#include "ior-internal.h"
#include "utilities.h"

extern char **environ;

static struct results *bw_values(int reps, IOR_offset_t *agg_file_size, double *vals);
static struct results *ops_values(int reps, IOR_offset_t *agg_file_size, IOR_offset_t transfer_size, double *vals);
static double mean_of_array_of_doubles(double *values, int len);
static void PPDouble(int leftjustify, double number, char *append);

void PrintReducedResult(IOR_test_t *test, int access, double bw, double *diff_subset, double totalTime, int rep){
  fprintf(out_logfile, "%-10s", access == WRITE ? "write" : "read");
  PPDouble(1, bw / MEBIBYTE, " ");
  PPDouble(1, (double)test->params.blockSize / KIBIBYTE, " ");
  PPDouble(1, (double)test->params.transferSize / KIBIBYTE, " ");
  PPDouble(1, diff_subset[0], " ");
  PPDouble(1, diff_subset[1], " ");
  PPDouble(1, diff_subset[2], " ");
  PPDouble(1, totalTime, " ");
  fprintf(out_logfile, "%-4d\n", rep);
  fflush(out_logfile);
}


/*
 * Message to print immediately after MPI_Init so we know that
 * ior has started.
 */
void PrintEarlyHeader()
{
        if (rank != 0)
                return;

        fprintf(out_logfile, "IOR-" META_VERSION ": MPI Coordinated Test of Parallel I/O\n");
        fflush(out_logfile);
}

void PrintHeader(int argc, char **argv)
{
        struct utsname unamebuf;
        int i;

        if (rank != 0)
                return;

        fprintf(out_logfile, "Began: %s", CurrentTimeString());
        fprintf(out_logfile, "Command line used: %s", argv[0]);
        for (i = 1; i < argc; i++) {
                fprintf(out_logfile, " \"%s\"", argv[i]);
        }
        fprintf(out_logfile, "\n");
        if (uname(&unamebuf) != 0) {
                EWARN("uname failed");
                fprintf(out_logfile, "Machine: Unknown");
        } else {
                fprintf(out_logfile, "Machine: %s %s", unamebuf.sysname,
                        unamebuf.nodename);
                if (verbose >= VERBOSE_2) {
                        fprintf(out_logfile, " %s %s %s", unamebuf.release,
                                unamebuf.version, unamebuf.machine);
                }
        }
        fprintf(out_logfile, "\n");
#ifdef _NO_MPI_TIMER
        if (verbose >= VERBOSE_2)
                fprintf(out_logfile, "Using unsynchronized POSIX timer\n");
#else                           /* not _NO_MPI_TIMER */
        if (MPI_WTIME_IS_GLOBAL) {
                if (verbose >= VERBOSE_2)
                        fprintf(out_logfile, "Using synchronized MPI timer\n");
        } else {
                if (verbose >= VERBOSE_2)
                        fprintf(out_logfile, "Using unsynchronized MPI timer\n");
        }
#endif                          /* _NO_MPI_TIMER */
        if (verbose >= VERBOSE_1) {
                fprintf(out_logfile, "Start time skew across all tasks: %.02f sec\n",
                        wall_clock_deviation);
        }
        if (verbose >= VERBOSE_3) {     /* show env */
                fprintf(out_logfile, "STARTING ENVIRON LOOP\n");
                for (i = 0; environ[i] != NULL; i++) {
                        fprintf(out_logfile, "%s\n", environ[i]);
                }
                fprintf(out_logfile, "ENDING ENVIRON LOOP\n");
        }
        fflush(out_logfile);
}

/*
 * Print header information for test output.
 */
void ShowTestInfo(IOR_param_t *params)
{
        fprintf(out_logfile, "\n");
        fprintf(out_logfile, "Test %d started: %s", params->id, CurrentTimeString());
        if (verbose >= VERBOSE_1) {
                /* if pvfs2:, then skip */
                if (Regex(params->testFileName, "^[a-z][a-z].*:") == 0) {
                        DisplayFreespace(params);
                }
        }
        fflush(out_logfile);
}

/*
 * Show simple test output with max results for iterations.
 */
void ShowSetup(IOR_param_t *params)
{

        if (strcmp(params->debug, "") != 0) {
                fprintf(out_logfile, "\n*** DEBUG MODE ***\n");
                fprintf(out_logfile, "*** %s ***\n\n", params->debug);
        }
        fprintf(out_logfile, "Summary:\n");
        fprintf(out_logfile, "\tapi                = %s\n", params->apiVersion);
        fprintf(out_logfile, "\ttest filename      = %s\n", params->testFileName);
        fprintf(out_logfile, "\taccess             = ");
        fprintf(out_logfile, params->filePerProc ? "file-per-process" : "single-shared-file");
        if (verbose >= VERBOSE_1 && strcmp(params->api, "POSIX") != 0) {
                fprintf(out_logfile, params->collective == FALSE ? ", independent" : ", collective");
        }
        fprintf(out_logfile, "\n");
        if (verbose >= VERBOSE_1) {
                if (params->segmentCount > 1) {
                        fprintf(out_logfile,
                                "\tpattern            = strided (%d segments)\n",
                                (int)params->segmentCount);
                } else {
                        fprintf(out_logfile,
                                "\tpattern            = segmented (1 segment)\n");
                }
        }
        fprintf(out_logfile, "\tordering in a file =");
        if (params->randomOffset == FALSE) {
                fprintf(out_logfile, " sequential offsets\n");
        } else {
                fprintf(out_logfile, " random offsets\n");
        }
        fprintf(out_logfile, "\tordering inter file=");
        if (params->reorderTasks == FALSE && params->reorderTasksRandom == FALSE) {
                fprintf(out_logfile, " no tasks offsets\n");
        }
        if (params->reorderTasks == TRUE) {
                fprintf(out_logfile, " constant task offsets = %d\n",
                        params->taskPerNodeOffset);
        }
        if (params->reorderTasksRandom == TRUE) {
                fprintf(out_logfile, " random task offsets >= %d, seed=%d\n",
                        params->taskPerNodeOffset, params->reorderTasksRandomSeed);
        }
        fprintf(out_logfile, "\tclients            = %d (%d per node)\n",
                params->numTasks, params->tasksPerNode);
        if (params->memoryPerTask != 0)
                fprintf(out_logfile, "\tmemoryPerTask      = %s\n",
                       HumanReadable(params->memoryPerTask, BASE_TWO));
        if (params->memoryPerNode != 0)
                fprintf(out_logfile, "\tmemoryPerNode      = %s\n",
                       HumanReadable(params->memoryPerNode, BASE_TWO));
        fprintf(out_logfile, "\trepetitions        = %d\n", params->repetitions);
        fprintf(out_logfile, "\txfersize           = %s\n",
                HumanReadable(params->transferSize, BASE_TWO));
        fprintf(out_logfile, "\tblocksize          = %s\n",
                HumanReadable(params->blockSize, BASE_TWO));
        fprintf(out_logfile, "\taggregate filesize = %s\n",
                HumanReadable(params->expectedAggFileSize, BASE_TWO));
#ifdef HAVE_LUSTRE_LUSTRE_USER_H
        if (params->lustre_set_striping) {
                fprintf(out_logfile, "\tLustre stripe size = %s\n",
                       ((params->lustre_stripe_size == 0) ? "Use default" :
                        HumanReadable(params->lustre_stripe_size, BASE_TWO)));
                if (params->lustre_stripe_count == 0) {
                        fprintf(out_logfile, "\t      stripe count = %s\n", "Use default");
                } else {
                        fprintf(out_logfile, "\t      stripe count = %d\n",
                               params->lustre_stripe_count);
                }
        }
#endif /* HAVE_LUSTRE_LUSTRE_USER_H */
        if (params->deadlineForStonewalling > 0) {
                fprintf(out_logfile, "\tUsing stonewalling = %d second(s)%s\n",
                        params->deadlineForStonewalling, params->stoneWallingWearOut ? " with phase out" : "");
        }
        fflush(out_logfile);
}

/*
 * Show test description.
 */
void ShowTest(IOR_param_t * test)
{
        const char* data_packets[] = {"g", "t","o","i"};

        fprintf(out_logfile, "TEST:\t%s=%d\n", "id", test->id);
        fprintf(out_logfile, "\t%s=%d\n", "refnum", test->referenceNumber);
        fprintf(out_logfile, "\t%s=%s\n", "api", test->api);
        fprintf(out_logfile, "\t%s=%s\n", "platform", test->platform);
        fprintf(out_logfile, "\t%s=%s\n", "testFileName", test->testFileName);
        fprintf(out_logfile, "\t%s=%s\n", "hintsFileName", test->hintsFileName);
        fprintf(out_logfile, "\t%s=%d\n", "deadlineForStonewall",
                test->deadlineForStonewalling);
        fprintf(out_logfile, "\t%s=%d\n", "stoneWallingWearOut", test->stoneWallingWearOut);
        fprintf(out_logfile, "\t%s=%d\n", "maxTimeDuration", test->maxTimeDuration);
        fprintf(out_logfile, "\t%s=%d\n", "outlierThreshold",
                test->outlierThreshold);
        fprintf(out_logfile, "\t%s=%s\n", "options", test->options);
        fprintf(out_logfile, "\t%s=%d\n", "nodes", test->nodes);
        fprintf(out_logfile, "\t%s=%lu\n", "memoryPerTask", (unsigned long) test->memoryPerTask);
        fprintf(out_logfile, "\t%s=%lu\n", "memoryPerNode", (unsigned long) test->memoryPerNode);
        fprintf(out_logfile, "\t%s=%d\n", "tasksPerNode", tasksPerNode);
        fprintf(out_logfile, "\t%s=%d\n", "repetitions", test->repetitions);
        fprintf(out_logfile, "\t%s=%d\n", "multiFile", test->multiFile);
        fprintf(out_logfile, "\t%s=%d\n", "interTestDelay", test->interTestDelay);
        fprintf(out_logfile, "\t%s=%d\n", "fsync", test->fsync);
        fprintf(out_logfile, "\t%s=%d\n", "fsYncperwrite", test->fsyncPerWrite);
        fprintf(out_logfile, "\t%s=%d\n", "useExistingTestFile",
                test->useExistingTestFile);
        fprintf(out_logfile, "\t%s=%d\n", "showHints", test->showHints);
        fprintf(out_logfile, "\t%s=%d\n", "uniqueDir", test->uniqueDir);
        fprintf(out_logfile, "\t%s=%d\n", "showHelp", test->showHelp);
        fprintf(out_logfile, "\t%s=%d\n", "individualDataSets",
                test->individualDataSets);
        fprintf(out_logfile, "\t%s=%d\n", "singleXferAttempt",
                test->singleXferAttempt);
        fprintf(out_logfile, "\t%s=%d\n", "readFile", test->readFile);
        fprintf(out_logfile, "\t%s=%d\n", "writeFile", test->writeFile);
        fprintf(out_logfile, "\t%s=%d\n", "filePerProc", test->filePerProc);
        fprintf(out_logfile, "\t%s=%d\n", "reorderTasks", test->reorderTasks);
        fprintf(out_logfile, "\t%s=%d\n", "reorderTasksRandom",
                test->reorderTasksRandom);
        fprintf(out_logfile, "\t%s=%d\n", "reorderTasksRandomSeed",
                test->reorderTasksRandomSeed);
        fprintf(out_logfile, "\t%s=%d\n", "randomOffset", test->randomOffset);
        fprintf(out_logfile, "\t%s=%d\n", "checkWrite", test->checkWrite);
        fprintf(out_logfile, "\t%s=%d\n", "checkRead", test->checkRead);
        fprintf(out_logfile, "\t%s=%d\n", "preallocate", test->preallocate);
        fprintf(out_logfile, "\t%s=%d\n", "useFileView", test->useFileView);
        fprintf(out_logfile, "\t%s=%lld\n", "setAlignment", test->setAlignment);
        fprintf(out_logfile, "\t%s=%d\n", "storeFileOffset", test->storeFileOffset);
        fprintf(out_logfile, "\t%s=%d\n", "useSharedFilePointer",
                test->useSharedFilePointer);
        fprintf(out_logfile, "\t%s=%d\n", "useO_DIRECT", test->useO_DIRECT);
        fprintf(out_logfile, "\t%s=%d\n", "useStridedDatatype",
                test->useStridedDatatype);
        fprintf(out_logfile, "\t%s=%d\n", "keepFile", test->keepFile);
        fprintf(out_logfile, "\t%s=%d\n", "keepFileWithError",
                test->keepFileWithError);
        fprintf(out_logfile, "\t%s=%d\n", "quitOnError", test->quitOnError);
        fprintf(out_logfile, "\t%s=%d\n", "verbose", verbose);
        fprintf(out_logfile, "\t%s=%s\n", "data packet type", data_packets[test->dataPacketType]);
        fprintf(out_logfile, "\t%s=%d\n", "setTimeStampSignature/incompressibleSeed",
                test->setTimeStampSignature); /* Seed value was copied into setTimeStampSignature as well */
        fprintf(out_logfile, "\t%s=%d\n", "collective", test->collective);
        fprintf(out_logfile, "\t%s=%lld", "segmentCount", test->segmentCount);
#ifdef HAVE_GPFS_FCNTL_H
        fprintf(out_logfile, "\t%s=%d\n", "gpfsHintAccess", test->gpfs_hint_access);
        fprintf(out_logfile, "\t%s=%d\n", "gpfsReleaseToken", test->gpfs_release_token);
#endif
        if (strcasecmp(test->api, "HDF5") == 0) {
                fprintf(out_logfile, " (datasets)");
        }
        fprintf(out_logfile, "\n");
        fprintf(out_logfile, "\t%s=%lld\n", "transferSize", test->transferSize);
        fprintf(out_logfile, "\t%s=%lld\n", "blockSize", test->blockSize);
}


/*
 * Summarize results
 *
 * operation is typically "write" or "read"
 */
void PrintLongSummaryOneOperation(IOR_test_t *test, double *times, char *operation)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;
        struct results *bw;
        struct results *ops;
        int reps;
        if (rank != 0 || verbose < VERBOSE_0)
                return;

        reps = params->repetitions;

        bw = bw_values(reps, results->aggFileSizeForBW, times);
        ops = ops_values(reps, results->aggFileSizeForBW,
                         params->transferSize, times);

        fprintf(out_logfile, "%-9s ", operation);
        fprintf(out_logfile, "%10.2f ", bw->max / MEBIBYTE);
        fprintf(out_logfile, "%10.2f ", bw->min / MEBIBYTE);
        fprintf(out_logfile, "%10.2f ", bw->mean / MEBIBYTE);
        fprintf(out_logfile, "%10.2f ", bw->sd / MEBIBYTE);
        fprintf(out_logfile, "%10.2f ", ops->max);
        fprintf(out_logfile, "%10.2f ", ops->min);
        fprintf(out_logfile, "%10.2f ", ops->mean);
        fprintf(out_logfile, "%10.2f ", ops->sd);
        fprintf(out_logfile, "%10.5f ", mean_of_array_of_doubles(times, reps));
        fprintf(out_logfile, "%5d ", params->id);
        fprintf(out_logfile, "%6d ", params->numTasks);
        fprintf(out_logfile, "%3d ", params->tasksPerNode);
        fprintf(out_logfile, "%4d ", params->repetitions);
        fprintf(out_logfile, "%3d ", params->filePerProc);
        fprintf(out_logfile, "%5d ", params->reorderTasks);
        fprintf(out_logfile, "%8d ", params->taskPerNodeOffset);
        fprintf(out_logfile, "%9d ", params->reorderTasksRandom);
        fprintf(out_logfile, "%4d ", params->reorderTasksRandomSeed);
        fprintf(out_logfile, "%6lld ", params->segmentCount);
        fprintf(out_logfile, "%8lld ", params->blockSize);
        fprintf(out_logfile, "%8lld ", params->transferSize);
        fprintf(out_logfile, "%9.1f ", (float)results->aggFileSizeForBW[0] / MEBIBYTE);
        fprintf(out_logfile, "%3s ", params->api);
        fprintf(out_logfile, "%6d", params->referenceNumber);
        fprintf(out_logfile, "\n");
        fflush(out_logfile);

        free(bw);
        free(ops);
}

void PrintLongSummaryOneTest(IOR_test_t *test)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;

        if (params->writeFile)
                PrintLongSummaryOneOperation(test, results->writeTime, "write");
        if (params->readFile)
                PrintLongSummaryOneOperation(test, results->readTime, "read");
}

void PrintLongSummaryHeader()
{
        if (rank != 0 || verbose < VERBOSE_0)
                return;

        fprintf(out_logfile, "\n");
        fprintf(out_logfile, "%-9s %10s %10s %10s %10s %10s %10s %10s %10s %10s",
                "Operation", "Max(MiB)", "Min(MiB)", "Mean(MiB)", "StdDev",
                "Max(OPs)", "Min(OPs)", "Mean(OPs)", "StdDev",
                "Mean(s)");
        fprintf(out_logfile, " Test# #Tasks tPN reps fPP reord reordoff reordrand seed"
                " segcnt ");
        fprintf(out_logfile, "%8s %8s %9s %5s", " blksiz", "xsize","aggs(MiB)", "API");
        fprintf(out_logfile, " RefNum\n");
}

void PrintLongSummaryAllTests(IOR_test_t *tests_head)
{
        IOR_test_t *tptr;
        if (rank != 0 || verbose < VERBOSE_0)
                return;

        fprintf(out_logfile, "\n");
        fprintf(out_logfile, "Summary of all tests:");
        PrintLongSummaryHeader();

        for (tptr = tests_head; tptr != NULL; tptr = tptr->next) {
                PrintLongSummaryOneTest(tptr);
        }
}

void PrintShortSummary(IOR_test_t * test)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;
        double max_write = 0.0;
        double max_read = 0.0;
        double bw;
        int reps;
        int i;

        if (rank != 0 || verbose < VERBOSE_0)
                return;

        reps = params->repetitions;

        max_write = results->writeTime[0];
        max_read = results->readTime[0];
        for (i = 0; i < reps; i++) {
                bw = (double)results->aggFileSizeForBW[i]/results->writeTime[i];
                max_write = MAX(bw, max_write);
                bw = (double)results->aggFileSizeForBW[i]/results->readTime[i];
                max_read = MAX(bw, max_read);
        }

        fprintf(out_logfile, "\n");
        if (params->writeFile) {
                fprintf(out_logfile, "Max Write: %.2f MiB/sec (%.2f MB/sec)\n",
                        max_write/MEBIBYTE, max_write/MEGABYTE);
        }
        if (params->readFile) {
                fprintf(out_logfile, "Max Read:  %.2f MiB/sec (%.2f MB/sec)\n",
                        max_read/MEBIBYTE, max_read/MEGABYTE);
        }
}


/*
 * Display freespace (df).
 */
void DisplayFreespace(IOR_param_t * test)
{
        char fileName[MAX_STR] = { 0 };
        int i;
        int directoryFound = FALSE;

        /* get outfile name */
        GetTestFileName(fileName, test);

        /* get directory for outfile */
        i = strlen(fileName);
        while (i-- > 0) {
                if (fileName[i] == '/') {
                        fileName[i] = '\0';
                        directoryFound = TRUE;
                        break;
                }
        }

        /* if no directory/, use '.' */
        if (directoryFound == FALSE) {
                strcpy(fileName, ".");
        }

        ShowFileSystemSize(fileName);

        return;
}


void PrintRemoveTiming(double start, double finish, int rep)
{
  if (rank != 0 || verbose < VERBOSE_0)
    return;

  fprintf(out_logfile, "remove    -          -          -          -          -          -          ");
  PPDouble(1, finish-start, " ");
  fprintf(out_logfile, "%-4d\n", rep);
}


/*
 * Pretty Print a Double.  The First parameter is a flag determining if left
 * justification should be used.  The third parameter a null-terminated string
 * that should be appended to the number field.
 */
static void PPDouble(int leftjustify, double number, char *append)
{
        char format[16];
        int width = 10;
        int precision;

        if (number < 0) {
                fprintf(out_logfile, "   -      %s", append);
                return;
        }

        if (number < 1)
                precision = 6;
        else if (number < 3600)
                precision = 2;
        else
                precision = 0;

        sprintf(format, "%%%s%d.%df%%s",
                leftjustify ? "-" : "",
                width, precision);

        fprintf(out_logfile, format, number, append);
}



static struct results *bw_values(int reps, IOR_offset_t *agg_file_size, double *vals)
{
        struct results *r;
        int i;

        r = (struct results *)malloc(sizeof(struct results)
                                     + (reps * sizeof(double)));
        if (r == NULL)
                ERR("malloc failed");
        r->val = (double *)&r[1];

        for (i = 0; i < reps; i++) {
                r->val[i] = (double)agg_file_size[i] / vals[i];
                if (i == 0) {
                        r->min = r->val[i];
                        r->max = r->val[i];
                        r->sum = 0.0;
                }
                r->min = MIN(r->min, r->val[i]);
                r->max = MAX(r->max, r->val[i]);
                r->sum += r->val[i];
        }
        r->mean = r->sum / reps;
        r->var = 0.0;
        for (i = 0; i < reps; i++) {
                r->var += pow((r->mean - r->val[i]), 2);
        }
        r->var = r->var / reps;
        r->sd = sqrt(r->var);

        return r;
}

static struct results *ops_values(int reps, IOR_offset_t *agg_file_size,
                                  IOR_offset_t transfer_size,
                                  double *vals)
{
        struct results *r;
        int i;

        r = (struct results *)malloc(sizeof(struct results)
                                     + (reps * sizeof(double)));
        if (r == NULL)
                ERR("malloc failed");
        r->val = (double *)&r[1];

        for (i = 0; i < reps; i++) {
                r->val[i] = (double)agg_file_size[i] / transfer_size / vals[i];
                if (i == 0) {
                        r->min = r->val[i];
                        r->max = r->val[i];
                        r->sum = 0.0;
                }
                r->min = MIN(r->min, r->val[i]);
                r->max = MAX(r->max, r->val[i]);
                r->sum += r->val[i];
        }
        r->mean = r->sum / reps;
        r->var = 0.0;
        for (i = 0; i < reps; i++) {
                r->var += pow((r->mean - r->val[i]), 2);
        }
        r->var = r->var / reps;
        r->sd = sqrt(r->var);

        return r;
}


static double mean_of_array_of_doubles(double *values, int len)
{
        double tot = 0.0;
        int i;

        for (i = 0; i < len; i++) {
                tot += values[i];
        }
        return tot / len;

}
