/*
 * Copyright (C) 2003, The Regents of the University of California.
 *  Produced at the Lawrence Livermore National Laboratory.
 *  Written by Christopher J. Morrone <morrone@llnl.gov>,
 *  Bill Loewe <loewe@loewe.net>, Tyce McLarty <mclarty@llnl.gov>,
 *  and Ryan Kroiss <rrkroiss@lanl.gov>.
 *  All rights reserved.
 *  UCRL-CODE-155800
 *
 *  Please read the COPYRIGHT file.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License (as published by
 *  the Free Software Foundation) version 2, dated June 1991.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  terms and conditions of the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <limits.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "option.h"
#include "utilities.h"

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#include <fcntl.h>
#include <string.h>

#if HAVE_STRINGS_H
#include <strings.h>
#endif

#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "aiori.h"
#include "ior.h"
#include "mdtest.h"

#include <mpi.h>

#define FILEMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
#define DIRMODE S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH
#define RELEASE_VERS "1.9.3"
#define TEST_DIR "#test-dir"
#define ITEM_COUNT 25000

#define LLU "%lu"

/* just use the POSIX backend for now */
static const char *backend_name;
static const ior_aiori_t *backend;
static IOR_param_t param;

static mdtest_results_t * summary_table;
/* This structure describes the processing status for stonewalling */
typedef struct{
  double start_time;

  int stone_wall_timer_seconds;

  uint64_t items_start;
  uint64_t items_done;

  uint64_t items_per_dir;
} rank_progress_t;

mdtest_runtime_t run;

#define CHECK_STONE_WALL(p) (((p)->stone_wall_timer_seconds != 0) && ((GetTimeStamp() - (p)->start_time) > (p)->stone_wall_timer_seconds))

/* for making/removing unique directory && stating/deleting subdirectory */
enum {MK_UNI_DIR, STAT_SUB_DIR, READ_SUB_DIR, RM_SUB_DIR, RM_UNI_DIR};

void offset_timers(double * t, int tcount) {
    double toffset;
    int i;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering offset_timers...\n" );
        fflush( out_logfile );
    }

    toffset = GetTimeStamp() - t[tcount];
    for (i = 0; i < tcount+1; i++) {
        t[i] += toffset;
    }
}

void parse_dirpath(char *dirpath_arg) {
    char * tmp, * token;
    char delimiter_string[3] = { '@', '\n', '\0' };
    int i = 0;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering parse_dirpath...\n" );
        fflush( out_logfile );
    }

    tmp = dirpath_arg;

    if (* tmp != '\0') run.path_count++;
    while (* tmp != '\0') {
        if (* tmp == '@') {
            run.path_count++;
        }
        tmp++;
    }
    run.filenames = (char **)malloc(run.path_count * sizeof(char **));
    if (run.filenames == NULL) {
        FAIL("out of memory");
    }

    token = strtok(dirpath_arg, delimiter_string);
    while (token != NULL) {
        run.filenames[i] = token;
        token = strtok(NULL, delimiter_string);
        i++;
    }
}

static void prep_testdir(int j, int dir_iter){
  int pos = sprintf(run.testdir, "%s", run.testdirpath);
  if ( run.testdir[strlen( run.testdir ) - 1] != '/' ) {
      pos += sprintf(& run.testdir[pos], "/");
  }
  pos += sprintf(& run.testdir[pos], "%s", TEST_DIR);
  pos += sprintf(& run.testdir[pos], ".%d-%d", j, dir_iter);
}

/*
 * This function copies the unique directory name for a given option to
 * the "to" parameter. Some memory must be allocated to the "to" parameter.
 */
void unique_dir_access(int opt, char *to) {
    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering unique_dir_access...\n" );
        fflush( out_logfile );
    }

    if (opt == MK_UNI_DIR) {
        MPI_Barrier(testComm);
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
    const char *operation = create ? "create" : "remove";

    if (( rank == 0 )                                         &&
        ( verbose >= 3 )                                      &&
        (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {

        fprintf(out_logfile, "V-3: %s dir: "LLU"\n", operation, itemNum);
        fflush(out_logfile);
    }

    //create dirs
    sprintf(curr_item, "%s/dir.%s%" PRIu64, path, create ? run.mk_name : run.rm_name, itemNum);
    if (rank == 0 && verbose >= 3) {
        fprintf(out_logfile, "V-3: create_remove_items_helper (dirs %s): curr_item is \"%s\"\n", operation, curr_item);
        fflush(out_logfile);
    }

    if (create) {
        if (backend->mkdir(curr_item, DIRMODE, &param) == -1) {
            FAIL("unable to create directory");
        }
    } else {
        if (backend->rmdir(curr_item, &param) == -1) {
            FAIL("unable to remove directory");
        }
    }
}

static void remove_file (const char *path, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];

    if (( rank == 0 )                                       &&
        ( verbose >= 3 )                                    &&
        (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {

        fprintf(out_logfile, "V-3: remove file: "LLU"\n", itemNum);
        fflush(out_logfile);
    }

    //remove files
    sprintf(curr_item, "%s/file.%s"LLU"", path, run.rm_name, itemNum);
    if (rank == 0 && verbose >= 3) {
        fprintf(out_logfile, "V-3: create_remove_items_helper (non-dirs remove): curr_item is \"%s\"\n", curr_item);
        fflush(out_logfile);
    }
    if (!(run.shared_file && rank != 0)) {
        backend->delete (curr_item, &param);
    }
}

static void create_file (const char *path, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];
    void *aiori_fh;

    if (( rank == 0 )                                             &&
        ( verbose >= 3 )                                          &&
        (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {

        fprintf(out_logfile, "V-3: create file: "LLU"\n", itemNum);
        fflush(out_logfile);
    }

    //create files
    sprintf(curr_item, "%s/file.%s"LLU"", path, run.mk_name, itemNum);
    if ((rank == 0 && verbose >= 3) || verbose >= 5) {
        fprintf(out_logfile, "V-3: create_remove_items_helper (non-dirs create): curr_item is \"%s\"\n", curr_item);
        fflush(out_logfile);
    }

    if (run.collective_creates) {
        param.openFlags = IOR_WRONLY;

        if (rank == 0 && verbose >= 3) {
            fprintf(out_logfile,  "V-3: create_remove_items_helper (collective): open...\n" );
            fflush( out_logfile );
        }

        aiori_fh = backend->open (curr_item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to open file");
        }

        /*
         * !run.collective_creates
         */
    } else {
        param.openFlags = IOR_CREAT | IOR_WRONLY;
        param.filePerProc = !run.shared_file;

        if (rank == 0 && verbose >= 3) {
            fprintf(out_logfile,  "V-3: create_remove_items_helper (non-collective, shared): open...\n" );
            fflush( out_logfile );
        }

        aiori_fh = backend->create (curr_item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to create file");
        }
    }

    if (run.write_bytes > 0) {
        if (rank == 0 && verbose >= 3) {
            fprintf(out_logfile,  "V-3: create_remove_items_helper: write...\n" );
            fflush( out_logfile );
        }

        /*
         * According to Bill Loewe, writes are only done one time, so they are always at
         * offset 0 (zero).
         */
        param.offset = 0;
        param.fsyncPerWrite = run.sync_file;
        if ( run.write_bytes != (size_t) backend->xfer (WRITE, aiori_fh, (IOR_size_t *) run.write_buffer, run.write_bytes, &param)) {
            FAIL("unable to write file");
        }
    }

    if (rank == 0 && verbose >= 3) {
        fprintf(out_logfile,  "V-3: create_remove_items_helper: close...\n" );
        fflush( out_logfile );
    }

    backend->close (aiori_fh, &param);
}

/* helper for creating/removing run.items */
void create_remove_items_helper(const int dirs, const int create, const char *path,
                                uint64_t itemNum, rank_progress_t * progress) {

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering create_remove_items_helper...\n" );
        fflush( out_logfile );
    }

    for (uint64_t i = progress->items_start; i < progress->items_per_dir ; ++i) {
        if (!dirs) {
            if (create) {
                create_file (path, itemNum + i);
            } else {
                remove_file (path, itemNum + i);
            }
        } else {
            create_remove_dirs (path, create, itemNum + i);
        }
        if(CHECK_STONE_WALL(progress)){
          if(progress->items_done == 0){
            progress->items_done = i + 1;
          }
          return;
        }
    }
    progress->items_done = progress->items_per_dir;
}

/* helper function to do collective operations */
void collective_helper(const int dirs, const int create, const char* path, uint64_t itemNum, rank_progress_t * progress) {
    char curr_item[MAX_PATHLEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering collective_helper...\n" );
        fflush( out_logfile );
    }
    for (uint64_t i = progress->items_start ; i < progress->items_per_dir ; ++i) {
        if (dirs) {
            create_remove_dirs (path, create, itemNum + i);
            continue;
        }

        sprintf(curr_item, "%s/file.%s"LLU"", path, create ? run.mk_name : run.rm_name, itemNum+i);
        if (rank == 0 && verbose >= 3) {
            fprintf(out_logfile, "V-3: create file: %s\n", curr_item);
            fflush(out_logfile);
        }

        if (create) {
            void *aiori_fh;

            //create files
            param.openFlags = IOR_WRONLY | IOR_CREAT;
            aiori_fh = backend->create (curr_item, &param);
            if (NULL == aiori_fh) {
                FAIL("unable to create file");
            }

            backend->close (aiori_fh, &param);
        } else if (!(run.shared_file && rank != 0)) {
            //remove files
            backend->delete (curr_item, &param);
        }
        if(CHECK_STONE_WALL(progress)){
          progress->items_done = i + 1;
          return;
        }
    }
    progress->items_done = progress->items_per_dir;
}

/* recusive function to create and remove files/directories from the
   directory tree */
void create_remove_items(int currDepth, const int dirs, const int create, const int collective, const char *path, uint64_t dirNum, rank_progress_t * progress) {
    unsigned i;
    char dir[MAX_PATHLEN];
    char temp_path[MAX_PATHLEN];
    unsigned long long currDir = dirNum;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering create_remove_items, currDepth = %d...\n", currDepth );
        fflush( out_logfile );
    }


    memset(dir, 0, MAX_PATHLEN);
    strcpy(temp_path, path);

    if (rank == 0 && verbose >= 3) {
        fprintf(out_logfile,  "V-3: create_remove_items (start): temp_path is \"%s\"\n", temp_path );
        fflush(out_logfile);
    }

    if (currDepth == 0) {
        /* create run.items at this run.depth */
        if (!run.leaf_only || (run.depth == 0 && run.leaf_only)) {
            if (collective) {
                collective_helper(dirs, create, temp_path, 0, progress);
            } else {
                create_remove_items_helper(dirs, create, temp_path, 0, progress);
            }
        }

        if (run.depth > 0) {
            create_remove_items(++currDepth, dirs, create,
                                collective, temp_path, ++dirNum, progress);
        }

    } else if (currDepth <= run.depth) {
        /* iterate through the branches */
        for (i=0; i<run.branch_factor; i++) {

            /* determine the current branch and append it to the path */
            sprintf(dir, "%s.%llu/", run.base_tree_name, currDir);
            strcat(temp_path, "/");
            strcat(temp_path, dir);

            if (rank == 0 && verbose >= 3) {
                fprintf(out_logfile,  "V-3: create_remove_items (for loop): temp_path is \"%s\"\n", temp_path );
                fflush(out_logfile);
            }

            /* create the run.items in this branch */
            if (!run.leaf_only || (run.leaf_only && currDepth == run.depth)) {
                if (collective) {
                    collective_helper(dirs, create, temp_path, currDir*run.items_per_dir, progress);
                } else {
                    create_remove_items_helper(dirs, create, temp_path, currDir*run.items_per_dir, progress);
                }
            }

            /* make the recursive call for the next level below this branch */
            create_remove_items(
                ++currDepth,
                dirs,
                create,
                collective,
                temp_path,
                ( currDir * ( unsigned long long )run.branch_factor ) + 1,
                progress
               );
            currDepth--;

            /* reset the path */
            strcpy(temp_path, path);
            currDir++;
        }
    }
}

/* stats all of the run.items created as specified by the input parameters */
void mdtest_stat(const int random, const int dirs, const long dir_iter, const char *path, rank_progress_t * progress) {
    struct stat buf;
    uint64_t parent_dir, item_num = 0;
    char item[MAX_PATHLEN], temp[MAX_PATHLEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering mdtest_stat...\n" );
        fflush( out_logfile );
    }

    uint64_t stop_items = run.items;

    if( run.directory_loops != 1 || run.leaf_only ){
      stop_items = run.items_per_dir;
    }

    /* iterate over all of the item IDs */
    for (uint64_t i = 0 ; i < stop_items ; ++i) {
        /*
         * It doesn't make sense to pass the address of the array because that would
         * be like passing char **. Tested it on a Cray and it seems to work either
         * way, but it seems that it is correct without the "&".
         *
         memset(&item, 0, MAX_PATHLEN);
        */
        memset(item, 0, MAX_PATHLEN);
        memset(temp, 0, MAX_PATHLEN);


        /* determine the item number to stat */
        if (random) {
            item_num = run.rand_array[i];
        } else {
            item_num = i;
        }

        /* make adjustments if in leaf only mode*/
        if (run.leaf_only) {
            item_num += run.items_per_dir *
                (run.num_dirs_in_tree - (uint64_t) pow( run.branch_factor, run.depth ));
        }

        /* create name of file/dir to stat */
        if (dirs) {
            if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
                fprintf(out_logfile, "V-3: stat dir: "LLU"\n", i);
                fflush(out_logfile);
            }
            sprintf(item, "dir.%s"LLU"", run.stat_name, item_num);
        } else {
            if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
                fprintf(out_logfile, "V-3: stat file: "LLU"\n", i);
                fflush(out_logfile);
            }
            sprintf(item, "file.%s"LLU"", run.stat_name, item_num);
        }

        /* determine the path to the file/dir to be stat'ed */
        parent_dir = item_num / run.items_per_dir;

        if (parent_dir > 0) {        //item is not in tree's root directory
            /* prepend parent directory to item's path */
            sprintf(temp, "%s."LLU"/%s", run.base_tree_name, parent_dir, item);
            strcpy(item, temp);

            //still not at the tree's root dir
            while (parent_dir > run.branch_factor) {
                parent_dir = (uint64_t) ((parent_dir-1) / run.branch_factor);
                sprintf(temp, "%s."LLU"/%s", run.base_tree_name, parent_dir, item);
                strcpy(item, temp);
            }
        }

        /* Now get item to have the full path */
        sprintf( temp, "%s/%s", path, item );
        strcpy( item, temp );

        /* below temp used to be hiername */
        if (rank == 0 && verbose >= 3) {
            if (dirs) {
                fprintf(out_logfile, "V-3: mdtest_stat dir : %s\n", item);
            } else {
                fprintf(out_logfile, "V-3: mdtest_stat file: %s\n", item);
            }
            fflush(out_logfile);
        }

        if (-1 == backend->stat (item, &buf, &param)) {
            if (dirs) {
                if ( verbose >= 3 ) {
                    fprintf( out_logfile, "V-3: Stat'ing directory \"%s\"\n", item );
                    fflush( out_logfile );
                }
                FAIL("unable to stat directory");
            } else {
                if ( verbose >= 3 ) {
                    fprintf( out_logfile, "V-3: Stat'ing file \"%s\"\n", item );
                    fflush( out_logfile );
                }
                FAIL("unable to stat file");
            }
        }
    }
}


/* reads all of the run.items created as specified by the input parameters */
void mdtest_read(int random, int dirs, const long dir_iter, char *path) {
    uint64_t parent_dir, item_num = 0;
    char item[MAX_PATHLEN], temp[MAX_PATHLEN];
    void *aiori_fh;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering mdtest_read...\n" );
        fflush( out_logfile );
    }

    /* allocate read buffer */
    if (run.read_bytes > 0) {
        run.read_buffer = (char *)malloc(run.read_bytes);
        if (run.read_buffer == NULL) {
            FAIL("out of memory");
        }
    }

    uint64_t stop_items = run.items;

    if( run.directory_loops != 1 ){
      stop_items = run.items_per_dir;
    }

    /* iterate over all of the item IDs */
    for (uint64_t i = 0 ; i < stop_items ; ++i) {
        /*
         * It doesn't make sense to pass the address of the array because that would
         * be like passing char **. Tested it on a Cray and it seems to work either
         * way, but it seems that it is correct without the "&".
         *
         * NTH: Both are technically correct in C.
         *
         * memset(&item, 0, MAX_PATHLEN);
         */
        memset(item, 0, MAX_PATHLEN);
        memset(temp, 0, MAX_PATHLEN);

        /* determine the item number to read */
        if (random) {
            item_num = run.rand_array[i];
        } else {
            item_num = i;
        }

        /* make adjustments if in leaf only mode*/
        if (run.leaf_only) {
            item_num += run.items_per_dir *
                (run.num_dirs_in_tree - (uint64_t) pow (run.branch_factor, run.depth));
        }

        /* create name of file to read */
        if (!dirs) {
            if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
                fprintf(out_logfile, "V-3: read file: "LLU"\n", i);
                fflush(out_logfile);
            }
            sprintf(item, "file.%s"LLU"", run.read_name, item_num);
        }

        /* determine the path to the file/dir to be read'ed */
        parent_dir = item_num / run.items_per_dir;

        if (parent_dir > 0) {        //item is not in tree's root directory

            /* prepend parent directory to item's path */
            sprintf(temp, "%s."LLU"/%s", run.base_tree_name, parent_dir, item);
            strcpy(item, temp);

            /* still not at the tree's root dir */
            while (parent_dir > run.branch_factor) {
                parent_dir = (unsigned long long) ((parent_dir-1) / run.branch_factor);
                sprintf(temp, "%s."LLU"/%s", run.base_tree_name, parent_dir, item);
                strcpy(item, temp);
            }
        }

        /* Now get item to have the full path */
        sprintf( temp, "%s/%s", path, item );
        strcpy( item, temp );

        /* below temp used to be hiername */
        if (rank == 0 && verbose >= 3) {
            if (!dirs) {
                fprintf(out_logfile, "V-3: mdtest_read file: %s\n", item);
            }
            fflush(out_logfile);
        }

        /* open file for reading */
        param.openFlags = O_RDONLY;
        aiori_fh = backend->open (item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to open file");
        }

        /* read file */
        if (run.read_bytes > 0) {
            if (run.read_bytes != (size_t) backend->xfer (READ, aiori_fh, (IOR_size_t *) run.read_buffer, run.read_bytes, &param)) {
                FAIL("unable to read file");
            }
        }

        /* close file */
        backend->close (aiori_fh, &param);
    }
}

/* This method should be called by rank 0.  It subsequently does all of
   the creates and removes for the other ranks */
void collective_create_remove(const int create, const int dirs, const int ntasks, const char *path, rank_progress_t * progress) {
    char temp[MAX_PATHLEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering collective_create_remove...\n" );
        fflush( out_logfile );
    }

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

        /* Now that everything is set up as it should be, do the create or remove */
        if (rank == 0 && verbose >= 3) {
            fprintf(out_logfile, "V-3: collective_create_remove (create_remove_items): temp is \"%s\"\n", temp);
            fflush( out_logfile );
        }

        create_remove_items(0, dirs, create, 1, temp, 0, progress);
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

void directory_test(const int iteration, const int ntasks, const char *path, rank_progress_t * progress) {
    double t[5] = {0};
    char temp_path[MAX_PATHLEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering directory_test...\n" );
        fflush( out_logfile );
    }

    MPI_Barrier(testComm);
    t[0] = GetTimeStamp();

    /* create phase */
    if(run.create_only) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 0);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: directory_test: create path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* "touch" the files */
        if (run.collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 1, ntasks, temp_path, progress);
            }
        } else {
            /* create directories */
            create_remove_items(0, 1, 1, 0, temp_path, 0, progress);
        }
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[1] = GetTimeStamp();

    /* stat phase */
    if (run.stat_only) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 1);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: directory_test: stat path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* stat directories */
        if (run.random_seed > 0) {
            mdtest_stat(1, 1, dir_iter, temp_path, progress);
        } else {
            mdtest_stat(0, 1, dir_iter, temp_path, progress);
        }
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[2] = GetTimeStamp();

    /* read phase */
    if (run.read_only) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(READ_SUB_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 2);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: directory_test: read path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* read directories */
        if (run.random_seed > 0) {
            ;        /* N/A */
        } else {
            ;        /* N/A */
        }
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[3] = GetTimeStamp();

    if (run.remove_only) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(RM_SUB_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 3);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: directory_test: remove directories path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* remove directories */
        if (run.collective_creates) {
            if (rank == 0) {
                collective_create_remove(0, 1, ntasks, temp_path, progress);
            }
        } else {
            create_remove_items(0, 1, 0, 0, temp_path, 0, progress);
        }
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[4] = GetTimeStamp();

    if (run.remove_only) {
        if (run.unique_dir_per_task) {
            unique_dir_access(RM_UNI_DIR, temp_path);
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: directory_test: remove unique directories path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }
    }

    if (run.unique_dir_per_task && !run.time_unique_dir_overhead) {
        offset_timers(t, 4);
    }

    /* calculate times */
    if (run.create_only) {
        summary_table[iteration].rate[0] = run.items*run.size/(t[1] - t[0]);
        summary_table[iteration].time[0] = t[1] - t[0];
        summary_table[iteration].items[0] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[0] = run.items;
    }
    if (run.stat_only) {
        summary_table[iteration].rate[1] = run.items*run.size/(t[2] - t[1]);
        summary_table[iteration].time[1] = t[2] - t[1];
        summary_table[iteration].items[1] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[1] = run.items;
    }
    if (run.read_only) {
        summary_table[iteration].rate[2] = run.items*run.size/(t[3] - t[2]);
        summary_table[iteration].time[2] = t[3] - t[2];
        summary_table[iteration].items[2] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[2] = run.items;
    }
    if (run.remove_only) {
        summary_table[iteration].rate[3] = run.items*run.size/(t[4] - t[3]);
        summary_table[iteration].time[3] = t[4] - t[3];
        summary_table[iteration].items[3] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[3] = run.items;
    }

    if (verbose >= 1 && rank == 0) {
        fprintf(out_logfile, "V-1:   Directory creation: %14.3f sec, %14.3f ops/sec\n",
               t[1] - t[0], summary_table[iteration].rate[0]);
        fprintf(out_logfile, "V-1:   Directory stat    : %14.3f sec, %14.3f ops/sec\n",
               t[2] - t[1], summary_table[iteration].rate[1]);
/* N/A
   fprintf(out_logfile, "V-1:   Directory read    : %14.3f sec, %14.3f ops/sec\n",
   t[3] - t[2], summary_table[iteration].rate[2]);
*/
        fprintf(out_logfile, "V-1:   Directory removal : %14.3f sec, %14.3f ops/sec\n",
               t[4] - t[3], summary_table[iteration].rate[3]);
        fflush(out_logfile);
    }
}

/* Returns if the stonewall was hit */
int updateStoneWallIterations(int iteration, rank_progress_t * progress, double tstart){
  int hit = 0;
  if (verbose >= 1 ) {
    fprintf( out_logfile, "V-1: rank %d stonewall hit with %lld items\n", rank, (long long) progress->items_done );
    fflush( out_logfile );
  }
  uint64_t done = progress->items_done;
  long long unsigned max_iter = 0;
  MPI_Allreduce(& progress->items_done, & max_iter, 1, MPI_LONG_LONG_INT, MPI_MAX, testComm);
  summary_table[iteration].stonewall_time[MDTEST_FILE_CREATE_NUM] = GetTimeStamp() - tstart;

  // continue to the maximum...
  long long min_accessed = 0;
  MPI_Reduce(& progress->items_done, & min_accessed, 1, MPI_LONG_LONG_INT, MPI_MIN, 0, testComm);
  long long sum_accessed = 0;
  MPI_Reduce(& progress->items_done, & sum_accessed, 1, MPI_LONG_LONG_INT, MPI_SUM, 0, testComm);

  if(run.items != (sum_accessed / run.size)){
    summary_table[iteration].stonewall_item_sum[MDTEST_FILE_CREATE_NUM] = sum_accessed;
    summary_table[iteration].stonewall_item_min[MDTEST_FILE_CREATE_NUM] = min_accessed * run.size;
    if (rank == 0){
      fprintf( out_logfile, "Continue stonewall hit min: %lld max: %lld avg: %.1f \n", min_accessed, max_iter, ((double) sum_accessed) / run.size);
      fflush( out_logfile );
    }
    hit = 1;
  }
  progress->items_start = done;
  progress->items_per_dir = max_iter;

  return hit;
}

void file_test(const int iteration, const int ntasks, const char *path, rank_progress_t * progress) {
    double t[5] = {0};
    char temp_path[MAX_PATHLEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering file_test...\n" );
        fflush( out_logfile );
    }

    MPI_Barrier(testComm);
    t[0] = GetTimeStamp();

    /* create phase */
    if (run.create_only ) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);

        if (run.unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 0);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: file_test: create path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* "touch" the files */
        if (run.collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 0, ntasks, temp_path, progress);
            }
            MPI_Barrier(testComm);
        }

        /* create files */
        create_remove_items(0, 0, 1, 0, temp_path, 0, progress);
        if(run.stone_wall_timer_seconds){
	        int hit = updateStoneWallIterations(iteration, progress, t[0]);

          if (hit){
            progress->stone_wall_timer_seconds = 0;
            if (verbose > 1){
              printf("stonewall rank %d: %lld of %lld \n", rank, (long long) progress->items_start, (long long) progress->items_per_dir);
            }
            create_remove_items(0, 0, 1, 0, temp_path, 0, progress);
            // now reset the values
            progress->stone_wall_timer_seconds = run.stone_wall_timer_seconds;
            run.items = progress->items_done;
          }
          if (run.stoneWallingStatusFile){
            StoreStoneWallingIterations(run.stoneWallingStatusFile, progress->items_done);
          }
          // reset stone wall timer to allow proper cleanup
          progress->stone_wall_timer_seconds = 0;
        }
      }
    }else{
      if (run.stoneWallingStatusFile){
        int64_t expected_items;
        /* The number of run.items depends on the stonewalling file */
        expected_items = ReadStoneWallingIterations(run.stoneWallingStatusFile);
        if(expected_items >= 0){
          run.items = expected_items;
          progress->items_per_dir = run.items;
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

    if (run.barriers) {
      MPI_Barrier(testComm);
    }
    t[1] = GetTimeStamp();

    /* stat phase */
    if (run.stat_only ) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 1);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: file_test: stat path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* stat files */
        mdtest_stat((run.random_seed > 0 ? 1 : 0), 0, dir_iter, temp_path, progress);
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[2] = GetTimeStamp();

    /* read phase */
    if (run.read_only ) {
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(READ_SUB_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 2);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: file_test: read path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        /* read files */
        if (run.random_seed > 0) {
                mdtest_read(1,0, dir_iter, temp_path);
        } else {
                mdtest_read(0,0, dir_iter, temp_path);
        }
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[3] = GetTimeStamp();

    if (run.remove_only) {
      progress->items_start = 0;

      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (run.unique_dir_per_task) {
            unique_dir_access(RM_SUB_DIR, temp_path);
            if (!run.time_unique_dir_overhead) {
                offset_timers(t, 3);
            }
        } else {
            sprintf( temp_path, "%s/%s", run.testdir, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: file_test: rm directories path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }

        if (run.collective_creates) {
            if (rank == 0) {
                collective_create_remove(0, 0, ntasks, temp_path, progress);
            }
        } else {
            create_remove_items(0, 0, 0, 0, temp_path, 0, progress);
        }
      }
    }

    if (run.barriers) {
        MPI_Barrier(testComm);
    }
    t[4] = GetTimeStamp();
    if (run.remove_only) {
        if (run.unique_dir_per_task) {
            unique_dir_access(RM_UNI_DIR, temp_path);
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            fprintf(out_logfile,  "V-3: file_test: rm unique directories path is \"%s\"\n", temp_path );
            fflush( out_logfile );
        }
    }

    if (run.unique_dir_per_task && !run.time_unique_dir_overhead) {
        offset_timers(t, 4);
    }

    if(run.num_dirs_in_tree_calc){ /* this is temporary fix needed when using -n and -i together */
      run.items *= run.num_dirs_in_tree_calc;
    }

    /* calculate times */
    if (run.create_only) {
        summary_table[iteration].rate[4] = run.items*run.size/(t[1] - t[0]);
        summary_table[iteration].time[4] = t[1] - t[0];
        summary_table[iteration].items[4] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[4] = run.items;
    }
    if (run.stat_only) {
        summary_table[iteration].rate[5] = run.items*run.size/(t[2] - t[1]);
        summary_table[iteration].time[5] = t[2] - t[1];
        summary_table[iteration].items[5] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[5] = run.items;
    }
    if (run.read_only) {
        summary_table[iteration].rate[6] = run.items*run.size/(t[3] - t[2]);
        summary_table[iteration].time[6] = t[3] - t[2];
        summary_table[iteration].items[6] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[6] = run.items;
    }
    if (run.remove_only) {
        summary_table[iteration].rate[7] = run.items*run.size/(t[4] - t[3]);
        summary_table[iteration].time[7] = t[4] - t[3];
        summary_table[iteration].items[7] = run.items*run.size;
        summary_table[iteration].stonewall_last_item[7] = run.items;
    }

    if (verbose >= 1 && rank == 0) {
        fprintf(out_logfile, "V-1:   File creation     : %14.3f sec, %14.3f ops/sec\n",
               t[1] - t[0], summary_table[iteration].rate[4]);
        fprintf(out_logfile, "V-1:   File stat         : %14.3f sec, %14.3f ops/sec\n",
               t[2] - t[1], summary_table[iteration].rate[5]);
        fprintf(out_logfile, "V-1:   File read         : %14.3f sec, %14.3f ops/sec\n",
               t[3] - t[2], summary_table[iteration].rate[6]);
        fprintf(out_logfile, "V-1:   File removal      : %14.3f sec, %14.3f ops/sec\n",
               t[4] - t[3], summary_table[iteration].rate[7]);
        fflush(out_logfile);
    }
}

void print_help (void) {
    int j;

    char APIs[1024];
    char APIs_legacy[1024];
    aiori_supported_apis(APIs, APIs_legacy);
    char apiStr[1024];
    sprintf(apiStr, "API for I/O [%s]", APIs);

    fprintf(out_logfile,
        "Usage: mdtest [-b branching_factor] [-B] [-c] [-C] [-d testdir] [-D] [-e number_of_bytes_to_read]\n"
        "              [-E] [-f first] [-F] [-h] [-i iterations] [-I items_per_dir] [-l last] [-L]\n"
        "              [-n number_of_items] [-N stride_length] [-p seconds] [-r]\n"
        "              [-R[seed]] [-s stride] [-S] [-t] [-T] [-u] [-v] [-a API]\n"
        "              [-V verbosity_value] [-w number_of_bytes_to_write] [-W seconds] [-y] [-z depth] -Z\n"
        "\t-a: %s\n"
        "\t-b: branching factor of hierarchical directory structure\n"
        "\t-B: no barriers between phases\n"
        "\t-c: collective creates: task 0 does all creates\n"
        "\t-C: only create files/dirs\n"
        "\t-d: the directory in which the tests will run\n"
        "\t-D: perform test on directories only (no files)\n"
        "\t-e: bytes to read from each file\n"
        "\t-E: only read files/dir\n"
        "\t-f: first number of tasks on which the test will run\n"
        "\t-F: perform test on files only (no directories)\n"
        "\t-h: prints this help message\n"
        "\t-i: number of iterations the test will run\n"
        "\t-I: number of items per directory in tree\n"
        "\t-l: last number of tasks on which the test will run\n"
        "\t-L: files only at leaf level of tree\n"
        "\t-n: every process will creat/stat/read/remove # directories and files\n"
        "\t-N: stride # between neighbor tasks for file/dir operation (local=0)\n"
        "\t-p: pre-iteration delay (in seconds)\n"
        "\t-r: only remove files or directories left behind by previous runs\n"
        "\t-R: randomly stat files (optional argument for random seed)\n"
        "\t-s: stride between the number of tasks for each test\n"
        "\t-S: shared file access (file only, no directories)\n"
        "\t-t: time unique working directory overhead\n"
        "\t-T: only stat files/dirs\n"
        "\t-u: unique working directory for each task\n"
        "\t-v: verbosity (each instance of option increments by one)\n"
        "\t-V: verbosity value\n"
        "\t-w: bytes to write to each file after it is created\n"
        "\t-W: number in seconds; stonewall timer, write as many seconds and ensure all processes did the same number of operations (currently only stops during create phase)\n"
        "\t-x: stoneWallingStatusFile; contains the number of iterations of the creation phase, can be used to split phases across runs\n"
        "\t-y: sync file after writing\n"
        "\t-z: depth of hierarchical directory structure\n"
        "\t-Z: print time instead of rate\n",
        apiStr
        );

    MPI_Initialized(&j);
    if (j) {
        MPI_Finalize();
    }
    exit(0);
}

void summarize_results(int iterations, int print_time) {
    char access[MAX_PATHLEN];
    int i, j, k;
    int start, stop, tableSize = MDTEST_LAST_NUM;
    double min, max, mean, sd, sum = 0, var = 0, curr = 0;

    double all[iterations * run.size * tableSize];


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering summarize_results...\n" );
        fflush( out_logfile );
    }

    MPI_Barrier(testComm);
    for(int i=0; i < iterations; i++){
      if(print_time){
        MPI_Gather(& summary_table[i].time[0], tableSize, MPI_DOUBLE, & all[i*tableSize*run.size], tableSize, MPI_DOUBLE, 0, testComm);
      }else{
        MPI_Gather(& summary_table[i].rate[0], tableSize, MPI_DOUBLE, & all[i*tableSize*run.size], tableSize, MPI_DOUBLE, 0, testComm);
      }
    }

    if (rank == 0) {

        fprintf(out_logfile, "\nSUMMARY %s: (of %d iterations)\n", print_time ? "time": "rate", iterations);
        fprintf(out_logfile,
            "   Operation                      Max            Min           Mean        Std Dev\n");
        fprintf(out_logfile,
            "   ---------                      ---            ---           ----        -------\n");
        fflush(out_logfile);

        /* if files only access, skip entries 0-3 (the dir tests) */
        if (run.files_only && !run.dirs_only) {
            start = 4;
        } else {
            start = 0;
        }

        /* if directories only access, skip entries 4-7 (the file tests) */
        if (run.dirs_only && !run.files_only) {
            stop = 4;
        } else {
            stop = 8;
        }

        /* special case: if no directory or file tests, skip all */
        if (!run.dirs_only && !run.files_only) {
            start = stop = 0;
        }

        for (i = start; i < stop; i++) {
                min = max = all[i];
                for (k=0; k < run.size; k++) {
                    for (j = 0; j < iterations; j++) {
                        curr = all[(k*tableSize*iterations)
                                   + (j*tableSize) + i];
                        if (min > curr) {
                            min = curr;
                        }
                        if (max < curr) {
                            max =  curr;
                        }
                        sum += curr;
                    }
                }
                mean = sum / (iterations * run.size);
                for (k=0; k<run.size; k++) {
                    for (j = 0; j < iterations; j++) {
                        var += pow((mean -  all[(k*tableSize*iterations)
                                                + (j*tableSize) + i]), 2);
                    }
                }
                var = var / (iterations * run.size);
                sd = sqrt(var);
                switch (i) {
                case 0: strcpy(access, "Directory creation:"); break;
                case 1: strcpy(access, "Directory stat    :"); break;
                    /* case 2: strcpy(access, "Directory read    :"); break; */
                case 2: ;                                      break; /* N/A */
                case 3: strcpy(access, "Directory removal :"); break;
                case 4: strcpy(access, "File creation     :"); break;
                case 5: strcpy(access, "File stat         :"); break;
                case 6: strcpy(access, "File read         :"); break;
                case 7: strcpy(access, "File removal      :"); break;
                default: strcpy(access, "ERR");                 break;
                }
                if (i != 2) {
                    fprintf(out_logfile, "   %s ", access);
                    fprintf(out_logfile, "%14.3f ", max);
                    fprintf(out_logfile, "%14.3f ", min);
                    fprintf(out_logfile, "%14.3f ", mean);
                    fprintf(out_logfile, "%14.3f\n", sd);
                    fflush(out_logfile);
                }
                sum = var = 0;

        }

        /* calculate tree create/remove rates */
        for (i = 8; i < tableSize; i++) {
            min = max = all[i];
            for (j = 0; j < iterations; j++) {
                if(print_time){
                  curr = summary_table[j].time[i];
                }else{
                  curr = summary_table[j].rate[i];
                }

                if (min > curr) {
                    min = curr;
                }
                if (max < curr) {
                    max =  curr;
                }
                sum += curr;
            }
            mean = sum / (iterations);
            for (j = 0; j < iterations; j++) {
                if(print_time){
                  curr = summary_table[j].time[i];
                }else{
                  curr = summary_table[j].rate[i];
                }

                var += pow((mean -  curr), 2);
            }
            var = var / (iterations);
            sd = sqrt(var);
            switch (i) {
            case 8: strcpy(access, "Tree creation     :"); break;
            case 9: strcpy(access, "Tree removal      :"); break;
            default: strcpy(access, "ERR");                 break;
            }
            fprintf(out_logfile, "   %s ", access);
            fprintf(out_logfile, "%14.3f ", max);
            fprintf(out_logfile, "%14.3f ", min);
            fprintf(out_logfile, "%14.3f ", mean);
            fprintf(out_logfile, "%14.3f\n", sd);
            fflush(out_logfile);
            sum = var = 0;
        }
    }
}

/* Checks to see if the test setup is valid.  If it isn't, fail. */
void valid_tests() {

    if (((run.stone_wall_timer_seconds > 0) && (run.branch_factor > 1)) || ! run.barriers) {
        fprintf(out_logfile, "Error, stone wall timer does only work with a branch factor <= 1 and with barriers\n");
        MPI_Abort(testComm, 1);
    }

    if (!run.create_only && !run.stat_only && !run.read_only && !run.remove_only) {
        run.create_only = run.stat_only = run.read_only = run.remove_only = 1;
        if (( rank == 0 ) && ( verbose >= 1 )) {
            fprintf( out_logfile, "V-1: main: Setting create/stat/read/remove_only to True\n" );
            fflush( out_logfile );
        }
    }

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering valid_tests...\n" );
        fflush( out_logfile );
    }

    /* if run.dirs_only and run.files_only were both left unset, set both now */
    if (!run.dirs_only && !run.files_only) {
        run.dirs_only = run.files_only = 1;
    }

    /* if shared file 'S' access, no directory tests */
    if (run.shared_file) {
        run.dirs_only = 0;
    }

    /* check for no run.barriers with shifting processes for different phases.
       that is, one may not specify both -B and -N as it will introduce
       race conditions that may cause errors stat'ing or deleting after
       creates.
    */
    if (( run.barriers == 0 ) && ( run.nstride != 0 ) && ( rank == 0 )) {
        FAIL( "Possible race conditions will occur: -B not compatible with -N");
    }

    /* check for run.collective_creates incompatibilities */
    if (run.shared_file && run.collective_creates && rank == 0) {
        FAIL("-c not compatible with -S");
    }
    if (run.path_count > 1 && run.collective_creates && rank == 0) {
        FAIL("-c not compatible with multiple test directories");
    }
    if (run.collective_creates && !run.barriers) {
        FAIL("-c not compatible with -B");
    }

    if (strcasecmp(backend_name, "POSIX") != 0 && strcasecmp(backend_name, "DUMMY") != 0) {
      FAIL("-a only supported interface is POSIX (and DUMMY) right now!");
    }

    /* check for shared file incompatibilities */
    if (run.unique_dir_per_task && run.shared_file && rank == 0) {
        FAIL("-u not compatible with -S");
    }

    /* check multiple directory paths and strided option */
    if (run.path_count > 1 && run.nstride > 0) {
        FAIL("cannot have multiple directory paths with -N strides between neighbor tasks");
    }

    /* check for shared directory and multiple directories incompatibility */
    if (run.path_count > 1 && run.unique_dir_per_task != 1) {
        FAIL("shared directory mode is not compatible with multiple directory paths");
    }

    /* check if more directory paths than ranks */
    if (run.path_count > run.size) {
        FAIL("cannot have more directory paths than MPI tasks");
    }

    /* check run.depth */
    if (run.depth < 0) {
            FAIL("depth must be greater than or equal to zero");
    }
    /* check run.branch_factor */
    if (run.branch_factor < 1 && run.depth > 0) {
            FAIL("branch factor must be greater than or equal to zero");
    }
    /* check for valid number of run.items */
    if ((run.items > 0) && (run.items_per_dir > 0)) {
       if(run.unique_dir_per_task){
         FAIL("only specify the number of items or the number of items per directory");
       }else if( run.items % run.items_per_dir != 0){
         FAIL("items must be a multiple of items per directory");
       }else if( run.stone_wall_timer_seconds != 0){
         FAIL("items + items_per_dir can only be set without stonewalling");
       }
    }

}

void show_file_system_size(char *file_system) {
    char          real_path[MAX_PATHLEN];
    char          file_system_unit_str[MAX_PATHLEN] = "GiB";
    char          inode_unit_str[MAX_PATHLEN]       = "Mi";
    int64_t       file_system_unit_val          = 1024 * 1024 * 1024;
    int64_t       inode_unit_val                = 1024 * 1024;
    int64_t       total_file_system_size,
        free_file_system_size,
        total_inodes,
        free_inodes;
    double        total_file_system_size_hr,
        used_file_system_percentage,
        used_inode_percentage;
    ior_aiori_statfs_t stat_buf;
    int ret;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering show_file_system_size...\n" );
        fflush( out_logfile );
    }

    ret = backend->statfs (file_system, &stat_buf, &param);
    if (0 != ret) {
        FAIL("unable to stat file system");
    }

    total_file_system_size = stat_buf.f_blocks * stat_buf.f_bsize;
    free_file_system_size = stat_buf.f_bfree * stat_buf.f_bsize;

    used_file_system_percentage = (1 - ((double)free_file_system_size
                                        / (double)total_file_system_size)) * 100;
    total_file_system_size_hr = (double)total_file_system_size
        / (double)file_system_unit_val;
    if (total_file_system_size_hr > 1024) {
        total_file_system_size_hr = total_file_system_size_hr / 1024;
        strcpy(file_system_unit_str, "TiB");
    }

    /* inodes */
    total_inodes = stat_buf.f_files;
    free_inodes = stat_buf.f_ffree;

    used_inode_percentage = (1 - ((double)free_inodes/(double)total_inodes))
        * 100;

    if (realpath(file_system, real_path) == NULL) {
        FAIL("unable to use realpath()");
    }


    /* show results */
    fprintf(out_logfile, "Path: %s\n", real_path);
    fprintf(out_logfile, "FS: %.1f %s   Used FS: %2.1f%%   ",
            total_file_system_size_hr, file_system_unit_str,
            used_file_system_percentage);
    fprintf(out_logfile, "Inodes: %.1f %s   Used Inodes: %2.1f%%\n",
            (double)total_inodes / (double)inode_unit_val,
            inode_unit_str, used_inode_percentage);
    fflush(out_logfile);

    return;
}

void display_freespace(char *testdirpath)
{
    char dirpath[MAX_PATHLEN] = {0};
    int  i;
    int  directoryFound   = 0;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering display_freespace...\n" );
        fflush( out_logfile );
    }

    if (verbose >= 3 && rank == 0) {
        fprintf(out_logfile,  "V-3: testdirpath is \"%s\"\n", testdirpath );
        fflush( out_logfile );
    }

    strcpy(dirpath, testdirpath);

    /* get directory for outfile */
    i = strlen(dirpath);
    while (i-- > 0) {
        if (dirpath[i] == '/') {
            dirpath[i] = '\0';
            directoryFound = 1;
            break;
        }
    }

    /* if no directory/, use '.' */
    if (directoryFound == 0) {
        strcpy(dirpath, ".");
    }

    if (verbose >= 3 && rank == 0) {
        fprintf(out_logfile,  "V-3: Before show_file_system_size, dirpath is \"%s\"\n", dirpath );
        fflush( out_logfile );
    }

    show_file_system_size(dirpath);

    if (verbose >= 3 && rank == 0) {
        fprintf(out_logfile,  "V-3: After show_file_system_size, dirpath is \"%s\"\n", dirpath );
        fflush( out_logfile );
    }

    return;
}

void create_remove_directory_tree(int create,
                                  int currDepth, char* path, int dirNum, rank_progress_t * progress) {

    unsigned i;
    char dir[MAX_PATHLEN];


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering create_remove_directory_tree, currDepth = %d...\n", currDepth );
        fflush( out_logfile );
    }

    if (currDepth == 0) {
        sprintf(dir, "%s/%s.%d/", path, run.base_tree_name, dirNum);

        if (create) {
            if (rank == 0 && verbose >= 2) {
                fprintf(out_logfile, "V-2: Making directory \"%s\"\n", dir);
                fflush(out_logfile);
            }

            if (-1 == backend->mkdir (dir, DIRMODE, &param)) {
                fprintf(out_logfile, "error could not create directory \"%s\"\n", dir);
            }
        }

        create_remove_directory_tree(create, ++currDepth, dir, ++dirNum, progress);

        if (!create) {
            if (rank == 0 && verbose >= 2) {
                fprintf(out_logfile, "V-2: Remove directory \"%s\"\n", dir);
                fflush(out_logfile);
            }

            if (-1 == backend->rmdir(dir, &param)) {
                FAIL("Unable to remove directory");
            }
        }
    } else if (currDepth <= run.depth) {

        char temp_path[MAX_PATHLEN];
        strcpy(temp_path, path);
        int currDir = dirNum;

        for (i=0; i<run.branch_factor; i++) {
            sprintf(dir, "%s.%d/", run.base_tree_name, currDir);
            strcat(temp_path, dir);

            if (create) {
                if (rank == 0 && verbose >= 2) {
                    fprintf(out_logfile, "V-2: Making directory \"%s\"\n", temp_path);
                    fflush(out_logfile);
                }

                if (-1 == backend->mkdir(temp_path, DIRMODE, &param)) {
                    FAIL("Unable to create directory");
                }
            }

            create_remove_directory_tree(create, ++currDepth,
                                         temp_path, (run.branch_factor*currDir)+1, progress);
            currDepth--;

            if (!create) {
                if (rank == 0 && verbose >= 2) {
                    fprintf(out_logfile, "V-2: Remove directory \"%s\"\n", temp_path);
                    fflush(out_logfile);
                }

                if (-1 == backend->rmdir(temp_path, &param)) {
                    FAIL("Unable to remove directory");
                }
            }

            strcpy(temp_path, path);
            currDir++;
        }
    }
}

static void mdtest_iteration(int i, int j, MPI_Group testgroup, mdtest_results_t * summary_table){
  rank_progress_t progress_o;
  memset(& progress_o, 0 , sizeof(progress_o));
  progress_o.start_time = GetTimeStamp();
  progress_o.stone_wall_timer_seconds = run.stone_wall_timer_seconds;
  progress_o.items_per_dir = run.items_per_dir;
  rank_progress_t * progress = & progress_o;

  /* start and end times of directory tree create/remove */
  double startCreate, endCreate;
  int k;

  if (rank == 0 && verbose >= 1) {
      fprintf(out_logfile, "V-1: main: * iteration %d *\n", j+1);
      fflush(out_logfile);
  }

  for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
    prep_testdir(j, dir_iter);

    if (verbose >= 2 && rank == 0) {
        fprintf(out_logfile,  "V-2: main (for j loop): making testdir, \"%s\"\n", run.testdir );
        fflush( out_logfile );
    }
    if ((rank < run.path_count) && backend->access(run.testdir, F_OK, &param) != 0) {
        if (backend->mkdir(run.testdir, DIRMODE, &param) != 0) {
            FAIL("Unable to create test directory");
        }
    }
  }

  if (run.create_only) {
      /* create hierarchical directory structure */
      MPI_Barrier(testComm);

      startCreate = GetTimeStamp();
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);

        if (run.unique_dir_per_task) {
            if (run.collective_creates && (rank == 0)) {
                /*
                 * This is inside two loops, one of which already uses "i" and the other uses "j".
                 * I don't know how this ever worked. I'm changing this loop to use "k".
                 */
                for (k=0; k<run.size; k++) {
                    sprintf(run.base_tree_name, "mdtest_tree.%d", k);

                    if (verbose >= 3 && rank == 0) {
                        fprintf(out_logfile,
                            "V-3: main (create hierarchical directory loop-collective): Calling create_remove_directory_tree with \"%s\"\n",
                            run.testdir );
                        fflush( out_logfile );
                    }

                    /*
                     * Let's pass in the path to the directory we most recently made so that we can use
                     * full paths in the other calls.
                     */
                    create_remove_directory_tree(1, 0, run.testdir, 0, progress);
                    if(CHECK_STONE_WALL(progress)){
                      run.size = k;
                      break;
                    }
                }
            } else if (!run.collective_creates) {
                if (verbose >= 3 && rank == 0) {
                    fprintf(out_logfile,
                        "V-3: main (create hierarchical directory loop-!collective_creates): Calling create_remove_directory_tree with \"%s\"\n",
                        run.testdir );
                    fflush( out_logfile );
                }

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(1, 0, run.testdir, 0, progress);
            }
        } else {
            if (rank == 0) {
                if (verbose >= 3 && rank == 0) {
                    fprintf(out_logfile,
                        "V-3: main (create hierarchical directory loop-!unque_dir_per_task): Calling create_remove_directory_tree with \"%s\"\n",
                        run.testdir );
                    fflush( out_logfile );
                }

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(1, 0 , run.testdir, 0, progress);
            }
        }
      }
      MPI_Barrier(testComm);
      endCreate = GetTimeStamp();
      summary_table->rate[8] =
          run.num_dirs_in_tree / (endCreate - startCreate);
      summary_table->time[8] = (endCreate - startCreate);
      summary_table->items[8] = run.num_dirs_in_tree;
      summary_table->stonewall_last_item[8] = run.num_dirs_in_tree;
      if (verbose >= 1 && rank == 0) {
          fprintf(out_logfile, "V-1: main:   Tree creation     : %14.3f sec, %14.3f ops/sec\n",
                 (endCreate - startCreate), summary_table->rate[8]);
          fflush(out_logfile);
      }
  }
  sprintf(run.unique_mk_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_chdir_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_stat_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_read_dir, "%s.0", run.base_tree_name);
  sprintf(run.unique_rm_dir, "%s.0", run.base_tree_name);
  run.unique_rm_uni_dir[0] = 0;

  if (!run.unique_dir_per_task) {
      if (verbose >= 3 && rank == 0) {
          fprintf(out_logfile,  "V-3: main: Using unique_mk_dir, \"%s\"\n", run.unique_mk_dir );
          fflush( out_logfile );
      }
  }

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

      if (verbose >= 3 && rank == 0) {
          fprintf(out_logfile,  "V-3: main: Copied unique_mk_dir, \"%s\", to topdir\n", run.unique_mk_dir );
          fflush( out_logfile );
      }

      if (run.dirs_only && !run.shared_file) {
          if (run.pre_delay) {
              DelaySecs(run.pre_delay);
          }
          directory_test(j, i, run.unique_mk_dir, progress);
      }
      if (run.files_only) {
          if (run.pre_delay) {
              DelaySecs(run.pre_delay);
          }
          file_test(j, i, run.unique_mk_dir, progress);
      }
  }

  /* remove directory structure */
  if (!run.unique_dir_per_task) {
      if (verbose >= 3 && rank == 0) {
          fprintf(out_logfile,  "V-3: main: Using testdir, \"%s\"\n", run.testdir );
          fflush( out_logfile );
      }
  }

  MPI_Barrier(testComm);
  if (run.remove_only) {
      progress->items_start = 0;
      startCreate = GetTimeStamp();
      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);
        if (run.unique_dir_per_task) {
            if (run.collective_creates && (rank == 0)) {
                /*
                 * This is inside two loops, one of which already uses "i" and the other uses "j".
                 * I don't know how this ever worked. I'm changing this loop to use "k".
                 */
                for (k=0; k<run.size; k++) {
                    sprintf(run.base_tree_name, "mdtest_tree.%d", k);

                    if (verbose >= 3 && rank == 0) {
                        fprintf(out_logfile,
                            "V-3: main (remove hierarchical directory loop-collective): Calling create_remove_directory_tree with \"%s\"\n",
                            run.testdir );
                        fflush( out_logfile );
                    }

                    /*
                     * Let's pass in the path to the directory we most recently made so that we can use
                     * full paths in the other calls.
                     */
                    create_remove_directory_tree(0, 0, run.testdir, 0, progress);
                    if(CHECK_STONE_WALL(progress)){
                      run.size = k;
                      break;
                    }
                }
            } else if (!run.collective_creates) {
                if (verbose >= 3 && rank == 0) {
                    fprintf(out_logfile,
                        "V-3: main (remove hierarchical directory loop-!collective): Calling create_remove_directory_tree with \"%s\"\n",
                        run.testdir );
                    fflush( out_logfile );
                }

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(0, 0, run.testdir, 0, progress);
            }
        } else {
            if (rank == 0) {
                if (verbose >= 3 && rank == 0) {
                    fprintf(out_logfile,
                        "V-3: main (remove hierarchical directory loop-!unique_dir_per_task): Calling create_remove_directory_tree with \"%s\"\n",
                        run.testdir );
                    fflush( out_logfile );
                }

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(0, 0 , run.testdir, 0, progress);
            }
        }
      }

      MPI_Barrier(testComm);
      endCreate = GetTimeStamp();
      summary_table->rate[9] = run.num_dirs_in_tree / (endCreate - startCreate);
      summary_table->time[9] = endCreate - startCreate;
      summary_table->items[9] = run.num_dirs_in_tree;
      summary_table->stonewall_last_item[8] = run.num_dirs_in_tree;
      if (verbose >= 1 && rank == 0) {
          fprintf(out_logfile, "V-1: main   Tree removal      : %14.3f sec, %14.3f ops/sec\n",
                 (endCreate - startCreate), summary_table->rate[9]);
          fflush(out_logfile);
      }

      if (( rank == 0 ) && ( verbose >=2 )) {
          fprintf( out_logfile, "V-2: main (at end of for j loop): Removing testdir of \"%s\"\n", run.testdir );
          fflush( out_logfile );
      }

      for (int dir_iter = 0; dir_iter < run.directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);
        if ((rank < run.path_count) && backend->access(run.testdir, F_OK, &param) == 0) {
            //if (( rank == 0 ) && access(run.testdir, F_OK) == 0) {
            if (backend->rmdir(run.testdir, &param) == -1) {
                FAIL("unable to remove directory");
            }
        }
      }
  } else {
      summary_table->rate[9] = 0;
  }
}

void mdtest_init_args(){
   backend_name = "POSIX";
   run.barriers = 1;
   run.branch_factor = 1;
   run.stoneWallingStatusFile = NULL;
   run.create_only = 0;
   run.stat_only = 0;
   run.read_only = 0;
   run.remove_only = 0;
   run.leaf_only = 0;
   run.depth = 0;
   run.num_dirs_in_tree = 0;
   run.items_per_dir = 0;
   run.random_seed = 0;
   run.print_time = 0;
   run.print_rate_and_time = 0;
   run.shared_file = 0;
   run.files_only = 0;
   run.dirs_only = 0;
   run.pre_delay = 0;
   run.unique_dir_per_task = 0;
   run.time_unique_dir_overhead = 0;
   run.items = 0;
   run.num_dirs_in_tree_calc = 0;
   run.collective_creates = 0;
   run.write_bytes = 0;
   run.stone_wall_timer_seconds = 0;
   run.read_bytes = 0;
   run.sync_file = 0;
   run.path_count = 0;
   run.nstride = 0;
}


mdtest_results_t * mdtest_run(int argc, char **argv, MPI_Comm world_com, FILE * world_out) {
    testComm = world_com;
    out_logfile = world_out;
    mpi_comm_world = world_com;

    init_clock();

    mdtest_init_args();
    int i, j;
    int nodeCount;
    MPI_Group worldgroup, testgroup;
    struct {
        int first;
        int last;
        int stride;
    } range = {0, 0, 1};
    int first = 1;
    int last = 0;
    int stride = 1;
    int iterations = 1;

    verbose = 0;
    int no_barriers = 0;
    char * path = "./out";
    int randomize = 0;
    char APIs[1024];
    char APIs_legacy[1024];
    aiori_supported_apis(APIs, APIs_legacy);
    char apiStr[1024];
    sprintf(apiStr, "API for I/O [%s]", APIs);

    option_help options [] = {
      {'a', NULL,        apiStr, OPTION_OPTIONAL_ARGUMENT, 's', & backend_name},
      {'b', NULL,        "branching factor of hierarchical directory structure", OPTION_OPTIONAL_ARGUMENT, 'd', & run.branch_factor},
      {'d', NULL,        "the directory in which the tests will run", OPTION_OPTIONAL_ARGUMENT, 's', & path},
      {'B', NULL,        "no barriers between phases", OPTION_OPTIONAL_ARGUMENT, 'd', & no_barriers},
      {'C', NULL,        "only create files/dirs", OPTION_FLAG, 'd', & run.create_only},
      {'T', NULL,        "only stat files/dirs", OPTION_FLAG, 'd', & run.stat_only},
      {'E', NULL,        "only read files/dir", OPTION_FLAG, 'd', & run.read_only},
      {'r', NULL,        "only remove files or directories left behind by previous runs", OPTION_FLAG, 'd', & run.remove_only},
      {'D', NULL,        "perform test on directories only (no files)", OPTION_FLAG, 'd', & run.dirs_only},
      {'e', NULL,        "bytes to read from each file", OPTION_OPTIONAL_ARGUMENT, 'l', & run.read_bytes},
      {'f', NULL,        "first number of tasks on which the test will run", OPTION_OPTIONAL_ARGUMENT, 'd', & first},
      {'F', NULL,        "perform test on files only (no directories)", OPTION_FLAG, 'd', & run.files_only},
      {'i', NULL,        "number of iterations the test will run", OPTION_OPTIONAL_ARGUMENT, 'd', & iterations},
      {'I', NULL,        "number of items per directory in tree", OPTION_OPTIONAL_ARGUMENT, 'l', & run.items_per_dir},
      {'l', NULL,        "last number of tasks on which the test will run", OPTION_OPTIONAL_ARGUMENT, 'd', & last},
      {'L', NULL,        "files only at leaf level of tree", OPTION_FLAG, 'd', & run.leaf_only},
      {'n', NULL,        "every process will creat/stat/read/remove # directories and files", OPTION_OPTIONAL_ARGUMENT, 'l', & run.items},
      {'N', NULL,        "stride # between neighbor tasks for file/dir operation (local=0)", OPTION_OPTIONAL_ARGUMENT, 'd', & run.nstride},
      {'p', NULL,        "pre-iteration delay (in seconds)", OPTION_OPTIONAL_ARGUMENT, 'd', & run.pre_delay},
      {'P', NULL,        "print rate AND time", OPTION_FLAG, 'd', & run.print_rate_and_time},
      {'R', NULL,        "random access to files (only for stat)", OPTION_FLAG, 'd', & randomize},
      {0, "random-seed", "random seed for -R", OPTION_OPTIONAL_ARGUMENT, 'd', & run.random_seed},
      {'s', NULL,        "stride between the number of tasks for each test", OPTION_OPTIONAL_ARGUMENT, 'd', & stride},
      {'S', NULL,        "shared file access (file only, no directories)", OPTION_FLAG, 'd', & run.shared_file},
      {'c', NULL,        "collective creates: task 0 does all creates", OPTION_FLAG, 'd', & run.collective_creates},
      {'t', NULL,        "time unique working directory overhead", OPTION_FLAG, 'd', & run.time_unique_dir_overhead},
      {'u', NULL,        "unique working directory for each task", OPTION_FLAG, 'd', & run.unique_dir_per_task},
      {'v', NULL,        "verbosity (each instance of option increments by one)", OPTION_FLAG, 'd', & verbose},
      {'V', NULL,        "verbosity value", OPTION_OPTIONAL_ARGUMENT, 'd', & verbose},
      {'w', NULL,        "bytes to write to each file after it is created", OPTION_OPTIONAL_ARGUMENT, 'l', & run.write_bytes},
      {'W', NULL,        "number in seconds; stonewall timer, write as many seconds and ensure all processes did the same number of operations (currently only stops during create phase)", OPTION_OPTIONAL_ARGUMENT, 'd', & run.stone_wall_timer_seconds},
      {'x', NULL,        "stoneWallingStatusFile; contains the number of iterations of the creation phase, can be used to split phases across runs", OPTION_OPTIONAL_ARGUMENT, 's', & run.stoneWallingStatusFile},
      {'y', NULL,        "sync file after writing", OPTION_FLAG, 'd', & run.sync_file},
      {'z', NULL,        "depth of hierarchical directory structure", OPTION_OPTIONAL_ARGUMENT, 'd', & run.depth},
      {'Z', NULL,        "print time instead of rate", OPTION_FLAG, 'd', & run.print_time},
      LAST_OPTION
    };
    airoi_parse_options(argc, argv, options);

    backend = aiori_select(backend_name);
    if (NULL == backend) {
        FAIL("Could not find suitable backend to use");
    }

    MPI_Comm_rank(testComm, &rank);
    MPI_Comm_size(testComm, &run.size);

    run.pid = getpid();
    run.uid = getuid();

    nodeCount = run.size / CountTasksPerNode(testComm);

    if (rank == 0) {
        fprintf(out_logfile, "-- started at %s --\n\n", PrintTimestamp());
        fprintf(out_logfile, "mdtest-%s was launched with %d total task(s) on %d node(s)\n",
               RELEASE_VERS, run.size, nodeCount);
        fflush(out_logfile);
    }

    if (rank == 0) {
        fprintf(out_logfile, "Command line used: %s", argv[0]);
        for (i = 1; i < argc; i++) {
            fprintf(out_logfile, " \"%s\"", argv[i]);
        }
        fprintf(out_logfile, "\n");
        fflush(out_logfile);
    }

    /* adjust special variables */
    run.barriers = ! no_barriers;
    if (path != NULL){
      parse_dirpath(path);
    }
    if( randomize > 0 ){
      if (run.random_seed == 0) {
        /* Ensure all procs have the same random number */
          run.random_seed = time(NULL);
          MPI_Barrier(testComm);
          MPI_Bcast(&run.random_seed, 1, MPI_INT, 0, testComm);
      }
      run.random_seed += rank;
    }
    if ((run.items > 0) && (run.items_per_dir > 0) && (! run.unique_dir_per_task)) {
      run.directory_loops = run.items / run.items_per_dir;
    }else{
      run.directory_loops = 1;
    }
    valid_tests();

    if (( rank == 0 ) && ( verbose >= 1 )) {
        // option_print_current(options);
        fprintf (out_logfile, "api                     : %s\n", backend_name);
        fprintf( out_logfile, "barriers                : %s\n", ( run.barriers ? "True" : "False" ));
        fprintf( out_logfile, "collective_creates      : %s\n", ( run.collective_creates ? "True" : "False" ));
        fprintf( out_logfile, "create_only             : %s\n", ( run.create_only ? "True" : "False" ));
        fprintf( out_logfile, "dirpath(s):\n" );
        for ( i = 0; i < run.path_count; i++ ) {
            fprintf( out_logfile, "\t%s\n", run.filenames[i] );
        }
        fprintf( out_logfile, "dirs_only               : %s\n", ( run.dirs_only ? "True" : "False" ));
        fprintf( out_logfile, "read_bytes              : "LLU"\n", run.read_bytes );
        fprintf( out_logfile, "read_only               : %s\n", ( run.read_only ? "True" : "False" ));
        fprintf( out_logfile, "first                   : %d\n", first );
        fprintf( out_logfile, "files_only              : %s\n", ( run.files_only ? "True" : "False" ));
        fprintf( out_logfile, "iterations              : %d\n", iterations );
        fprintf( out_logfile, "items_per_dir           : "LLU"\n", run.items_per_dir );
        fprintf( out_logfile, "last                    : %d\n", last );
        fprintf( out_logfile, "leaf_only               : %s\n", ( run.leaf_only ? "True" : "False" ));
        fprintf( out_logfile, "items                   : "LLU"\n", run.items );
        fprintf( out_logfile, "run.nstride                 : %d\n", run.nstride );
        fprintf( out_logfile, "pre_delay               : %d\n", run.pre_delay );
        fprintf( out_logfile, "remove_only             : %s\n", ( run.leaf_only ? "True" : "False" ));
        fprintf( out_logfile, "random_seed             : %d\n", run.random_seed );
        fprintf( out_logfile, "stride                  : %d\n", stride );
        fprintf( out_logfile, "shared_file             : %s\n", ( run.shared_file ? "True" : "False" ));
        fprintf( out_logfile, "time_unique_dir_overhead: %s\n", ( run.time_unique_dir_overhead ? "True" : "False" ));
        fprintf( out_logfile, "stone_wall_timer_seconds: %d\n", run.stone_wall_timer_seconds);
        fprintf( out_logfile, "stat_only               : %s\n", ( run.stat_only ? "True" : "False" ));
        fprintf( out_logfile, "unique_dir_per_task     : %s\n", ( run.unique_dir_per_task ? "True" : "False" ));
        fprintf( out_logfile, "write_bytes             : "LLU"\n", run.write_bytes );
        fprintf( out_logfile, "sync_file               : %s\n", ( run.sync_file ? "True" : "False" ));
        fprintf( out_logfile, "depth                   : %d\n", run.depth );
        fflush( out_logfile );
    }

    /* setup total number of run.items and number of run.items per dir */
    if (run.depth <= 0) {
        run.num_dirs_in_tree = 1;
    } else {
        if (run.branch_factor < 1) {
            run.num_dirs_in_tree = 1;
        } else if (run.branch_factor == 1) {
            run.num_dirs_in_tree = run.depth + 1;
        } else {
            run.num_dirs_in_tree = (pow(run.branch_factor, run.depth+1) - 1) / (run.branch_factor - 1);
        }
    }
    if (run.items_per_dir > 0) {
        if(run.items == 0){
          run.items = run.items_per_dir * run.num_dirs_in_tree;
        }else{
          run.num_dirs_in_tree_calc = run.num_dirs_in_tree;
        }
    } else {
        if (run.leaf_only) {
            if (run.branch_factor <= 1) {
                run.items_per_dir = run.items;
            } else {
                run.items_per_dir = (uint64_t) (run.items / pow(run.branch_factor, run.depth));
                run.items = run.items_per_dir * (uint64_t) pow(run.branch_factor, run.depth);
            }
        } else {
            run.items_per_dir = run.items / run.num_dirs_in_tree;
            run.items = run.items_per_dir * run.num_dirs_in_tree;
        }
    }

    /* initialize run.rand_array */
    if (run.random_seed > 0) {
        srand(run.random_seed);

        uint64_t s;

        run.rand_array = (uint64_t *) malloc( run.items * sizeof(*run.rand_array));

        for (s=0; s < run.items; s++) {
            run.rand_array[s] = s;
        }

        /* shuffle list randomly */
        uint64_t n = run.items;
        while (n>1) {
            n--;

            /*
             * Generate a random number in the range 0 .. n
             *
             * rand() returns a number from 0 .. RAND_MAX. Divide that
             * by RAND_MAX and you get a floating point number in the
             * range 0 .. 1. Multiply that by n and you get a number in
             * the range 0 .. n.
             */
            uint64_t k = ( uint64_t ) ((( double )rand() / ( double )RAND_MAX ) * ( double )n );

            /*
             * Now move the nth element to the kth (randomly chosen)
             * element, and the kth element to the nth element.
             */

            uint64_t tmp = run.rand_array[k];
            run.rand_array[k] = run.rand_array[n];
            run.rand_array[n] = tmp;
        }
    }

    /* allocate and initialize write buffer with # */
    if (run.write_bytes > 0) {
        run.write_buffer = (char *)malloc(run.write_bytes);
        if (run.write_buffer == NULL) {
            FAIL("out of memory");
        }
        memset(run.write_buffer, 0x23, run.write_bytes);
    }

    /* setup directory path to work in */
    if (run.path_count == 0) { /* special case where no directory path provided with '-d' option */
        char *ret = getcwd(run.testdirpath, MAX_PATHLEN);
        if (ret == NULL) {
            FAIL("Unable to get current working directory");
        }
        run.path_count = 1;
    } else {
        strcpy(run.testdirpath, run.filenames[rank%run.path_count]);
    }

    /*   if directory does not exist, create it */
    if ((rank < run.path_count) && backend->access(run.testdirpath, F_OK, &param) != 0) {
        if (backend->mkdir(run.testdirpath, DIRMODE, &param) != 0) {
            FAIL("Unable to create test directory path");
        }
    }

    /* display disk usage */
    if (verbose >= 3 && rank == 0) {
        fprintf(out_logfile,  "V-3: main (before display_freespace): testdirpath is \"%s\"\n", run.testdirpath );
        fflush( out_logfile );
    }

    if (rank == 0) display_freespace(run.testdirpath);

    if (verbose >= 3 && rank == 0) {
        fprintf(out_logfile,  "V-3: main (after display_freespace): testdirpath is \"%s\"\n", run.testdirpath );
        fflush( out_logfile );
    }

    if (rank == 0) {
        if (run.random_seed > 0) {
            fprintf(out_logfile, "random seed: %d\n", run.random_seed);
        }
    }

    if (gethostname(run.hostname, MAX_PATHLEN) == -1) {
        perror("gethostname");
        MPI_Abort(testComm, 2);
    }

    if (last == 0) {
        first = run.size;
        last = run.size;
    }

    /* setup summary table for recording results */
    summary_table = (mdtest_results_t *) malloc(iterations * sizeof(mdtest_results_t));
    memset(summary_table, 0, sizeof(mdtest_results_t));
    for(int i=0; i < iterations; i++){
      for(int j=0; j < MDTEST_LAST_NUM; j++){
        summary_table[i].rate[j] = 0.0;
        summary_table[i].time[j] = 0.0;
      }
    }

    if (summary_table == NULL) {
        FAIL("out of memory");
    }

    if (run.unique_dir_per_task) {
        sprintf(run.base_tree_name, "mdtest_tree.%d", rank);
    } else {
        sprintf(run.base_tree_name, "mdtest_tree");
    }

    /* default use shared directory */
    strcpy(run.mk_name, "mdtest.shared.");
    strcpy(run.stat_name, "mdtest.shared.");
    strcpy(run.read_name, "mdtest.shared.");
    strcpy(run.rm_name, "mdtest.shared.");

    MPI_Comm_group(testComm, &worldgroup);

    /* Run the tests */
    for (i = first; i <= last && i <= run.size; i += stride) {
        range.last = i - 1;
        MPI_Group_range_incl(worldgroup, 1, (void *)&range, &testgroup);
        MPI_Comm_create(testComm, testgroup, &testComm);
        if (rank == 0) {
            uint64_t items_all = i * run.items; // TODO this looks fishy
            if(run.num_dirs_in_tree_calc){
              items_all *= run.num_dirs_in_tree_calc;
            }
            if (run.files_only && run.dirs_only) {
                fprintf(out_logfile, "\n%d tasks, "LLU" files/directories\n", i, items_all);
            } else if (run.files_only) {
                if (!run.shared_file) {
                    fprintf(out_logfile, "\n%d tasks, "LLU" files\n", i, items_all);
                }
                else {
                    fprintf(out_logfile, "\n%d tasks, 1 file\n", i);
                }
            } else if (run.dirs_only) {
                fprintf(out_logfile, "\n%d tasks, "LLU" directories\n", i, items_all);
            }
        }
        if (rank == 0 && verbose >= 1) {
            fprintf(out_logfile, "\n");
            fprintf(out_logfile, "   Operation               Duration              Rate\n");
            fprintf(out_logfile, "   ---------               --------              ----\n");
        }

        //mdtest_generate_filenames();

        for (j = 0; j < iterations; j++) {
            // keep track of the current status for stonewalling
            mdtest_iteration(i, j, testgroup, & summary_table[j]);
        }
        if (run.print_rate_and_time){
          summarize_results(iterations, 0);
          summarize_results(iterations, 1);
        }else{
          summarize_results(iterations, run.print_time);
        }
        if (i == 1 && stride > 1) {
            i = 0;
        }
    }

    if (rank == 0) {
        fprintf(out_logfile, "\n-- finished at %s --\n", PrintTimestamp());
        fflush(out_logfile);
    }

    if (run.random_seed > 0) {
        free(run.rand_array);
    }
    return summary_table;
}
