#ifndef _MDTEST_H
#define _MDTEST_H

#include <mpi.h>
#include <stdio.h>
#include <stdint.h>

#include "utilities.h"

/*
 * Struct contains all runtime options and used previously global variables
 */
typedef struct{
  int size;
  uint64_t *rand_array;
  char testdir[MAX_PATHLEN];
  char testdirpath[MAX_PATHLEN];
  char base_tree_name[MAX_PATHLEN];
  char **filenames;
  char hostname[MAX_PATHLEN];
  char mk_name[MAX_PATHLEN];
  char stat_name[MAX_PATHLEN];
  char read_name[MAX_PATHLEN];
  char rm_name[MAX_PATHLEN];
  char unique_mk_dir[MAX_PATHLEN];
  char unique_chdir_dir[MAX_PATHLEN];
  char unique_stat_dir[MAX_PATHLEN];
  char unique_read_dir[MAX_PATHLEN];
  char unique_rm_dir[MAX_PATHLEN];
  char unique_rm_uni_dir[MAX_PATHLEN];
  char *write_buffer;
  char *read_buffer;
  char *stoneWallingStatusFile;

  int barriers;
  int create_only;
  int stat_only;
  int read_only;
  int remove_only;
  int leaf_only;
  unsigned branch_factor;
  int depth;

  /*
   * This is likely a small value, but it's sometimes computed by
   * branch_factor^(depth+1), so we'll make it a larger variable,
   * just in case.
   */
  uint64_t num_dirs_in_tree;
  /*
   * As we start moving towards Exascale, we could have billions
   * of files in a directory. Make room for that possibility with
   * a larger variable.
   */
  uint64_t items;
  uint64_t items_per_dir;
  uint64_t num_dirs_in_tree_calc; /* this is a workaround until the overal code is refactored */
  int directory_loops;
  int print_time;
  int print_rate_and_time;
  int random_seed;
  int shared_file;
  int files_only;
  int dirs_only;
  int pre_delay;
  int unique_dir_per_task;
  int time_unique_dir_overhead;
  int collective_creates;
  size_t write_bytes;
  int stone_wall_timer_seconds;
  size_t read_bytes;
  int sync_file;
  int path_count;
  int nstride; /* neighbor stride */

  pid_t pid;
  uid_t uid;

  //char  **    file_name;
  //uint64_t    file_count;

  //char  **    dir_name;
  //uint64_t    dir_count;
} mdtest_runtime_t;

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

void mdtest_generate_filenames();
mdtest_results_t * mdtest_run(int argc, char **argv, MPI_Comm world_com, FILE * out_logfile);

#endif
