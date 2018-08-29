#ifndef _MDTEST_H
#define _MDTEST_H

#include <mpi.h>
#include <stdio.h>
#include <stdint.h>

typedef enum {
  MDTEST_DIR_CREATE_NUM = 0,
  MDTEST_DIR_STAT_NUM = 1,
  MDTEST_DIR_READ_NUM = 1,
  MDTEST_DIR_REMOVE_NUM = 3,
  MDTEST_FILE_CREATE_NUM = 4,
  MDTEST_FILE_STAT_NUM = 5,
  MDTEST_FILE_READ_NUM = 6,
  MDTEST_FILE_REMOVE_NUM = 7,
  MDTEST_TREE_CREATE_NUM = 8,
  MDTEST_TREE_REMOVE_NUM = 9,
  MDTEST_LAST_NUM
} mdtest_test_num_t;

typedef struct
{
    double rate[MDTEST_LAST_NUM]; /* Calculated throughput */
    double time[MDTEST_LAST_NUM]; /* Time */
    uint64_t items[MDTEST_LAST_NUM]; /* Number of operations done */

    /* Statistics when hitting the stonewall */
    double   stonewall_time[MDTEST_LAST_NUM];     /* runtime until completion / hit of the stonewall */
    uint64_t stonewall_last_item[MDTEST_LAST_NUM]; /* Max number of items a process has accessed */
    uint64_t stonewall_item_min[MDTEST_LAST_NUM];  /* Min number of items a process has accessed */
    uint64_t stonewall_item_sum[MDTEST_LAST_NUM];  /* Total number of items accessed until stonewall */
} mdtest_results_t;

mdtest_results_t * mdtest_run(int argc, char **argv, MPI_Comm world_com, FILE * out_logfile);

#endif
