#ifndef _MDTEST_H
#define _MDTEST_H

#include <mpi.h>
#include <stdio.h>
#include <stdint.h>
#include <utilities.h>

typedef enum {
  MDTEST_DIR_CREATE_NUM = 0,
  MDTEST_DIR_STAT_NUM = 1,
  MDTEST_DIR_READ_NUM = 2,
  MDTEST_DIR_RENAME_NUM = 3,
  MDTEST_DIR_REMOVE_NUM = 4,
  MDTEST_FILE_CREATE_NUM = 5,
  MDTEST_FILE_STAT_NUM = 6,
  MDTEST_FILE_READ_NUM = 7,
  MDTEST_FILE_REMOVE_NUM = 8,
  MDTEST_TREE_CREATE_NUM = 9,
  MDTEST_TREE_REMOVE_NUM = 10,
  MDTEST_LAST_NUM
} mdtest_test_num_t;

typedef struct
{
    double rate[MDTEST_LAST_NUM]; /* Calculated throughput after the barrier */
    double rate_before_barrier[MDTEST_LAST_NUM]; /* Calculated throughput before the barrier */
    double time[MDTEST_LAST_NUM]; /* Time */
    double time_before_barrier[MDTEST_LAST_NUM]; /* individual time before executing the barrier */
    uint64_t items[MDTEST_LAST_NUM]; /* Number of operations done in this process*/
    uint64_t total_errors;

    /* Statistics when hitting the stonewall */
    double   stonewall_time[MDTEST_LAST_NUM];     /* Max runtime of any process until completion / hit of the stonewall */
    uint64_t stonewall_last_item[MDTEST_LAST_NUM]; /* The number of items a process has accessed */
    uint64_t stonewall_item_min[MDTEST_LAST_NUM];  /* Min number of items any process has accessed */
    uint64_t stonewall_item_sum[MDTEST_LAST_NUM];  /* Total number of items accessed by all processes until stonewall */
} mdtest_results_t;

mdtest_results_t * mdtest_run(int argc, char **argv, MPI_Comm world_com, FILE * out_logfile);

#endif
