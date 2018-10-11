#ifndef _WIN32
# include <sys/utsname.h>        /* uname() */
#endif

#include <math.h>
#include <stddef.h>             /* needed for offsetof on some compilers */

#include "ior.h"
#include "ior-internal.h"
#include "utilities.h"

extern char **environ;

static double mean_of_array_of_doubles(double *values, int len);
static void PPDouble(int leftjustify, double number, char *append);
static void PrintNextToken();

void PrintTableHeader(){
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n");
    fprintf(out_resultfile, "access    bw(MiB/s)  block(KiB) xfer(KiB)  open(s)    wr/rd(s)   close(s)   total(s)   iter\n");
    fprintf(out_resultfile, "------    ---------  ---------- ---------  --------   --------   --------   --------   ----\n");
  }
}

static int indent = 0;
static int needNextToken = 0;

static void PrintIndent(){
  if(outputFormat != OUTPUT_JSON){
    return;
  }
  for(int i=0; i < indent; i++){
    fprintf(out_resultfile, "  ");
  }
}


static void PrintKeyValStart(char * key){
  PrintNextToken();
  if (outputFormat == OUTPUT_DEFAULT){
    PrintIndent();
    fprintf(out_resultfile, "%-20s: ", key);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": \"", key);
  }else if(outputFormat == OUTPUT_CSV){

  }
}

static void PrintNextToken(){
  if(needNextToken){
    needNextToken = 0;
    if(outputFormat == OUTPUT_JSON){
      fprintf(out_resultfile, ", \n");
    }
  }
  PrintIndent();
}

static void PrintKeyValEnd(){
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"");
  }
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n");
  }
  needNextToken = 1;
}

static void PrintKeyVal(char * key, char * value){
  if(value != NULL && value[0] != 0 && value[strlen(value) -1 ] == '\n'){
    // remove \n
    value[strlen(value) -1 ] = 0;
  }
  PrintNextToken();
  needNextToken = 1;
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%-20s: %s\n", key, value);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": \"%s\"", key, value);
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "%s", value);
  }
}

static void PrintKeyValDouble(char * key, double value){
  PrintNextToken();
  needNextToken = 1;
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%-20s: %.4f\n", key, value);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": %.4f", key, value);
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "%.4f", value);
  }
}


static void PrintKeyValInt(char * key, int64_t value){
  PrintNextToken();
  needNextToken = 1;
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%-20s: %lld\n", key, (long long) value);
    return;
  }
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": %lld", key, (long long)  value);
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "%lld", (long long) value);
  }
}

static void PrintStartSection(){
  PrintNextToken();
  needNextToken = 0;
  if(outputFormat == OUTPUT_JSON){
    PrintIndent();
    fprintf(out_resultfile, "{\n");
  }
  indent++;
}

static void PrintNamedSectionStart(char * key){
  PrintNextToken();
  needNextToken = 0;
  indent++;

  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": {\n", key);
  }else if(outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n%s: \n", key);
  }
}

static void PrintNamedArrayStart(char * key){
  PrintNextToken();
  needNextToken = 0;
  indent++;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": [\n", key);
  }else if(outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n%s: \n", key);
  }
}

static void PrintEndSection(){
  if (rank != 0)
    return;

  indent--;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\n");
    PrintIndent();
    fprintf(out_resultfile, "}\n");
  }
  needNextToken = 1;
}

static void PrintArrayStart(){
  if (rank != 0)
    return;
  PrintNextToken();
  needNextToken = 0;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "[ ");
  }
}

static void PrintArrayNamedStart(char * key){
  if (rank != 0)
    return;
  PrintNextToken();
  needNextToken = 0;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "\"%s\": [\n", key);
  }
}

static void PrintArrayEnd(){
  if (rank != 0)
    return;

  indent--;
  if(outputFormat == OUTPUT_JSON){
    fprintf(out_resultfile, "]\n");
  }
  needNextToken = 1;
}

void PrintRepeatEnd(){
  if (rank != 0)
          return;
  PrintArrayEnd();
}

void PrintRepeatStart(){
  if (rank != 0)
          return;
  if( outputFormat == OUTPUT_DEFAULT){
    return;
  }
  PrintArrayStart();
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
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%-10s", access == WRITE ? "write" : "read");
    PPDouble(1, bw / MEBIBYTE, " ");
    PPDouble(1, (double)test->params.blockSize / KIBIBYTE, " ");
    PPDouble(1, (double)test->params.transferSize / KIBIBYTE, " ");
    PPDouble(1, diff_subset[0], " ");
    PPDouble(1, diff_subset[1], " ");
    PPDouble(1, diff_subset[2], " ");
    PPDouble(1, totalTime, " ");
    fprintf(out_resultfile, "%-4d\n", rep);
  }else if (outputFormat == OUTPUT_JSON){
    PrintStartSection();
    PrintKeyVal("access", access == WRITE ? "write" : "read");
    PrintKeyValDouble("bwMiB", bw / MEBIBYTE);
    PrintKeyValDouble("blockKiB", (double)test->params.blockSize / KIBIBYTE);
    PrintKeyValDouble("xferKiB", (double)test->params.transferSize / KIBIBYTE);
    PrintKeyValDouble("openTime", diff_subset[0]);
    PrintKeyValDouble("wrRdTime", diff_subset[1]);
    PrintKeyValDouble("closeTime", diff_subset[2]);
    PrintKeyValDouble("totalTime", totalTime);
    PrintEndSection();
  }
  fflush(out_resultfile);
}

void PrintHeader(int argc, char **argv)
{
        struct utsname unamebuf;
        int i;

        if (rank != 0)
                return;

        PrintStartSection();
        if (outputFormat != OUTPUT_DEFAULT){
          PrintKeyVal("Version", META_VERSION);
        }else{
          printf("IOR-" META_VERSION ": MPI Coordinated Test of Parallel I/O\n");
        }
        PrintKeyVal("Began", CurrentTimeString());
        PrintKeyValStart("Command line");
        fprintf(out_resultfile, "%s", argv[0]);
        for (i = 1; i < argc; i++) {
                fprintf(out_resultfile, " %s", argv[i]);
        }
        PrintKeyValEnd();
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

        PrintArrayNamedStart("tests");
        fflush(out_resultfile);
        fflush(out_logfile);
}

/*
 * Print header information for test output.
 */
void ShowTestStart(IOR_param_t *test)
{
  PrintStartSection();
  PrintKeyValInt("TestID", test->id);
  PrintKeyVal("StartTime", CurrentTimeString());
  /* if pvfs2:, then skip */
  if (Regex(test->testFileName, "^[a-z][a-z].*:") == 0) {
      DisplayFreespace(test);
  }

  if (verbose >= VERBOSE_3 || outputFormat == OUTPUT_JSON) {
    char* data_packets[] = {"g","t","o","i"};

    PrintNamedSectionStart("Parameters");
    PrintKeyValInt("testID", test->id);
    PrintKeyValInt("refnum", test->referenceNumber);
    PrintKeyVal("api", test->api);
    PrintKeyVal("platform", test->platform);
    PrintKeyVal("testFileName", test->testFileName);
    PrintKeyVal("hintsFileName", test->hintsFileName);
    PrintKeyValInt("deadlineForStonewall", test->deadlineForStonewalling);
    PrintKeyValInt("stoneWallingWearOut", test->stoneWallingWearOut);
    PrintKeyValInt("maxTimeDuration", test->maxTimeDuration);
    PrintKeyValInt("outlierThreshold", test->outlierThreshold);

    PrintKeyVal("options", test->options);
    PrintKeyValInt("nodes", test->nodes);
    PrintKeyValInt("memoryPerTask", (unsigned long) test->memoryPerTask);
    PrintKeyValInt("memoryPerNode", (unsigned long) test->memoryPerNode);
    PrintKeyValInt("tasksPerNode", tasksPerNode);
    PrintKeyValInt("repetitions", test->repetitions);
    PrintKeyValInt("multiFile", test->multiFile);
    PrintKeyValInt("interTestDelay", test->interTestDelay);
    PrintKeyValInt("fsync", test->fsync);
    PrintKeyValInt("fsyncperwrite", test->fsyncPerWrite);
    PrintKeyValInt("useExistingTestFile", test->useExistingTestFile);
    PrintKeyValInt("showHints", test->showHints);
    PrintKeyValInt("uniqueDir", test->uniqueDir);
    PrintKeyValInt("individualDataSets", test->individualDataSets);
    PrintKeyValInt("singleXferAttempt", test->singleXferAttempt);
    PrintKeyValInt("readFile", test->readFile);
    PrintKeyValInt("writeFile", test->writeFile);
    PrintKeyValInt("filePerProc", test->filePerProc);
    PrintKeyValInt("reorderTasks", test->reorderTasks);
    PrintKeyValInt("reorderTasksRandom", test->reorderTasksRandom);
    PrintKeyValInt("reorderTasksRandomSeed", test->reorderTasksRandomSeed);
    PrintKeyValInt("randomOffset", test->randomOffset);
    PrintKeyValInt("checkWrite", test->checkWrite);
    PrintKeyValInt("checkRead", test->checkRead);
    PrintKeyValInt("preallocate", test->preallocate);
    PrintKeyValInt("useFileView", test->useFileView);
    PrintKeyValInt("setAlignment", test->setAlignment);
    PrintKeyValInt("storeFileOffset", test->storeFileOffset);
    PrintKeyValInt("useSharedFilePointer", test->useSharedFilePointer);
    PrintKeyValInt("useO_DIRECT", test->useO_DIRECT);
    PrintKeyValInt("useStridedDatatype", test->useStridedDatatype);
    PrintKeyValInt("keepFile", test->keepFile);
    PrintKeyValInt("keepFileWithError", test->keepFileWithError);
    PrintKeyValInt("quitOnError", test->quitOnError);
    PrintKeyValInt("verbose", verbose);
    PrintKeyVal("data packet type", data_packets[test->dataPacketType]);
    PrintKeyValInt("setTimeStampSignature/incompressibleSeed", test->setTimeStampSignature); /* Seed value was copied into setTimeStampSignature as well */
    PrintKeyValInt("collective", test->collective);
    PrintKeyValInt("segmentCount", test->segmentCount);
    #ifdef HAVE_GPFS_FCNTL_H
    PrintKeyValInt("gpfsHintAccess", test->gpfs_hint_access);
    PrintKeyValInt("gpfsReleaseToken", test->gpfs_release_token);
    #endif
    PrintKeyValInt("transferSize", test->transferSize);
    PrintKeyValInt("blockSize", test->blockSize);
    PrintEndSection();
  }

  fflush(out_resultfile);
}

void ShowTestEnd(IOR_test_t *tptr){
  if(rank == 0 && tptr->params.stoneWallingWearOut){

    size_t pairs_accessed = tptr->results->write.pairs_accessed;
    if (tptr->params.stoneWallingStatusFile){
      StoreStoneWallingIterations(tptr->params.stoneWallingStatusFile, pairs_accessed);
    }else{
      fprintf(out_logfile, "Pairs deadlineForStonewallingaccessed: %ld\n", pairs_accessed);
    }
  }
  PrintEndSection();
}

/*
 * Show simple test output with max results for iterations.
 */
void ShowSetup(IOR_param_t *params)
{
  if (params->debug) {
      fprintf(out_logfile, "\n*** DEBUG MODE ***\n");
      fprintf(out_logfile, "*** %s ***\n\n", params->debug);
  }
  PrintNamedSectionStart("Options");
  PrintKeyVal("api", params->api);
  PrintKeyVal("apiVersion", params->apiVersion);
  PrintKeyVal("test filename", params->testFileName);
  PrintKeyVal("access", params->filePerProc ? "file-per-process" : "single-shared-file");
  PrintKeyVal("type", params->collective ? "collective" : "independent");
  PrintKeyValInt("segments", params->segmentCount);
  PrintKeyVal("ordering in a file", params->randomOffset ? "random" : "sequential");
  if (params->reorderTasks == FALSE && params->reorderTasksRandom == FALSE) {
    PrintKeyVal("ordering inter file", "no tasks offsets");
  }
  if (params->reorderTasks == TRUE) {
    PrintKeyVal("ordering inter file", "constant task offset");
    PrintKeyValInt("task offset", params->taskPerNodeOffset);
  }
  if (params->reorderTasksRandom == TRUE) {
    PrintKeyVal("ordering inter file", "random task offset");
    PrintKeyValInt("task offset", params->taskPerNodeOffset);
    PrintKeyValInt("reorder random seed", params->reorderTasksRandomSeed);
  }
  PrintKeyValInt("tasks", params->numTasks);
  PrintKeyValInt("clients per node", params->tasksPerNode);
  if (params->memoryPerTask != 0){
    PrintKeyVal("memoryPerTask", HumanReadable(params->memoryPerTask, BASE_TWO));
  }
  if (params->memoryPerNode != 0){
    PrintKeyVal("memoryPerNode", HumanReadable(params->memoryPerNode, BASE_TWO));
  }
  PrintKeyValInt("repetitions", params->repetitions);
  PrintKeyVal("xfersize", HumanReadable(params->transferSize, BASE_TWO));
  PrintKeyVal("blocksize", HumanReadable(params->blockSize, BASE_TWO));
  PrintKeyVal("aggregate filesize", HumanReadable(params->expectedAggFileSize, BASE_TWO));

#ifdef HAVE_LUSTRE_LUSTRE_USER_H
  if (params->lustre_set_striping) {
    PrintKeyVal("Lustre stripe size", ((params->lustre_stripe_size == 0) ? "Use default" :
     HumanReadable(params->lustre_stripe_size, BASE_TWO)));
    PrintKeyVal("stripe count", (params->lustre_stripe_count == 0 ? "Use default" : HumanReadable(params->lustre_stripe_count, BASE_TWO)));
  }
#endif /* HAVE_LUSTRE_LUSTRE_USER_H */
  if (params->deadlineForStonewalling > 0) {
    PrintKeyValInt("stonewallingTime", params->deadlineForStonewalling);
    PrintKeyValInt("stoneWallingWearOut", params->stoneWallingWearOut );
  }
  PrintEndSection();

  PrintNamedArrayStart("Results");

  fflush(out_resultfile);
}

static struct results *bw_ops_values(const int reps, IOR_results_t *measured,
                                     IOR_offset_t transfer_size,
                                     const double *vals, const int access)
{
        struct results *r;
        int i;

        r = (struct results *)malloc(sizeof(struct results)
                                     + (reps * sizeof(double)));
        if (r == NULL)
                ERR("malloc failed");
        r->val = (double *)&r[1];

        for (i = 0; i < reps; i++, measured++) {
                IOR_point_t *point = (access == WRITE) ? &measured->write :
                                                         &measured->read;

                r->val[i] = ((double) (point->aggFileSizeForBW))
                            / transfer_size / vals[i];

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

static struct results *bw_values(const int reps, IOR_results_t *measured,
                                 const double *vals, const int access)
{
        return bw_ops_values(reps, measured, 1, vals, access);
}

static struct results *ops_values(const int reps, IOR_results_t *measured,
                                  IOR_offset_t transfer_size,
                                  const double *vals, const int access)
{
        return bw_ops_values(reps, measured, transfer_size, vals, access);
}

/*
 * Summarize results
 */
static void PrintLongSummaryOneOperation(IOR_test_t *test, const int access)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;
        struct results *bw;
        struct results *ops;

        int reps;
        if (rank != 0 || verbose < VERBOSE_0)
                return;

        reps = params->repetitions;

        double * times = malloc(sizeof(double)* reps);
        for(int i=0; i < reps; i++){
                IOR_point_t *point = (access == WRITE) ? &results[i].write :
                                                         &results[i].read;
                times[i] = point->time;
        }

        bw = bw_values(reps, results, times, access);
        ops = ops_values(reps, results, params->transferSize, times, access);

        IOR_point_t *point = (access == WRITE) ? &results[0].write :
                                                 &results[0].read;


        if(outputFormat == OUTPUT_DEFAULT){
          fprintf(out_resultfile, "%-9s ", access == WRITE ? "write" : "read");
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
          fprintf(out_resultfile, "%9.1f ", (float)point->aggFileSizeForBW / MEBIBYTE);
          fprintf(out_resultfile, "%3s ", params->api);
          fprintf(out_resultfile, "%6d", params->referenceNumber);
          fprintf(out_resultfile, "\n");
        }else if (outputFormat == OUTPUT_JSON){
          PrintStartSection();
          PrintKeyVal("operation", access == WRITE ? "write" : "read");
          PrintKeyVal("API", params->api);
          PrintKeyValInt("TestID", params->id);
          PrintKeyValInt("ReferenceNumber", params->referenceNumber);
          PrintKeyValInt("segmentCount", params->segmentCount);
          PrintKeyValInt("blockSize", params->blockSize);
          PrintKeyValInt("transferSize", params->transferSize);
          PrintKeyValInt("numTasks", params->numTasks);
          PrintKeyValInt("tasksPerNode", params->tasksPerNode);
          PrintKeyValInt("repetitions", params->repetitions);
          PrintKeyValInt("filePerProc", params->filePerProc);
          PrintKeyValInt("reorderTasks", params->reorderTasks);
          PrintKeyValInt("taskPerNodeOffset", params->taskPerNodeOffset);
          PrintKeyValInt("reorderTasksRandom", params->reorderTasksRandom);
          PrintKeyValInt("reorderTasksRandomSeed", params->reorderTasksRandomSeed);
          PrintKeyValInt("segmentCount", params->segmentCount);
          PrintKeyValInt("blockSize", params->blockSize);
          PrintKeyValInt("transferSize", params->transferSize);
          PrintKeyValDouble("bwMaxMIB", bw->max / MEBIBYTE);
          PrintKeyValDouble("bwMinMIB", bw->min / MEBIBYTE);
          PrintKeyValDouble("bwMeanMIB", bw->mean / MEBIBYTE);
          PrintKeyValDouble("bwStdMIB", bw->sd / MEBIBYTE);
          PrintKeyValDouble("OPsMax", ops->max);
          PrintKeyValDouble("OPsMin", ops->min);
          PrintKeyValDouble("OPsMean", ops->mean);
          PrintKeyValDouble("OPsSD", ops->sd);
          PrintKeyValDouble("MeanTime", mean_of_array_of_doubles(times, reps));
          PrintKeyValDouble("xsizeMiB", (double) point->aggFileSizeForBW / MEBIBYTE);
          PrintEndSection();
        }else if (outputFormat == OUTPUT_CSV){

        }

        fflush(out_resultfile);

        free(bw);
        free(ops);
        free(times);
}

void PrintLongSummaryOneTest(IOR_test_t *test)
{
        IOR_param_t *params = &test->params;

        if (params->writeFile)
                PrintLongSummaryOneOperation(test, WRITE);
        if (params->readFile)
                PrintLongSummaryOneOperation(test, READ);
}

void PrintLongSummaryHeader()
{
        if (rank != 0 || verbose < VERBOSE_0)
                return;
        if(outputFormat != OUTPUT_DEFAULT){
          return;
        }

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

  PrintArrayEnd();

  if(outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n");
    fprintf(out_resultfile, "Summary of all tests:");
  }else if (outputFormat == OUTPUT_JSON){
    PrintNamedArrayStart("summary");
  }else if (outputFormat == OUTPUT_CSV){

  }

  PrintLongSummaryHeader();

  for (tptr = tests_head; tptr != NULL; tptr = tptr->next) {
          PrintLongSummaryOneTest(tptr);
  }

  PrintArrayEnd();
}

void PrintShortSummary(IOR_test_t * test)
{
        IOR_param_t *params = &test->params;
        IOR_results_t *results = test->results;
        double max_write_bw = 0.0;
        double max_read_bw = 0.0;
        double bw;
        int reps;
        int i;

        if (rank != 0 || verbose < VERBOSE_0)
                return;

        PrintArrayEnd();

        reps = params->repetitions;

        for (i = 0; i < reps; i++) {
                bw = (double)results[i].write.aggFileSizeForBW / results[i].write.time;
                max_write_bw = MAX(bw, max_write_bw);
                bw = (double)results[i].read.aggFileSizeForBW / results[i].read.time;
                max_read_bw = MAX(bw, max_read_bw);
        }

        if(outputFormat == OUTPUT_DEFAULT){
          if (params->writeFile) {
                  fprintf(out_resultfile, "Max Write: %.2f MiB/sec (%.2f MB/sec)\n",
                          max_write_bw/MEBIBYTE, max_write_bw/MEGABYTE);
          }
          if (params->readFile) {
                  fprintf(out_resultfile, "Max Read:  %.2f MiB/sec (%.2f MB/sec)\n",
                          max_read_bw/MEBIBYTE, max_read_bw/MEGABYTE);
          }
        }else if (outputFormat == OUTPUT_JSON){
          PrintNamedSectionStart("max");
          if (params->writeFile) {
            PrintKeyValDouble("writeMiB", max_write_bw/MEBIBYTE);
            PrintKeyValDouble("writeMB", max_write_bw/MEGABYTE);
          }
          if (params->readFile) {
            PrintKeyValDouble("readMiB", max_read_bw/MEBIBYTE);
            PrintKeyValDouble("readMB", max_read_bw/MEGABYTE);
          }
          PrintEndSection();
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

  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "remove    -          -          -          -          -          -          ");
    PPDouble(1, finish-start, " ");
    fprintf(out_resultfile, "%-4d\n", rep);
  }else if (outputFormat == OUTPUT_JSON){
    PrintStartSection();
    PrintKeyVal("access", "remove");
    PrintKeyValDouble("totalTime", finish - start);
    PrintEndSection();
  }
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

static double mean_of_array_of_doubles(double *values, int len)
{
        double tot = 0.0;
        int i;

        for (i = 0; i < len; i++) {
                tot += values[i];
        }
        return tot / len;

}
