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
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "aiori.h"
#include "ior.h"

#include <mpi.h>

#define FILEMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
#define DIRMODE S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH
/*
 * Try using the system's PATH_MAX, which is what realpath and such use.
 */
#define MAX_LEN PATH_MAX
/*
  #define MAX_LEN 1024
*/
#define RELEASE_VERS "1.9.3"
#define TEST_DIR "#test-dir"
#define ITEM_COUNT 25000

typedef struct
{
    double entry[10];
} table_t;

int rank;
static int size;
static uint64_t *rand_array;
static char testdir[MAX_LEN];
static char testdirpath[MAX_LEN];
static char top_dir[MAX_LEN];
static char base_tree_name[MAX_LEN];
static char **filenames;
static char hostname[MAX_LEN];
static char unique_dir[MAX_LEN];
static char mk_name[MAX_LEN];
static char stat_name[MAX_LEN];
static char read_name[MAX_LEN];
static char rm_name[MAX_LEN];
static char unique_mk_dir[MAX_LEN];
static char unique_chdir_dir[MAX_LEN];
static char unique_stat_dir[MAX_LEN];
static char unique_read_dir[MAX_LEN];
static char unique_rm_dir[MAX_LEN];
static char unique_rm_uni_dir[MAX_LEN];
static char *write_buffer;
static char *read_buffer;
static int barriers = 1;
static int create_only;
static int stat_only;
static int read_only;
static int remove_only;
static int leaf_only;
static int branch_factor = 1;
static int depth;

/* needed for MPI/IO backend to link correctly */
int rankOffset = 0;

/* needed for NCMPI backend to link correctly */
int numTasksWorld = 0;

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
static uint64_t items_per_dir;
static int random_seed;
static int shared_file;
static int files_only;
static int dirs_only;
static int pre_delay;
static int unique_dir_per_task;
static int time_unique_dir_overhead;
int verbose;
static int throttle = 1;
static uint64_t items;
static int collective_creates;
static size_t write_bytes;
static size_t read_bytes;
static int sync_file;
static int path_count;
static int nstride; /* neighbor stride */
MPI_Comm testComm;
static table_t * summary_table;
static pid_t pid;
static uid_t uid;

/* just use the POSIX backend for now */
static const char *backend_name = "POSIX";
static const ior_aiori_t *backend;

static IOR_param_t param;

/* for making/removing unique directory && stating/deleting subdirectory */
enum {MK_UNI_DIR, STAT_SUB_DIR, READ_SUB_DIR, RM_SUB_DIR, RM_UNI_DIR};

#ifdef __linux__
#define FAIL(msg) do {                                                  \
        fprintf(stdout, "%s: Process %d(%s): FAILED in %s, %s: %s\n",   \
                print_timestamp(), rank, hostname, __func__,            \
                msg, strerror(errno));                                  \
        fflush(stdout);                                                 \
        MPI_Abort(MPI_COMM_WORLD, 1);                                   \
    } while(0)
#else
#define FAIL(msg) do {                                                  \
        fprintf(stdout, "%s: Process %d(%s): FAILED at %d, %s: %s\n",   \
                print_timestamp(), rank, hostname, __LINE__,            \
                msg, strerror(errno));                                  \
        fflush(stdout);                                                 \
        MPI_Abort(MPI_COMM_WORLD, 1);                                   \
    } while(0)
#endif

static char *print_timestamp() {
    static char datestring[80];
    time_t cur_timestamp;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering print_timestamp...\n" );
    }

    fflush(stdout);
    cur_timestamp = time(NULL);
    strftime(datestring, 80, "%m/%d/%Y %T", localtime(&cur_timestamp));

    return datestring;
}

#if MPI_VERSION >= 3
int count_tasks_per_node(void) {
    /* modern MPI provides a simple way to get the local process count */
    MPI_Comm shared_comm;
    int rc, count;

    MPI_Comm_split_type (MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
                         &shared_comm);

    MPI_Comm_size (shared_comm, &count);

    MPI_Comm_free (&shared_comm);

    return count;
}
#else
int count_tasks_per_node(void) {
    char       localhost[MAX_LEN],
        hostname[MAX_LEN];
    int        count               = 1,
        i;
    MPI_Status status;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering count_tasks_per_node...\n" );
        fflush( stdout );
    }

    if (gethostname(localhost, MAX_LEN) != 0) {
        FAIL("gethostname()");
    }
    if (rank == 0) {
        /* MPI_receive all hostnames, and compare to local hostname */
        for (i = 0; i < size-1; i++) {
            MPI_Recv(hostname, MAX_LEN, MPI_CHAR, MPI_ANY_SOURCE,
                     MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (strcmp(hostname, localhost) == 0) {
                count++;
            }
        }
    } else {
        /* MPI_send hostname to root node */
        MPI_Send(localhost, MAX_LEN, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    }
    MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);

    return(count);
}
#endif

void delay_secs(int delay) {


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering delay_secs...\n" );
        fflush( stdout );
    }

    if (rank == 0 && delay > 0) {
        if (verbose >= 1) {
            fprintf(stdout, "delaying %d seconds . . .\n", delay);
            fflush(stdout);
        }
        sleep(delay);
    }
    MPI_Barrier(testComm);
}

void offset_timers(double * t, int tcount) {
    double toffset;
    int i;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering offset_timers...\n" );
        fflush( stdout );
    }

    toffset = MPI_Wtime() - t[tcount];
    for (i = 0; i < tcount+1; i++) {
        t[i] += toffset;
    }
}

void parse_dirpath(char *dirpath_arg) {
    char * tmp, * token;
    char delimiter_string[3] = { '@', '\n', '\0' };
    int i = 0;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering parse_dirpath...\n" );
        fflush( stdout );
    }

    tmp = dirpath_arg;

    if (* tmp != '\0') path_count++;
    while (* tmp != '\0') {
        if (* tmp == '@') {
            path_count++;
        }
        tmp++;
    }
    filenames = (char **)malloc(path_count * sizeof(char **));
    if (filenames == NULL) {
        FAIL("out of memory");
    }

    token = strtok(dirpath_arg, delimiter_string);
    while (token != NULL) {
        filenames[i] = token;
        token = strtok(NULL, delimiter_string);
        i++;
    }
}

/*
 * This function copies the unique directory name for a given option to
 * the "to" parameter. Some memory must be allocated to the "to" parameter.
 */

void unique_dir_access(int opt, char *to) {


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering unique_dir_access...\n" );
        fflush( stdout );
    }

    if (opt == MK_UNI_DIR) {
        MPI_Barrier(testComm);
        strcpy( to, unique_chdir_dir );
    } else if (opt == STAT_SUB_DIR) {
        strcpy( to, unique_stat_dir );
    } else if (opt == READ_SUB_DIR) {
        strcpy( to, unique_read_dir );
    } else if (opt == RM_SUB_DIR) {
        strcpy( to, unique_rm_dir );
    } else if (opt == RM_UNI_DIR) {
        strcpy( to, unique_rm_uni_dir );
    }
}

static void create_remove_dirs (const char *path, bool create, uint64_t itemNum) {
    char curr_item[MAX_LEN];
    const char *operation = create ? "create" : "remove";

    if (( rank == 0 )                                         &&
        ( verbose >= 3 )                                      &&
        (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {

        printf("V-3: %s dir: %llu\n", operation, itemNum);
        fflush(stdout);
    }

    //create dirs
    sprintf(curr_item, "%s/dir.%s%" PRIu64, path, create ? mk_name : rm_name, itemNum);
    if (rank == 0 && verbose >= 3) {
        printf("V-3: create_remove_items_helper (dirs %s): curr_item is \"%s\"\n", operation, curr_item);
        fflush(stdout);
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
    char curr_item[MAX_LEN];

    if (( rank == 0 )                                       &&
        ( verbose >= 3 )                                    &&
        (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {

        printf("V-3: remove file: %llu\n", itemNum);
        fflush(stdout);
    }

    //remove files
    sprintf(curr_item, "%s/file.%s%llu", path, rm_name, itemNum);
    if (rank == 0 && verbose >= 3) {
        printf("V-3: create_remove_items_helper (non-dirs remove): curr_item is \"%s\"\n", curr_item);
        fflush(stdout);
    }

    if (!(shared_file && rank != 0)) {
        backend->delete (curr_item, &param);
    }
}

static void create_file (const char *path, uint64_t itemNum) {
    char curr_item[MAX_LEN];
    void *aiori_fh;

    if (( rank == 0 )                                             &&
        ( verbose >= 3 )                                          &&
        (itemNum % ITEM_COUNT==0 && (itemNum != 0))) {

        printf("V-3: create file: %llu\n", itemNum);
        fflush(stdout);
    }

    //create files
    sprintf(curr_item, "%s/file.%s%llu", path, mk_name, itemNum);
    if (rank == 0 && verbose >= 3) {
        printf("V-3: create_remove_items_helper (non-dirs create): curr_item is \"%s\"\n", curr_item);
        fflush(stdout);
    }

    if (collective_creates) {
        param.openFlags = IOR_WRONLY;

        if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper (collective): open...\n" );
            fflush( stdout );
        }

        aiori_fh = backend->open (curr_item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to open file");
        }

        /*
         * !collective_creates
         */
    } else {
        param.openFlags = IOR_CREAT | IOR_WRONLY;
        param.filePerProc = !shared_file;

        if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper (non-collective, shared): open...\n" );
            fflush( stdout );
        }

        aiori_fh = backend->create (curr_item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to create file");
        }
    }

    if (write_bytes > 0) {
        if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: write...\n" );
            fflush( stdout );
        }

        /*
         * According to Bill Loewe, writes are only done one time, so they are always at
         * offset 0 (zero).
         */
        param.offset = 0;
        param.fsyncPerWrite = sync_file;
        if (write_bytes != backend->xfer (WRITE, aiori_fh, (IOR_size_t *) write_buffer, write_bytes, &param)) {
            FAIL("unable to write file");
        }
    }

    if (rank == 0 && verbose >= 3) {
        printf( "V-3: create_remove_items_helper: close...\n" );
        fflush( stdout );
    }

    backend->close (aiori_fh, &param);
}

/* helper for creating/removing items */
void create_remove_items_helper(const int dirs, const int create, const char *path,
                                uint64_t itemNum) {

    char curr_item[MAX_LEN];


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering create_remove_items_helper...\n" );
        fflush( stdout );
    }

    for (uint64_t i = 0 ; i < items_per_dir ; ++i) {
        if (!dirs) {
            if (create) {
                create_file (path, itemNum + i);
            } else {
                remove_file (path, itemNum + i);
            }
        } else {
            create_remove_dirs (path, create, itemNum + i);
        }
    }
}

/* helper function to do collective operations */
void collective_helper(const int dirs, const int create, const char* path, uint64_t itemNum) {

    char curr_item[MAX_LEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering collective_helper...\n" );
        fflush( stdout );
    }

    for (uint64_t i = 0 ; i < items_per_dir ; ++i) {
        if (dirs) {
            create_remove_dirs (path, create, itemNum + i);
            continue;
        }

        sprintf(curr_item, "%s/file.%s%llu", path, create ? mk_name : rm_name, itemNum+i);
        if (rank == 0 && verbose >= 3) {
            printf("V-3: create file: %s\n", curr_item);
            fflush(stdout);
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
        } else if (!(shared_file && rank != 0)) {
            //remove files
            backend->delete (curr_item, &param);
        }
    }
}

/* recusive function to create and remove files/directories from the
   directory tree */
void create_remove_items(int currDepth, const int dirs, const int create, const int collective,
                         const char *path, uint64_t dirNum) {
    int i;
    char dir[MAX_LEN];
    char temp_path[MAX_LEN];
    unsigned long long currDir = dirNum;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering create_remove_items, currDepth = %d...\n", currDepth );
        fflush( stdout );
    }


    memset(dir, 0, MAX_LEN);
    strcpy(temp_path, path);

    if (rank == 0 && verbose >= 3) {
        printf( "V-3: create_remove_items (start): temp_path is \"%s\"\n", temp_path );
        fflush(stdout);
    }

    if (currDepth == 0) {
        /* create items at this depth */
        if (!leaf_only || (depth == 0 && leaf_only)) {
            if (collective) {
                collective_helper(dirs, create, temp_path, 0);
            } else {
                create_remove_items_helper(dirs, create, temp_path, 0);
            }
        }

        if (depth > 0) {
            create_remove_items(++currDepth, dirs, create,
                                collective, temp_path, ++dirNum);
        }

    } else if (currDepth <= depth) {
        /* iterate through the branches */
        for (i=0; i<branch_factor; i++) {

            /* determine the current branch and append it to the path */
            sprintf(dir, "%s.%llu/", base_tree_name, currDir);
            strcat(temp_path, "/");
            strcat(temp_path, dir);

            if (rank == 0 && verbose >= 3) {
                printf( "V-3: create_remove_items (for loop): temp_path is \"%s\"\n", temp_path );
                fflush(stdout);
            }

            /* create the items in this branch */
            if (!leaf_only || (leaf_only && currDepth == depth)) {
                if (collective) {
                    collective_helper(dirs, create, temp_path, currDir*items_per_dir);
                } else {
                    create_remove_items_helper(dirs, create, temp_path, currDir*items_per_dir);
                }
            }

            /* make the recursive call for the next level below this branch */
            create_remove_items(
                ++currDepth,
                dirs,
                create,
                collective,
                temp_path,
                ( currDir * ( unsigned long long )branch_factor ) + 1 );
            currDepth--;

            /* reset the path */
            strcpy(temp_path, path);
            currDir++;
        }
    }
}

/* stats all of the items created as specified by the input parameters */
void mdtest_stat(const int random, const int dirs, const char *path) {
    struct stat buf;
    uint64_t parent_dir, item_num = 0;
    char item[MAX_LEN], temp[MAX_LEN];
    uint64_t stop;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering mdtest_stat...\n" );
        fflush( stdout );
    }

    /* determine the number of items to stat*/
    if (leaf_only) {
        stop = items_per_dir * (uint64_t) pow( branch_factor, depth );
    } else {
        stop = items;
    }

    /* iterate over all of the item IDs */
    for (uint64_t i = 0 ; i < stop ; ++i) {
        /*
         * It doesn't make sense to pass the address of the array because that would
         * be like passing char **. Tested it on a Cray and it seems to work either
         * way, but it seems that it is correct without the "&".
         *
         memset(&item, 0, MAX_LEN);
        */
        memset(item, 0, MAX_LEN);
        memset(temp, 0, MAX_LEN);

        /* determine the item number to stat */
        if (random) {
            item_num = rand_array[i];
        } else {
            item_num = i;
        }

        /* make adjustments if in leaf only mode*/
        if (leaf_only) {
            item_num += items_per_dir *
                (num_dirs_in_tree - ( unsigned long long )pow( branch_factor, depth ));
        }

        /* create name of file/dir to stat */
        if (dirs) {
            if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
                printf("V-3: stat dir: %llu\n", i);
                fflush(stdout);
            }
            sprintf(item, "dir.%s%llu", stat_name, item_num);
        } else {
            if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
                printf("V-3: stat file: %llu\n", i);
                fflush(stdout);
            }
            sprintf(item, "file.%s%llu", stat_name, item_num);
        }

        /* determine the path to the file/dir to be stat'ed */
        parent_dir = item_num / items_per_dir;

        if (parent_dir > 0) {        //item is not in tree's root directory

            /* prepend parent directory to item's path */
            sprintf(temp, "%s.%llu/%s", base_tree_name, parent_dir, item);
            strcpy(item, temp);

            //still not at the tree's root dir
            while (parent_dir > branch_factor) {
                parent_dir = (uint64_t) ((parent_dir-1) / branch_factor);
                sprintf(temp, "%s.%llu/%s", base_tree_name, parent_dir, item);
                strcpy(item, temp);
            }
        }

        /* Now get item to have the full path */
        sprintf( temp, "%s/%s", path, item );
        strcpy( item, temp );

        /* below temp used to be hiername */
        if (rank == 0 && verbose >= 3) {
            if (dirs) {
                printf("V-3: mdtest_stat dir : %s\n", item);
            } else {
                printf("V-3: mdtest_stat file: %s\n", item);
            }
            fflush(stdout);
        }

        if (-1 == backend->stat (item, &buf, &param)) {
            if (dirs) {
                if ( verbose >= 3 ) {
                    fprintf( stdout, "V-3: Stat'ing directory \"%s\"\n", item );
                    fflush( stdout );
                }
                FAIL("unable to stat directory");
            } else {
                if ( verbose >= 3 ) {
                    fprintf( stdout, "V-3: Stat'ing file \"%s\"\n", item );
                    fflush( stdout );
                }
                FAIL("unable to stat file");
            }
        }
    }
}


/* reads all of the items created as specified by the input parameters */
void mdtest_read(int random, int dirs, char *path) {
    uint64_t stop, parent_dir, item_num = 0;
    char item[MAX_LEN], temp[MAX_LEN];
    void *aiori_fh;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering mdtest_read...\n" );
        fflush( stdout );
    }

    /* allocate read buffer */
    if (read_bytes > 0) {
        read_buffer = (char *)malloc(read_bytes);
        if (read_buffer == NULL) {
            FAIL("out of memory");
        }
    }

    /* determine the number of items to read */
    if (leaf_only) {
        stop = items_per_dir * ( unsigned long long )pow( branch_factor, depth );
    } else {
        stop = items;
    }

    /* iterate over all of the item IDs */
    for (uint64_t i = 0 ; i < stop ; ++i) {
        /*
         * It doesn't make sense to pass the address of the array because that would
         * be like passing char **. Tested it on a Cray and it seems to work either
         * way, but it seems that it is correct without the "&".
         *
         * NTH: Both are technically correct in C.
         *
         * memset(&item, 0, MAX_LEN);
         */
        memset(item, 0, MAX_LEN);
        memset(temp, 0, MAX_LEN);

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
            if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
                printf("V-3: read file: %llu\n", i);
                fflush(stdout);
            }
            sprintf(item, "file.%s%llu", read_name, item_num);
        }

        /* determine the path to the file/dir to be read'ed */
        parent_dir = item_num / items_per_dir;

        if (parent_dir > 0) {        //item is not in tree's root directory

            /* prepend parent directory to item's path */
            sprintf(temp, "%s.%llu/%s", base_tree_name, parent_dir, item);
            strcpy(item, temp);

            /* still not at the tree's root dir */
            while (parent_dir > branch_factor) {
                parent_dir = (unsigned long long) ((parent_dir-1) / branch_factor);
                sprintf(temp, "%s.%llu/%s", base_tree_name, parent_dir, item);
                strcpy(item, temp);
            }
        }

        /* Now get item to have the full path */
        sprintf( temp, "%s/%s", path, item );
        strcpy( item, temp );

        /* below temp used to be hiername */
        if (rank == 0 && verbose >= 3) {
            if (!dirs) {
                printf("V-3: mdtest_read file: %s\n", item);
            }
            fflush(stdout);
        }

        /* open file for reading */
        param.openFlags = O_RDONLY;
        aiori_fh = backend->open (item, &param);
        if (NULL == aiori_fh) {
            FAIL("unable to open file");
        }

        /* read file */
        if (read_bytes > 0) {
            if (read_bytes != backend->xfer (READ, aiori_fh, (IOR_size_t *) read_buffer, read_bytes, &param)) {
                FAIL("unable to read file");
            }
        }

        /* close file */
        backend->close (aiori_fh, &param);
    }
}

/* This method should be called by rank 0.  It subsequently does all of
   the creates and removes for the other ranks */
void collective_create_remove(const int create, const int dirs, const int ntasks, const char *path) {
    char temp[MAX_LEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering collective_create_remove...\n" );
        fflush( stdout );
    }

    /* rank 0 does all of the creates and removes for all of the ranks */
    for (int i = 0 ; i < ntasks ; ++i) {
        memset(temp, 0, MAX_LEN);

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
        if (rank == 0 && verbose >= 3) {
            printf("V-3: collective_create_remove (create_remove_items): temp is \"%s\"\n", temp);
            fflush( stdout );
        }

        create_remove_items(0, dirs, create, 1, temp, 0);
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

void directory_test(const int iteration, const int ntasks, const char *path) {
    int size;
    double t[5] = {0};
    char temp_path[MAX_LEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering directory_test...\n" );
        fflush( stdout );
    }

    MPI_Barrier(testComm);
    t[0] = MPI_Wtime();

    /* create phase */
    if(create_only) {
        if (unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 0);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: directory_test: create path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* "touch" the files */
        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 1, ntasks, temp_path);
            }
        } else {
            /* create directories */
            create_remove_items(0, 1, 1, 0, temp_path, 0);
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[1] = MPI_Wtime();

    /* stat phase */
    if (stat_only) {
        if (unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 1);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: directory_test: stat path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* stat directories */
        if (random_seed > 0) {
            mdtest_stat(1, 1, temp_path);
        } else {
            mdtest_stat(0, 1, temp_path);
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[2] = MPI_Wtime();

    /* read phase */
    if (read_only) {
        if (unique_dir_per_task) {
            unique_dir_access(READ_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 2);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: directory_test: read path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* read directories */
        if (random_seed > 0) {
            ;        /* N/A */
        } else {
            ;        /* N/A */
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[3] = MPI_Wtime();

    if (remove_only) {
        if (unique_dir_per_task) {
            unique_dir_access(RM_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 3);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: directory_test: remove directories path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* remove directories */
        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(0, 1, ntasks, temp_path);
            }
        } else {
            create_remove_items(0, 1, 0, 0, temp_path, 0);
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[4] = MPI_Wtime();

    if (remove_only) {
        if (unique_dir_per_task) {
            unique_dir_access(RM_UNI_DIR, temp_path);
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: directory_test: remove unique directories path is \"%s\"\n", temp_path );
            fflush( stdout );
        }
    }

    if (unique_dir_per_task && !time_unique_dir_overhead) {
        offset_timers(t, 4);
    }

    MPI_Comm_size(testComm, &size);

    /* calculate times */
    if (create_only) {
        summary_table[iteration].entry[0] = items*size/(t[1] - t[0]);
    } else {
        summary_table[iteration].entry[0] = 0;
    }
    if (stat_only) {
        summary_table[iteration].entry[1] = items*size/(t[2] - t[1]);
    } else {
        summary_table[iteration].entry[1] = 0;
    }
    if (read_only) {
        summary_table[iteration].entry[2] = items*size/(t[3] - t[2]);
    } else {
        summary_table[iteration].entry[2] = 0;
    }
    if (remove_only) {
        summary_table[iteration].entry[3] = items*size/(t[4] - t[3]);
    } else {
        summary_table[iteration].entry[3] = 0;
    }

    if (verbose >= 1 && rank == 0) {
        printf("V-1:   Directory creation: %14.3f sec, %14.3f ops/sec\n",
               t[1] - t[0], summary_table[iteration].entry[0]);
        printf("V-1:   Directory stat    : %14.3f sec, %14.3f ops/sec\n",
               t[2] - t[1], summary_table[iteration].entry[1]);
/* N/A
   printf("V-1:   Directory read    : %14.3f sec, %14.3f ops/sec\n",
   t[3] - t[2], summary_table[iteration].entry[2]);
*/
        printf("V-1:   Directory removal : %14.3f sec, %14.3f ops/sec\n",
               t[4] - t[3], summary_table[iteration].entry[3]);
        fflush(stdout);
    }
}

void file_test(const int iteration, const int ntasks, const char *path) {
    int size;
    double t[5] = {0};
    char temp_path[MAX_LEN];

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering file_test...\n" );
        fflush( stdout );
    }

    MPI_Barrier(testComm);
    t[0] = MPI_Wtime();

    /* create phase */
    if (create_only) {
        if (unique_dir_per_task) {
            unique_dir_access(MK_UNI_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 0);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: file_test: create path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* "touch" the files */
        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(1, 0, ntasks, temp_path);
            }
            MPI_Barrier(testComm);
        }

        /* create files */
        create_remove_items(0, 0, 1, 0, temp_path, 0);
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[1] = MPI_Wtime();

    /* stat phase */
    if (stat_only) {
        if (unique_dir_per_task) {
            unique_dir_access(STAT_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 1);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: file_test: stat path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* stat files */
        if (random_seed > 0) {
                mdtest_stat(1,0,temp_path);
        } else {
                mdtest_stat(0,0,temp_path);
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[2] = MPI_Wtime();

    /* read phase */
    if (read_only) {
        if (unique_dir_per_task) {
            unique_dir_access(READ_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 2);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: file_test: read path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        /* read files */
        if (random_seed > 0) {
                mdtest_read(1,0,temp_path);
        } else {
                mdtest_read(0,0,temp_path);
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[3] = MPI_Wtime();

    if (remove_only) {
        if (unique_dir_per_task) {
            unique_dir_access(RM_SUB_DIR, temp_path);
            if (!time_unique_dir_overhead) {
                offset_timers(t, 3);
            }
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: file_test: rm directories path is \"%s\"\n", temp_path );
            fflush( stdout );
        }

        if (collective_creates) {
            if (rank == 0) {
                collective_create_remove(0, 0, ntasks, temp_path);
            }
        } else {
            create_remove_items(0, 0, 0, 0, temp_path, 0);
        }
    }

    if (barriers) {
        MPI_Barrier(testComm);
    }
    t[4] = MPI_Wtime();

    if (remove_only) {
        if (unique_dir_per_task) {
            unique_dir_access(RM_UNI_DIR, temp_path);
        } else {
            strcpy( temp_path, path );
        }

        if (verbose >= 3 && rank == 0) {
            printf( "V-3: file_test: rm unique directories path is \"%s\"\n", temp_path );
            fflush( stdout );
        }
    }

    if (unique_dir_per_task && !time_unique_dir_overhead) {
        offset_timers(t, 4);
    }

    MPI_Comm_size(testComm, &size);

    /* calculate times */
    if (create_only) {
        summary_table[iteration].entry[4] = items*size/(t[1] - t[0]);
    } else {
        summary_table[iteration].entry[4] = 0;
    }
    if (stat_only) {
        summary_table[iteration].entry[5] = items*size/(t[2] - t[1]);
    } else {
        summary_table[iteration].entry[5] = 0;
    }
    if (read_only) {
        summary_table[iteration].entry[6] = items*size/(t[3] - t[2]);
    } else {
        summary_table[iteration].entry[6] = 0;
    }
    if (remove_only) {
        summary_table[iteration].entry[7] = items*size/(t[4] - t[3]);
    } else {
        summary_table[iteration].entry[7] = 0;
    }

    if (verbose >= 1 && rank == 0) {
        printf("V-1:   File creation     : %14.3f sec, %14.3f ops/sec\n",
               t[1] - t[0], summary_table[iteration].entry[4]);
        printf("V-1:   File stat         : %14.3f sec, %14.3f ops/sec\n",
               t[2] - t[1], summary_table[iteration].entry[5]);
        printf("V-1:   File read         : %14.3f sec, %14.3f ops/sec\n",
               t[3] - t[2], summary_table[iteration].entry[6]);
        printf("V-1:   File removal      : %14.3f sec, %14.3f ops/sec\n",
               t[4] - t[3], summary_table[iteration].entry[7]);
        fflush(stdout);
    }
}

void print_help (void) {
    int j;

    printf (
        "Usage: mdtest [-b branching_factor] [-B] [-c] [-C] [-d testdir] [-D] [-e number_of_bytes_to_read]\n"
        "              [-E] [-f first] [-F] [-h] [-i iterations] [-I items_per_dir] [-l last] [-L]\n"
        "              [-n number_of_items] [-N stride_length] [-p seconds] [-r]\n"
        "              [-R[seed]] [-s stride] [-S] [-t] [-T] [-u] [-v] [-a API]\n"
        "              [-V verbosity_value] [-w number_of_bytes_to_write] [-y] [-z depth]\n"
        "\t-a: API for I/O [POSIX|MPIIO|HDF5|HDFS|S3|S3_EMC|NCMPI]\n"
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
        "\t-y: sync file after writing\n"
        "\t-z: depth of hierarchical directory structure\n"
        );

    MPI_Initialized(&j);
    if (j) {
        MPI_Finalize();
    }
    exit(0);
}

void summarize_results(int iterations) {
    char access[MAX_LEN];
    int i, j, k;
    int start, stop, tableSize = 10;
    double min, max, mean, sd, sum = 0, var = 0, curr = 0;

    double all[iterations * size * tableSize];


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering summarize_results...\n" );
        fflush( stdout );
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Gather(&summary_table->entry[0], tableSize*iterations,
               MPI_DOUBLE, all, tableSize*iterations, MPI_DOUBLE,
               0, MPI_COMM_WORLD);

    if (rank == 0) {

        printf("\nSUMMARY: (of %d iterations)\n", iterations);
        printf(
            "   Operation                      Max            Min           Mean        Std Dev\n");
        printf(
            "   ---------                      ---            ---           ----        -------\n");
        fflush(stdout);

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

        /* calculate aggregates */
        if (barriers) {
            double maxes[iterations];


            /* Because each proc times itself, in the case of barriers we
             * have to backwards calculate the time to simulate the use
             * of barriers.
             */
            for (i = start; i < stop; i++) {
                for (j=0; j<iterations; j++) {
                    maxes[j] = all[j*tableSize + i];
                    for (k=0; k<size; k++) {
                        curr = all[(k*tableSize*iterations)
                                   + (j*tableSize) + i];
                        if (maxes[j] < curr) {
                            maxes[j] = curr;
                        }
                    }
                }

                min = max = maxes[0];
                for (j=0; j<iterations; j++) {
                    if (min > maxes[j]) {
                        min = maxes[j];
                    }
                    if (max < maxes[j]) {
                        max = maxes[j];
                    }
                    sum += maxes[j];
                }
                mean = sum / iterations;
                for (j=0; j<iterations; j++) {
                    var += pow((mean - maxes[j]), 2);
                }
                var = var / iterations;
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
                    printf("   %s ", access);
                    printf("%14.3f ", max);
                    printf("%14.3f ", min);
                    printf("%14.3f ", mean);
                    printf("%14.3f\n", sd);
                    fflush(stdout);
                }
                sum = var = 0;

            }

        } else {
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
                    printf("   %s ", access);
                    printf("%14.3f ", max);
                    printf("%14.3f ", min);
                    printf("%14.3f ", mean);
                    printf("%14.3f\n", sd);
                    fflush(stdout);
                }
                sum = var = 0;

            }
        }

        /* calculate tree create/remove rates */
        for (i = 8; i < tableSize; i++) {
            min = max = all[i];
            for (j = 0; j < iterations; j++) {
                curr = summary_table[j].entry[i];
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
                var += pow((mean -  summary_table[j].entry[i]), 2);
            }
            var = var / (iterations);
            sd = sqrt(var);
            switch (i) {
            case 8: strcpy(access, "Tree creation     :"); break;
            case 9: strcpy(access, "Tree removal      :"); break;
            default: strcpy(access, "ERR");                 break;
            }
            printf("   %s ", access);
            printf("%14.3f ", max);
            printf("%14.3f ", min);
            printf("%14.3f ", mean);
            printf("%14.3f\n", sd);
            fflush(stdout);
            sum = var = 0;
        }
    }
}

/* Checks to see if the test setup is valid.  If it isn't, fail. */
void valid_tests() {


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering valid_tests...\n" );
        fflush( stdout );
    }

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
            FAIL("only specify the number of items or the number of items per directory");
    }

}

void show_file_system_size(char *file_system) {
    char          real_path[MAX_LEN];
    char          file_system_unit_str[MAX_LEN] = "GiB";
    char          inode_unit_str[MAX_LEN]       = "Mi";
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
        fprintf( stdout, "V-1: Entering show_file_system_size...\n" );
        fflush( stdout );
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

    /* show results */
    fprintf(stdout, "Path: %s\n", real_path);
    fprintf(stdout, "FS: %.1f %s   Used FS: %2.1f%%   ",
            total_file_system_size_hr, file_system_unit_str,
            used_file_system_percentage);
    fprintf(stdout, "Inodes: %.1f %s   Used Inodes: %2.1f%%\n",
            (double)total_inodes / (double)inode_unit_val,
            inode_unit_str, used_inode_percentage);
    fflush(stdout);

    return;
}

void display_freespace(char *testdirpath)
{
    char dirpath[MAX_LEN] = {0};
    int  i;
    int  directoryFound   = 0;


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering display_freespace...\n" );
        fflush( stdout );
    }

    if (verbose >= 3 && rank == 0) {
        printf( "V-3: testdirpath is \"%s\"\n", testdirpath );
        fflush( stdout );
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
        printf( "V-3: Before show_file_system_size, dirpath is \"%s\"\n", dirpath );
        fflush( stdout );
    }

    show_file_system_size(dirpath);

    if (verbose >= 3 && rank == 0) {
        printf( "V-3: After show_file_system_size, dirpath is \"%s\"\n", dirpath );
        fflush( stdout );
    }

    return;
}

void create_remove_directory_tree(int create,
                                  int currDepth, char* path, int dirNum) {

    int i;
    char dir[MAX_LEN];


    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( stdout, "V-1: Entering create_remove_directory_tree, currDepth = %d...\n", currDepth );
        fflush( stdout );
    }

    if (currDepth == 0) {
        sprintf(dir, "%s/%s.%d/", path, base_tree_name, dirNum);

        if (create) {
            if (rank == 0 && verbose >= 2) {
                printf("V-2: Making directory \"%s\"\n", dir);
                fflush(stdout);
            }

            if (-1 == backend->mkdir (dir, DIRMODE, &param)) {
                FAIL("Unable to create directory");
            }
        }

        create_remove_directory_tree(create, ++currDepth, dir, ++dirNum);

        if (!create) {
            if (rank == 0 && verbose >= 2) {
                printf("V-2: Remove directory \"%s\"\n", dir);
                fflush(stdout);
            }

            if (-1 == backend->rmdir(dir, &param)) {
                FAIL("Unable to remove directory");
            }
        }
    } else if (currDepth <= depth) {

        char temp_path[MAX_LEN];
        strcpy(temp_path, path);
        int currDir = dirNum;

        for (i=0; i<branch_factor; i++) {
            sprintf(dir, "%s.%d/", base_tree_name, currDir);
            strcat(temp_path, dir);

            if (create) {
                if (rank == 0 && verbose >= 2) {
                    printf("V-2: Making directory \"%s\"\n", temp_path);
                    fflush(stdout);
                }

                if (-1 == backend->mkdir(temp_path, DIRMODE, &param)) {
                    FAIL("Unable to create directory");
                }
            }

            create_remove_directory_tree(create, ++currDepth,
                                         temp_path, (branch_factor*currDir)+1);
            currDepth--;

            if (!create) {
                if (rank == 0 && verbose >= 2) {
                    printf("V-2: Remove directory \"%s\"\n", temp_path);
                    fflush(stdout);
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

int main(int argc, char **argv) {
    int i, j, k, c;
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

    /* Check for -h parameter before MPI_Init so the mdtest binary can be
       called directly, without, for instance, mpirun. */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_help();
        }
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* NCMPI backend uses numTaskWorld as size */
    numTasksWorld = size;

    pid = getpid();
    uid = getuid();

    nodeCount = size / count_tasks_per_node();

    if (rank == 0) {
        printf("-- started at %s --\n\n", print_timestamp());
        printf("mdtest-%s was launched with %d total task(s) on %d node(s)\n",
               RELEASE_VERS, size, nodeCount);
        fflush(stdout);
    }

    if (rank == 0) {
        fprintf(stdout, "Command line used:");
        for (i = 0; i < argc; i++) {
            fprintf(stdout, " %s", argv[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    /* Parse command line options */
    while (1) {
        c = getopt(argc, argv, "a:b:BcCd:De:Ef:Fhi:I:l:Ln:N:p:rR::s:StTuvV:w:yz:");
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'a':
            backend_name = optarg; break;
        case 'b':
            branch_factor = atoi(optarg); break;
        case 'B':
            barriers = 0;                 break;
        case 'c':
            collective_creates = 1;       break;
        case 'C':
            create_only = 1;              break;
        case 'd':
            parse_dirpath(optarg);        break;
        case 'D':
            dirs_only = 1;                break;
        case 'e':
            read_bytes = ( size_t )strtoul( optarg, ( char ** )NULL, 10 );   break;
            //read_bytes = atoi(optarg);    break;
        case 'E':
            read_only = 1;                break;
        case 'f':
            first = atoi(optarg);         break;
        case 'F':
            files_only = 1;               break;
        case 'h':
            print_help();                 break;
        case 'i':
            iterations = atoi(optarg);    break;
        case 'I':
            items_per_dir = (uint64_t) strtoul( optarg, ( char ** )NULL, 10 );   break;
            //items_per_dir = atoi(optarg); break;
        case 'l':
            last = atoi(optarg);          break;
        case 'L':
            leaf_only = 1;                break;
        case 'n':
            items = (uint64_t) strtoul( optarg, ( char ** )NULL, 10 );   break;
            //items = atoi(optarg);         break;
        case 'N':
            nstride = atoi(optarg);       break;
        case 'p':
            pre_delay = atoi(optarg);     break;
        case 'r':
            remove_only = 1;              break;
        case 'R':
            if (optarg == NULL) {
                random_seed = time(NULL);
                MPI_Barrier(MPI_COMM_WORLD);
                MPI_Bcast(&random_seed, 1, MPI_INT, 0, MPI_COMM_WORLD);
                random_seed += rank;
            } else {
                random_seed = atoi(optarg)+rank;
            }
            break;
        case 's':
            stride = atoi(optarg);        break;
        case 'S':
            shared_file = 1;              break;
        case 't':
            time_unique_dir_overhead = 1; break;
        case 'T':
            stat_only = 1;                break;
        case 'u':
            unique_dir_per_task = 1;      break;
        case 'v':
            verbose += 1;                 break;
        case 'V':
            verbose = atoi(optarg);       break;
        case 'w':
            write_bytes = ( size_t )strtoul( optarg, ( char ** )NULL, 10 );   break;
            //write_bytes = atoi(optarg);   break;
        case 'y':
            sync_file = 1;                break;
        case 'z':
            depth = atoi(optarg);                  break;
        }
    }

    if (!create_only && !stat_only && !read_only && !remove_only) {
        create_only = stat_only = read_only = remove_only = 1;
        if (( rank == 0 ) && ( verbose >= 1 )) {
            fprintf( stdout, "V-1: main: Setting create/stat/read/remove_only to True\n" );
            fflush( stdout );
        }
    }

    valid_tests();

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf (stdout, "api                     : %s\n", backend_name);
        fprintf( stdout, "barriers                : %s\n", ( barriers ? "True" : "False" ));
        fprintf( stdout, "collective_creates      : %s\n", ( collective_creates ? "True" : "False" ));
        fprintf( stdout, "create_only             : %s\n", ( create_only ? "True" : "False" ));
        fprintf( stdout, "dirpath(s):\n" );
        for ( i = 0; i < path_count; i++ ) {
            fprintf( stdout, "\t%s\n", filenames[i] );
        }
        fprintf( stdout, "dirs_only               : %s\n", ( dirs_only ? "True" : "False" ));
        fprintf( stdout, "read_bytes              : %llu\n", read_bytes );
        fprintf( stdout, "read_only               : %s\n", ( read_only ? "True" : "False" ));
        fprintf( stdout, "first                   : %d\n", first );
        fprintf( stdout, "files_only              : %s\n", ( files_only ? "True" : "False" ));
        fprintf( stdout, "iterations              : %d\n", iterations );
        fprintf( stdout, "items_per_dir           : %llu\n", items_per_dir );
        fprintf( stdout, "last                    : %d\n", last );
        fprintf( stdout, "leaf_only               : %s\n", ( leaf_only ? "True" : "False" ));
        fprintf( stdout, "items                   : %llu\n", items );
        fprintf( stdout, "nstride                 : %d\n", nstride );
        fprintf( stdout, "pre_delay               : %d\n", pre_delay );
        fprintf( stdout, "remove_only             : %s\n", ( leaf_only ? "True" : "False" ));
        fprintf( stdout, "random_seed             : %d\n", random_seed );
        fprintf( stdout, "stride                  : %d\n", stride );
        fprintf( stdout, "shared_file             : %s\n", ( shared_file ? "True" : "False" ));
        fprintf( stdout, "time_unique_dir_overhead: %s\n", ( time_unique_dir_overhead ? "True" : "False" ));
        fprintf( stdout, "stat_only               : %s\n", ( stat_only ? "True" : "False" ));
        fprintf( stdout, "unique_dir_per_task     : %s\n", ( unique_dir_per_task ? "True" : "False" ));
        fprintf( stdout, "write_bytes             : %llu\n", write_bytes );
        fprintf( stdout, "sync_file               : %s\n", ( sync_file ? "True" : "False" ));
        fprintf( stdout, "depth                   : %d\n", depth );
        fflush( stdout );
    }

    /* setup total number of items and number of items per dir */
    if (depth <= 0) {
        num_dirs_in_tree = 1;
    } else {
        if (branch_factor < 1) {
            num_dirs_in_tree = 1;
        } else if (branch_factor == 1) {
            num_dirs_in_tree = depth + 1;
        } else {
            num_dirs_in_tree =
                (1 - pow(branch_factor, depth+1)) / (1 - branch_factor);
        }
    }
    if (items_per_dir > 0) {
        items = items_per_dir * num_dirs_in_tree;
    } else {
        if (leaf_only) {
            if (branch_factor <= 1) {
                items_per_dir = items;
            } else {
                items_per_dir = items / pow(branch_factor, depth);
                items = items_per_dir * pow(branch_factor, depth);
            }
        } else {
            items_per_dir = items / num_dirs_in_tree;
            items = items_per_dir * num_dirs_in_tree;
        }
    }

    /* initialize rand_array */
    if (random_seed > 0) {
        srand(random_seed);

        uint64_t stop = 0;
        uint64_t s;

        if (leaf_only) {
            stop = items_per_dir * (uint64_t) pow(branch_factor, depth);
        } else {
            stop = items;
        }
        rand_array = (uint64_t *) malloc( stop * sizeof(*rand_array));

        for (s=0; s<stop; s++) {
            rand_array[s] = s;
        }

        /* shuffle list randomly */
        uint64_t n = stop;
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

            uint64_t k =
                ( uint64_t ) ((( double )rand() / ( double )RAND_MAX ) * ( double )n );

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
        memset(write_buffer, 0x23, write_bytes);
    }

    /* setup directory path to work in */
    if (path_count == 0) { /* special case where no directory path provided with '-d' option */
        getcwd(testdirpath, MAX_LEN);
        path_count = 1;
    } else {
        strcpy(testdirpath, filenames[rank%path_count]);
    }

    backend = aiori_select (backend_name);
    if (NULL == backend) {
        FAIL("Could not find suitable backend to use");
    }

    /*   if directory does not exist, create it */
    if ((rank < path_count) && backend->access(testdirpath, F_OK, &param) != 0) {
        if (backend->mkdir(testdirpath, DIRMODE, &param) != 0) {
            FAIL("Unable to create test directory path");
        }
    }

    /* display disk usage */
    if (verbose >= 3 && rank == 0) {
        printf( "V-3: main (before display_freespace): testdirpath is \"%s\"\n", testdirpath );
        fflush( stdout );
    }

    if (rank == 0) display_freespace(testdirpath);

    if (verbose >= 3 && rank == 0) {
        printf( "V-3: main (after display_freespace): testdirpath is \"%s\"\n", testdirpath );
        fflush( stdout );
    }

    if (rank == 0) {
        if (random_seed > 0) {
            printf("random seed: %d\n", random_seed);
        }
    }

    if (gethostname(hostname, MAX_LEN) == -1) {
        perror("gethostname");
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    if (last == 0) {
        first = size;
        last = size;
    }

    /* setup summary table for recording results */
    summary_table = (table_t *)malloc(iterations * sizeof(table_t));
    if (summary_table == NULL) {
        FAIL("out of memory");
    }

    if (unique_dir_per_task) {
        sprintf(base_tree_name, "mdtest_tree.%d", rank);
    } else {
        sprintf(base_tree_name, "mdtest_tree");
    }

    /* start and end times of directory tree create/remove */
    double startCreate, endCreate;

    /* default use shared directory */
    strcpy(mk_name, "mdtest.shared.");
    strcpy(stat_name, "mdtest.shared.");
    strcpy(read_name, "mdtest.shared.");
    strcpy(rm_name, "mdtest.shared.");

    MPI_Comm_group(MPI_COMM_WORLD, &worldgroup);
    /* Run the tests */
    for (i = first; i <= last && i <= size; i += stride) {
        range.last = i - 1;
        MPI_Group_range_incl(worldgroup, 1, (void *)&range, &testgroup);
        MPI_Comm_create(MPI_COMM_WORLD, testgroup, &testComm);
        if (rank == 0) {
            if (files_only && dirs_only) {
                printf("\n%d tasks, %llu files/directories\n", i, i * items);
            } else if (files_only) {
                if (!shared_file) {
                    printf("\n%d tasks, %llu files\n", i, i * items);
                }
                else {
                    printf("\n%d tasks, 1 file\n", i);
                }
            } else if (dirs_only) {
                printf("\n%d tasks, %llu directories\n", i, i * items);
            }
        }
        if (rank == 0 && verbose >= 1) {
            printf("\n");
            printf("   Operation               Duration              Rate\n");
            printf("   ---------               --------              ----\n");
        }
        for (j = 0; j < iterations; j++) {
            if (rank == 0 && verbose >= 1) {
                printf("V-1: main: * iteration %d *\n", j+1);
                fflush(stdout);
            }

            int pos = sprintf(testdir, "%s", testdirpath);
            if ( testdir[strlen( testdir ) - 1] != '/' ) {
                pos += sprintf(& testdir[pos], "/");
            }
            pos += sprintf(& testdir[pos], "%s", TEST_DIR);
            pos += sprintf(& testdir[pos], ".%d", j);

            if (verbose >= 2 && rank == 0) {
                printf( "V-2: main (for j loop): making testdir, \"%s\"\n", testdir );
                fflush( stdout );
            }
            if ((rank < path_count) && backend->access(testdir, F_OK, &param) != 0) {
                if (backend->mkdir(testdir, DIRMODE, &param) != 0) {
                    FAIL("Unable to create test directory");
                }
            }
            MPI_Barrier(MPI_COMM_WORLD);

            /* create hierarchical directory structure */
            MPI_Barrier(MPI_COMM_WORLD);
            if (create_only) {
                startCreate = MPI_Wtime();
                if (unique_dir_per_task) {
                    if (collective_creates && (rank == 0)) {
                        /*
                         * This is inside two loops, one of which already uses "i" and the other uses "j".
                         * I don't know how this ever worked. I'm changing this loop to use "k".
                         */
                        for (k=0; k<size; k++) {
                            sprintf(base_tree_name, "mdtest_tree.%d", k);

                            if (verbose >= 3 && rank == 0) {
                                printf(
                                    "V-3: main (create hierarchical directory loop-collective): Calling create_remove_directory_tree with \"%s\"\n",
                                    testdir );
                                fflush( stdout );
                            }

                            /*
                             * Let's pass in the path to the directory we most recently made so that we can use
                             * full paths in the other calls.
                             */
                            create_remove_directory_tree(1, 0, testdir, 0);
                        }
                    } else if (!collective_creates) {
                        if (verbose >= 3 && rank == 0) {
                            printf(
                                "V-3: main (create hierarchical directory loop-!collective_creates): Calling create_remove_directory_tree with \"%s\"\n",
                                testdir );
                            fflush( stdout );
                        }

                        /*
                         * Let's pass in the path to the directory we most recently made so that we can use
                         * full paths in the other calls.
                         */
                        create_remove_directory_tree(1, 0, testdir, 0);
                    }
                } else {
                    if (rank == 0) {
                        if (verbose >= 3 && rank == 0) {
                            printf(
                                "V-3: main (create hierarchical directory loop-!unque_dir_per_task): Calling create_remove_directory_tree with \"%s\"\n",
                                testdir );
                            fflush( stdout );
                        }

                        /*
                         * Let's pass in the path to the directory we most recently made so that we can use
                         * full paths in the other calls.
                         */
                        create_remove_directory_tree(1, 0 , testdir, 0);
                    }
                }
                MPI_Barrier(MPI_COMM_WORLD);
                endCreate = MPI_Wtime();
                summary_table[j].entry[8] =
                    num_dirs_in_tree / (endCreate - startCreate);
                if (verbose >= 1 && rank == 0) {
                    printf("V-1: main:   Tree creation     : %14.3f sec, %14.3f ops/sec\n",
                           (endCreate - startCreate), summary_table[j].entry[8]);
                    fflush(stdout);
                }
            } else {
                summary_table[j].entry[8] = 0;
            }
            sprintf(unique_mk_dir, "%s/%s.0", testdir, base_tree_name);
            sprintf(unique_chdir_dir, "%s/%s.0", testdir, base_tree_name);
            sprintf(unique_stat_dir, "%s/%s.0", testdir, base_tree_name);
            sprintf(unique_read_dir, "%s/%s.0", testdir, base_tree_name);
            sprintf(unique_rm_dir, "%s/%s.0", testdir, base_tree_name);
            sprintf(unique_rm_uni_dir, "%s", testdir);

            if (!unique_dir_per_task) {
                if (verbose >= 3 && rank == 0) {
                    printf( "V-3: main: Using unique_mk_dir, \"%s\"\n", unique_mk_dir );
                    fflush( stdout );
                }
            }

            if (rank < i) {
                if (!shared_file) {
                    sprintf(mk_name, "mdtest.%d.", (rank+(0*nstride))%i);
                    sprintf(stat_name, "mdtest.%d.", (rank+(1*nstride))%i);
                    sprintf(read_name, "mdtest.%d.", (rank+(2*nstride))%i);
                    sprintf(rm_name, "mdtest.%d.", (rank+(3*nstride))%i);
                }
                if (unique_dir_per_task) {
                    sprintf(unique_mk_dir, "%s/mdtest_tree.%d.0", testdir,
                            (rank+(0*nstride))%i);
                    sprintf(unique_chdir_dir, "%s/mdtest_tree.%d.0", testdir,
                            (rank+(1*nstride))%i);
                    sprintf(unique_stat_dir, "%s/mdtest_tree.%d.0", testdir,
                            (rank+(2*nstride))%i);
                    sprintf(unique_read_dir, "%s/mdtest_tree.%d.0", testdir,
                            (rank+(3*nstride))%i);
                    sprintf(unique_rm_dir, "%s/mdtest_tree.%d.0", testdir,
                            (rank+(4*nstride))%i);
                    sprintf(unique_rm_uni_dir, "%s", testdir);
                }
                strcpy(top_dir, unique_mk_dir);

                if (verbose >= 3 && rank == 0) {
                    printf( "V-3: main: Copied unique_mk_dir, \"%s\", to topdir\n", unique_mk_dir );
                    fflush( stdout );
                }

                if (dirs_only && !shared_file) {
                    if (pre_delay) {
                        delay_secs(pre_delay);
                    }
                    directory_test(j, i, unique_mk_dir);
                }
                if (files_only) {
                    if (pre_delay) {
                        delay_secs(pre_delay);
                    }
                    file_test(j, i, unique_mk_dir);
                }
            }

            /* remove directory structure */
            if (!unique_dir_per_task) {
                if (verbose >= 3 && rank == 0) {
                    printf( "V-3: main: Using testdir, \"%s\"\n", testdir );
                    fflush( stdout );
                }
            }

            MPI_Barrier(MPI_COMM_WORLD);
            if (remove_only) {
                startCreate = MPI_Wtime();
                if (unique_dir_per_task) {
                    if (collective_creates && (rank == 0)) {
                        /*
                         * This is inside two loops, one of which already uses "i" and the other uses "j".
                         * I don't know how this ever worked. I'm changing this loop to use "k".
                         */
                        for (k=0; k<size; k++) {
                            sprintf(base_tree_name, "mdtest_tree.%d", k);

                            if (verbose >= 3 && rank == 0) {
                                printf(
                                    "V-3: main (remove hierarchical directory loop-collective): Calling create_remove_directory_tree with \"%s\"\n",
                                    testdir );
                                fflush( stdout );
                            }

                            /*
                             * Let's pass in the path to the directory we most recently made so that we can use
                             * full paths in the other calls.
                             */
                            create_remove_directory_tree(0, 0, testdir, 0);
                        }
                    } else if (!collective_creates) {
                        if (verbose >= 3 && rank == 0) {
                            printf(
                                "V-3: main (remove hierarchical directory loop-!collective): Calling create_remove_directory_tree with \"%s\"\n",
                                testdir );
                            fflush( stdout );
                        }

                        /*
                         * Let's pass in the path to the directory we most recently made so that we can use
                         * full paths in the other calls.
                         */
                        create_remove_directory_tree(0, 0, testdir, 0);
                    }
                } else {
                    if (rank == 0) {
                        if (verbose >= 3 && rank == 0) {
                            printf(
                                "V-3: main (remove hierarchical directory loop-!unique_dir_per_task): Calling create_remove_directory_tree with \"%s\"\n",
                                testdir );
                            fflush( stdout );
                        }

                        /*
                         * Let's pass in the path to the directory we most recently made so that we can use
                         * full paths in the other calls.
                         */
                        create_remove_directory_tree(0, 0 , testdir, 0);
                    }
                }

                MPI_Barrier(MPI_COMM_WORLD);
                endCreate = MPI_Wtime();
                summary_table[j].entry[9] = num_dirs_in_tree
                    / (endCreate - startCreate);
                if (verbose >= 1 && rank == 0) {
                    printf("V-1: main   Tree removal      : %14.3f sec, %14.3f ops/sec\n",
                           (endCreate - startCreate), summary_table[j].entry[9]);
                    fflush(stdout);
                }

                if (( rank == 0 ) && ( verbose >=2 )) {
                    fprintf( stdout, "V-2: main (at end of for j loop): Removing testdir of \"%s\"\n", testdir );
                    fflush( stdout );
                }

                if ((rank < path_count) && backend->access(testdir, F_OK, &param) == 0) {
                    //if (( rank == 0 ) && access(testdir, F_OK) == 0) {
                    if (backend->rmdir(testdir, &param) == -1) {
                        FAIL("unable to remove directory");
                    }
                }
            } else {
                summary_table[j].entry[9] = 0;
            }
        }

        summarize_results(iterations);
        if (i == 1 && stride > 1) {
            i = 0;
        }
    }

    if (rank == 0) {
        printf("\n-- finished at %s --\n", print_timestamp());
        fflush(stdout);
    }

    if (random_seed > 0) {
        free(rand_array);
    }

    MPI_Finalize();
    exit(0);
}
