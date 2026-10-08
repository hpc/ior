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
    fprintf(out_resultfile, "access    bw(MiB/s)  IOPS       Latency(s)  block(KiB) xfer(KiB)  open(s)    wr/rd(s)   close(s)   total(s)   iter\n");
    fprintf(out_resultfile, "------    ---------  ----       ----------  ---------- ---------  --------   --------   --------   --------   ----\n");
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "access,bw(MiB/s),IOPS,Latency,block(KiB),xfer(KiB),open(s),wr/rd(s),close(s),total(s),numTasks,iter\n");
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

void PrintKeyVal(char * key, char * value){
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
    fprintf(out_resultfile, "%s,", value);
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
    if(isinf(value)){
      fprintf(out_resultfile, "\"%s\": \"%.4f\"", key, value);
    }else{
      fprintf(out_resultfile, "\"%s\": %.4f", key, value);
    }
  }else if(outputFormat == OUTPUT_CSV){
    fprintf(out_resultfile, "%.4f,", value);
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
    fprintf(out_resultfile, "%lld,", (long long) value);
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
  if(outputFormat == OUTPUT_DEFAULT){
    return;
  }
  PrintArrayStart();
}

void PrintTestEnds(){
  if (outputFormat == OUTPUT_CSV){
      return;
  }
  if (rank != 0 ||  verbose <= VERBOSE_0) {
    PrintEndSection();
    return;
  }

  PrintKeyVal("Finished", CurrentTimeString());
  PrintEndSection();
}

void PrintReducedResult(IOR_test_t *test, int access, double bw, double iops, double latency,
			double *diff_subset, double totalTime, int rep){
  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "%-10s", access == WRITE ? "write" : "read");
    PPDouble(1, bw / MEBIBYTE, " ");
    PPDouble(1, iops, " ");
    PPDouble(1, latency, "  ");
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
    PrintKeyValDouble("iops", iops);
    PrintKeyValDouble("latency", latency);
    PrintKeyValDouble("openTime", diff_subset[0]);
    PrintKeyValDouble("wrRdTime", diff_subset[1]);
    PrintKeyValDouble("closeTime", diff_subset[2]);
    PrintKeyValDouble("totalTime", totalTime);
    PrintEndSection();
  }else if (outputFormat == OUTPUT_CSV){
    PrintKeyVal("access", access == WRITE ? "write" : "read");
    PrintKeyValDouble("bwMiB", bw / MEBIBYTE);
    PrintKeyValDouble("iops", iops);
    PrintKeyValDouble("latency", latency);
    PrintKeyValDouble("blockKiB", (double)test->params.blockSize / KIBIBYTE);
    PrintKeyValDouble("xferKiB", (double)test->params.transferSize / KIBIBYTE);
    PrintKeyValDouble("openTime", diff_subset[0]);
    PrintKeyValDouble("wrRdTime", diff_subset[1]);
    PrintKeyValDouble("closeTime", diff_subset[2]);
    PrintKeyValDouble("totalTime", totalTime);
    PrintKeyValInt("Numtasks", test->params.numTasks);
    fprintf(out_resultfile, "%d\n", rep);
  }

  fflush(out_resultfile);
}

void PrintHeader(int argc, char **argv)
{
        struct utsname unamebuf;
        int i;

        if (rank != 0)
                return;

        if (outputFormat == OUTPUT_CSV){
            return;
        }

        PrintStartSection();
        if (outputFormat != OUTPUT_DEFAULT){
          PrintKeyVal("Version", META_VERSION);
        }else{
          fprintf(out_resultfile, "IOR-" META_VERSION ": MPI Coordinated Test of Parallel I/O\n");
        }
        PrintKeyVal("Began", CurrentTimeString());
        PrintKeyValStart("Command line");
        fprintf(out_resultfile, "%s", argv[0]);
        for (i = 1; i < argc; i++) {
                fprintf(out_resultfile, " %s", argv[i]);
        }
        PrintKeyValEnd();
        if (uname(&unamebuf) != 0) {
                WARN("uname failed");
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
  if (outputFormat == OUTPUT_CSV){
      return;
  }
  PrintStartSection();
  PrintKeyValInt("TestID", test->id);
  PrintKeyVal("StartTime", CurrentTimeString());

  char filename[MAX_PATHLEN];
  GetTestFileName(filename, test);
  ShowFileSystemSize(filename, test->backend, test->backend_options);

  if (verbose >= VERBOSE_3 || outputFormat == OUTPUT_JSON) {
    char* data_packets[] = {"g","t","o","i"};

    PrintNamedSectionStart("Parameters");
    PrintKeyValInt("testID", test->id);
    PrintKeyValInt("refnum", test->referenceNumber);
    PrintKeyVal("api", test->api);
    PrintKeyVal("platform", test->platform);
    PrintKeyVal("testFileName", test->testFileName);
    PrintKeyValInt("deadlineForStonewall", test->deadlineForStonewalling);
    PrintKeyValInt("stoneWallingWearOut", test->stoneWallingWearOut);
    PrintKeyValInt("maxTimeDuration", test->maxTimeDuration);
    PrintKeyValInt("outlierThreshold", test->outlierThreshold);

    PrintKeyVal("options", test->options);
    PrintKeyValInt("dryRun", test->dryRun);
    PrintKeyValInt("nodes", test->numNodes);
    PrintKeyValInt("memoryPerTask", (unsigned long) test->memoryPerTask);
    PrintKeyValInt("memoryPerNode", (unsigned long) test->memoryPerNode);
    PrintKeyValInt("tasksPerNode", test->numTasksOnNode0);
    PrintKeyValInt("repetitions", test->repetitions);
    PrintKeyValInt("multiFile", test->multiFile);
    PrintKeyValInt("interTestDelay", test->interTestDelay);
    PrintKeyValInt("fsync", test->fsync);
    PrintKeyValInt("fsyncperwrite", test->fsyncPerWrite);
    PrintKeyValInt("useExistingTestFile", test->useExistingTestFile);
    PrintKeyValInt("uniqueDir", test->uniqueDir);
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
    PrintKeyValInt("dataPacketType", test->dataPacketType);
    PrintKeyValInt("keepFile", test->keepFile);
    PrintKeyValInt("keepFileWithError", test->keepFileWithError);
    PrintKeyValInt("warningAsErrors", test->warningAsErrors);
    PrintKeyValInt("verbose", verbose);
    PrintKeyVal("data packet type", data_packets[test->dataPacketType]);
    PrintKeyValInt("setTimeStampSignature/incompressibleSeed", test->setTimeStampSignature); /* Seed value was copied into setTimeStampSignature as well */
    PrintKeyValInt("collective", test->collective);
    PrintKeyValInt("segmentCount", test->segmentCount);
    //#ifdef HAVE_GPFS_FCNTL_H
    //PrintKeyValInt("gpfsHintAccess", test->gpfs_hint_access);
    //PrintKeyValInt("gpfsReleaseToken", test->gpfs_release_token);
    //#endif
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
  if (outputFormat == OUTPUT_CSV){
      return;
  }
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
  PrintKeyValInt("nodes", params->numNodes);
  PrintKeyValInt("tasks", params->numTasks);
  PrintKeyValInt("clients per node", params->numTasksOnNode0);
  if (params->memoryPerTask != 0){
    PrintKeyVal("memoryPerTask", HumanReadable(params->memoryPerTask, BASE_TWO));
  }
  if (params->memoryPerNode != 0){
    PrintKeyVal("memoryPerNode", HumanReadable(params->memoryPerNode, BASE_TWO));
  }
  
  PrintKeyVal("memoryBuffer", params->gpuMemoryFlags == IOR_MEMORY_TYPE_CPU ? "CPU" :  params->gpuMemoryFlags == IOR_MEMORY_TYPE_GPU_DEVICE_ONLY ? "GPU" : "Managed");
  PrintKeyVal("dataAccess", params->gpuMemoryFlags > IOR_MEMORY_TYPE_GPU_MANAGED_CHECK_GPU ? "GPU" : "CPU");
  PrintKeyVal("GPUDirect", params->gpuDirect ? "1" : "0");

  PrintKeyValInt("repetitions", params->repetitions);
  PrintKeyVal("xfersize", HumanReadable(params->transferSize, BASE_TWO));
  PrintKeyVal("blocksize", HumanReadable(params->blockSize, BASE_TWO));
  PrintKeyVal("aggregate filesize", HumanReadable(params->expectedAggFileSize, BASE_TWO));
  if(params->dryRun){
    PrintKeyValInt("dryRun", params->dryRun);
  }
  if(params->verbose) {
    PrintKeyValInt("verbose", params->verbose);
  }

  if (params->deadlineForStonewalling > 0) {
    PrintKeyValInt("stonewallingTime", params->deadlineForStonewalling);
    PrintKeyValInt("stoneWallingWearOut", params->stoneWallingWearOut );
  }
  PrintEndSection();

  PrintNamedArrayStart("Results");

  fflush(out_resultfile);
}

static struct results *bw_ops_values(const int repetitions,
                                     IOR_results_t *measured,
                                     const IOR_offset_t transfer_size,
                                     const int access)
{
        struct results *r;
        int i;

        r = (struct results *)malloc(sizeof(struct results)
                                     + (repetitions * sizeof(double)));
        if (r == NULL)
                ERR("malloc failed");
        r->val = (double *)&r[1];
        r->count = 0;
        r->min = r->max = r->mean = r->var = r->sd = r->sum = 0.0;

        for (i = 0; i < repetitions; i++, measured++) {
                IOR_point_t *point = (access == WRITE) ? &measured->write :
                                                         &measured->read;
                double value;

                if (!point->completed || point->time <= 0.0 ||
                    !isfinite(point->time) || point->aggFileSizeForBW <= 0 ||
                    transfer_size <= 0)
                        continue;

                value = ((double)point->aggFileSizeForBW) /
                        (double)transfer_size / point->time;
                if (!isfinite(value))
                        continue;

                r->val[r->count] = value;
                if (r->count == 0) {
                        r->min = value;
                        r->max = value;
                } else {
                        r->min = MIN(r->min, value);
                        r->max = MAX(r->max, value);
                }
                r->sum += value;
                r->count++;
        }

        if (r->count > 0) {
                r->mean = r->sum / r->count;
                for (i = 0; i < r->count; i++)
                        r->var += pow((r->mean - r->val[i]), 2);
                r->var /= r->count;
                r->sd = sqrt(r->var);
        }

        return r;
}

static struct results *bw_values(const int repetitions,
                                 IOR_results_t *measured, const int access)
{
        return bw_ops_values(repetitions, measured, 1, access);
}

static struct results *ops_values(const int repetitions,
                                  IOR_results_t *measured,
                                  IOR_offset_t transfer_size,
                                  const int access)
{
        return bw_ops_values(repetitions, measured, transfer_size, access);
}

static int summary_point_is_valid(const IOR_point_t *point)
{
        return point->completed && point->time > 0.0 &&
               isfinite(point->time) && point->aggFileSizeForBW > 0;
}

static void PrintKeyValNull(char *key)
{
        PrintNextToken();
        needNextToken = 1;
        if (outputFormat == OUTPUT_JSON)
                fprintf(out_resultfile, "\"%s\": null", key);
        else if (outputFormat == OUTPUT_DEFAULT)
                fprintf(out_resultfile, "%-20s: NA\n", key);
}

static void PrintSummaryKeyValDouble(char *key, double value, int valid)
{
        if (valid && isfinite(value))
                PrintKeyValDouble(key, value);
        else
                PrintKeyValNull(key);
}

static void PrintDefaultSummaryDouble(double value, int valid,
                                      int width, int precision)
{
        if (!valid || !isfinite(value))
                fprintf(out_resultfile, "%*s ", width, "NA");
        else
                fprintf(out_resultfile, "%*.*f ", width, precision, value);
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
        IOR_point_t *first_point = NULL;
        int completed_repetitions = 0;
        double mean_time;
        long long stonewall_avg_data_accessed = 0;
        double stonewall_time = 0.0;
        double stonewall_mean_time = 0.0;
        double stonewall_bandwidth = 0.0;
        int stonewall_valid;
        double *times;

        if (rank != 0 || verbose <= VERBOSE_0)
                return;

        times = malloc(sizeof(double) * params->repetitions);
        if (times == NULL)
                ERR("malloc failed");

        for (int i = 0; i < params->repetitions; i++) {
                IOR_point_t *point = (access == WRITE) ? &results[i].write :
                                                         &results[i].read;
                if (!summary_point_is_valid(point))
                        continue;
                if (first_point == NULL)
                        first_point = point;
                times[completed_repetitions++] = point->time;
                stonewall_time += point->stonewall_time;
                stonewall_avg_data_accessed += point->stonewall_avg_data_accessed;
        }

        bw = bw_values(params->repetitions, results, access);
        ops = ops_values(params->repetitions, results, params->transferSize,
                         access);
        mean_time = completed_repetitions > 0 ?
                    mean_of_array_of_doubles(times, completed_repetitions) : 0.0;
        stonewall_valid = completed_repetitions > 0 &&
                          isfinite(stonewall_time) && stonewall_time > 0.0;
        if (stonewall_valid) {
                stonewall_mean_time = stonewall_time / completed_repetitions;
                stonewall_bandwidth = (double)stonewall_avg_data_accessed /
                                      stonewall_time / MEBIBYTE;
        }

        if (outputFormat == OUTPUT_DEFAULT) {
                fprintf(out_resultfile, "%-9s ", access == WRITE ? "write" : "read");
                PrintDefaultSummaryDouble(bw->max / MEBIBYTE, bw->count > 0, 10, 2);
                PrintDefaultSummaryDouble(bw->min / MEBIBYTE, bw->count > 0, 10, 2);
                PrintDefaultSummaryDouble(bw->mean / MEBIBYTE, bw->count > 0, 10, 2);
                PrintDefaultSummaryDouble(bw->sd / MEBIBYTE, bw->count > 0, 10, 2);
                PrintDefaultSummaryDouble(ops->max, ops->count > 0, 10, 2);
                PrintDefaultSummaryDouble(ops->min, ops->count > 0, 10, 2);
                PrintDefaultSummaryDouble(ops->mean, ops->count > 0, 10, 2);
                PrintDefaultSummaryDouble(ops->sd, ops->count > 0, 10, 2);
                PrintDefaultSummaryDouble(mean_time, completed_repetitions > 0, 10, 5);
                if (params->stoneWallingWearOut) {
                        PrintDefaultSummaryDouble(stonewall_mean_time,
                                                  stonewall_valid, 10, 2);
                        PrintDefaultSummaryDouble(stonewall_bandwidth,
                                                  stonewall_valid, 13, 2);
                } else {
                        fprintf(out_resultfile, "%10s %13s ", "NA", "NA");
                }
                fprintf(out_resultfile, "%5d ", params->id);
                fprintf(out_resultfile, "%6d ", params->numTasks);
                fprintf(out_resultfile, "%3d ", params->numTasksOnNode0);
                fprintf(out_resultfile, "%4d ", params->repetitions);
                fprintf(out_resultfile, "%9d ", completed_repetitions);
                fprintf(out_resultfile, "%3d ", params->filePerProc);
                fprintf(out_resultfile, "%5d ", params->reorderTasks);
                fprintf(out_resultfile, "%8d ", params->taskPerNodeOffset);
                fprintf(out_resultfile, "%9d ", params->reorderTasksRandom);
                fprintf(out_resultfile, "%4d ", params->reorderTasksRandomSeed);
                fprintf(out_resultfile, "%6lld ", params->segmentCount);
                fprintf(out_resultfile, "%8lld ", params->blockSize);
                fprintf(out_resultfile, "%8lld ", params->transferSize);
                PrintDefaultSummaryDouble(first_point != NULL ?
                                          (double)first_point->aggFileSizeForBW / MEBIBYTE : 0.0,
                                          first_point != NULL, 9, 1);
                fprintf(out_resultfile, "%3s ", params->api);
                fprintf(out_resultfile, "%6d\n", params->referenceNumber);
        } else if (outputFormat == OUTPUT_JSON) {
                PrintStartSection();
                PrintKeyVal("operation", access == WRITE ? "write" : "read");
                PrintKeyVal("API", params->api);
                PrintKeyValInt("TestID", params->id);
                PrintKeyValInt("ReferenceNumber", params->referenceNumber);
                PrintKeyValInt("segmentCount", params->segmentCount);
                PrintKeyValInt("blockSize", params->blockSize);
                PrintKeyValInt("transferSize", params->transferSize);
                PrintKeyValInt("numTasks", params->numTasks);
                PrintKeyValInt("tasksPerNode", params->numTasksOnNode0);
                PrintKeyValInt("repetitions", params->repetitions);
                PrintKeyValInt("completedRepetitions", completed_repetitions);
                PrintKeyValInt("filePerProc", params->filePerProc);
                PrintKeyValInt("reorderTasks", params->reorderTasks);
                PrintKeyValInt("taskPerNodeOffset", params->taskPerNodeOffset);
                PrintKeyValInt("reorderTasksRandom", params->reorderTasksRandom);
                PrintKeyValInt("reorderTasksRandomSeed", params->reorderTasksRandomSeed);
                PrintSummaryKeyValDouble("bwMaxMIB", bw->max / MEBIBYTE, bw->count > 0);
                PrintSummaryKeyValDouble("bwMinMIB", bw->min / MEBIBYTE, bw->count > 0);
                PrintSummaryKeyValDouble("bwMeanMIB", bw->mean / MEBIBYTE, bw->count > 0);
                PrintSummaryKeyValDouble("bwStdMIB", bw->sd / MEBIBYTE, bw->count > 0);
                PrintSummaryKeyValDouble("OPsMax", ops->max, ops->count > 0);
                PrintSummaryKeyValDouble("OPsMin", ops->min, ops->count > 0);
                PrintSummaryKeyValDouble("OPsMean", ops->mean, ops->count > 0);
                PrintSummaryKeyValDouble("OPsSD", ops->sd, ops->count > 0);
                PrintSummaryKeyValDouble("MeanTime", mean_time,
                                         completed_repetitions > 0);
                if (params->stoneWallingWearOut) {
                        PrintSummaryKeyValDouble("StoneWallTime",
                                                 stonewall_mean_time,
                                                 stonewall_valid);
                        PrintSummaryKeyValDouble("StoneWallbwMeanMIB",
                                                 stonewall_bandwidth,
                                                 stonewall_valid);
                }
                PrintSummaryKeyValDouble("xsizeMiB",
                                         first_point != NULL ?
                                         (double)first_point->aggFileSizeForBW / MEBIBYTE : 0.0,
                                         first_point != NULL);
                PrintEndSection();
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
        if (params->readFile || params->checkRead)
                PrintLongSummaryOneOperation(test, READ);
}

void PrintLongSummaryHeader()
{
        if (rank != 0 || verbose <= VERBOSE_0)
                return;
        if(outputFormat != OUTPUT_DEFAULT){
            return;
        }

        fprintf(out_resultfile, "\n");
        fprintf(out_resultfile, "%-9s %10s %10s %10s %10s %10s %10s %10s %10s %10s %10s %13s",
                "Operation", "Max(MiB)", "Min(MiB)", "Mean(MiB)", "StdDev",
                "Max(OPs)", "Min(OPs)", "Mean(OPs)", "StdDev",
                "Mean(s)", "Stonewall(s)", "Stonewall(MiB)");
        fprintf(out_resultfile, " Test# #Tasks tPN reps completed fPP reord reordoff reordrand seed"
                " segcnt ");
        fprintf(out_resultfile, "%8s %8s %9s %5s", " blksiz", "xsize","aggs(MiB)", "API");
        fprintf(out_resultfile, " RefNum\n");
}

void PrintLongSummaryAllTests(IOR_test_t *tests_head)
{
  IOR_test_t *tptr;
  if (rank != 0 || verbose <= VERBOSE_0)
          return;

  PrintArrayEnd();

  if(outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "\n");
    fprintf(out_resultfile, "Summary of all tests:");
  }else if (outputFormat == OUTPUT_JSON){
    PrintNamedArrayStart("summary");
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
        struct results *write_bw = NULL;
        struct results *read_bw = NULL;

        if (rank != 0 || verbose <= VERBOSE_0)
                return;

        if (params->writeFile)
                write_bw = bw_values(params->repetitions, results, WRITE);
        if (params->readFile || params->checkRead)
                read_bw = bw_values(params->repetitions, results, READ);

        if (outputFormat == OUTPUT_DEFAULT) {
          if (params->writeFile) {
                  if (write_bw->count > 0) {
                          fprintf(out_resultfile,
                                  "Max Write: %.2f MiB/sec (%.2f MB/sec), completed: %d/%d\n",
                                  write_bw->max / MEBIBYTE,
                                  write_bw->max / MEGABYTE,
                                  write_bw->count, params->repetitions);
                  } else {
                          fprintf(out_resultfile,
                                  "Max Write: NA MiB/sec (NA MB/sec), completed: 0/%d\n",
                                  params->repetitions);
                  }
          }
          if (params->readFile || params->checkRead) {
                  if (read_bw->count > 0) {
                          fprintf(out_resultfile,
                                  "Max Read:  %.2f MiB/sec (%.2f MB/sec), completed: %d/%d\n",
                                  read_bw->max / MEBIBYTE,
                                  read_bw->max / MEGABYTE,
                                  read_bw->count, params->repetitions);
                  } else {
                          fprintf(out_resultfile,
                                  "Max Read:  NA MiB/sec (NA MB/sec), completed: 0/%d\n",
                                  params->repetitions);
                  }
          }
        } else if (outputFormat == OUTPUT_JSON) {
          PrintNamedSectionStart("max");
          if (params->writeFile) {
            PrintSummaryKeyValDouble("writeMiB",
                                     write_bw->count > 0 ? write_bw->max / MEBIBYTE : 0.0,
                                     write_bw->count > 0);
            PrintSummaryKeyValDouble("writeMB",
                                     write_bw->count > 0 ? write_bw->max / MEGABYTE : 0.0,
                                     write_bw->count > 0);
            PrintKeyValInt("writeCompletedRepetitions", write_bw->count);
            PrintKeyValInt("writeRepetitions", params->repetitions);
          }
          if (params->readFile || params->checkRead) {
            PrintSummaryKeyValDouble("readMiB",
                                     read_bw->count > 0 ? read_bw->max / MEBIBYTE : 0.0,
                                     read_bw->count > 0);
            PrintSummaryKeyValDouble("readMB",
                                     read_bw->count > 0 ? read_bw->max / MEGABYTE : 0.0,
                                     read_bw->count > 0);
            PrintKeyValInt("readCompletedRepetitions", read_bw->count);
            PrintKeyValInt("readRepetitions", params->repetitions);
          }
          PrintEndSection();
        }

        free(write_bw);
        free(read_bw);
}

void PrintRemoveTiming(double start, double finish, int rep)
{
  if (rank != 0 || verbose <= VERBOSE_0)
    return;

  if (outputFormat == OUTPUT_DEFAULT){
    fprintf(out_resultfile, "remove    -          -          -           -          -          -          -          -          ");
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
