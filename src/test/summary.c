#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ior.h"
#include "../ior-internal.h"
#include "../utilities.h"

static void set_sample(IOR_point_t *point, int rate_mib, int iteration)
{
  point->completed = TRUE;
  point->time = (double)iteration + 1.0;
  point->accessTime = point->time / 2.0;
  point->aggFileSizeForBW = (IOR_offset_t)rate_mib *
                            (IOR_offset_t)(point->time * MEBIBYTE);
}

static int configure_fixture(const char *name, IOR_test_t *test,
                             IOR_results_t *results)
{
  int write_mask = 0;
  int read_mask = 0;
  int repetitions = 4;
  IOR_offset_t transfer_size = MEBIBYTE;

  if (strcmp(name, "full") == 0) {
    write_mask = read_mask = 0xF;
  } else if (strcmp(name, "partial") == 0) {
    write_mask = 0xD;
    read_mask = 0x1;
  } else if (strcmp(name, "write-only") == 0) {
    write_mask = 0x5;
  } else if (strcmp(name, "read-only") == 0) {
    read_mask = 0xA;
  } else if (strcmp(name, "rates") == 0) {
    write_mask = read_mask = 0xF;
    transfer_size = MEBIBYTE / 2;
  } else if (strcmp(name, "single") == 0) {
    repetitions = 1;
    write_mask = read_mask = 0x1;
  } else if (strcmp(name, "zero") != 0) {
    return 0;
  }

  memset(test, 0, sizeof(*test));
  memset(results, 0, sizeof(IOR_results_t) * 4);
  test->params.id = 7;
  test->params.api = "POSIX";
  test->params.numTasks = 4;
  test->params.numTasksOnNode0 = 4;
  test->params.repetitions = repetitions;
  test->params.writeFile = strcmp(name, "read-only") != 0;
  test->params.readFile = strcmp(name, "write-only") != 0;
  test->params.filePerProc = TRUE;
  test->params.segmentCount = 1;
  test->params.blockSize = MEBIBYTE;
  test->params.transferSize = transfer_size;
  test->params.referenceNumber = 0;
  test->results = results;

  for (int i = 0; i < repetitions; i++) {
    /* Deliberately poison skipped slots; summary code must ignore them. */
    results[i].write.time = 1.0;
    results[i].write.accessTime = 0.5;
    results[i].write.aggFileSizeForBW = 100 * MEBIBYTE;
    results[i].read.time = 1.0;
    results[i].read.accessTime = 0.5;
    results[i].read.aggFileSizeForBW = 100 * MEBIBYTE;
    if (write_mask & (1 << i))
      set_sample(&results[i].write, i + 1, i);
    if (read_mask & (1 << i))
      set_sample(&results[i].read, 2 * (i + 1), i);
  }

  return 1;
}

int main(int argc, char **argv)
{
  IOR_test_t test;
  IOR_results_t results[4];

  if (argc != 3 ||
      (strcmp(argv[2], "text") != 0 && strcmp(argv[2], "json") != 0) ||
      !configure_fixture(argv[1], &test, results)) {
    fprintf(stderr, "usage: testsummary FIXTURE text|json\n");
    return 2;
  }

  rank = 0;
  verbose = VERBOSE_1;
  outputFormat = strcmp(argv[2], "json") == 0 ? OUTPUT_JSON : OUTPUT_DEFAULT;
  out_resultfile = stdout;

  if (outputFormat == OUTPUT_JSON)
    fprintf(out_resultfile, "{\n\"long\": [");
  else
    PrintLongSummaryHeader();

  PrintLongSummaryOneTest(&test);
  if (outputFormat == OUTPUT_JSON)
    fprintf(out_resultfile, "]");
  PrintShortSummary(&test);
  if (outputFormat == OUTPUT_JSON)
    fprintf(out_resultfile, "\n}\n");

  return 0;
}
