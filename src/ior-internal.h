/*
 * This file contains header information for support code that is only used within IOR.
 * For code shared across benchmarks, see utilities.h
 */
#ifndef _IOR_INTERNAL_H
#define _IOR_INTERNAL_H

/* Part of ior-output.c */
void PrintEarlyHeader();
void PrintHeader(int argc, char **argv);
void ShowTestInfo(IOR_param_t *params);
void ShowSetup(IOR_param_t *params);
void ShowTest(IOR_param_t * test);
void PrintShortSummary(IOR_test_t * test);
void PrintLongSummaryAllTests(IOR_test_t *tests_head);
void PrintLongSummaryHeader();
void PrintLongSummaryOneTest(IOR_test_t *test);
void DisplayFreespace(IOR_param_t * test);
void GetTestFileName(char *, IOR_param_t *);
void PrintRemoveTiming(double start, double finish, int rep);
void PrintReducedResult(IOR_test_t *test, int access, double bw, double *diff_subset, double totalTime, int rep);
/* End of ior-output */

struct results {
  double min;
  double max;
  double mean;
  double var;
  double sd;
  double sum;
  double *val;
};


#endif
