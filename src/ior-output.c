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

void PrintTableHeader(){
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n");
    fprintf(out_resultfile, "access    bw(MiB/s)  block(KiB) xfer(KiB)  open(s)    wr/rd(s)   close(s)   total(s)   iter\n");
    fprintf(out_resultfile, "------    ---------  ---------- ---------  --------   --------   --------   --------   ----\n");
  }
}

static int indent = 0;

static void PrintKeyValStart(char * key){
  if (outputFormat == OUTPUT_DEFAULT){
    for(int i=0; i < indent; i++){
      fprintf(out_resultfile, " ");
    }
    fprintf(out_resultfile, "%s: ", key);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": \"", key);
  }else if(outputFormat == OUTPUT_CSV){

  }
}

static void PrintNextToken(){
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, ", \n");
  }
}

static void PrintKeyValEnd(){
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"");
  }
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n");
  }
}

static void PrintIndent(){
  if(outputFormat == OUTPUT_CSV){
    return;
  }
  for(int i=0; i < indent; i++){
    fprintf(out_resultfile, "  ");
  }
}

static void PrintKeyVal(char * key, char * value){
  if(value[strlen(value) -1 ] == '\n'){
    // remove \n
    value[strlen(value) -1 ] = 0;
  }
  PrintIndent();
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%s: %s\n", key, value);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": \"%s\"", key, value);
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "%s", value);
  }
}

static void PrintKeyValInt(char * key, int64_t value){
  PrintIndent();
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%s: %lld\n", key, (long long) value);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": %lld", key, (long long)  value);
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "%lld", (long long) value);
  }
}

static void PrintStartSection(){
  indent++;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "{\n");
  }
}

static void PrintNamedSectionStart(char * key){
  PrintIndent();
  indent++;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": {\n", key);
  }else if(outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%s: \n", key);
  }
}

static void PrintEndSection(){
  indent--;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\n}\n");
  }
}

static void PrintArrayStart(char * key){
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": [\n", key);
  }
}

static void PrintArrayEnd(){
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "]\n");
  }
}

void PrintTestEnds(){
  if (rank != 0 ||  verbose < VERBOSE_0) {
    PrintEndSection();
    return;
  }

  PrintKeyVal("Finished", CurrentTimeString());
  PrintEndSection();
}

void PrintReducedResult(IOR_test_t *test, int access, double bw, double *diff_subset, double totalTime, int rep){
  fprintf(out_resultfile, "%-10s", access == WRITE ? "write" : "read");
  PPDouble(1, bw / MEBIBYTE, " ");
  PPDouble(1, (double)test->params.blockSize / KIBIBYTE, " ");
  PPDouble(1, (double)test->params.transferSize / KIBIBYTE, " ");
  PPDouble(1, diff_subset[0], " ");
  PPDouble(1, diff_subset[1], " ");
  PPDouble(1, diff_subset[2], " ");
  PPDouble(1, totalTime, " ");
  fprintf(out_resultfile, "%-4d\n", rep);
  fflush(out_resultfile);
}


/*
 * Message to print immediately after MPI_Init so we know that
 * ior has started.
 */
void PrintEarlyHeader()
{
        if (rank != 0)
                return;

        fprintf(out_resultfile, "IOR-" META_VERSION ": MPI Coordinated Test of Parallel I/O\n");
        fflush(out_resultfile);
}

void PrintHeader(int argc, char **argv)
{
        struct utsname unamebuf;
        int i;

        if (rank != 0)
                return;
        PrintStartSection();

        PrintKeyVal("Began", CurrentTimeString());
        PrintNextToken();
        PrintKeyValStart("Command line");
        fprintf(out_resultfile, "%s", argv[0]);
        for (i = 1; i < argc; i++) {
                fprintf(out_resultfile, " %s", argv[i]);
        }
        PrintKeyValEnd();
        PrintNextToken();
        if (uname(&unamebuf) != 0) {
                EWARN("uname failed");
                PrintKeyVal("Machine", "Unknown");
        } else {
                PrintKeyValStart("Machine");
                fprintf(out_resultfile, "%s %s", unamebuf.sysname,
                        unamebuf.nodename);
                if (verbose >= VERBOSE_2) {
                        fprintf(out_resultfile, " %s %s %s", unamebuf.release,
                                unamebuf.version, unamebuf.machine);
                }
                PrintKeyValEnd();
        }

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

        PrintNextToken();
        PrintArrayStart("tests");
        fflush(out_resultfile);
        fflush(out_logfile);
}

/*
 * Print header information for test output.
 */
void ShowTestInfo(IOR_param_t *params)
{
  PrintStartSection();
  PrintKeyValInt("TestID", params->id);
  PrintNextToken();
  PrintKeyVal("StartTime", CurrentTimeString());
  PrintNextToken();
  /* if pvfs2:, then skip */
  if (Regex(params->testFileName, "^[a-z][a-z].*:") == 0) {
          DisplayFreespace(params);
  }
  fflush(out_resultfile);
}

void ShowTestEnd(IOR_test_t *tptr){
  if(rank == 0 && tptr->params.stoneWallingWearOut){
    if (tptr->params.stoneWallingStatusFile[0]){
      StoreStoneWallingIterations(tptr->params.stoneWallingStatusFile, tptr->results->pairs_accessed);
    }else{
      fprintf(out_logfile, "Pairs deadlineForStonewallingaccessed: %lld\n", (long long) tptr->results->pairs_accessed);
    }
  }
  PrintEndSection();
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
  PrintNamedSectionStart("Flags");
  PrintKeyVal("api", params->apiVersion);
  PrintNextToken();
  PrintKeyVal("test filename", params->testFileName);
  PrintNextToken();
  PrintKeyVal("access", params->filePerProc ? "file-per-process" : "single-shared-file");
  PrintNextToken();
  PrintKeyVal("type", params->collective == FALSE ? "independent" : "collective");
  PrintNextToken();
        if (verbose >= VERBOSE_1) {
                if (params->segmentCount > 1) {
                        fprintf(out_resultfile,
                                "\tpattern            = strided (%d segments)\n",
                                (int)params->segmentCount);
                } else {
                        fprintf(out_resultfile,
                                "\tpattern            = segmented (1 segment)\n");
                }
        }
        fprintf(out_resultfile, "\tordering in a file =");
        if (params->randomOffset == FALSE) {
                fprintf(out_resultfile, " sequential offsets\n");
        } else {
                fprintf(out_resultfile, " random offsets\n");
        }
        fprintf(out_resultfile, "\tordering inter file=");
        if (params->reorderTasks == FALSE && params->reorderTasksRandom == FALSE) {
                fprintf(out_resultfile, " no tasks offsets\n");
        }
        if (params->reorderTasks == TRUE) {
                fprintf(out_resultfile, " constant task offsets = %d\n",
                        params->taskPerNodeOffset);
        }
        if (params->reorderTasksRandom == TRUE) {
                fprintf(out_resultfile, " random task offsets >= %d, seed=%d\n",
                        params->taskPerNodeOffset, params->reorderTasksRandomSeed);
        }
        fprintf(out_resultfile, "\tclients            = %d (%d per node)\n",
                params->numTasks, params->tasksPerNode);
        if (params->memoryPerTask != 0)
                fprintf(out_resultfile, "\tmemoryPerTask      = %s\n",
                       HumanReadable(params->memoryPerTask, BASE_TWO));
        if (params->memoryPerNode != 0)
                fprintf(out_resultfile, "\tmemoryPerNode      = %s\n",
                       HumanReadable(params->memoryPerNode, BASE_TWO));
        fprintf(out_resultfile, "\trepetitions        = %d\n", params->repetitions);
        fprintf(out_resultfile, "\txfersize           = %s\n",
                HumanReadable(params->transferSize, BASE_TWO));
        fprintf(out_resultfile, "\tblocksize          = %s\n",
                HumanReadable(params->blockSize, BASE_TWO));
        fprintf(out_resultfile, "\taggregate filesize = %s\n",
                HumanReadable(params->expectedAggFileSize, BASE_TWO));
#ifdef HAVE_LUSTRE_LUSTRE_USER_H
        if (params->lustre_set_striping) {
                fprintf(out_resultfile, "\tLustre stripe size = %s\n",
                       ((params->lustre_stripe_size == 0) ? "Use default" :
                        HumanReadable(params->lustre_stripe_size, BASE_TWO)));
                if (params->lustre_stripe_count == 0) {
                        fprintf(out_resultfile, "\t      stripe count = %s\n", "Use default");
                } else {
                        fprintf(out_resultfile, "\t      stripe count = %d\n",
                               params->lustre_stripe_count);
                }
        }
#endif /* HAVE_LUSTRE_LUSTRE_USER_H */
        if (params->deadlineForStonewalling > 0) {
                fprintf(out_resultfile, "\tUsing stonewalling = %d second(s)%s\n",
                        params->deadlineForStonewalling, params->stoneWallingWearOut ? " with phase out" : "");
        }
        PrintEndSection();
        fflush(out_resultfile);
}

/*
 * Show test description.
 */
void ShowTest(IOR_param_t * test)
{
        const char* data_packets[] = {"g", "t","o","i"};

        fprintf(out_resultfile, "TEST:\t%s=%d\n", "id", test->id);
        fprintf(out_resultfile, "\t%s=%d\n", "refnum", test->referenceNumber);
        fprintf(out_resultfile, "\t%s=%s\n", "api", test->api);
        fprintf(out_resultfile, "\t%s=%s\n", "platform", test->platform);
        fprintf(out_resultfile, "\t%s=%s\n", "testFileName", test->testFileName);
        fprintf(out_resultfile, "\t%s=%s\n", "hintsFileName", test->hintsFileName);
        fprintf(out_resultfile, "\t%s=%d\n", "deadlineForStonewall",
                test->deadlineForStonewalling);
        fprintf(out_resultfile, "\t%s=%d\n", "stoneWallingWearOut", test->stoneWallingWearOut);
        fprintf(out_resultfile, "\t%s=%d\n", "maxTimeDuration", test->maxTimeDuration);
        fprintf(out_resultfile, "\t%s=%d\n", "outlierThreshold",
                test->outlierThreshold);
        fprintf(out_resultfile, "\t%s=%s\n", "options", test->options);
        fprintf(out_resultfile, "\t%s=%d\n", "nodes", test->nodes);
        fprintf(out_resultfile, "\t%s=%lu\n", "memoryPerTask", (unsigned long) test->memoryPerTask);
        fprintf(out_resultfile, "\t%s=%lu\n", "memoryPerNode", (unsigned long) test->memoryPerNode);
        fprintf(out_resultfile, "\t%s=%d\n", "tasksPerNode", tasksPerNode);
        fprintf(out_resultfile, "\t%s=%d\n", "repetitions", test->repetitions);
        fprintf(out_resultfile, "\t%s=%d\n", "multiFile", test->multiFile);
        fprintf(out_resultfile, "\t%s=%d\n", "interTestDelay", test->interTestDelay);
        fprintf(out_resultfile, "\t%s=%d\n", "fsync", test->fsync);
        fprintf(out_resultfile, "\t%s=%d\n", "fsYncperwrite", test->fsyncPerWrite);
        fprintf(out_resultfile, "\t%s=%d\n", "useExistingTestFile",
                test->useExistingTestFile);
        fprintf(out_resultfile, "\t%s=%d\n", "showHints", test->showHints);
        fprintf(out_resultfile, "\t%s=%d\n", "uniqueDir", test->uniqueDir);
        fprintf(out_resultfile, "\t%s=%d\n", "showHelp", test->showHelp);
        fprintf(out_resultfile, "\t%s=%d\n", "individualDataSets",
                test->individualDataSets);
        fprintf(out_resultfile, "\t%s=%d\n", "singleXferAttempt",
                test->singleXferAttempt);
        fprintf(out_resultfile, "\t%s=%d\n", "readFile", test->readFile);
        fprintf(out_resultfile, "\t%s=%d\n", "writeFile", test->writeFile);
        fprintf(out_resultfile, "\t%s=%d\n", "filePerProc", test->filePerProc);
        fprintf(out_resultfile, "\t%s=%d\n", "reorderTasks", test->reorderTasks);
        fprintf(out_resultfile, "\t%s=%d\n", "reorderTasksRandom",
                test->reorderTasksRandom);
        fprintf(out_resultfile, "\t%s=%d\n", "reorderTasksRandomSeed",
                test->reorderTasksRandomSeed);
        fprintf(out_resultfile, "\t%s=%d\n", "randomOffset", test->randomOffset);
        fprintf(out_resultfile, "\t%s=%d\n", "checkWrite", test->checkWrite);
        fprintf(out_resultfile, "\t%s=%d\n", "checkRead", test->checkRead);
        fprintf(out_resultfile, "\t%s=%d\n", "preallocate", test->preallocate);
        fprintf(out_resultfile, "\t%s=%d\n", "useFileView", test->useFileView);
        fprintf(out_resultfile, "\t%s=%lld\n", "setAlignment", test->setAlignment);
        fprintf(out_resultfile, "\t%s=%d\n", "storeFileOffset", test->storeFileOffset);
        fprintf(out_resultfile, "\t%s=%d\n", "useSharedFilePointer",
                test->useSharedFilePointer);
        fprintf(out_resultfile, "\t%s=%d\n", "useO_DIRECT", test->useO_DIRECT);
        fprintf(out_resultfile, "\t%s=%d\n", "useStridedDatatype",
                test->useStridedDatatype);
        fprintf(out_resultfile, "\t%s=%d\n", "keepFile", test->keepFile);
        fprintf(out_resultfile, "\t%s=%d\n", "keepFileWithError",
                test->keepFileWithError);
        fprintf(out_resultfile, "\t%s=%d\n", "quitOnError", test->quitOnError);
        fprintf(out_resultfile, "\t%s=%d\n", "verbose", verbose);
        fprintf(out_resultfile, "\t%s=%s\n", "data packet type", data_packets[test->dataPacketType]);
        fprintf(out_resultfile, "\t%s=%d\n", "setTimeStampSignature/incompressibleSeed",
                test->setTimeStampSignature); /* Seed value was copied into setTimeStampSignature as well */
        fprintf(out_resultfile, "\t%s=%d\n", "collective", test->collective);
        fprintf(out_resultfile, "\t%s=%lld", "segmentCount", test->segmentCount);
#ifdef HAVE_GPFS_FCNTL_H
        fprintf(out_resultfile, "\t%s=%d\n", "gpfsHintAccess", test->gpfs_hint_access);
        fprintf(out_resultfile, "\t%s=%d\n", "gpfsReleaseToken", test->gpfs_release_token);
#endif
        if (strcasecmp(test->api, "HDF5") == 0) {
                fprintf(out_resultfile, " (datasets)");
        }
        fprintf(out_resultfile, "\n");
        fprintf(out_resultfile, "\t%s=%lld\n", "transferSize", test->transferSize);
        fprintf(out_resultfile, "\t%s=%lld\n", "blockSize", test->blockSize);
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

        fprintf(out_resultfile, "%-9s ", operation);
        fprintf(out_resultfile, "%10.2f ", bw->max / MEBIBYTE);
        fprintf(out_resultfile, "%10.2f ", bw->min / MEBIBYTE);
        fprintf(out_resultfile, "%10.2f ", bw->mean / MEBIBYTE);
        fprintf(out_resultfile, "%10.2f ", bw->sd / MEBIBYTE);
        fprintf(out_resultfile, "%10.2f ", ops->max);
        fprintf(out_resultfile, "%10.2f ", ops->min);
        fprintf(out_resultfile, "%10.2f ", ops->mean);
        fprintf(out_resultfile, "%10.2f ", ops->sd);
        fprintf(out_resultfile, "%10.5f ", mean_of_array_of_doubles(times, reps));
        fprintf(out_resultfile, "%5d ", params->id);
        fprintf(out_resultfile, "%6d ", params->numTasks);
        fprintf(out_resultfile, "%3d ", params->tasksPerNode);
        fprintf(out_resultfile, "%4d ", params->repetitions);
        fprintf(out_resultfile, "%3d ", params->filePerProc);
        fprintf(out_resultfile, "%5d ", params->reorderTasks);
        fprintf(out_resultfile, "%8d ", params->taskPerNodeOffset);
        fprintf(out_resultfile, "%9d ", params->reorderTasksRandom);
        fprintf(out_resultfile, "%4d ", params->reorderTasksRandomSeed);
        fprintf(out_resultfile, "%6lld ", params->segmentCount);
        fprintf(out_resultfile, "%8lld ", params->blockSize);
        fprintf(out_resultfile, "%8lld ", params->transferSize);
        fprintf(out_resultfile, "%9.1f ", (float)results->aggFileSizeForBW[0] / MEBIBYTE);
        fprintf(out_resultfile, "%3s ", params->api);
        fprintf(out_resultfile, "%6d", params->referenceNumber);
        fprintf(out_resultfile, "\n");
        fflush(out_resultfile);

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

        fprintf(out_resultfile, "\n");
        fprintf(out_resultfile, "%-9s %10s %10s %10s %10s %10s %10s %10s %10s %10s",
                "Operation", "Max(MiB)", "Min(MiB)", "Mean(MiB)", "StdDev",
                "Max(OPs)", "Min(OPs)", "Mean(OPs)", "StdDev",
                "Mean(s)");
        fprintf(out_resultfile, " Test# #Tasks tPN reps fPP reord reordoff reordrand seed"
                " segcnt ");
        fprintf(out_resultfile, "%8s %8s %9s %5s", " blksiz", "xsize","aggs(MiB)", "API");
        fprintf(out_resultfile, " RefNum\n");
}

void PrintLongSummaryAllTests(IOR_test_t *tests_head)
{
        IOR_test_t *tptr;
        if (rank != 0 || verbose < VERBOSE_0)
                return;

        fprintf(out_resultfile, "\n");
        fprintf(out_resultfile, "Summary of all tests:");
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

        fprintf(out_resultfile, "\n");
        if (params->writeFile) {
                fprintf(out_resultfile, "Max Write: %.2f MiB/sec (%.2f MB/sec)\n",
                        max_write/MEBIBYTE, max_write/MEGABYTE);
        }
        if (params->readFile) {
                fprintf(out_resultfile, "Max Read:  %.2f MiB/sec (%.2f MB/sec)\n",
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
}


void PrintRemoveTiming(double start, double finish, int rep)
{
  if (rank != 0 || verbose < VERBOSE_0)
    return;

  fprintf(out_resultfile, "remove    -          -          -          -          -          -          ");
  PPDouble(1, finish-start, " ");
  fprintf(out_resultfile, "%-4d\n", rep);
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
                fprintf(out_resultfile, "   -      %s", append);
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

        fprintf(out_resultfile, format, number, append);
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
