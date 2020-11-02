#ifndef IOR_MD_WORKBENCH_H
#define IOR_MD_WORKBENCH_H

#include <stdint.h>
#include <stdio.h>
#include <mpi.h>

// successfull, errors
typedef struct {
  int suc;
  int err;
} op_stat_t;

// A runtime for an operation and when the operation was started
typedef struct{
  float time_since_app_start;
  float runtime;
} time_result_t;

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
  double t; // maximum time
  double * t_all;

  op_stat_t dset_create;
  op_stat_t dset_delete;

  op_stat_t obj_create;
  op_stat_t obj_read;
  op_stat_t obj_stat;
  op_stat_t obj_delete;

  // time measurements of individual runs, these are not returned for now by the API!
  uint64_t repeats;
  time_result_t * time_create;
  time_result_t * time_read;
  time_result_t * time_stat;
  time_result_t * time_delete;

  time_statistics_t stats_create;
  time_statistics_t stats_read;
  time_statistics_t stats_stat;
  time_statistics_t stats_delete;

  // the maximum time for any single operation
  double max_op_time;
  double phase_start_timer;
  int stonewall_iterations;
} phase_stat_t;

// @Return The first statistics returned are precreate, then iteration many benchmark runs, the last is cleanup
phase_stat_t* md_workbench_run(int argc, char ** argv, MPI_Comm world_com, FILE * out_logfile);

#endif
