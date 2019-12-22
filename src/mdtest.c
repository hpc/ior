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
 *
 * CVS info:
 *   $RCSfile: mdtest.c,v $
 *   $Revision: 1.4 $
 *   $Date: 2013/11/27 17:05:31 $
 *   $Author: brettkettering $
 */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>

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

#ifdef HAVE_LUSTRE_LUSTREAPI
#include <lustre/lustreapi.h>
#endif /* HAVE_LUSTRE_LUSTREAPI */

#define FILEMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
#define DIRMODE S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH
#define RELEASE_VERS META_VERSION
#define TEST_DIR "test-dir"
#define ITEM_COUNT 25000

#define LLU "%lu"

static int size;
static uint64_t *rand_array;
static char testdir[MAX_PATHLEN];
static char testdirpath[MAX_PATHLEN];
static char base_tree_name[MAX_PATHLEN];
static char **filenames;
static char hostname[MAX_PATHLEN];
static char mk_name[MAX_PATHLEN];
static char stat_name[MAX_PATHLEN];
static char read_name[MAX_PATHLEN];
static char rm_name[MAX_PATHLEN];
static char unique_mk_dir[MAX_PATHLEN];
static char unique_chdir_dir[MAX_PATHLEN];
static char unique_stat_dir[MAX_PATHLEN];
static char unique_read_dir[MAX_PATHLEN];
static char unique_rm_dir[MAX_PATHLEN];
static char unique_rm_uni_dir[MAX_PATHLEN];
static char *write_buffer;
static char *read_buffer;
static char *verify_read_buffer;
static char *stoneWallingStatusFile;


static int barriers;
static int create_only;
static int stat_only;
static int read_only;
static int verify_read;
static int verification_error;
static int remove_only;
static int leaf_only;
static unsigned branch_factor;
static int depth;

/*
 * This is likely a small value, but it's sometimes computed by
 * branch_factor^(depth+1), so we'll make it a larger variable,
 * just in case.
 */
static uint64_t num_dirs_in_tree;
/*
 * As we start moving towards Exascale, we could have billions
 * of files in a directory. Make room for that possibility with
 * a larger variable.
 */
static uint64_t items;
static uint64_t items_per_dir;
static uint64_t num_dirs_in_tree_calc; /* this is a workaround until the overal code is refactored */
static int directory_loops;
static int print_time;
static int print_rate_and_time;
static int random_seed;
static int shared_file;
static int files_only;
static int dirs_only;
static int pre_delay;
static int unique_dir_per_task;
static int time_unique_dir_overhead;
static int throttle;
static int collective_creates;
static size_t write_bytes;
static int stone_wall_timer_seconds;
static size_t read_bytes;
static int sync_file;
static int call_sync;
static int path_count;
static int nstride; /* neighbor stride */
static int make_node = 0;
#ifdef HAVE_LUSTRE_LUSTREAPI
static int global_dir_layout;
#endif /* HAVE_LUSTRE_LUSTREAPI */

static mdtest_results_t * summary_table;
static pid_t pid;
static uid_t uid;

/* Use the POSIX backend by default */
static const ior_aiori_t *backend;

static IOR_param_t param;

/* This structure describes the processing status for stonewalling */
typedef struct{
  double start_time;

  int stone_wall_timer_seconds;

  uint64_t items_start;
  uint64_t items_done;

  uint64_t items_per_dir;
} rank_progress_t;

#define CHECK_STONE_WALL(p) (((p)->stone_wall_timer_seconds != 0) && ((GetTimeStamp() - (p)->start_time) > (p)->stone_wall_timer_seconds))

/* for making/removing unique directory && stating/deleting subdirectory */
enum {MK_UNI_DIR, STAT_SUB_DIR, READ_SUB_DIR, RM_SUB_DIR, RM_UNI_DIR};

/* a helper function for passing debug and verbose messages.
   use the MACRO as it will insert __LINE__ for you.
   Pass the verbose level for root to print, then the verbose level for anyone to print.
   Pass -1 to suppress the print for anyone.
   Then do the standard printf stuff.  This function adds the newline for you.
*/
#define VERBOSE(root,any,...) VerboseMessage(root,any,__LINE__,__VA_ARGS__)
void VerboseMessage (int root_level, int any_level, int line, char * format, ...) {
    if ((rank==0 && verbose >= root_level) || (any_level > 0 && verbose >= any_level)) {
        char buffer[1024];
        va_list args;
        va_start (args, format);
        vsnprintf (buffer, 1024, format, args);
        va_end (args);
        if (root_level == 0 && any_level == -1) {
            /* No header when it is just the standard output */
            fprintf( out_logfile, "%s\n", buffer );
        } else {
            /* add a header when the verbose is greater than 0 */
            fprintf( out_logfile, "V-%d: Rank %3d Line %5d %s\n", root_level, rank, line, buffer );
        }
        fflush(out_logfile);
    }
}

void generate_memory_pattern(char * buffer, size_t bytes){
  for(int i=0; i < bytes; i++){
    buffer[i] = i + 1;
  }
}

void offset_timers(double * t, int tcount) {
    double toffset;
    int i;


    VERBOSE(1,-1,"V-1: Entering offset_timers..." );

    toffset = GetTimeStamp() - t[tcount];
    for (i = 0; i < tcount+1; i++) {
        t[i] += toffset;
    }
}

void parse_dirpath(char *dirpath_arg) {
    char * tmp, * token;
    char delimiter_string[3] = { '@', '\n', '\0' };
    int i = 0;


    VERBOSE(1,-1, "Entering parse_dirpath on %s...", dirpath_arg );

    tmp = dirpath_arg;

    if (* tmp != '\0') path_count++;
    while (* tmp != '\0') {
        if (* tmp == '@') {
            path_count++;
        }
        tmp++;
    }
    // prevent changes to the original dirpath_arg
    dirpath_arg = strdup(dirpath_arg);
    filenames = (char **)malloc(path_count * sizeof(char **));
    if (filenames == NULL || dirpath_arg == NULL) {
        FAIL("out of memory");
    }

    token = strtok(dirpath_arg, delimiter_string);
    while (token != NULL) {
        filenames[i] = token;
        token = strtok(NULL, delimiter_string);
        i++;
    }
}

static void prep_testdir(int j, int dir_iter){
  int pos = sprintf(testdir, "%s", testdirpath);
  if ( testdir[strlen( testdir ) - 1] != '/' ) {
      pos += sprintf(& testdir[pos], "/");
  }
  pos += sprintf(& testdir[pos], "%s", TEST_DIR);
  pos += sprintf(& testdir[pos], ".%d-%d", j, dir_iter);
}

static void phase_end(){
  if (call_sync){
    if(! backend->sync){
      FAIL("Error, backend does not provide the sync method, but your requested to use sync.");
    }
    backend->sync(& param);
  }

  if (barriers) {
    MPI_Barrier(testComm);
  }
}

/*
 * This function copies the unique directory name for a given option to
 * the "to" parameter. Some memory must be allocated to the "to" parameter.
 */

void unique_dir_access(int opt, char *to) {
    if (opt == MK_UNI_DIR) {
        MPI_Barrier(testComm);
        sprintf( to, "%s/%s", testdir, unique_chdir_dir );
    } else if (opt == STAT_SUB_DIR) {
        sprintf( to, "%s/%s", testdir, unique_stat_dir );
    } else if (opt == READ_SUB_DIR) {
        sprintf( to, "%s/%s", testdir, unique_read_dir );
    } else if (opt == RM_SUB_DIR) {
        sprintf( to, "%s/%s", testdir, unique_rm_dir );
    } else if (opt == RM_UNI_DIR) {
        sprintf( to, "%s/%s", testdir, unique_rm_uni_dir );
    }
    VERBOSE(1,-1,"Entering unique_dir_access, set it to %s", to );
}

static void create_remove_dirs (const char *path, bool create, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];
    const char *operation = create ? "create" : "remove";

    if ( (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {
        VERBOSE(3,5,"dir: "LLU"", operation, itemNum);
    }

    //create dirs
    sprintf(curr_item, "%s/dir.%s%" PRIu64, path, create ? mk_name : rm_name, itemNum);
    VERBOSE(3,5,"create_remove_items_helper (dirs %s): curr_item is '%s'", operation, curr_item);

    if (create) {
        if (backend->mkdir(curr_item, DIRMODE, &param) == -1) {
            FAIL("unable to create directory %s", curr_item);
        }
    } else {
        if (backend->rmdir(curr_item, &param) == -1) {
            FAIL("unable to remove directory %s", curr_item);
        }
    }
}

static void remove_file (const char *path, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];

    if ( (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {
        VERBOSE(3,5,"remove file: "LLU"\n", itemNum);
    }

    //remove files
    sprintf(curr_item, "%s/file.%s"LLU"", path, rm_name, itemNum);
    VERBOSE(3,5,"create_remove_items_helper (non-dirs remove): curr_item is '%s'", curr_item);
    if (!(shared_file && rank != 0)) {
        backend->delete (curr_item, &param);
    }
}

static void create_file (const char *path, uint64_t itemNum) {
    char curr_item[MAX_PATHLEN];
    void *aiori_fh = NULL;

    if ( (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {
        VERBOSE(3,5,"create file: "LLU"", itemNum);
    }

    //create files
    sprintf(curr_item, "%s/file.%s"LLU"", path, mk_name, itemNum);
    VERBOSE(3,5,"create_remove_items_helper (non-dirs create): curr_item is '%s'", curr_item);

    param.openFlags = IOR_WRONLY;

    if (make_node) {
        int ret;
        VERBOSE(3,5,"create_remove_items_helper : mknod..." );

        ret = backend->mknod (curr_item);
        if (ret != 0)
            FAIL("unable to mknode file %s", curr_item);

        return;
    } else if (collective_creates) {
        VERBOSE(3,5,"create_remove_items_helper (collective): open..." );

        aiori_fh = backend->open (curr_item, &param);
        if (NULL == aiori_fh)
            FAIL("unable to open file %s", curr_item);

        /*
         * !collective_creates
         */
    } else {
        param.openFlags |= IOR_CREAT;
        param.filePerProc = !shared_file;
        param.mode = FILEMODE;
        VERBOSE(3,5,"create_remove_items_helper (non-collective, shared): open..." );

        aiori_fh = backend->create (curr_item, &param);
        if (NULL == aiori_fh)
            FAIL("unable to create file %s", curr_item);
    }

    if (write_bytes > 0) {
        VERBOSE(3,5,"create_remove_items_helper: write..." );

        /*
         * According to Bill Loewe, writes are only done one time, so they are always at
         * offset 0 (zero).
         */
        param.offset = 0;
        param.fsyncPerWrite = sync_file;
        if ( write_bytes != (size_t) backend->xfer (WRITE, aiori_fh, (IOR_size_t *) write_buffer, write_bytes, &param)) {
            FAIL("unable to write file %s", curr_item);
        }
    }

    VERBOSE(3,5,"create_remove_items_helper: close..." );
    backend->close (aiori_fh, &param);
}

/* helper for creating/removing items */
void create_remove_items_helper(const int dirs, const int create, const char *path,
                                uint64_t itemNum, rank_progress_t * progress) {

    VERBOSE(1,-1,"Entering create_remove_items_helper on %s", path );

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

    VERBOSE(1,-1,"Entering collective_helper on %s", path );
    for (uint64_t i = progress->items_start ; i < progress->items_per_dir ; ++i) {
        if (dirs) {
            create_remove_dirs (path, create, itemNum + i);
            continue;
        }

        sprintf(curr_item, "%s/file.%s"LLU"", path, create ? mk_name : rm_name, itemNum+i);
        VERBOSE(3,5,"create file: %s", curr_item);

        if (create) {
            void *aiori_fh;

            //create files
            param.openFlags = IOR_WRONLY | IOR_CREAT;
	    param.mode = FILEMODE;
            aiori_fh = backend->create (curr_item, &param);
            if (NULL == aiori_fh) {
                FAIL("unable to create file %s", curr_item);
            }

            backend->close (aiori_fh, &param);
        } else if (!(shared_file && rank != 0)) {
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


    VERBOSE(1,-1,"Entering create_remove_items on %s, currDepth = %d...", path, currDepth );


    memset(dir, 0, MAX_PATHLEN);
    strcpy(temp_path, path);

    VERBOSE(3,5,"create_remove_items (start): temp_path is '%s'", temp_path );

    if (currDepth == 0) {
        /* create items at this depth */
        if (!leaf_only || (depth == 0 && leaf_only)) {
            if (collective) {
                collective_helper(dirs, create, temp_path, 0, progress);
            } else {
                create_remove_items_helper(dirs, create, temp_path, 0, progress);
            }
        }

        if (depth > 0) {
            create_remove_items(++currDepth, dirs, create,
                                collective, temp_path, ++dirNum, progress);
        }

    } else if (currDepth <= depth) {
        /* iterate through the branches */
        for (i=0; i<branch_factor; i++) {

            /* determine the current branch and append it to the path */
            sprintf(dir, "%s.%llu/", base_tree_name, currDir);
            strcat(temp_path, "/");
            strcat(temp_path, dir);

            VERBOSE(3,5,"create_remove_items (for loop): temp_path is '%s'", temp_path );

            /* create the items in this branch */
            if (!leaf_only || (leaf_only && currDepth == depth)) {
                if (collective) {
                    collective_helper(dirs, create, temp_path, currDir*items_per_dir, progress);
                } else {
                    create_remove_items_helper(dirs, create, temp_path, currDir*items_per_dir, progress);
                }
            }

            /* make the recursive call for the next level below this branch */
            create_remove_items(
                ++currDepth,
                dirs,
                create,
                collective,
                temp_path,
                ( currDir * ( unsigned long long )branch_factor ) + 1,
                progress
               );
            currDepth--;

            /* reset the path */
            strcpy(temp_path, path);
            currDir++;
        }
    }
}

/* stats all of the items created as specified by the input parameters */
void mdtest_stat(const int random, const int dirs, const long dir_iter, const char *path, rank_progress_t * progress) {
    struct stat buf;
    uint64_t parent_dir, item_num = 0;
    char item[MAX_PATHLEN], temp[MAX_PATHLEN];

    VERBOSE(1,-1,"Entering mdtest_stat on %s", path );

    uint64_t stop_items = items;

    if( directory_loops != 1 ){
      stop_items = items_per_dir;
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
            item_num = rand_array[i];
        } else {
            item_num = i;
        }

        /* make adjustments if in leaf only mode*/
        if (leaf_only) {
            item_num += items_per_dir *
                (num_dirs_in_tree - (uint64_t) pow( branch_factor, depth ));
        }

        /* create name of file/dir to stat */
        if (dirs) {
            if ( (i % ITEM_COUNT == 0) && (i != 0)) {
                VERBOSE(3,5,"stat dir: "LLU"", i);
            }
            sprintf(item, "dir.%s"LLU"", stat_name, item_num);
        } else {
            if ( (i % ITEM_COUNT == 0) && (i != 0)) {
                VERBOSE(3,5,"stat file: "LLU"", i);
            }
            sprintf(item, "file.%s"LLU"", stat_name, item_num);
        }

        /* determine the path to the file/dir to be stat'ed */
        parent_dir = item_num / items_per_dir;

        if (parent_dir > 0) {        //item is not in tree's root directory

            /* prepend parent directory to item's path */
            sprintf(temp, "%s."LLU"/%s", base_tree_name, parent_dir, item);
            strcpy(item, temp);

            //still not at the tree's root dir
            while (parent_dir > branch_factor) {
                parent_dir = (uint64_t) ((parent_dir-1) / branch_factor);
                sprintf(temp, "%s."LLU"/%s", base_tree_name, parent_dir, item);
                strcpy(item, temp);
            }
        }

        /* Now get item to have the full path */
        sprintf( temp, "%s/%s", path, item );
        strcpy( item, temp );

        /* below temp used to be hiername */
        VERBOSE(3,5,"mdtest_stat %4s: %s", (dirs ? "dir" : "file"), item);
        if (-1 == backend->stat (item, &buf, &param)) {
            FAIL("unable to stat %s %s", dirs ? "directory" : "file", item);
        }
    }
}


/* reads all of the items created as specified by the input parameters */
void mdtest_read(int random, int dirs, const long dir_iter, char *path) {
    uint64_t parent_dir, item_num = 0;
    char item[MAX_PATHLEN], temp[MAX_PATHLEN];
    void *aiori_fh;

    VERBOSE(1,-1,"Entering mdtest_read on %s", path );

    /* allocate read buffer */
    if (read_bytes > 0) {
        read_buffer = (char *)malloc(read_bytes);
        if (read_buffer == NULL) {
            FAIL("out of memory");
        }

        if (verify_read > 0) {
            verify_read_buffer = (char *)malloc(read_bytes);
            if (verify_read_buffer == NULL) {
                FAIL("out of memory");
            }
            generate_memory_pattern(verify_read_buffer, read_bytes);
        }
    }

    uint64_t stop_items = items;

    if( directory_loops != 1 ){
      stop_items = items_per_dir;
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
            item_num = rand_array[i];
        } else {
            item_num = i;
        }

        /* make adjustments if in leaf only mode*/
        if (leaf_only) {
            item_num += items_per_dir *
                (num_dirs_in_tree - (uint64_t) pow (branch_factor, depth));
        }

        /* create name of file to read */
        if (!dirs) {
            if ((i%ITEM_COUNT == 0) && (i != 0)) {
                VERBOSE(3,5,"read file: "LLU"", i);
            }
            sprintf(item, "file.%s"LLU"", read_name, item_num);
        }

        /* determine the path to the file/dir to be read'ed */
        parent_dir = item_num / items_per_dir;

        if (parent_dir > 0) {        //item is not in tree's root directory

            /* prepend parent directory to item's path */
            sprintf(temp, "%s."LLU"/%s", base_tree_name, parent_dir, item);
            strcpy(item, temp);

            /* still not at the tree's root dir */
            while (parent_dir > branch_factor) {
                parent_dir = (unsigned long long) ((parent_dir-1) / branch_factor);
                sprintf(temp, "%s."LLU"/%s", base_tree_name, parent_dir, item);
                strcpy(item, temp);
            }
        }

        /* Now get item to have the full path */
        sprintf( temp, "%s/%s", path, item );
        strcpy( item, temp );

        /* below temp used to be hiername */
        VERBOSE(3,5,"mdtest_read file: %s", item);

        /* open file for reading */
        param.openFlags = O_RDONLY;
        aiori_fh = backend->open (item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to open file %s", item);
        }

        /* read file */
        if (read_bytes > 0) {
            read_buffer[0] = 42; /* use a random value to ensure that the read_buffer is now different from the expected buffer and read isn't sometimes NOOP */
            if (read_bytes != (size_t) backend->xfer (READ, aiori_fh, (IOR_size_t *) read_buffer, read_bytes, &param)) {
                FAIL("unable to read file %s", item);
            }
            if(verify_read){
              if (memcmp(read_buffer, verify_read_buffer, read_bytes) != 0){
                VERBOSE(2, -1, "Error verifying %s", item);
                verification_error++;
              }
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

    VERBOSE(1,-1,"Entering collective_create_remove on %s", path );

    /* rank 0 does all of the creates and removes for all of the ranks */
    for (int i = 0 ; i < ntasks ; ++i) {
        memset(temp, 0, MAX_PATHLEN);

        strcpy(temp, testdir);
        strcat(temp, "/");

        /* set the base tree name appropriately */
        if (unique_dir_per_task) {
            sprintf(base_tree_name, "mdtest_tree.%d", i);
        } else {
            sprintf(base_tree_name, "mdtest_tree");
        }

        /* Setup to do I/O to the appropriate test dir */
        strcat(temp, base_tree_name);
        strcat(temp, ".0");

        /* set all item names appropriately */
        if (!shared_file) {
            sprintf(mk_name, "mdtest.%d.", (i+(0*nstride))%ntasks);
            sprintf(stat_name, "mdtest.%d.", (i+(1*nstride))%ntasks);
            sprintf(read_name, "mdtest.%d.", (i+(2*nstride))%ntasks);
            sprintf(rm_name, "mdtest.%d.", (i+(3*nstride))%ntasks);
        }
        if (unique_dir_per_task) {
            VERBOSE(3,5,"i %d nstride %d ntasks %d", i, nstride, ntasks);
            sprintf(unique_mk_dir, "%s/mdtest_tree.%d.0", testdir,
                    (i+(0*nstride))%ntasks);
            sprintf(unique_chdir_dir, "%s/mdtest_tree.%d.0", testdir,
                    (i+(1*nstride))%ntasks);
            sprintf(unique_stat_dir, "%s/mdtest_tree.%d.0", testdir,
                    (i+(2*nstride))%ntasks);
            sprintf(unique_read_dir, "%s/mdtest_tree.%d.0", testdir,
                    (i+(3*nstride))%ntasks);
            sprintf(unique_rm_dir, "%s/mdtest_tree.%d.0", testdir,
                    (i+(4*nstride))%ntasks);
            sprintf(unique_rm_uni_dir, "%s", testdir);
        }

        /* Now that everything is set up as it should be, do the create or remove */
        VERBOSE(3,5,"collective_create_remove (create_remove_items): temp is '%s'", temp);

        create_remove_items(0, dirs, create, 1, temp, 0, progress);
    }

    /* reset all of the item names */
    if (unique_dir_per_task) {
        sprintf(base_tree_name, "mdtest_tree.0");
    } else {
        sprintf(base_tree_name, "mdtest_tree");
    }
    if (!shared_file) {
        sprintf(mk_name, "mdtest.%d.", (0+(0*nstride))%ntasks);
        sprintf(stat_name, "mdtest.%d.", (0+(1*nstride))%ntasks);
        sprintf(read_name, "mdtest.%d.", (0+(2*nstride))%ntasks);
        sprintf(rm_name, "mdtest.%d.", (0+(3*nstride))%ntasks);
    }
    if (unique_dir_per_task) {
        sprintf(unique_mk_dir, "%s/mdtest_tree.%d.0", testdir,
                (0+(0*nstride))%ntasks);
        sprintf(unique_chdir_dir, "%s/mdtest_tree.%d.0", testdir,
                (0+(1*nstride))%ntasks);
        sprintf(unique_stat_dir, "%s/mdtest_tree.%d.0", testdir,
                (0+(2*nstride))%ntasks);
        sprintf(unique_read_dir, "%s/mdtest_tree.%d.0", testdir,
                (0+(3*nstride))%ntasks);
        sprintf(unique_rm_dir, "%s/mdtest_tree.%d.0", testdir,
                (0+(4*nstride))%ntasks);
        sprintf(unique_rm_uni_dir, "%s", testdir);
    }
}

void directory_test(const int iteration, const int ntasks, const char *path, rank_progress_t * progress) {
    int size;
    double t[5] = {0};
    char temp_path[MAX_PATHLEN];

    MPI_Comm_size(testComm, &size);

    VERBOSE(1,-1,"Entering directory_test on %s", path );

    MPI_Barrier(testComm);
    t[0] = GetTimeStamp();

    /* create phase */
    if(create_only) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 0);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,-1,"directory_test: create path is '%s'", temp_path );

        /* "touch" the files */
        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 1, ntasks, temp_path, progress);
            }
        } else {
            /* create directories */
            create_remove_items(0, 1, 1, 0, temp_path, 0, progress);
        }
      }
    }

    phase_end();
    t[1] = GetTimeStamp();

    /* stat phase */
    if (stat_only) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 1);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"stat path is '%s'", temp_path );

        /* stat directories */
        if (random_seed > 0) {
            mdtest_stat(1, 1, dir_iter, temp_path, progress);
        } else {
            mdtest_stat(0, 1, dir_iter, temp_path, progress);
        }
      }
    }
    phase_end();
    t[2] = GetTimeStamp();

    /* read phase */
    if (read_only) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(READ_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 2);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"directory_test: read path is '%s'", temp_path );

        /* read directories */
        if (random_seed > 0) {
            ;        /* N/A */
        } else {
            ;        /* N/A */
        }
      }
    }

    phase_end();
    t[3] = GetTimeStamp();

    if (remove_only) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(RM_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 3);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"directory_test: remove directories path is '%s'", temp_path );

        /* remove directories */
        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(0, 1, ntasks, temp_path, progress);
            }
        } else {
            create_remove_items(0, 1, 0, 0, temp_path, 0, progress);
        }
      }
    }

    phase_end();
    t[4] = GetTimeStamp();

    if (remove_only) {
        if (unique_dir_per_task) {
            unique_dir_access(RM_UNI_DIR, temp_path);
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"directory_test: remove unique directories path is '%s'\n", temp_path );
    }

    if (unique_dir_per_task && !time_unique_dir_overhead) {
        offset_timers(t, 4);
    }

    /* calculate times */
    if (create_only) {
        summary_table[iteration].rate[0] = items*size/(t[1] - t[0]);
        summary_table[iteration].time[0] = t[1] - t[0];
        summary_table[iteration].items[0] = items*size;
        summary_table[iteration].stonewall_last_item[0] = items;
    }
    if (stat_only) {
        summary_table[iteration].rate[1] = items*size/(t[2] - t[1]);
        summary_table[iteration].time[1] = t[2] - t[1];
        summary_table[iteration].items[1] = items*size;
        summary_table[iteration].stonewall_last_item[1] = items;
    }
    if (read_only) {
        summary_table[iteration].rate[2] = items*size/(t[3] - t[2]);
        summary_table[iteration].time[2] = t[3] - t[2];
        summary_table[iteration].items[2] = items*size;
        summary_table[iteration].stonewall_last_item[2] = items;
    }
    if (remove_only) {
        summary_table[iteration].rate[3] = items*size/(t[4] - t[3]);
        summary_table[iteration].time[3] = t[4] - t[3];
        summary_table[iteration].items[3] = items*size;
        summary_table[iteration].stonewall_last_item[3] = items;
    }

    VERBOSE(1,-1,"   Directory creation: %14.3f sec, %14.3f ops/sec", t[1] - t[0], summary_table[iteration].rate[0]);
    VERBOSE(1,-1,"   Directory stat    : %14.3f sec, %14.3f ops/sec", t[2] - t[1], summary_table[iteration].rate[1]);
    /* N/A
    VERBOSE(1,-1,"   Directory read    : %14.3f sec, %14.3f ops/sec", t[3] - t[2], summary_table[iteration].rate[2]);
    */
    VERBOSE(1,-1,"   Directory removal : %14.3f sec, %14.3f ops/sec", t[4] - t[3], summary_table[iteration].rate[3]);
}

/* Returns if the stonewall was hit */
int updateStoneWallIterations(int iteration, rank_progress_t * progress, double tstart){
  int hit = 0;
  uint64_t done = progress->items_done;
  long long unsigned max_iter = 0;

  VERBOSE(1,1,"stonewall hit with %lld items", (long long) progress->items_done );
  MPI_Allreduce(& progress->items_done, & max_iter, 1, MPI_LONG_LONG_INT, MPI_MAX, testComm);
  summary_table[iteration].stonewall_time[MDTEST_FILE_CREATE_NUM] = GetTimeStamp() - tstart;

  // continue to the maximum...
  long long min_accessed = 0;
  MPI_Reduce(& progress->items_done, & min_accessed, 1, MPI_LONG_LONG_INT, MPI_MIN, 0, testComm);
  long long sum_accessed = 0;
  MPI_Reduce(& progress->items_done, & sum_accessed, 1, MPI_LONG_LONG_INT, MPI_SUM, 0, testComm);
  summary_table[iteration].stonewall_item_sum[MDTEST_FILE_CREATE_NUM] = sum_accessed;
  summary_table[iteration].stonewall_item_min[MDTEST_FILE_CREATE_NUM] = min_accessed * size;

  if(items != (sum_accessed / size)){
    VERBOSE(0,-1, "Continue stonewall hit min: %lld max: %lld avg: %.1f \n", min_accessed, max_iter, ((double) sum_accessed) / size);
    hit = 1;
  }
  progress->items_start = done;
  progress->items_per_dir = max_iter;

  return hit;
}

void file_test(const int iteration, const int ntasks, const char *path, rank_progress_t * progress) {
    int size;
    double t[5] = {0};
    char temp_path[MAX_PATHLEN];
    MPI_Comm_size(testComm, &size);

    VERBOSE(3,5,"Entering file_test on %s", path);

    MPI_Barrier(testComm);
    t[0] = GetTimeStamp();

    /* create phase */
    if (create_only ) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);

        if (unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
            VERBOSE(5,5,"operating on %s", temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 0);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }



        VERBOSE(3,-1,"file_test: create path is '%s'", temp_path );

        /* "touch" the files */
        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 0, ntasks, temp_path, progress);
            }
            MPI_Barrier(testComm);
        }

        /* create files */
        create_remove_items(0, 0, 1, 0, temp_path, 0, progress);
        if(stone_wall_timer_seconds){
	        int hit = updateStoneWallIterations(iteration, progress, t[0]);

          if (hit){
            progress->stone_wall_timer_seconds = 0;
            VERBOSE(1,1,"stonewall: %lld of %lld", (long long) progress->items_start, (long long) progress->items_per_dir);
            create_remove_items(0, 0, 1, 0, temp_path, 0, progress);
            // now reset the values
            progress->stone_wall_timer_seconds = stone_wall_timer_seconds;
            items = progress->items_done;
          }
          if (stoneWallingStatusFile){
            StoreStoneWallingIterations(stoneWallingStatusFile, progress->items_done);
          }
          // reset stone wall timer to allow proper cleanup
          progress->stone_wall_timer_seconds = 0;
        }
      }
    }else{
      if (stoneWallingStatusFile){
        int64_t expected_items;
        /* The number of items depends on the stonewalling file */
        expected_items = ReadStoneWallingIterations(stoneWallingStatusFile);
        if(expected_items >= 0){
          items = expected_items;
          progress->items_per_dir = items;
        }
        if (rank == 0) {
          if(expected_items == -1){
            fprintf(out_logfile, "WARNING: could not read stonewall status file\n");
          }else {
            VERBOSE(1,1, "Read stonewall status; items: "LLU"\n", items);
          }
        }
      }
    }

    phase_end();
    t[1] = GetTimeStamp();

    /* stat phase */
    if (stat_only ) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 1);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"file_test: stat path is '%s'", temp_path );

        /* stat files */
        mdtest_stat((random_seed > 0 ? 1 : 0), 0, dir_iter, temp_path, progress);
      }
    }

    phase_end();
    t[2] = GetTimeStamp();

    /* read phase */
    if (read_only ) {
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(READ_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 2);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"file_test: read path is '%s'", temp_path );

        /* read files */
        if (random_seed > 0) {
                mdtest_read(1,0, dir_iter, temp_path);
        } else {
                mdtest_read(0,0, dir_iter, temp_path);
        }
      }
    }

    phase_end();
    t[3] = GetTimeStamp();

    if (remove_only) {
      progress->items_start = 0;

      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(iteration, dir_iter);
        if (unique_dir_per_task) {
            unique_dir_access(RM_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 3);
            }
        } else {
            sprintf( temp_path, "%s/%s", testdir, path );
        }

        VERBOSE(3,5,"file_test: rm directories path is '%s'", temp_path );

        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(0, 0, ntasks, temp_path, progress);
            }
        } else {
            VERBOSE(3,5,"gonna create %s", temp_path);
            create_remove_items(0, 0, 0, 0, temp_path, 0, progress);
        }
      }
    }

    phase_end();
    t[4] = GetTimeStamp();
    if (remove_only) {
        if (unique_dir_per_task) {
            unique_dir_access(RM_UNI_DIR, temp_path);
        } else {
            strcpy( temp_path, path );
        }

        VERBOSE(3,5,"file_test: rm unique directories path is '%s'", temp_path );
    }

    if (unique_dir_per_task && !time_unique_dir_overhead) {
        offset_timers(t, 4);
    }

    if(num_dirs_in_tree_calc){ /* this is temporary fix needed when using -n and -i together */
      items *= num_dirs_in_tree_calc;
    }

    /* calculate times */
    if (create_only) {
        summary_table[iteration].rate[4] = items*size/(t[1] - t[0]);
        summary_table[iteration].time[4] = t[1] - t[0];
        summary_table[iteration].items[4] = items*size;
        summary_table[iteration].stonewall_last_item[4] = items;
    }
    if (stat_only) {
        summary_table[iteration].rate[5] = items*size/(t[2] - t[1]);
        summary_table[iteration].time[5] = t[2] - t[1];
        summary_table[iteration].items[5] = items*size;
        summary_table[iteration].stonewall_last_item[5] = items;
    }
    if (read_only) {
        summary_table[iteration].rate[6] = items*size/(t[3] - t[2]);
        summary_table[iteration].time[6] = t[3] - t[2];
        summary_table[iteration].items[6] = items*size;
        summary_table[iteration].stonewall_last_item[6] = items;
    }
    if (remove_only) {
        summary_table[iteration].rate[7] = items*size/(t[4] - t[3]);
        summary_table[iteration].time[7] = t[4] - t[3];
        summary_table[iteration].items[7] = items*size;
        summary_table[iteration].stonewall_last_item[7] = items;
    }

    VERBOSE(1,-1,"  File creation     : %14.3f sec, %14.3f ops/sec", t[1] - t[0], summary_table[iteration].rate[4]);
    if(summary_table[iteration].stonewall_time[MDTEST_FILE_CREATE_NUM]){
      VERBOSE(1,-1,"  File creation (stonewall): %14.3f sec, %14.3f ops/sec", summary_table[iteration].stonewall_time[MDTEST_FILE_CREATE_NUM], summary_table[iteration].stonewall_item_sum[MDTEST_FILE_CREATE_NUM]);
    }
    VERBOSE(1,-1,"  File stat         : %14.3f sec, %14.3f ops/sec", t[2] - t[1], summary_table[iteration].rate[5]);
    VERBOSE(1,-1,"  File read         : %14.3f sec, %14.3f ops/sec", t[3] - t[2], summary_table[iteration].rate[6]);
    VERBOSE(1,-1,"  File removal      : %14.3f sec, %14.3f ops/sec", t[4] - t[3], summary_table[iteration].rate[7]);
}

void summarize_results(int iterations, int print_time) {
    char access[MAX_PATHLEN];
    int i, j, k;
    int start, stop, tableSize = MDTEST_LAST_NUM;
    double min, max, mean, sd, sum = 0, var = 0, curr = 0;

    double all[iterations * size * tableSize];


    VERBOSE(1,-1,"Entering summarize_results..." );

    MPI_Barrier(testComm);
    for(int i=0; i < iterations; i++){
      if(print_time){
        MPI_Gather(& summary_table[i].time[0], tableSize, MPI_DOUBLE, & all[i*tableSize*size], tableSize, MPI_DOUBLE, 0, testComm);
      }else{
        MPI_Gather(& summary_table[i].rate[0], tableSize, MPI_DOUBLE, & all[i*tableSize*size], tableSize, MPI_DOUBLE, 0, testComm);
      }
    }

    if (rank != 0) {
      return;
    }

    VERBOSE(0,-1,"\nSUMMARY %s: (of %d iterations)", print_time ? "time": "rate", iterations);
    VERBOSE(0,-1,"   Operation                      Max            Min           Mean        Std Dev");
    VERBOSE(0,-1,"   ---------                      ---            ---           ----        -------");

    /* if files only access, skip entries 0-3 (the dir tests) */
    if (files_only && !dirs_only) {
        start = 4;
    } else {
        start = 0;
    }

    /* if directories only access, skip entries 4-7 (the file tests) */
    if (dirs_only && !files_only) {
        stop = 4;
    } else {
        stop = 8;
    }

    /* special case: if no directory or file tests, skip all */
    if (!dirs_only && !files_only) {
        start = stop = 0;
    }

    for (i = start; i < stop; i++) {
            min = max = all[i];
            for (k=0; k < size; k++) {
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
            mean = sum / (iterations * size);
            for (k=0; k<size; k++) {
                for (j = 0; j < iterations; j++) {
                    var += pow((mean -  all[(k*tableSize*iterations)
                                            + (j*tableSize) + i]), 2);
                }
            }
            var = var / (iterations * size);
            sd = sqrt(var);
            switch (i) {
            case 0: strcpy(access, "Directory creation        :"); break;
            case 1: strcpy(access, "Directory stat            :"); break;
                /* case 2: strcpy(access, "Directory read    :"); break; */
            case 2: ;                                      break; /* N/A */
            case 3: strcpy(access, "Directory removal         :"); break;
            case 4: strcpy(access, "File creation             :"); break;
            case 5: strcpy(access, "File stat                 :"); break;
            case 6: strcpy(access, "File read                 :"); break;
            case 7: strcpy(access, "File removal              :"); break;
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

    // TODO generalize once more stonewall timers are supported
    double stonewall_time = 0;
    uint64_t stonewall_items = 0;
    for(int i=0; i < iterations; i++){
      if(summary_table[i].stonewall_time[MDTEST_FILE_CREATE_NUM]){
        stonewall_time += summary_table[i].stonewall_time[MDTEST_FILE_CREATE_NUM];
        stonewall_items += summary_table[i].stonewall_item_sum[MDTEST_FILE_CREATE_NUM];
      }
    }
    if(stonewall_items != 0){
      fprintf(out_logfile, "   File create (stonewall)   : ");
      fprintf(out_logfile, "%14s %14s %14.3f %14s\n", "NA", "NA", print_time ? stonewall_time :  stonewall_items / stonewall_time, "NA");
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
        case 8: strcpy(access, "Tree creation             :"); break;
        case 9: strcpy(access, "Tree removal              :"); break;
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

/* Checks to see if the test setup is valid.  If it isn't, fail. */
void valid_tests() {

    if (((stone_wall_timer_seconds > 0) && (branch_factor > 1)) || ! barriers) {
        FAIL( "Error, stone wall timer does only work with a branch factor <= 1 (current is %d) and with barriers\n", branch_factor);
    }

    if (!create_only && !stat_only && !read_only && !remove_only) {
        create_only = stat_only = read_only = remove_only = 1;
        VERBOSE(1,-1,"main: Setting create/stat/read/remove_only to True" );
    }

    VERBOSE(1,-1,"Entering valid_tests..." );

    /* if dirs_only and files_only were both left unset, set both now */
    if (!dirs_only && !files_only) {
        dirs_only = files_only = 1;
    }

    /* if shared file 'S' access, no directory tests */
    if (shared_file) {
        dirs_only = 0;
    }

    /* check for no barriers with shifting processes for different phases.
       that is, one may not specify both -B and -N as it will introduce
       race conditions that may cause errors stat'ing or deleting after
       creates.
    */
    if (( barriers == 0 ) && ( nstride != 0 ) && ( rank == 0 )) {
        FAIL( "Possible race conditions will occur: -B not compatible with -N");
    }

    /* check for collective_creates incompatibilities */
    if (shared_file && collective_creates && rank == 0) {
        FAIL("-c not compatible with -S");
    }
    if (path_count > 1 && collective_creates && rank == 0) {
        FAIL("-c not compatible with multiple test directories");
    }
    if (collective_creates && !barriers) {
        FAIL("-c not compatible with -B");
    }

    /* check for shared file incompatibilities */
    if (unique_dir_per_task && shared_file && rank == 0) {
        FAIL("-u not compatible with -S");
    }

    /* check multiple directory paths and strided option */
    if (path_count > 1 && nstride > 0) {
        FAIL("cannot have multiple directory paths with -N strides between neighbor tasks");
    }

    /* check for shared directory and multiple directories incompatibility */
    if (path_count > 1 && unique_dir_per_task != 1) {
        FAIL("shared directory mode is not compatible with multiple directory paths");
    }

    /* check if more directory paths than ranks */
    if (path_count > size) {
        FAIL("cannot have more directory paths than MPI tasks");
    }

    /* check depth */
    if (depth < 0) {
            FAIL("depth must be greater than or equal to zero");
    }
    /* check branch_factor */
    if (branch_factor < 1 && depth > 0) {
            FAIL("branch factor must be greater than or equal to zero");
    }
    /* check for valid number of items */
    if ((items > 0) && (items_per_dir > 0)) {
       if(unique_dir_per_task){
         FAIL("only specify the number of items or the number of items per directory");
       }else if( items % items_per_dir != 0){
         FAIL("items must be a multiple of items per directory");
       }else if( stone_wall_timer_seconds != 0){
         FAIL("items + items_per_dir can only be set without stonewalling");
       }
    }
    /* check for using mknod */
    if (write_bytes > 0 && make_node) {
        FAIL("-k not compatible with -w");
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

    VERBOSE(1,-1,"Entering show_file_system_size on %s", file_system );

    ret = backend->statfs (file_system, &stat_buf, &param);
    if (0 != ret) {
        FAIL("unable to stat file system %s", file_system);
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
        FAIL("unable to use realpath() on file system %s", file_system);
    }


    /* show results */
    VERBOSE(0,-1,"Path: %s", real_path);
    VERBOSE(0,-1,"FS: %.1f %s   Used FS: %2.1f%%   Inodes: %.1f %s   Used Inodes: %2.1f%%\n",
            total_file_system_size_hr, file_system_unit_str, used_file_system_percentage,
            (double)total_inodes / (double)inode_unit_val, inode_unit_str, used_inode_percentage);

    return;
}

void display_freespace(char *testdirpath)
{
    char dirpath[MAX_PATHLEN] = {0};
    int  i;
    int  directoryFound   = 0;


    VERBOSE(3,5,"Entering display_freespace on %s...", testdirpath );

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

    if (param.api && strcasecmp(param.api, "DFS") == 0)
	    return;

    VERBOSE(3,5,"Before show_file_system_size, dirpath is '%s'", dirpath );
    show_file_system_size(dirpath);
    VERBOSE(3,5, "After show_file_system_size, dirpath is '%s'\n", dirpath );

    return;
}

void create_remove_directory_tree(int create,
                                  int currDepth, char* path, int dirNum, rank_progress_t * progress) {

    unsigned i;
    char dir[MAX_PATHLEN];


    VERBOSE(1,5,"Entering create_remove_directory_tree on %s, currDepth = %d...", path, currDepth );

    if (currDepth == 0) {
        sprintf(dir, "%s/%s.%d/", path, base_tree_name, dirNum);

        if (create) {
            VERBOSE(2,5,"Making directory '%s'", dir);
            if (-1 == backend->mkdir (dir, DIRMODE, &param)) {
                fprintf(out_logfile, "error could not create directory '%s'\n", dir);
            }
#ifdef HAVE_LUSTRE_LUSTREAPI
            /* internal node for branching, can be non-striped for children */
            if (global_dir_layout && \
                llapi_dir_set_default_lmv_stripe(dir, -1, 0,
                                                 LMV_HASH_TYPE_FNV_1A_64,
                                                 NULL) == -1) {
                FAIL("Unable to reset to global default directory layout");
            }
#endif /* HAVE_LUSTRE_LUSTREAPI */
        }

        create_remove_directory_tree(create, ++currDepth, dir, ++dirNum, progress);

        if (!create) {
            VERBOSE(2,5,"Remove directory '%s'", dir);
            if (-1 == backend->rmdir(dir, &param)) {
                FAIL("Unable to remove directory %s", dir);
            }
        }
    } else if (currDepth <= depth) {

        char temp_path[MAX_PATHLEN];
        strcpy(temp_path, path);
        int currDir = dirNum;

        for (i=0; i<branch_factor; i++) {
            sprintf(dir, "%s.%d/", base_tree_name, currDir);
            strcat(temp_path, dir);

            if (create) {
                VERBOSE(2,5,"Making directory '%s'", temp_path);
                if (-1 == backend->mkdir(temp_path, DIRMODE, &param)) {
                    FAIL("Unable to create directory %s", temp_path);
                }
            }

            create_remove_directory_tree(create, ++currDepth,
                                         temp_path, (branch_factor*currDir)+1, progress);
            currDepth--;

            if (!create) {
                VERBOSE(2,5,"Remove directory '%s'", temp_path);
                if (-1 == backend->rmdir(temp_path, &param)) {
                    FAIL("Unable to remove directory %s", temp_path);
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
  progress_o.stone_wall_timer_seconds = stone_wall_timer_seconds;
  progress_o.items_per_dir = items_per_dir;
  rank_progress_t * progress = & progress_o;

  /* start and end times of directory tree create/remove */
  double startCreate, endCreate;
  int k;

  VERBOSE(1,-1,"main: * iteration %d *", j+1);

  for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
    prep_testdir(j, dir_iter);

    VERBOSE(2,5,"main (for j loop): making testdir, '%s'", testdir );
    if ((rank < path_count) && backend->access(testdir, F_OK, &param) != 0) {
        if (backend->mkdir(testdir, DIRMODE, &param) != 0) {
            FAIL("Unable to create test directory %s", testdir);
        }
#ifdef HAVE_LUSTRE_LUSTREAPI
        /* internal node for branching, can be non-striped for children */
        if (global_dir_layout && unique_dir_per_task && llapi_dir_set_default_lmv_stripe(testdir, -1, 0, LMV_HASH_TYPE_FNV_1A_64, NULL) == -1) {
            FAIL("Unable to reset to global default directory layout");
        }
#endif /* HAVE_LUSTRE_LUSTREAPI */
    }
  }

  if (create_only) {
      /* create hierarchical directory structure */
      MPI_Barrier(testComm);

      startCreate = GetTimeStamp();
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);

        if (unique_dir_per_task) {
            if (collective_creates && (rank == 0)) {
                /*
                 * This is inside two loops, one of which already uses "i" and the other uses "j".
                 * I don't know how this ever worked. I'm changing this loop to use "k".
                 */
                for (k=0; k<size; k++) {
                    sprintf(base_tree_name, "mdtest_tree.%d", k);

                    VERBOSE(3,5,"main (create hierarchical directory loop-collective): Calling create_remove_directory_tree with '%s'", testdir );
                    /*
                     * Let's pass in the path to the directory we most recently made so that we can use
                     * full paths in the other calls.
                     */
                    create_remove_directory_tree(1, 0, testdir, 0, progress);
                    if(CHECK_STONE_WALL(progress)){
                      size = k;
                      break;
                    }
                }
            } else if (!collective_creates) {
                VERBOSE(3,5,"main (create hierarchical directory loop-!collective_creates): Calling create_remove_directory_tree with '%s'", testdir );
                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(1, 0, testdir, 0, progress);
            }
        } else {
            if (rank == 0) {
                VERBOSE(3,5,"main (create hierarchical directory loop-!unque_dir_per_task): Calling create_remove_directory_tree with '%s'", testdir );

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(1, 0 , testdir, 0, progress);
            }
        }
      }
      MPI_Barrier(testComm);
      endCreate = GetTimeStamp();
      summary_table->rate[8] =
          num_dirs_in_tree / (endCreate - startCreate);
      summary_table->time[8] = (endCreate - startCreate);
      summary_table->items[8] = num_dirs_in_tree;
      summary_table->stonewall_last_item[8] = num_dirs_in_tree;
      VERBOSE(1,-1,"V-1: main:   Tree creation     : %14.3f sec, %14.3f ops/sec", (endCreate - startCreate), summary_table->rate[8]);
  }
  sprintf(unique_mk_dir, "%s.0", base_tree_name);
  sprintf(unique_chdir_dir, "%s.0", base_tree_name);
  sprintf(unique_stat_dir, "%s.0", base_tree_name);
  sprintf(unique_read_dir, "%s.0", base_tree_name);
  sprintf(unique_rm_dir, "%s.0", base_tree_name);
  unique_rm_uni_dir[0] = 0;

  if (!unique_dir_per_task) {
    VERBOSE(3,-1,"V-3: main: Using unique_mk_dir, '%s'", unique_mk_dir );
  }

  if (rank < i) {
      if (!shared_file) {
          sprintf(mk_name, "mdtest.%d.", (rank+(0*nstride))%i);
          sprintf(stat_name, "mdtest.%d.", (rank+(1*nstride))%i);
          sprintf(read_name, "mdtest.%d.", (rank+(2*nstride))%i);
          sprintf(rm_name, "mdtest.%d.", (rank+(3*nstride))%i);
      }
      if (unique_dir_per_task) {
          VERBOSE(3,5,"i %d nstride %d", i, nstride);
          sprintf(unique_mk_dir, "mdtest_tree.%d.0",  (rank+(0*nstride))%i);
          sprintf(unique_chdir_dir, "mdtest_tree.%d.0", (rank+(1*nstride))%i);
          sprintf(unique_stat_dir, "mdtest_tree.%d.0", (rank+(2*nstride))%i);
          sprintf(unique_read_dir, "mdtest_tree.%d.0", (rank+(3*nstride))%i);
          sprintf(unique_rm_dir, "mdtest_tree.%d.0", (rank+(4*nstride))%i);
          unique_rm_uni_dir[0] = 0;
          VERBOSE(5,5,"mk_dir %s chdir %s stat_dir %s read_dir %s rm_dir %s\n", unique_mk_dir,unique_chdir_dir,unique_stat_dir,unique_read_dir,unique_rm_dir);
      }

      VERBOSE(3,-1,"V-3: main: Copied unique_mk_dir, '%s', to topdir", unique_mk_dir );

      if (dirs_only && !shared_file) {
          if (pre_delay) {
              DelaySecs(pre_delay);
          }
          directory_test(j, i, unique_mk_dir, progress);
      }
      if (files_only) {
          if (pre_delay) {
              DelaySecs(pre_delay);
          }
          VERBOSE(3,5,"will file_test on %s", unique_mk_dir);
          file_test(j, i, unique_mk_dir, progress);
      }
  }

  /* remove directory structure */
  if (!unique_dir_per_task) {
      VERBOSE(3,-1,"main: Using testdir, '%s'", testdir );
  }

  MPI_Barrier(testComm);
  if (remove_only) {
      progress->items_start = 0;
      startCreate = GetTimeStamp();
      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);
        if (unique_dir_per_task) {
            if (collective_creates && (rank == 0)) {
                /*
                 * This is inside two loops, one of which already uses "i" and the other uses "j".
                 * I don't know how this ever worked. I'm changing this loop to use "k".
                 */
                for (k=0; k<size; k++) {
                    sprintf(base_tree_name, "mdtest_tree.%d", k);

                    VERBOSE(3,-1,"main (remove hierarchical directory loop-collective): Calling create_remove_directory_tree with '%s'", testdir );

                    /*
                     * Let's pass in the path to the directory we most recently made so that we can use
                     * full paths in the other calls.
                     */
                    create_remove_directory_tree(0, 0, testdir, 0, progress);
                    if(CHECK_STONE_WALL(progress)){
                      size = k;
                      break;
                    }
                }
            } else if (!collective_creates) {
                VERBOSE(3,-1,"main (remove hierarchical directory loop-!collective): Calling create_remove_directory_tree with '%s'", testdir );

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(0, 0, testdir, 0, progress);
            }
        } else {
            if (rank == 0) {
                VERBOSE(3,-1,"V-3: main (remove hierarchical directory loop-!unique_dir_per_task): Calling create_remove_directory_tree with '%s'", testdir );

                /*
                 * Let's pass in the path to the directory we most recently made so that we can use
                 * full paths in the other calls.
                 */
                create_remove_directory_tree(0, 0 , testdir, 0, progress);
            }
        }
      }

      MPI_Barrier(testComm);
      endCreate = GetTimeStamp();
      summary_table->rate[9] = num_dirs_in_tree / (endCreate - startCreate);
      summary_table->time[9] = endCreate - startCreate;
      summary_table->items[9] = num_dirs_in_tree;
      summary_table->stonewall_last_item[8] = num_dirs_in_tree;
      VERBOSE(1,-1,"main   Tree removal      : %14.3f sec, %14.3f ops/sec", (endCreate - startCreate), summary_table->rate[9]);
      VERBOSE(2,-1,"main (at end of for j loop): Removing testdir of '%s'\n", testdir );

      for (int dir_iter = 0; dir_iter < directory_loops; dir_iter ++){
        prep_testdir(j, dir_iter);
        if ((rank < path_count) && backend->access(testdir, F_OK, &param) == 0) {
            //if (( rank == 0 ) && access(testdir, F_OK) == 0) {
            if (backend->rmdir(testdir, &param) == -1) {
                FAIL("unable to remove directory %s", testdir);
            }
        }
      }
  } else {
      summary_table->rate[9] = 0;
  }
}

void mdtest_init_args(){
   barriers = 1;
   branch_factor = 1;
   throttle = 1;
   stoneWallingStatusFile = NULL;
   create_only = 0;
   stat_only = 0;
   read_only = 0;
   verify_read = 0;
   verification_error = 0;
   remove_only = 0;
   leaf_only = 0;
   depth = 0;
   num_dirs_in_tree = 0;
   items_per_dir = 0;
   random_seed = 0;
   print_time = 0;
   print_rate_and_time = 0;
   shared_file = 0;
   files_only = 0;
   dirs_only = 0;
   pre_delay = 0;
   unique_dir_per_task = 0;
   time_unique_dir_overhead = 0;
   items = 0;
   num_dirs_in_tree_calc = 0;
   collective_creates = 0;
   write_bytes = 0;
   stone_wall_timer_seconds = 0;
   read_bytes = 0;
   sync_file = 0;
   call_sync = 0;
   path_count = 0;
   nstride = 0;
   make_node = 0;
#ifdef HAVE_LUSTRE_LUSTREAPI
   global_dir_layout = 0;
#endif /* HAVE_LUSTRE_LUSTREAPI */
}

mdtest_results_t * mdtest_run(int argc, char **argv, MPI_Comm world_com, FILE * world_out) {
    testComm = world_com;
    out_logfile = world_out;
    mpi_comm_world = world_com;

    init_clock();

    mdtest_init_args();
    int i, j;
    int numNodes;
    int numTasksOnNode0 = 0;
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
    aiori_supported_apis(APIs, APIs_legacy, MDTEST);
    char apiStr[1024];
    sprintf(apiStr, "API for I/O [%s]", APIs);

    option_help options [] = {
      {'a', NULL,        apiStr, OPTION_OPTIONAL_ARGUMENT, 's', & param.api},
      {'b', NULL,        "branching factor of hierarchical directory structure", OPTION_OPTIONAL_ARGUMENT, 'd', & branch_factor},
      {'d', NULL,        "the directory in which the tests will run", OPTION_OPTIONAL_ARGUMENT, 's', & path},
      {'B', NULL,        "no barriers between phases", OPTION_OPTIONAL_ARGUMENT, 'd', & no_barriers},
      {'C', NULL,        "only create files/dirs", OPTION_FLAG, 'd', & create_only},
      {'T', NULL,        "only stat files/dirs", OPTION_FLAG, 'd', & stat_only},
      {'E', NULL,        "only read files/dir", OPTION_FLAG, 'd', & read_only},
      {'r', NULL,        "only remove files or directories left behind by previous runs", OPTION_FLAG, 'd', & remove_only},
      {'D', NULL,        "perform test on directories only (no files)", OPTION_FLAG, 'd', & dirs_only},
      {'e', NULL,        "bytes to read from each file", OPTION_OPTIONAL_ARGUMENT, 'l', & read_bytes},
      {'f', NULL,        "first number of tasks on which the test will run", OPTION_OPTIONAL_ARGUMENT, 'd', & first},
      {'F', NULL,        "perform test on files only (no directories)", OPTION_FLAG, 'd', & files_only},
#ifdef HAVE_LUSTRE_LUSTREAPI
      {'g', NULL,        "global default directory layout for test subdirectories (deletes inherited striping layout)", OPTION_FLAG, 'd', & global_dir_layout},
#endif /* HAVE_LUSTRE_LUSTREAPI */
      {'i', NULL,        "number of iterations the test will run", OPTION_OPTIONAL_ARGUMENT, 'd', & iterations},
      {'I', NULL,        "number of items per directory in tree", OPTION_OPTIONAL_ARGUMENT, 'l', & items_per_dir},
      {'k', NULL,        "use mknod to create file", OPTION_FLAG, 'd', & make_node},
      {'l', NULL,        "last number of tasks on which the test will run", OPTION_OPTIONAL_ARGUMENT, 'd', & last},
      {'L', NULL,        "files only at leaf level of tree", OPTION_FLAG, 'd', & leaf_only},
      {'n', NULL,        "every process will creat/stat/read/remove # directories and files", OPTION_OPTIONAL_ARGUMENT, 'l', & items},
      {'N', NULL,        "stride # between tasks for file/dir operation (local=0; set to 1 to avoid client cache)", OPTION_OPTIONAL_ARGUMENT, 'd', & nstride},
      {'p', NULL,        "pre-iteration delay (in seconds)", OPTION_OPTIONAL_ARGUMENT, 'd', & pre_delay},
      {'P', NULL,        "print rate AND time", OPTION_FLAG, 'd', & print_rate_and_time},
      {'R', NULL,        "random access to files (only for stat)", OPTION_FLAG, 'd', & randomize},
      {0, "random-seed", "random seed for -R", OPTION_OPTIONAL_ARGUMENT, 'd', & random_seed},
      {'s', NULL,        "stride between the number of tasks for each test", OPTION_OPTIONAL_ARGUMENT, 'd', & stride},
      {'S', NULL,        "shared file access (file only, no directories)", OPTION_FLAG, 'd', & shared_file},
      {'c', NULL,        "collective creates: task 0 does all creates", OPTION_FLAG, 'd', & collective_creates},
      {'t', NULL,        "time unique working directory overhead", OPTION_FLAG, 'd', & time_unique_dir_overhead},
      {'u', NULL,        "unique working directory for each task", OPTION_FLAG, 'd', & unique_dir_per_task},
      {'v', NULL,        "verbosity (each instance of option increments by one)", OPTION_FLAG, 'd', & verbose},
      {'V', NULL,        "verbosity value", OPTION_OPTIONAL_ARGUMENT, 'd', & verbose},
      {'w', NULL,        "bytes to write to each file after it is created", OPTION_OPTIONAL_ARGUMENT, 'l', & write_bytes},
      {'W', NULL,        "number in seconds; stonewall timer, write as many seconds and ensure all processes did the same number of operations (currently only stops during create phase)", OPTION_OPTIONAL_ARGUMENT, 'd', & stone_wall_timer_seconds},
      {'x', NULL,        "StoneWallingStatusFile; contains the number of iterations of the creation phase, can be used to split phases across runs", OPTION_OPTIONAL_ARGUMENT, 's', & stoneWallingStatusFile},
      {'X', "verify-read", "Verify the data read", OPTION_FLAG, 'd', & verify_read},
      {'y', NULL,        "sync file after writing", OPTION_FLAG, 'd', & sync_file},
      {'Y', NULL,        "call the sync command after each phase (included in the timing; note it causes all IO to be flushed from your node)", OPTION_FLAG, 'd', & call_sync},
      {'z', NULL,        "depth of hierarchical directory structure", OPTION_OPTIONAL_ARGUMENT, 'd', & depth},
      {'Z', NULL,        "print time instead of rate", OPTION_FLAG, 'd', & print_time},
      LAST_OPTION
    };
    options_all_t * global_options = airoi_create_all_module_options(options);
    option_parse(argc, argv, global_options);
    updateParsedOptions(& param, global_options);

    free(global_options->modules);
    free(global_options);
    backend = param.backend;

    MPI_Comm_rank(testComm, &rank);
    MPI_Comm_size(testComm, &size);

    if (backend->initialize)
	    backend->initialize();

    pid = getpid();
    uid = getuid();

    numNodes = GetNumNodes(testComm);
    numTasksOnNode0 = GetNumTasksOnNode0(testComm);

    char cmd_buffer[4096];
    strncpy(cmd_buffer, argv[0], 4096);
    for (i = 1; i < argc; i++) {
        snprintf(&cmd_buffer[strlen(cmd_buffer)], 4096-strlen(cmd_buffer), " '%s'", argv[i]);
    }

    VERBOSE(0,-1,"-- started at %s --\n", PrintTimestamp());
    VERBOSE(0,-1,"mdtest-%s was launched with %d total task(s) on %d node(s)", RELEASE_VERS, size, numNodes);
    VERBOSE(0,-1,"Command line used: %s", cmd_buffer);

    /* adjust special variables */
    barriers = ! no_barriers;
    if (path != NULL){
      parse_dirpath(path);
    }
    if( randomize > 0 ){
      if (random_seed == 0) {
        /* Ensure all procs have the same random number */
          random_seed = time(NULL);
          MPI_Barrier(testComm);
          MPI_Bcast(&random_seed, 1, MPI_INT, 0, testComm);
      }
      random_seed += rank;
    }
    if ((items > 0) && (items_per_dir > 0) && (! unique_dir_per_task)) {
      directory_loops = items / items_per_dir;
    }else{
      directory_loops = 1;
    }
    valid_tests();

    // option_print_current(options);
    VERBOSE(1,-1, "api                     : %s", param.api);
    VERBOSE(1,-1, "barriers                : %s", ( barriers ? "True" : "False" ));
    VERBOSE(1,-1, "collective_creates      : %s", ( collective_creates ? "True" : "False" ));
    VERBOSE(1,-1, "create_only             : %s", ( create_only ? "True" : "False" ));
    VERBOSE(1,-1, "dirpath(s):" );
    for ( i = 0; i < path_count; i++ ) {
        VERBOSE(1,-1, "\t%s", filenames[i] );
    }
    VERBOSE(1,-1, "dirs_only               : %s", ( dirs_only ? "True" : "False" ));
    VERBOSE(1,-1, "read_bytes              : "LLU"", read_bytes );
    VERBOSE(1,-1, "read_only               : %s", ( read_only ? "True" : "False" ));
    VERBOSE(1,-1, "first                   : %d", first );
    VERBOSE(1,-1, "files_only              : %s", ( files_only ? "True" : "False" ));
#ifdef HAVE_LUSTRE_LUSTREAPI
    VERBOSE(1,-1, "global_dir_layout       : %s", ( global_dir_layout ? "True" : "False" ));
#endif /* HAVE_LUSTRE_LUSTREAPI */
    VERBOSE(1,-1, "iterations              : %d", iterations );
    VERBOSE(1,-1, "items_per_dir           : "LLU"", items_per_dir );
    VERBOSE(1,-1, "last                    : %d", last );
    VERBOSE(1,-1, "leaf_only               : %s", ( leaf_only ? "True" : "False" ));
    VERBOSE(1,-1, "items                   : "LLU"", items );
    VERBOSE(1,-1, "nstride                 : %d", nstride );
    VERBOSE(1,-1, "pre_delay               : %d", pre_delay );
    VERBOSE(1,-1, "remove_only             : %s", ( leaf_only ? "True" : "False" ));
    VERBOSE(1,-1, "random_seed             : %d", random_seed );
    VERBOSE(1,-1, "stride                  : %d", stride );
    VERBOSE(1,-1, "shared_file             : %s", ( shared_file ? "True" : "False" ));
    VERBOSE(1,-1, "time_unique_dir_overhead: %s", ( time_unique_dir_overhead ? "True" : "False" ));
    VERBOSE(1,-1, "stone_wall_timer_seconds: %d", stone_wall_timer_seconds);
    VERBOSE(1,-1, "stat_only               : %s", ( stat_only ? "True" : "False" ));
    VERBOSE(1,-1, "unique_dir_per_task     : %s", ( unique_dir_per_task ? "True" : "False" ));
    VERBOSE(1,-1, "write_bytes             : "LLU"", write_bytes );
    VERBOSE(1,-1, "sync_file               : %s", ( sync_file ? "True" : "False" ));
    VERBOSE(1,-1, "call_sync               : %s", ( call_sync ? "True" : "False" ));
    VERBOSE(1,-1, "depth                   : %d", depth );
    VERBOSE(1,-1, "make_node               : %d", make_node );

    /* setup total number of items and number of items per dir */
    if (depth <= 0) {
        num_dirs_in_tree = 1;
    } else {
        if (branch_factor < 1) {
            num_dirs_in_tree = 1;
        } else if (branch_factor == 1) {
            num_dirs_in_tree = depth + 1;
        } else {
            num_dirs_in_tree = (pow(branch_factor, depth+1) - 1) / (branch_factor - 1);
        }
    }
    if (items_per_dir > 0) {
        if(items == 0){
          if (leaf_only) {
              items = items_per_dir * (uint64_t) pow(branch_factor, depth);
          } else {
              items = items_per_dir * num_dirs_in_tree;
          }
        }else{
          num_dirs_in_tree_calc = num_dirs_in_tree;
        }
    } else {
        if (leaf_only) {
            if (branch_factor <= 1) {
                items_per_dir = items;
            } else {
                items_per_dir = (uint64_t) (items / pow(branch_factor, depth));
                items = items_per_dir * (uint64_t) pow(branch_factor, depth);
            }
        } else {
            items_per_dir = items / num_dirs_in_tree;
            items = items_per_dir * num_dirs_in_tree;
        }
    }

    /* initialize rand_array */
    if (random_seed > 0) {
        srand(random_seed);

        uint64_t s;

        rand_array = (uint64_t *) malloc( items * sizeof(*rand_array));

        for (s=0; s < items; s++) {
            rand_array[s] = s;
        }

        /* shuffle list randomly */
        uint64_t n = items;
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

            uint64_t tmp = rand_array[k];
            rand_array[k] = rand_array[n];
            rand_array[n] = tmp;
        }
    }

    /* allocate and initialize write buffer with # */
    if (write_bytes > 0) {
        write_buffer = (char *)malloc(write_bytes);
        if (write_buffer == NULL) {
            FAIL("out of memory");
        }
        generate_memory_pattern(write_buffer, write_bytes);
    }

    /* setup directory path to work in */
    if (path_count == 0) { /* special case where no directory path provided with '-d' option */
        char *ret = getcwd(testdirpath, MAX_PATHLEN);
        if (ret == NULL) {
            FAIL("Unable to get current working directory on %s", testdirpath);
        }
        path_count = 1;
    } else {
        strcpy(testdirpath, filenames[rank%path_count]);
    }

    /*   if directory does not exist, create it */
    if ((rank < path_count) && backend->access(testdirpath, F_OK, &param) != 0) {
        if (backend->mkdir(testdirpath, DIRMODE, &param) != 0) {
            FAIL("Unable to create test directory path %s", testdirpath);
        }
    }

    /* display disk usage */
    VERBOSE(3,-1,"main (before display_freespace): testdirpath is '%s'", testdirpath );

    if (rank == 0) display_freespace(testdirpath);
    int tasksBlockMapping = QueryNodeMapping(testComm, true);

    /* set the shift to mimic IOR and shift by procs per node */
    if (nstride > 0) {
        if ( numNodes > 1 && tasksBlockMapping ) {
            /* the user set the stride presumably to get the consumer tasks on a different node than the producer tasks
               however, if the mpirun scheduler placed the tasks by-slot (in a contiguous block) then we need to adjust the shift by ppn */
            nstride *= numTasksOnNode0;
        }
        VERBOSE(0,5,"Shifting ranks by %d for each phase.", nstride);
    }

    VERBOSE(3,-1,"main (after display_freespace): testdirpath is '%s'", testdirpath );

    if (rank == 0) {
        if (random_seed > 0) {
            VERBOSE(0,-1,"random seed: %d", random_seed);
        }
    }

    if (gethostname(hostname, MAX_PATHLEN) == -1) {
        perror("gethostname");
        MPI_Abort(testComm, 2);
    }

    if (last == 0) {
        first = size;
        last = size;
    }

    /* setup summary table for recording results */
    summary_table = (mdtest_results_t *) malloc(iterations * sizeof(mdtest_results_t));
    memset(summary_table, 0, iterations * sizeof(mdtest_results_t));
    for(int i=0; i < iterations; i++){
      for(int j=0; j < MDTEST_LAST_NUM; j++){
        summary_table[i].rate[j] = 0.0;
        summary_table[i].time[j] = 0.0;
      }
    }

    if (summary_table == NULL) {
        FAIL("out of memory");
    }

    if (unique_dir_per_task) {
        sprintf(base_tree_name, "mdtest_tree.%d", rank);
    } else {
        sprintf(base_tree_name, "mdtest_tree");
    }

    /* default use shared directory */
    strcpy(mk_name, "mdtest.shared.");
    strcpy(stat_name, "mdtest.shared.");
    strcpy(read_name, "mdtest.shared.");
    strcpy(rm_name, "mdtest.shared.");

    MPI_Comm_group(testComm, &worldgroup);

    /* Run the tests */
    for (i = first; i <= last && i <= size; i += stride) {
        range.last = i - 1;
        MPI_Group_range_incl(worldgroup, 1, (void *)&range, &testgroup);
        MPI_Comm_create(testComm, testgroup, &testComm);
        if (rank == 0) {
            uint64_t items_all = i * items;
            if(num_dirs_in_tree_calc){
              items_all *= num_dirs_in_tree_calc;
            }
            if (files_only && dirs_only) {
                VERBOSE(0,-1,"%d tasks, "LLU" files/directories", i, items_all);
            } else if (files_only) {
                if (!shared_file) {
                    VERBOSE(0,-1,"%d tasks, "LLU" files", i, items_all);
                }
                else {
                    VERBOSE(0,-1,"%d tasks, 1 file", i);
                }
            } else if (dirs_only) {
                VERBOSE(0,-1,"%d tasks, "LLU" directories", i, items_all);
            }
        }
        VERBOSE(1,-1,"");
        VERBOSE(1,-1,"   Operation               Duration              Rate");
        VERBOSE(1,-1,"   ---------               --------              ----");

        for (j = 0; j < iterations; j++) {
            // keep track of the current status for stonewalling
            mdtest_iteration(i, j, testgroup, & summary_table[j]);
        }
        if (print_rate_and_time){
          summarize_results(iterations, 0);
          summarize_results(iterations, 1);
        }else{
          summarize_results(iterations, print_time);
        }
        if (i == 1 && stride > 1) {
            i = 0;
        }
    }

    if(verification_error){
      VERBOSE(0, -1, "\nERROR: verifying the data read! Take the performance values with care!\n");
    }
    VERBOSE(0,-1,"-- finished at %s --\n", PrintTimestamp());

    if (random_seed > 0) {
        free(rand_array);
    }

    if (backend->finalize)
            backend->finalize();

    return summary_table;
}
