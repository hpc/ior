#ifndef IOR_MD_WORKBENCH_H
#define IOR_MD_WORKBENCH_H

#include <stdint.h>
#include <stdio.h>
#include <mpi.h>

typedef struct{
  float min;
  float q1;
  float median;
  float q3;
  float q90;
  float q99;
  float max;
} time_statistics_t;


// statistics for running a single phase
typedef struct{ // NOTE: if this type is changed, adjust end_phase() !!!
  time_statistics_t stats_create;
  time_statistics_t stats_read;
  time_statistics_t stats_stat;
  time_statistics_t stats_delete;

  int errors;
  double rate;
  double max_op_time;
  double runtime;
  uint64_t iterations_done;
} mdworkbench_result_t;

typedef struct{
  int count; // the number of results
  int errors;
  mdworkbench_result_t result[];
} mdworkbench_results_t;

// @Return The first statistics returned are precreate, then iteration many benchmark runs, the last is cleanup
mdworkbench_results_t* md_workbench_run(int argc, char ** argv, MPI_Comm world_com, FILE * out_logfile);

#endif
