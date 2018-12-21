#include <stdbool.h>
#include <assert.h>

#include <mdtest.h>

/*
 * This file contains the logic to setup the list of accessed files
 * Randomization is made externally
 */

// position of the directory index
static uint64_t posD;
// position of the file index
static uint64_t posF;
static uint64_t posT;

extern mdtest_runtime_t run;

static void add_file(char name[]){
  assert(posF < run.file_count);
  //printf("%d file: %s\n", rank, name);
  run.file_name[posF] = strdup(name);
  posF++;
}

static void add_treedir(char name[]){
  printf("%d tree: %s\n", rank, name);
  assert(posT < run.treedir_count);
  run.treedir_name[posT] = strdup(name);
  posT++;
}

static void add_dir(char name[]){
  printf("%d dir: %s\n", rank, name);
  assert(posD < run.dir_count);
  run.dir_name[posD] = strdup(name);
  posD++;
}

static void prep_testdir(int j, int dir_iter){
  int pos = sprintf(run.testdir, "%s", run.testdirpath);
  if ( run.testdir[strlen( run.testdir ) - 1] != '/' ) {
      pos += sprintf(& run.testdir[pos], "/");
  }
  pos += sprintf(& run.testdir[pos], "%s", TEST_DIR);
  pos += sprintf(& run.testdir[pos], ".%d-%d", j, dir_iter);
}

static void unique_dir_access(int opt, char *to) {
    if (opt == MK_UNI_DIR) {
        sprintf( to, "%s/%s", run.testdir, run.unique_chdir_dir );
    } else if (opt == STAT_SUB_DIR) {
        sprintf( to, "%s/%s", run.testdir, run.unique_stat_dir );
    } else if (opt == READ_SUB_DIR) {
        sprintf( to, "%s/%s", run.testdir, run.unique_read_dir );
    } else if (opt == RM_SUB_DIR) {
        sprintf( to, "%s/%s", run.testdir, run.unique_rm_dir );
    } else if (opt == RM_UNI_DIR) {
        sprintf( to, "%s/%s", run.testdir, run.unique_rm_uni_dir );
    }
}

static void create_remove_dirs (const char *path, bool create, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];
    sprintf(curr_item, "%s/dir.%s%" PRIu64, path, create ? run.mk_name : run.rm_name, itemNum);
    add_dir(curr_item);
}

static void create_file (const char *path, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];

    sprintf(curr_item, "%s/file.%s"LLU"", path, run.rm_name, itemNum);
    if (!(run.shared_file && rank != 0)) {
        add_file(curr_item);
    }
}

static void create_remove_items_helper(const int dirs, const int create, const char *path, uint64_t itemNum) {
    for (uint64_t i = 0; i < run.items_per_dir ; ++i) {
        if (!dirs) {
            create_file (path, itemNum + i);
        } else {
            create_remove_dirs (path, create, itemNum + i);
        }
    }
}

/* helper function to do collective operations */
static void collective_helper(const int dirs, const int create, const char* path, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];

    for (uint64_t i = 0 ; i < run.items_per_dir ; ++i) {
        if (dirs) {
            create_remove_dirs (path, create, itemNum + i);
            continue;
        }

        sprintf(curr_item, "%s/file.%s"LLU"", path, create ? run.mk_name : run.rm_name, itemNum+i);

        add_file(curr_item);
    }
}

static void create_remove_items(int currDepth, const int dirs, const int create, const int collective, const char *path, uint64_t dirNum) {
    unsigned i;
    char dir[MAX_PATHLEN];
    char temp_path[MAX_PATHLEN];
    unsigned long long currDir = dirNum;

    memset(dir, 0, MAX_PATHLEN);
    strcpy(temp_path, path);

    if (currDepth == 0) {
        /* create run.items at this run.depth */
        if (!run.leaf_only || (run.depth == 0 && run.leaf_only)) {
            if (collective) {
                collective_helper(dirs, create, temp_path, 0);
            } else {
                create_remove_items_helper(dirs, create, temp_path, 0);
            }
        }

        if (run.depth > 0) {
            create_remove_items(++currDepth, dirs, create,
                                collective, temp_path, ++dirNum);
        }

    } else if (currDepth <= run.depth) {
        /* iterate through the branches */
        for (i=0; i<run.branch_factor; i++) {

            /* determine the current branch and append it to the path */
            sprintf(dir, "%s.%llu/", run.base_tree_name, currDir);
            strcat(temp_path, "/");
            strcat(temp_path, dir);

            /* create the run.items in this branch */
            if (!run.leaf_only || (run.leaf_only && currDepth == run.depth)) {
                if (collective) {
                    collective_helper(dirs, create, temp_path, currDir*run.items_per_dir);
                } else {
                    create_remove_items_helper(dirs, create, temp_path, currDir*run.items_per_dir);
                }
            }

            /* make the recursive call for the next level below this branch */
            create_remove_items(
                ++currDepth,
                dirs,
                create,
                collective,
                temp_path,
                ( currDir * ( unsigned long long )run.branch_factor ) + 1
               );
            currDepth--;

            /* reset the path */
            strcpy(temp_path, path);
            currDir++;
        }
    }
}

/* This method should be called by rank 0.  It subsequently does all of
   the creates and removes for the other ranks */
static void collective_create_remove(const int create, const int dirs, const int ntasks, const char *path) {
    char temp[MAX_PATHLEN];

    /* rank 0 does all of the creates and removes for all of the ranks */
    for (int i = 0 ; i < ntasks ; ++i) {
        memset(temp, 0, MAX_PATHLEN);

        strcpy(temp, run.testdir);
        strcat(temp, "/");

        /* set the base tree name appropriately */
        if (run.unique_dir_per_task) {
            sprintf(run.base_tree_name, "mdtest_tree.%d", i);
        } else {
            sprintf(run.base_tree_name, "mdtest_tree");
        }

        /* Setup to do I/O to the appropriate test dir */
        strcat(temp, run.base_tree_name);
        strcat(temp, ".0");

        /* set all item names appropriately */
        if (!run.shared_file) {
            sprintf(run.mk_name, "mdtest.%d.", (i+(0*run.nstride))%ntasks);
            sprintf(run.stat_name, "mdtest.%d.", (i+(1*run.nstride))%ntasks);
            sprintf(run.read_name, "mdtest.%d.", (i+(2*run.nstride))%ntasks);
            sprintf(run.rm_name, "mdtest.%d.", (i+(3*run.nstride))%ntasks);
        }
        if (run.unique_dir_per_task) {
            sprintf(run.unique_mk_dir, "%s/mdtest_tree.%d.0", run.testdir,
                    (i+(0*run.nstride))%ntasks);
            sprintf(run.unique_chdir_dir, "%s/mdtest_tree.%d.0", run.testdir,
                    (i+(1*run.nstride))%ntasks);
            sprintf(run.unique_stat_dir, "%s/mdtest_tree.%d.0", run.testdir,
                    (i+(2*run.nstride))%ntasks);
            sprintf(run.unique_read_dir, "%s/mdtest_tree.%d.0", run.testdir,
                    (i+(3*run.nstride))%ntasks);
            sprintf(run.unique_rm_dir, "%s/mdtest_tree.%d.0", run.testdir,
                    (i+(4*run.nstride))%ntasks);
            sprintf(run.unique_rm_uni_dir, "%s", run.testdir);
        }

        create_remove_items(0, dirs, create, 1, temp, 0);
    }

    /* reset all of the item names */
    if (run.unique_dir_per_task) {
        sprintf(run.base_tree_name, "mdtest_tree.0");
    } else {
        sprintf(run.base_tree_name, "mdtest_tree");
    }
    if (!run.shared_file) {
        sprintf(run.mk_name, "mdtest.%d.", (0+(0*run.nstride))%ntasks);
        sprintf(run.stat_name, "mdtest.%d.", (0+(1*run.nstride))%ntasks);
        sprintf(run.read_name, "mdtest.%d.", (0+(2*run.nstride))%ntasks);
        sprintf(run.rm_name, "mdtest.%d.", (0+(3*run.nstride))%ntasks);
    }
    if (run.unique_dir_per_task) {
        sprintf(run.unique_mk_dir, "%s/mdtest_tree.%d.0", run.testdir,
                (0+(0*run.nstride))%ntasks);
        sprintf(run.unique_chdir_dir, "%s/mdtest_tree.%d.0", run.testdir,
                (0+(1*run.nstride))%ntasks);
        sprintf(run.unique_stat_dir, "%s/mdtest_tree.%d.0", run.testdir,
                (0+(2*run.nstride))%ntasks);
        sprintf(run.unique_read_dir, "%s/mdtest_tree.%d.0", run.testdir,
                (0+(3*run.nstride))%ntasks);
        sprintf(run.unique_rm_dir, "%s/mdtest_tree.%d.0", run.testdir,
                (0+(4*run.nstride))%ntasks);
        sprintf(run.unique_rm_uni_dir, "%s", run.testdir);
    }
}

static void directory_test(const int iteration, const int ntasks, const char *path) {
    char temp_path[MAX_PATHLEN];

    /* create phase */
    if(run.create_only) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        /* "touch" the files */
        if (run.collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 1, ntasks, temp_path);
            }
        } else {
            /* create directories */
            create_remove_items(0, 1, 1, 0, temp_path, 0);
        }
      }
    }
}


static void file_test(const int iteration, const int ntasks, const char *path) {
    char temp_path[MAX_PATHLEN];
    /* create phase */
    if (run.create_only ) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);

        if (run.unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }
        if (run.collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 0, ntasks, temp_path);
            }
        }

        /* create files */
        create_remove_items(0, 0, 1, 0, temp_path, 0);
      }
    }else{
      if (run.stoneWallingStatusFile){
        int64_t expected_items;
        /* The number of run.items depends on the stonewalling file */
        expected_items = ReadStoneWallingIterations(run.stoneWallingStatusFile);
        if(expected_items >= 0){
          run.items = expected_items;
          run.items_per_dir = run.items;
        }
        if (rank == 0) {
          if(expected_items == -1){
            fprintf(out_logfile, "WARNING: could not read stonewall status file\n");
          }else if(verbose >= 1){
            fprintf(out_logfile, "Read stonewall status; items: "LLU"\n", run.items);
          }
        }
      }
    }

    /* stat phase */
    if (run.stat_only ) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }


        /* stat files */
        //mdtest_stat((run.random_seed > 0 ? 1 : 0), 0, dir_iter, temp_path);
      }
    }
}

static void create_remove_directory_tree(int create, int currDepth, char* path, int dirNum) {

    unsigned i;
    char dir[MAX_PATHLEN];

    if (currDepth == 0) {
        sprintf(dir, "%s/%s.%d/", path, run.base_tree_name, dirNum);
        add_treedir(dir);

        create_remove_directory_tree(create, ++currDepth, dir, ++dirNum);
    } else if (currDepth <= run.depth) {
        char temp_path[MAX_PATHLEN];
        strcpy(temp_path, path);
        int currDir = dirNum;

        for (i=0; i<run.branch_factor; i++) {
            sprintf(dir, "%s.%d/", run.base_tree_name, currDir);
            strcat(temp_path, dir);
            add_treedir(temp_path);
            create_remove_directory_tree(create, ++currDepth,
                                         temp_path, (run.branch_factor*currDir)+1);
            currDepth--;

            strcpy(temp_path, path);
            currDir++;
        }
    }
}

static void mdtest_iteration(int i, int j){
  /* start and end times of directory tree create/remove */
  double startCreate, endCreate;
  int k;

  if(run.create_only || run.remove_only){
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);

        if (run.unique_dir_per_task) {
            if (run.collective_creates && (rank == 0)) {
                for (k=0; k<run.size; k++) {
                    sprintf(run.base_tree_name, "mdtest_tree.%d", k);
                    create_remove_directory_tree(1, 0, run.testdir, 0);
                }
            } else if (!run.collective_creates) {
                create_remove_directory_tree(1, 0, run.testdir, 0);
            }
        } else {
            if (rank == 0) {
                create_remove_directory_tree(1, 0 , run.testdir, 0);
            }
        }
      }
  }

  sprintf(run.unique_mk_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_chdir_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_stat_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_read_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_rm_dir, "%s.0", run.base_tree_name);
  run.unique_rm_uni_dir[0] = 0;

  if (rank < i) {
      if (!run.shared_file) {
          sprintf(run.mk_name, "mdtest.%d.", (rank+(0*run.nstride))%i);
          sprintf(run.stat_name, "mdtest.%d.", (rank+(1*run.nstride))%i);
          sprintf(run.read_name, "mdtest.%d.", (rank+(2*run.nstride))%i);
          sprintf(run.rm_name, "mdtest.%d.", (rank+(3*run.nstride))%i);
      }
      if (run.unique_dir_per_task) {
          sprintf(run.unique_mk_dir, "mdtest_tree.%d.0",  (rank+(0*run.nstride))%i);
          sprintf(run.unique_chdir_dir, "mdtest_tree.%d.0", (rank+(1*run.nstride))%i);
          sprintf(run.unique_stat_dir, "mdtest_tree.%d.0", (rank+(2*run.nstride))%i);
          sprintf(run.unique_read_dir, "mdtest_tree.%d.0", (rank+(3*run.nstride))%i);
          sprintf(run.unique_rm_dir, "mdtest_tree.%d.0", (rank+(4*run.nstride))%i);
          run.unique_rm_uni_dir[0] = 0;
      }

      if (run.dirs_only && !run.shared_file) {
          directory_test(j, i, run.unique_mk_dir);
      }
      if (run.files_only) {
          file_test(j, i, run.unique_mk_dir);
      }
  }
}



void mdtest_generate_filenames(int i, int j){
  static int initialized = 0;
  if (initialized){
    for(int i = 0; i < run.file_count; i++){
      free(run.file_name[i]);
    }
    free(run.file_name);

    for(int i = 0; i < run.dir_count; i++){
      free(run.dir_name[i]);
    }
    free(run.dir_name);

    for(int i = 0; i < run.treedir_count; i++){
      free(run.treedir_name[i]);
    }
    free(run.treedir_name);
  }
  initialized = 1;

  run.file_count = run.items;
  if(run.num_dirs_in_tree_calc){
    run.file_count *= run.num_dirs_in_tree_calc;
  }

  run.dir_count = run.file_count;
  if(run.directory_loops > 0){
    run.dir_count *= run.directory_loops;
    run.file_count *= run.directory_loops;
  }
  if(verbose > 3){
    printf("%d V=3: Allocating %d dirs %d files\n", rank, run.dir_count, run.file_count);
  }

  run.treedir_count = run.num_dirs_in_tree;

  run.file_name = malloc(sizeof(char*)* run.file_count);
  memset(run.file_name, 0, sizeof(char*)* run.file_count);
  run.dir_name = malloc(sizeof(char*)* run.dir_count);
  memset(run.dir_name, 0, sizeof(char*)* run.dir_count);
  run.treedir_name = malloc(sizeof(char*)* run.treedir_count);
  memset(run.treedir_name, 0, sizeof(char*)* run.treedir_count);

  posF = 0;
  posD = 0;
  posT = 0;
  mdtest_iteration(i, j);
  run.dir_count = posD;
  run.treedir_count = posT;
  run.file_count = posF;
}
