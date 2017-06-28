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

#include "mpi.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef Darwin
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#endif

#ifdef _HAS_PLFS
#include <plfs.h>
#include <plfs_error.h>
#include <sys/statvfs.h>
#endif

#ifdef _HAS_HDFS
#include <hdfs.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#ifdef _HAS_S3
#include "aws4c.h"
#include "aws4c_extra.h"
#include <curl/curl.h>
#endif

#define FILEMODE S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH
#define DIRMODE S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH
/*
 * Try using the system's PATH_MAX, which is what realpath and such use.
 */
#define MAX_LEN PATH_MAX
/*
  #define MAX_LEN 1024
*/
#define RELEASE_VERS "1.9.4-rc1"

#define ITEM_COUNT 25000

#define MDTEST_SUCCESS 0
#define MDTEST_FAILURE -1

typedef struct
{
  double entry[10];
} table_t;

int rank;
int size;
unsigned long long* rand_array;
char testdir[MAX_LEN];
char testdirpath[MAX_LEN];
char top_dir[MAX_LEN];
char base_tree_name[MAX_LEN];
char ** filenames = NULL;
char hostname[MAX_LEN];
char unique_dir[MAX_LEN];
char mk_name[MAX_LEN];
char stat_name[MAX_LEN];
char read_name[MAX_LEN];
char rm_name[MAX_LEN];
char unique_mk_dir[MAX_LEN];
char unique_chdir_dir[MAX_LEN];
char unique_stat_dir[MAX_LEN];
char unique_read_dir[MAX_LEN];
char unique_rm_dir[MAX_LEN];
char unique_rm_uni_dir[MAX_LEN];
char * write_buffer = NULL;
char * read_buffer = NULL;
int barriers = 1;
int create_only = 0;
int stat_only = 0;
int read_only = 0;
int remove_only = 0;
int leaf_only = 0;
int branch_factor = 1;
int depth = 0;

/*
 * This is likely a small value, but it's sometimes computed by
 * branch_factor^(depth+1), so we'll make it a larger variable,
 * just in case.
 */
unsigned long long num_dirs_in_tree = 0;

/*
 * As we start moving towards Exascale, we could have billions
 * of files in a directory. Make room for that possibility with
 * a larger variable.
 */
unsigned long long items_per_dir = 0;
int random_seed = 0;
int shared_file = 0;
int files_only = 0;
int dirs_only = 0;
int pre_delay = 0;
int unique_dir_per_task = 0;
int time_unique_dir_overhead = 0;
int verbose = 0;
int throttle = 1;
unsigned long long items = 0;
int collective_creates = 0;
size_t write_bytes = 0;
size_t read_bytes = 0;
int sync_file = 0;
int path_count = 0;
int nstride = 0; /* neighbor stride */
MPI_Comm testcomm;
table_t * summary_table;

/***** LUSTRE ***********/
/* Try to split workload across all Lustre Meta Data Servers */
struct  MDTS { /* info about Meta data target servers for split */
  unsigned int* indexes;
  unsigned int num; /* Guessing 4 billion meta data servers will do for now */
  unsigned int max;
} *mdts = NULL;

int MDTS_stripe_on = 0;

/*************** PLFS ************/

#ifdef _HAS_PLFS
char using_plfs_path = 0;
pid_t pid;
uid_t uid;
Plfs_fd *wpfd = NULL;
Plfs_fd *rpfd = NULL;
Plfs_fd *cpfd = NULL;
#endif


/************   HDFS **************/
#ifdef _HAS_HDFS
hdfsFS hd_fs;
hdfsFile hd_file;
int hdfs_ret;
#endif


/*************  S3 ****************/
#ifdef _HAS_S3
IOBuf *bf;
IOBuf *buffer_iter;
enum{S3_CREATE, S3_STAT, S3_DELETE};
#define HTTP_OK 200
#define HTTP_NO_CONTENT 204
#define TEST_DIR "test-dir"
char *dir_slash = "--";
char *file_dot = "-";
int bucket_created = 0;
char * s3_host_ip = NULL;
char * s3_host_id = NULL;
int ident = -1;
#else
char *dir_slash = "/";
char *file_dot = ".";
#define TEST_DIR "#test-dir"
#endif



/* for making/removing unique directory && stating/deleting subdirectory */
enum {MK_UNI_DIR, STAT_SUB_DIR, READ_SUB_DIR, RM_SUB_DIR, RM_UNI_DIR};

#ifdef __linux__
#define FAIL(msg) do {							\
    fprintf(stdout, "%s: Process %d(%s): FAILED in %s, %s: %s\n",	\
	    timestamp(), rank, hostname, __func__,			\
	    msg, strerror(errno));					\
    fflush(stdout);							\
    MPI_Abort(MPI_COMM_WORLD, 1);					\
  } while(0)
#else
#define FAIL(msg) do {							\
    fprintf(stdout, "%s: Process %d(%s): FAILED at %d, %s: %s\n",	\
	    timestamp(), rank, hostname, __LINE__,			\
	    msg, strerror(errno));					\
    fflush(stdout);							\
    MPI_Abort(MPI_COMM_WORLD, 1);					\
  } while(0)
#endif

/**
 * A directory making wrapper for the various types
 * of filesystems.  Makes for one place to change directory
 * creation, instead of 6.
 */
int mdtest_mkdir(const char* path, mode_t mode) {

#ifdef _HAS_PLFS

  if ( using_plfs_path ) {
    plfs_ret = plfs_mkdir( path, mode );
    if ( plfs_ret != PLFS_SUCCESS ) {
      fprintf(stderr,"PLFS mkdir unable to make directory");
      return MDTEST_FAILURE;
    }
  } else {
    if ( mkdir( path , mode ) == -1 ) {
      fprintf(stderr,"mkdir unable to make directory");
      return MDTEST_FAILURE;
    }
  }
#else
  if(mdts != NULL && MDTS_stripe_on) {
    char buf[1024] = {0};
    sprintf(buf,"lfs mkdir -i %d %s", mdts->indexes[rank % mdts->num], path);
    if(system(buf) != 0) {
      fprintf(stderr,"LFS mkdir unable to make directory");
      return MDTEST_FAILURE;
    }
  }
  else if (mkdir(path , DIRMODE) == -1) {
    fprintf(stderr,"mkdir unable to make directory");
    return MDTEST_FAILURE;
  }
#endif

  return MDTEST_SUCCESS;

}

/**
 * An access wrapper for the various types of filesystems.
 */

int mdtest_access(const char* path, int mode) {
#ifdef _HAS_PLFS
  if ( using_plfs_path ) {
    plfs_ret = plfs_access( path, mode );
    if ( plfs_ret == PLFS_SUCCESS )
      return MDTEST_SUCCESS;
  }
  return MDTEST_FAILURE;

#else
  if(access(path,mode) == 0) {
    return MDTEST_SUCCESS;
  }
  return MDTEST_FAILURE;
#endif

}


char *timestamp() {
  static char datestring[80];
  time_t timestamp;


  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering timestamp...\n" );
  }

  fflush(stdout);
  timestamp = time(NULL);
  strftime(datestring, 80, "%m/%d/%Y %T", localtime(&timestamp));

  return datestring;
}

int count_tasks_per_node(void) {
  char       localhost[MAX_LEN],
    hostname[MAX_LEN];
  int        count               = 1,
    i;
  char       *hosts;

  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering count_tasks_per_node...\n" );
    fflush( stdout );
  }

  if (gethostname(localhost, MAX_LEN) != 0) {
    FAIL("gethostname()");
  }
    /* MPI_gather all hostnames, and compare to local hostname */
  hosts = (char *) malloc(size * MAX_LEN);
  MPI_Gather(localhost, MAX_LEN, MPI_CHAR, hosts, MAX_LEN, MPI_CHAR, 0, MPI_COMM_WORLD);
  if (rank == 0) {
     for (i = 1; i < size-1; i++) {
         if (strcmp(&(hosts[i*MAX_LEN]), localhost) == 0) {
             count++;
         }
     }
  }
  free(hosts);
  MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);

  return(count);
}

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
  MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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


#ifdef _HAS_S3
/*
 * This function is used to check S3 error codes including Curl return codes
 * and HTTP codes.
 *
 */
void check_S3_error( CURLcode curl_return, IOBuf *s3_buf, int action )
{
  if ( curl_return == CURLE_OK ) {
    if (action == S3_CREATE || action == S3_STAT ) {
      if (s3_buf->code != HTTP_OK) {
        printf("HTTP Code:  %d\n", s3_buf->code);
        FAIL("CURL OK but problem with HTTP return");
      }
    }
    else {// action == S3_DELETE
      if (s3_buf->code != HTTP_NO_CONTENT) {
        printf("HTTP Code:  %d\n", s3_buf->code);
        FAIL("CURL OK but problem with HTTP return when deleting");
      }
    }
  }
  else {
    printf("Curl Return Code:  %d\n", curl_return);
    FAIL("Bad return for Curl call");
  }
}
#endif

/* helper for creating/removing items */
void create_remove_items_helper(int dirs,
                                int create, char* path, unsigned long long itemNum) {

  unsigned long long i;
  char curr_item[MAX_LEN];
#ifdef _HAS_PLFS
  int open_flags;
  plfs_error_t plfs_ret;
  ssize_t bytes_written;
  int num_ref;
#endif

#ifdef _HAS_HDFS
  int open_flags;
#endif

#ifdef _HAS_S3
  CURLcode rv;
  char bucket[MAX_LEN];
//  char object[MAX_LEN];
#endif

  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering create_remove_items_helper...\n" );
    fflush( stdout );
  }

  for (i=0; i<items_per_dir; i++) {
    if (dirs) {
      if (create) {
        if (( rank == 0 )                                         &&
            ( verbose >= 3 )                                      &&
            ((itemNum+i) % ITEM_COUNT==0 && (itemNum+i != 0))) {

          printf("V-3: create dir: %llu\n", itemNum+i);
          fflush(stdout);
        }

        //create dirs
        //  printf("%llu %d %s\n", itemNum, i, mk_name);


        sprintf(curr_item, "%s%sdir%s%s%llu", path,dir_slash,file_dot,mk_name, itemNum+i);
        if (rank == 0 && verbose >= 3) {
          printf("V-3: create_remove_items_helper (dirs create): curr_item is \"%s\"\n", curr_item);
          fflush(stdout);
          printf("%llu %llu\n", itemNum, i);
        }
# ifdef _HAS_HDFS
        if ( hdfsCreateDirectory(hd_fs, curr_item) == -1 ) {
          FAIL("Unable to create test directory path");
        }

#elif defined _HAS_S3
	rv = s3_create_bucket ( bf, curr_item );
	check_S3_error(rv, bf, S3_CREATE);
	aws_iobuf_reset(bf);
#else

	if(mdtest_mkdir(curr_item, DIRMODE) != MDTEST_SUCCESS) {
	  FAIL("Unable to make directory");
	}
#endif

	/*
	 * !create
	 */
      } else {
        if (( rank == 0 )                                       &&
            ( verbose >= 3 )                                    &&
            ((itemNum+i) % ITEM_COUNT==0 && (itemNum+i != 0))) {

          printf("V-3: remove dir: %llu\n", itemNum+i);
          fflush(stdout);
        }

        //remove dirs
        sprintf(curr_item, "%s%sdir%s%s%llu", path,dir_slash,file_dot,rm_name, itemNum+i);

        if (rank == 0 && verbose >= 3) {
          printf("V-3: create_remove_items_helper (dirs remove): curr_item is \"%s\"\n", curr_item);
          fflush(stdout);
        }
#ifdef _HAS_PLFS
        if ( using_plfs_path ) {
          plfs_ret = plfs_rmdir( curr_item );
          if ( plfs_ret != PLFS_SUCCESS ) {
            FAIL("Unable to plfs_rmdir directory");
          }
        } else {
          if (rmdir(curr_item) == -1) {
            FAIL("unable to remove directory");
          }
        }
#elif defined _HAS_HDFS
        if ( hdfsDelete(hd_fs, curr_item, 1) == -1 ) {
          FAIL("unable to remove directory");
        }
#elif defined _HAS_S3
        rv = s3_delete_bucket(bf, curr_item);
        check_S3_error(rv, bf, S3_DELETE);
        aws_iobuf_reset(bf);
#else
        if (rmdir(curr_item) == -1) {
          FAIL("unable to remove directory");
        }
#endif
      }
      /*
       * !dirs
       */
    } else {
      __attribute__ ((unused)) int fd;

#ifdef _HAS_S3
      strcpy(bucket, path);
#endif

      if (create) {
        if (( rank == 0 )                                             &&
            ( verbose >= 3 )                                          &&
            ((itemNum+i) % ITEM_COUNT==0 && (itemNum+i != 0))) {

          printf("V-3: create file: %llu\n", itemNum+i);
          fflush(stdout);
        }
#ifdef _HAS_S3
        if (unique_dir_per_task && !bucket_created ) {
          rv = s3_create_bucket ( bf, path );
          check_S3_error(rv, bf, S3_CREATE);
          aws_iobuf_reset(bf);
          bucket_created = 1;
        }
        else if ( rank == 0 ) {
          if (!bucket_created) {
            rv = s3_create_bucket ( bf, path );
            check_S3_error(rv, bf, S3_CREATE);
            bucket_created = 1;
          }
          aws_iobuf_reset(bf);
        }
        MPI_Barrier(testcomm);
        s3_set_bucket(path);
#endif
        //create files
#ifdef _HAS_S3
        sprintf(curr_item, "file-%s%llu", mk_name, itemNum+i);
#else
        sprintf(curr_item, "%s/file.%s%llu", path, mk_name, itemNum+i);
#endif
        if (rank == 0 && verbose >= 3) {
          printf("V-3: create_remove_items_helper (non-dirs create): curr_item is \"%s\"\n", curr_item);
          fflush(stdout);
        }

        if (collective_creates) {
#ifdef _HAS_PLFS
          if ( using_plfs_path ) {
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (collective): plfs_open...\n" );
              fflush( stdout );
            }
	    /*
	     * If PLFS opens a file as O_RDWR, it suffers a bad performance hit. Looking through the
	     * code that follows up to the close, this file only gets one write, so we'll open it as
	     * write-only.
	     */
            open_flags = O_WRONLY;
            wpfd = NULL;

            plfs_ret = plfs_open( &wpfd, curr_item, open_flags, rank, FILEMODE, NULL );
            if ( plfs_ret != PLFS_SUCCESS ) {
              FAIL( "Unable to plfs_open file" );
            }
          } else {
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (collective): open...\n" );
              fflush( stdout );
            }

            if ((fd = open(curr_item, O_RDWR)) == -1) {
              FAIL("unable to open file");
            }
          }
#elif defined _HAS_HDFS
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper (collective): hdfsOpenFile...\n" );
            fflush( stdout );
          }
          open_flags = O_WRONLY;
          if ( (hd_file = hdfsOpenFile( hd_fs, curr_item, open_flags, 0, 0, 0)) == NULL) {
            FAIL( "Unable to hdfsOpenFile" );
          }
#elif defined _HAS_S3
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper (collective): S3 create object...\n" );
            fflush( stdout );
          }
          rv = s3_put( bf, curr_item );
          check_S3_error(rv, bf, S3_CREATE);
          aws_iobuf_reset ( bf );
#else
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper (collective): open...\n" );
            fflush( stdout );
          }

          if ((fd = open(curr_item, O_RDWR)) == -1) {
            FAIL("unable to open file");
          }
#endif
	  /*
	   * !collective_creates
	   */
        } else {
          if (shared_file) {
#ifdef _HAS_PLFS
            if ( using_plfs_path ) {
              if (rank == 0 && verbose >= 3) {
                printf( "V-3: create_remove_items_helper (non-collective, shared): plfs_open...\n" );
                fflush( stdout );
              }
	      /*
	       * If PLFS opens a file as O_RDWR, it suffers a bad performance hit. Looking through the
	       * code that follows up to the close, this file only gets one write, so we'll open it as
	       * write-only.
	       */
              open_flags = O_CREAT | O_WRONLY;
              wpfd = NULL;

              plfs_ret = plfs_open( &wpfd, curr_item, open_flags, rank, FILEMODE, NULL );
              if ( plfs_ret != PLFS_SUCCESS ) {
                FAIL( "Unable to plfs_open for create file" );
              }
            } else {
              if (rank == 0 && verbose >= 3) {
                printf( "V-3: create_remove_items_helper (non-collective, shared): open...\n" );
                fflush( stdout );
              }

              if ((fd = open(curr_item, O_CREAT|O_RDWR, FILEMODE)) == -1) {
                FAIL("unable to create file");
              }
            }
#elif defined _HAS_HDFS
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (non-collective, shared): hdfsOpenFile...\n" );
              fflush( stdout );
            }
            open_flags = O_CREAT | O_WRONLY;
            if ( (hd_file = hdfsOpenFile( hd_fs, curr_item, open_flags, 0, 0, 0)) == NULL) {
              FAIL( "Unable to hdfsOpenFile" );
            }
#elif defined _HAS_S3
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (non-collective, shared): S3 create object...\n" );
              fflush( stdout );
            }
            rv = s3_put( bf, curr_item );
            check_S3_error(rv, bf, S3_CREATE);
            aws_iobuf_reset ( bf );
#else
            if (rank >= 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (non-collective, shared): open...\n" );
              printf( "V-3: %s\n", curr_item );
              fflush( stdout );
            }

            if ((fd = open(curr_item, O_CREAT|O_RDWR, FILEMODE)) == -1) {
              FAIL("unable to create file");
            }
#endif
	    /*
	     * !shared_file
	     */
          } else {
#ifdef _HAS_PLFS
            if ( using_plfs_path ) {
              if (rank == 0 && verbose >= 3) {
                printf( "V-3: create_remove_items_helper (non-collective, non-shared): plfs_open...\n" );
                fflush( stdout );
              }
	      /*
	       * If PLFS opens a file as O_RDWR, it suffers a bad performance hit. Looking through the
	       * code that follows up to the close, this file only gets one write, so we'll open it as
	       * write-only.
	       */
              open_flags = O_CREAT | O_WRONLY;
              wpfd = NULL;

              plfs_ret = plfs_open( &wpfd, curr_item, open_flags, rank, FILEMODE, NULL );
              if ( plfs_ret != PLFS_SUCCESS ) {
                FAIL( "Unable to plfs_open for create file" );
              }
            } else {
              if (rank == 0 && verbose >= 3) {
                printf( "V-3: create_remove_items_helper (non-collective, non-shared): open...\n" );
                fflush( stdout );
              }

              if ((fd = creat(curr_item, FILEMODE)) == -1) {
                FAIL("unable to create file");
              }
            }
#elif defined _HAS_HDFS
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (non-collective, non-shared): hdfsOpenFilen...\n" );
              fflush( stdout );
            }
            open_flags = O_CREAT | O_WRONLY;
            if ( (hd_file = hdfsOpenFile( hd_fs, curr_item, open_flags, 0, 0, 0)) == NULL) {
              FAIL( "Unable to hdfsOpenFile" );
            }
#elif defined _HAS_S3
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (non-collective, non-shared): S3 create object...\n" );
              fflush( stdout );
            }
            rv = s3_put( bf, curr_item );
            check_S3_error(rv, bf, S3_CREATE);
            aws_iobuf_reset ( bf );
#else
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper (non-collective, non-shared): open...\n" );
              fflush( stdout );
            }

            if ((fd = creat(curr_item, FILEMODE)) == -1) {
              FAIL("unable to create file");
            }
#endif
          }
        }

        if (write_bytes > 0) {
#ifdef _HAS_PLFS
	  /*
	   * According to Bill Loewe, writes are only done one time, so they are always at
	   * offset 0 (zero).
	   */
          if ( using_plfs_path ) {
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper: plfs_write...\n" );
              fflush( stdout );
            }

            plfs_ret = plfs_write( wpfd, write_buffer, write_bytes, 0, pid, &bytes_written );
            if ( plfs_ret != PLFS_SUCCESS ) {
              FAIL( "Unable to plfs_write file" );
            }
            if ( bytes_written != write_bytes ) {
              FAIL( "Did not plfs_write the correct number of bytes to the file" );
            }
          } else {
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper: write...\n" );
              fflush( stdout );
            }

            if (write(fd, write_buffer, write_bytes) != write_bytes) {
              FAIL("unable to write file");
            }
          }
#elif defined _HAS_HDFS
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: hdfsWrite:w...\n" );
            fflush( stdout );
          }
          hdfs_ret = hdfsWrite ( hd_fs, hd_file, write_buffer, write_bytes);
          if ( hdfs_ret == -1 ) {
            FAIL( "Unable to hdfsWrite file" );
          }
          if ( hdfs_ret != write_bytes ) {
            FAIL( "Did not plfs_write the correct number of bytes to the file" );
          }
#elif defined _HAS_S3
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: S3 write to object..\n" );
            fflush( stdout );
          }
          //aws_iobuf_append_dynamic(buffer_iter, write_buffer, write_bytes);
          aws_iobuf_reset ( bf );
          aws_iobuf_append_static(bf, write_buffer, write_bytes);
          rv = s3_put( bf, curr_item );
          check_S3_error(rv, bf, S3_CREATE);
#else
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: write...\n" );
            fflush( stdout );
          }

          if (write(fd, write_buffer, write_bytes) != write_bytes) {
            FAIL("unable to write file");
          }
#endif
        }

#ifdef _HAS_PLFS
        if ( using_plfs_path ) {
          if ( sync_file ) {
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper: plfs_sync...\n" );
              fflush( stdout );
            }

            plfs_ret = plfs_sync( wpfd );
            if ( plfs_ret != PLFS_SUCCESS ) {
              FAIL( "Unable to plfs_sync file" );
            }
          }
        } else {
          if ( sync_file ) {
            if (rank == 0 && verbose >= 3) {
              printf( "V-3: create_remove_items_helper: fsync...\n" );
              fflush( stdout );
            }

            if ( fsync(fd) == -1 ) {
              FAIL("unable to sync file");
            }
          }
        }
#elif defined _HAS_HDFS
        if ( sync_file ) {
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: plfs_sync...\n" );
            fflush( stdout );
          }
          if ( hdfsFlush(hd_fs, hd_file) == -1) {
	    FAIL( "Unable to hdfsFlush file" );
          }
        }
#elif defined _HAS_S3

#else
        if ( sync_file ) {
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: fsync...\n" );
            fflush( stdout );
          }

          if ( fsync(fd) == -1 ) {
            FAIL("unable to sync file");
          }
        }
#endif

#ifdef _HAS_PLFS
        if ( using_plfs_path ) {
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: plfs_close...\n" );
            fflush( stdout );
          }

          plfs_ret = plfs_close( wpfd, rank, uid, open_flags, NULL, &num_ref );
          if ( plfs_ret != PLFS_SUCCESS ) {
            FAIL( "Unable to plfs_close file" );
          }
        } else {
          if (rank == 0 && verbose >= 3) {
            printf( "V-3: create_remove_items_helper: close...\n" );
            fflush( stdout );
          }

          if (close(fd) == -1) {
            FAIL("unable to close file");
          }
        }
#elif defined _HAS_HDFS
        if (rank == 0 && verbose >= 3) {
          printf( "V-3: create_remove_items_helper: plfs_close...\n" );
          fflush( stdout );
        }
        if (hdfsCloseFile(hd_fs, hd_file) == -1) {
          FAIL( "Unable to hdfsCloseFilee" );
        }
#elif defined _HAS_S3

#else
        if (rank == 0 && verbose >= 3) {
          printf( "V-3: create_remove_items_helper: close...\n" );
          fflush( stdout );
        }

        if (close(fd) == -1) {
          FAIL("unable to close file");
        }
#endif
	/*
	 * !create
	 */
      } else {
        if (( rank == 0 )                                       &&
            ( verbose >= 3 )                                    &&
            ((itemNum+i) % ITEM_COUNT==0 && (itemNum+i != 0))) {

          printf("V-3: remove file: %llu\n", itemNum+i);
          fflush(stdout);
        }

        //remove files
#ifdef _HAS_S3
        sprintf(curr_item, "file-%s%llu", rm_name, itemNum+i);
#else
        sprintf(curr_item, "%s/file.%s%llu", path, rm_name, itemNum+i);
#endif
        if (rank == 0 && verbose >= 3) {
          printf("V-3: create_remove_items_helper (non-dirs remove): curr_item is \"%s\"\n", curr_item);
          printf("V-3: create_remove_items_helper (non-dirs remove): rm_name is \"%s\"\n", rm_name);
          fflush(stdout);
        }

        if (!(shared_file && rank != 0)) {
#ifdef _HAS_PLFS
          if ( using_plfs_path ) {
            plfs_ret = plfs_unlink( curr_item );
            if ( plfs_ret != PLFS_SUCCESS ) {
              FAIL( "Unable to plfs_unlink file" );
            }
          } else {
            if (unlink(curr_item) == -1) {
              FAIL("unable to unlink file");
            }
          }
#elif defined _HAS_HDFS
          if (hdfsDelete(hd_fs, curr_item, 1) == -1 ) {
            FAIL( "Unable to hdfsDelete file" );

          }
#elif defined _HAS_S3
          s3_set_bucket(bucket);
          rv = s3_delete(bf, curr_item);
          if (( rank == 0 ) && ( verbose >= 1 )) {
            printf("V-3: create_remove_items_helper (bucket remove): curr_item is \"%s\"\n", curr_item);
            printf("V-3: create_remove_items_helper (bucket remove): bucket is \"%s\"\n", bucket);
          }
          aws_iobuf_reset(bf);
#else
          if (unlink(curr_item) == -1) {
            FAIL("unable to unlink file");
          }
#endif
        }
      }
    }
  }
}

/* helper function to do collective operations */
void collective_helper(int dirs, int create, char* path, unsigned long long itemNum) {

  unsigned long long i;
  char curr_item[MAX_LEN];
#ifdef _HAS_PLFS
  int open_flags;
  plfs_error_t plfs_ret;
  int num_ref;
#endif

#ifdef _HAS_HDFS
  int open_flags;
#endif

#ifdef _HAS_S3
  char bucket[MAX_LEN];
//  char object[MAX_LEN];
  int rv;
  //    int bucket_created = 0;
  strcpy(bucket, path);
#endif

  //MPI_Barrier(testcomm);
  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering collective_helper...\n" );
    fprintf( stdout, "V-1: Entering collective_helper %s\n", path );
    fflush( stdout );
  }

  for (i=0; i<items_per_dir; i++) {
    if (dirs) {
      if (create) {

	//create dirs
	sprintf(curr_item, "%s%sdir%s%s%llu", path, dir_slash,file_dot,mk_name, itemNum+i);

	if (rank == 0 && verbose >= 3) {
	  printf("V-3: create dir : %s\n", curr_item);
	  fflush(stdout);
	}

#ifdef _HAS_HDFS
	if ( hdfsCreateDirectory(hd_fs, curr_item) == -1 ) {
	  FAIL("Unable to create test directory path");
	}
#elif defined _HAS_S3
	rv = s3_create_bucket ( bf, curr_item );
	check_S3_error(rv, bf, S3_CREATE);
	aws_iobuf_reset(bf);
#else
	if(mdtest_mkdir(curr_item, DIRMODE) != MDTEST_SUCCESS) {
	  FAIL("Unable to make directory");
	}

#endif
      } else {

	/* remove dirs */
	sprintf(curr_item, "%s%sdir%s%s%llu", path, dir_slash,file_dot,rm_name, itemNum+i);

	if (rank == 0 && verbose >= 3) {
	  printf("V-3: remove dir : %s\n", curr_item);
	  fflush(stdout);
	}
#ifdef _HAS_PLFS
	if ( using_plfs_path ) {
	  plfs_ret = plfs_rmdir( curr_item );
	  if ( plfs_ret != PLFS_SUCCESS ) {
	    FAIL("Unable to plfs_rmdir directory");
	  }
	} else {
	  if (rmdir(curr_item) == -1) {
	    FAIL("unable to remove directory");
	  }
	}
#elif defined _HAS_HDFS
	if ( hdfsDelete(hd_fs, curr_item, 1) == -1 ) {
	  FAIL("unable to remove directory");
	}
#elif defined _HAS_S3
	rv = s3_delete_bucket(bf, curr_item);
	check_S3_error(rv, bf, S3_DELETE);
	aws_iobuf_reset(bf);
#else
	if (rmdir(curr_item) == -1) {
	  FAIL("unable to remove directory");
	}
#endif
      }

    } else {  //!dirs

      __attribute__ ((unused)) int fd;
      if (create) {

#ifdef _HAS_S3
	// This code is necessary in order to create buckets prior to creating objects
	// If not unique dir per prcess, rank 0 will create the bucket and a flag is set
	// to state the bucket has been created.
	if (!unique_dir_per_task) {
	  if (!bucket_created) {
	    rv = s3_create_bucket ( bf, path );
	    check_S3_error(rv, bf, S3_CREATE);
	    bucket_created = 1;
	    aws_iobuf_reset(bf);
	  }
	}
	// elseif create buckets if unique_dir_per_process but create only if i==0
	else if (i==0) {
	  rv = s3_create_bucket ( bf, path );
	  check_S3_error(rv, bf, S3_CREATE);
	  aws_iobuf_reset(bf);
	}
	// else set the bucket based on input parameters
	else {
	  sprintf(curr_item, "%s%sdir%s%s0", path, dir_slash,file_dot,mk_name);
	}
	s3_set_bucket(path);
	//s3_set_bucket(curr_item);
	sprintf(curr_item, "file-%s%llu", mk_name, itemNum+i);
#else
	sprintf(curr_item, "%s/file.%s%llu", path, mk_name, itemNum+i);
#endif

	if (rank == 0 && verbose >= 3) {
	  printf("V-3: create file: %s\n", curr_item);
	  fflush(stdout);
	}
#ifdef _HAS_PLFS
	if ( using_plfs_path ) {
	  open_flags = O_CREAT | O_WRONLY;
	  cpfd = NULL;

	  plfs_ret = plfs_open( &cpfd, curr_item, open_flags, rank, FILEMODE, NULL );
	  if ( plfs_ret != PLFS_SUCCESS ) {
	    FAIL( "Unable to plfs_open for create file" );
	  }
	} else {
	  if ((fd = creat(curr_item, FILEMODE)) == -1) {
	    FAIL("unable to create file");
	  }
	}
#elif defined _HAS_HDFS
	open_flags = O_CREAT | O_WRONLY;
	if ( (hd_file = hdfsOpenFile( hd_fs, curr_item, open_flags, 0, 0, 0)) == NULL) {
	  FAIL( "Unable to hdfsOpenFile" );
	}
#elif defined _HAS_S3
	rv = s3_put( bf, curr_item );
	check_S3_error(rv, bf, S3_CREATE);
	aws_iobuf_reset ( bf );
#else
	if ((fd = creat(curr_item, FILEMODE)) == -1) {
	  FAIL("unable to create file");
	}
#endif

#ifdef _HAS_PLFS
	if ( using_plfs_path ) {
	  plfs_ret = plfs_close( cpfd, rank, uid, open_flags, NULL, &num_ref );
	  if ( plfs_ret != PLFS_SUCCESS ) {
	    FAIL( "Unable to plfs_close file" );
	  }
	} else {
	  if (close(fd) == -1) {
	    FAIL("unable to close file");
	  }
	}
#elif defined _HAS_HDFS
	if (hdfsCloseFile(hd_fs, hd_file) == -1) {
	  FAIL( "Unable to hdfsCloseFilee" );
	}
#elif defined _HAS_S3
	/* No meaning to this operation on S3 */
#else
	if (close(fd) == -1) {
	  FAIL("unable to close file");
	}
#endif

      } else {  //remove files
	sprintf(curr_item, "%s%sfile%s%s%llu", path,dir_slash,file_dot, rm_name, itemNum+i);

	if (rank == 0 && verbose >= 3) {
	  printf("V-3: remove file: curr_item is \"%s\"\n", curr_item);
	  fflush(stdout);
	}
	if (!(shared_file && rank != 0)) {
#ifdef _HAS_PLFS
	  if ( using_plfs_path ) {
	    plfs_ret = plfs_unlink( curr_item );
	    if ( plfs_ret != PLFS_SUCCESS ) {
	      FAIL( "Unable to plfs_unlink file" );
	    }
	  } else {
	    if (unlink(curr_item) == -1) {
	      FAIL("unable to unlink file");
	    }
	  }
#elif defined _HAS_HDFS
	  if (hdfsDelete(hd_fs, curr_item, 1) == -1 ) {
	    FAIL( "Unable to hdfsDelete file" );
	  }
#elif defined _HAS_S3
	  sprintf(curr_item, "file-%s%llu", mk_name, itemNum+i);
	  s3_set_bucket(bucket);
	  rv = s3_delete(bf, curr_item);
	  aws_iobuf_reset(bf);
#else
	  if (unlink(curr_item) == -1) {
	    FAIL("unable to unlink file");
	  }
#endif
	}
      }
    }
  }
}

/* recusive function to create and remove files/directories from the
   directory tree */
void create_remove_items(int currDepth, int dirs, int create, int collective,
                         char *path, unsigned long long dirNum) {


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
      sprintf(dir, "%s%s%llu/", base_tree_name, file_dot, currDir);
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
void mdtest_stat(int random, int dirs, char *path) {

  __attribute__ ((unused)) struct stat buf;
  unsigned long long i, parent_dir, item_num = 0;
  char item[MAX_LEN], temp[MAX_LEN];
#ifdef _HAS_PLFS
  plfs_error_t plfs_ret;
#endif

#ifdef _HAS_S3
  int rv;
  char bucket[MAX_LEN];
  char object[MAX_LEN];
#endif


  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering mdtest_stat...\n" );
    fflush( stdout );
  }

  /* determine the number of items to stat*/
  unsigned long long stop = 0;
  if (leaf_only) {
    stop = items_per_dir * ( unsigned long long )pow( branch_factor, depth );
  } else {
    stop = items;
  }

  /* iterate over all of the item IDs */
  for (i = 0; i < stop; i++) {

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

      sprintf(item, "dir%s%s%llu", file_dot, stat_name, item_num);

    } else {
      if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
        printf("V-3: stat file: %llu\n", i);
        fflush(stdout);
      }

      sprintf(item, "file%s%s%llu", file_dot, stat_name, item_num);

    }

    /* determine the path to the file/dir to be stat'ed */
    parent_dir = item_num / items_per_dir;

    if (parent_dir > 0) {        //item is not in tree's root directory

      /* prepend parent directory to item's path */

      sprintf(temp, "%s%s%llu%s%s", base_tree_name, file_dot, parent_dir, dir_slash, item);
      strcpy(item, temp);

      //still not at the tree's root dir
      while (parent_dir > branch_factor) {
        parent_dir = (unsigned long long) ((parent_dir-1) / branch_factor);

        sprintf(temp, "%s%s%llu%s%s", base_tree_name, file_dot, parent_dir, dir_slash, item);
        strcpy(item, temp);
      }
    }

    /* Now get item to have the full path */

    sprintf( temp, "%s%s%s", path, dir_slash, item );
#ifdef _HAS_S3
    if (!dirs) {
      strcpy( bucket, path);
      strcpy( object, item);
    }
#endif

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

#ifdef _HAS_PLFS
    if ( using_plfs_path ) {
      plfs_ret = plfs_getattr( NULL, item, &buf, 0 );
      if ( plfs_ret != PLFS_SUCCESS ) {
        if (dirs) {
          if ( verbose >= 3 ) {
            fprintf( stdout, "V-3: Stat'ing directory \"%s\"\n", item );
            fflush( stdout );
          }
          FAIL( "Unable to plfs_getattr directory" );
        } else {
          if ( verbose >= 3 ) {
            fprintf( stdout, "V-3: Stat'ing file \"%s\"\n", item );
            fflush( stdout );
          }
          FAIL( "Unable to plfs_getattr file" );
        }
      }
    } else {
      if (stat(item, &buf) == -1) {
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
#elif defined _HAS_HDFS
    hdfsFileInfo *file_info;
    file_info = hdfsGetPathInfo(hd_fs, item);
    if ( file_info == NULL ) {
      if (dirs) {
        if ( verbose >= 3 ) {
          fprintf( stdout, "V-3: Stat'ing directory \"%s\"\n", item );
          fflush( stdout );
        }
        FAIL( "Unable to hdfsGetPathInfo for directory" );
      } else {
        if ( verbose >= 3 ) {
          fprintf( stdout, "V-3: Stat'ing file \"%s\"\n", item );
          fflush( stdout );
        }
        FAIL( "Unable to hdfsGetPathInfo for file" );
      }
    }
#elif defined _HAS_S3
    if (dirs) {
      rv = s3_stat_bucket( bf, item);
      check_S3_error(rv, bf, S3_STAT);
      if ( verbose >= 3 ) {
	fprintf( stdout, "V-3: Stat'ing bucket \"%s\"\n", item );
	fflush( stdout );
      }
    }
    else {
      rv = s3_stat_object (bf, bucket, object);
      check_S3_error(rv, bf, S3_STAT);
      if ( verbose >= 3 ) {
	fprintf( stdout, "V-3: Stat'ing file %s in bucket %s\n", object, bucket );
	fflush( stdout );
      }
    }
    aws_iobuf_reset(bf);
#else
    if (stat(item, &buf) == -1) {
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
#endif
  }
}


/* reads all of the items created as specified by the input parameters */
void mdtest_read(int random, int dirs, char *path) {

  unsigned long long i, parent_dir, item_num = 0;
  __attribute__ ((unused)) int fd;
  char item[MAX_LEN], temp[MAX_LEN];
#ifdef _HAS_PLFS
  plfs_error_t plfs_ret;
  ssize_t bytes_read;
  int num_ref;
#endif
#ifdef _HAS_S3
  int rv;
  s3_set_bucket(path);
  char object[MAX_LEN];
#endif

  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering mdtest_read...\n" );
    fprintf( stdout, "V-1: mdtest_read path = %s\n", path );
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
  unsigned long long stop = 0;
  if (leaf_only) {
    stop = items_per_dir * ( unsigned long long )pow( branch_factor, depth );
  } else {
    stop = items;
  }

  /* iterate over all of the item IDs */
  for (i = 0; i < stop; i++) {

    /*
     * It doesn't make sense to pass the address of the array because that would
     * be like passing char **. Tested it on a Cray and it seems to work either
     * way, but it seems that it is correct without the "&".
     *
     memset(&item, 0, MAX_LEN);
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
        (num_dirs_in_tree - ( unsigned long long )pow( branch_factor, depth ));
    }

    /* create name of file to read */
    if (dirs) {
      ; /* N/A */
    } else {
      if (rank == 0 && verbose >= 3 && (i%ITEM_COUNT == 0) && (i != 0)) {
        printf("V-3: read file: %llu\n", i);
        fflush(stdout);
      }

      sprintf(item, "file%s%s%llu", file_dot, read_name, item_num);

    }

    /* determine the path to the file/dir to be read'ed */
    parent_dir = item_num / items_per_dir;

    if (parent_dir > 0) {        //item is not in tree's root directory

      /* prepend parent directory to item's path */

      sprintf(temp, "%s%s%llu%s%s", base_tree_name, file_dot, parent_dir, dir_slash, item);

      strcpy(item, temp);

      /* still not at the tree's root dir */
      while (parent_dir > branch_factor) {
        parent_dir = (unsigned long long) ((parent_dir-1) / branch_factor);

        sprintf(temp, "%s%s%llu%s%s", base_tree_name, file_dot, parent_dir, dir_slash, item);

        strcpy(item, temp);
      }
    }

    /* Now get item to have the full path */
#ifdef _HAS_S3
    strcpy(object, item);
#endif
    sprintf( temp, "%s%s%s", path, dir_slash, item );
    strcpy( item, temp );

    /* below temp used to be hiername */
    if (rank == 0 && verbose >= 3) {
      if (dirs) {
        ;
      } else {
        printf("V-3: mdtest_read file: %s\n", item);
      }
      fflush(stdout);
    }

    /* open file for reading */
#ifdef _HAS_PLFS
    if ( using_plfs_path ) {
      /*
       * If PLFS opens a file as O_RDWR, it suffers a bad performance hit. Looking through the
       * code that follows up to the close, this file only gets one read, so we'll open it as
       * read-only.
       */
      rpfd = NULL;
      plfs_ret = plfs_open( &rpfd, item, O_RDONLY, rank, FILEMODE, NULL );
      if ( plfs_ret != PLFS_SUCCESS ) {
        FAIL( "Unable to plfs_open for read file" );
      }
    } else {
      if ((fd = open(item, O_RDWR, FILEMODE)) == -1) {
        FAIL("unable to open file");
      }
    }
#elif defined _HAS_HDFS
    if ( (hd_file = hdfsOpenFile( hd_fs, item, O_RDONLY, 0, 0, 0)) == NULL) {
      FAIL( "Unable to hdfsOpenFile" );
    }
#elif defined _HAS_S3
    /* Do Nothing */
#else
    if ((fd = open(item, O_RDWR, FILEMODE)) == -1) {
      FAIL("unable to open file");
    }
#endif

    /* read file */
    if (read_bytes > 0) {
#ifdef _HAS_PLFS
      /*
       * According to Bill Loewe, reads are only done one time, so they are always at
       * offset 0 (zero).
       */
      if ( using_plfs_path ) {
        plfs_ret = plfs_read( rpfd, read_buffer, read_bytes, 0, &bytes_read );
        if ( plfs_ret != PLFS_SUCCESS ) {
          FAIL( "Unable to plfs_read file" );
        }
        if ( bytes_read != read_bytes ) {
          FAIL( "Did not plfs_read the correct number of bytes from the file" );
        }
      } else {
        if (read(fd, read_buffer, read_bytes) != read_bytes) {
          FAIL("unable to read file");
        }
      }
#elif defined _HAS_HDFS
      hdfs_ret = hdfsRead( hd_fs, hd_file, read_buffer, read_bytes);
      if ( hdfs_ret == -1 ) {
        FAIL( "Unable to hdfsRead file" );
      }
      if ( hdfs_ret != read_bytes ) {
        FAIL( "Did not plfs_read the correct number of bytes from the file" );
      }
#elif defined _HAS_S3
      aws_iobuf_reset(bf);
      aws_iobuf_extend_dynamic(bf, read_buffer, read_bytes);
      rv = s3_get(bf, object);
      check_S3_error(rv, bf, S3_STAT);
      aws_iobuf_reset(bf);
#else
      if (read(fd, read_buffer, read_bytes) != read_bytes) {
        FAIL("unable to read file");
      }
#endif
    }

    /* close file */
#ifdef _HAS_PLFS
    if ( using_plfs_path ) {
      plfs_ret = plfs_close( rpfd, rank, uid, O_RDONLY, NULL, &num_ref );
      if ( plfs_ret != PLFS_SUCCESS ) {
        FAIL( "Unable to plfs_close file" );
      }
    } else {
      if (close(fd) == -1) {
        FAIL("unable to close file");
      }
    }
#elif defined _HAS_HDFS
    if (hdfsCloseFile(hd_fs, hd_file) == -1) {
      FAIL( "Unable to hdfsCloseFilee" );
    }
#elif defined _HAS_S3
    /* Do Nothing */
#else
    if (close(fd) == -1) {
      FAIL("unable to close file");
    }
#endif
  }
}

/* This method should be called by rank 0.  It subsequently does all of
   the creates and removes for the other ranks */
void collective_create_remove(int create, int dirs, int ntasks, char *path) {

  int i;
  char temp[MAX_LEN];


  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering collective_create_remove...\n" );
    fflush( stdout );
  }

  /* rank 0 does all of the creates and removes for all of the ranks */
  for (i=0; i<ntasks; i++) {

    memset(temp, 0, MAX_LEN);

    strcpy(temp, testdir);

    //strcat(temp, "/");
    strcat(temp, dir_slash);

    /* set the base tree name appropriately */
    if (unique_dir_per_task) {
      sprintf(base_tree_name, "mdtest_tree%s%d", file_dot, i);

    } else {
      sprintf(base_tree_name, "mdtest_tree");
    }

    /* Setup to do I/O to the appropriate test dir */
    strcat(temp, base_tree_name);
#ifdef _HAS_S3
    strcat(temp, "-0");
#else
    strcat(temp, ".0");
#endif
    /* set all item names appropriately */
    if (!shared_file) {
      sprintf(mk_name, "mdtest%s%d%s", file_dot, (i+(0*nstride))%ntasks, file_dot);
      sprintf(stat_name, "mdtest%s%d%s", file_dot, (i+(1*nstride))%ntasks, file_dot);
      sprintf(read_name, "mdtest%s%d%s", file_dot, (i+(2*nstride))%ntasks, file_dot);
      sprintf(rm_name, "mdtest%s%d%s", file_dot, (i+(3*nstride))%ntasks, file_dot);
    }
    if (unique_dir_per_task) {
      sprintf(unique_mk_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	      (i+(0*nstride))%ntasks, file_dot);
      sprintf(unique_chdir_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	      (i+(1*nstride))%ntasks, file_dot);
      sprintf(unique_stat_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	      (i+(2*nstride))%ntasks, file_dot);
      sprintf(unique_read_dir, "%s%smdtest_tree%s%d%s", testdir, dir_slash, file_dot,
	      (i+(3*nstride))%ntasks, file_dot);
      sprintf(unique_rm_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	      (i+(4*nstride))%ntasks, file_dot );
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
    sprintf(mk_name, "mdtest%s%d%s", file_dot, (0+(0*nstride))%ntasks, file_dot);
    sprintf(stat_name, "mdtest%s%d%s", file_dot, (0+(1*nstride))%ntasks, file_dot);
    sprintf(read_name, "mdtest%s%d%s", file_dot, (0+(2*nstride))%ntasks, file_dot);
    sprintf(rm_name, "mdtest%s%d%s", file_dot, (0+(3*nstride))%ntasks, file_dot);
  }
  if (unique_dir_per_task) {
    sprintf(unique_mk_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	    (0+(0*nstride))%ntasks, file_dot);
    sprintf(unique_chdir_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	    (0+(1*nstride))%ntasks, file_dot);
    sprintf(unique_stat_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	    (0+(2*nstride))%ntasks, file_dot);
    sprintf(unique_read_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	    (0+(3*nstride))%ntasks, file_dot);
    sprintf(unique_rm_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash, file_dot,
	    (0+(4*nstride))%ntasks, file_dot);

    sprintf(unique_rm_uni_dir, "%s", testdir);
  }
}

void directory_test(int iteration, int ntasks, char *path) {

  int size;
  double t[5] = {0};
  char temp_path[MAX_LEN];


  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering directory_test...\n" );
    fflush( stdout );
  }

  MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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
      ;	/* N/A */
    } else {
      ;	/* N/A */
    }

  }

  if (barriers) {
    MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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

  MPI_Comm_size(testcomm, &size);

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

void file_test(int iteration, int ntasks, char *path) {
  int size;
  double t[5] = {0};
  char temp_path[MAX_LEN];


  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering file_test...\n" );
    fflush( stdout );
  }

  MPI_Barrier(testcomm);
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
      MPI_Barrier(testcomm);
    }

    // xxxatxxxI think the following line is a bug.  If collective, it should not call this after calling collective_
    // create_remove I am going to make this an else statement now but needs further testing

    else {
      /* create files */
      create_remove_items(0, 0, 1, 0, temp_path, 0);
    }

  }
  //printf("XXX rank %d after create and write\n", rank);
  fflush( stdout );

  if (barriers) {
    MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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
    MPI_Barrier(testcomm);
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

  MPI_Comm_size(testcomm, &size);

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

void print_help() {
  char * opts[] = {
    "Usage: mdtest [-a S3_userid] [-A S3 IP/Hostname] [-b branching factor]",
    "              [-B] [-c] [-C] [-d testdir] [-D] [-e number_of_bytes_to_read]",
    "              [-E] [-f first] [-F] [-g S3 bucket identifier] [-h] [-i iterations]",
    "              [-I items_per_dir] [-l last] [-L] [-M] [-n number_of_items] [-N stride_length]",
    "              [-p seconds] [-r] [-R[seed]] [-s stride] [-S] [-t] [-T] [-u] [-v]",
    "              [-V verbosity_value] [-w number_of_bytes_to_write] [-y] [-z depth]",
    "\t-a: userid for S3 target device",
    "\t-A: IP or hostname for S3 target device",
    "\t-b: branching factor of hierarchical directory structure",
    "\t-B: no barriers between phases",
    "\t-c: collective creates: task 0 does all creates",
    "\t-C: only create files/dirs",
    "\t-d: the directory in which the tests will run",
    "\t-D: perform test on directories only (no files)",
    "\t-e: bytes to read from each file",
    "\t-E: only read files/dir",
    "\t-f: first number of tasks on which the test will run",
    "\t-F: perform test on files only (no directories)",
    "\t-g: integer identifier added to bucket name for uniqueness",
    "\t-h: prints this help message",
    "\t-i: number of iterations the test will run",
    "\t-I: number of items per directory in tree",
    "\t-l: last number of tasks on which the test will run",
    "\t-L: files only at leaf level of tree",
    "\t-M: every process will stripe directory creation across LUSTRE MDTS",
    "\t-n: every process will creat/stat/read/remove # directories and files",
    "\t-N: stride # between neighbor tasks for file/dir operation (local=0)",
    "\t-p: pre-iteration delay (in seconds)",
    "\t-r: only remove files or directories left behind by previous runs",
    "\t-R: randomly stat files (optional argument for random seed)",
    "\t-s: stride between the number of tasks for each test",
    "\t-S: shared file access (file only, no directories)",
    "\t-t: time unique working directory overhead",
    "\t-T: only stat files/dirs",
    "\t-u: unique working directory for each task",
    "\t-v: verbosity (each instance of option increments by one)",
    "\t-V: verbosity value",
    "\t-w: bytes to write to each file after it is created",
    "\t-y: sync file after writing",
    "\t-z: depth of hierarchical directory structure",
    ""
  };
  int i, j;

  for (i = 0; strlen(opts[i]) > 0; i++)
    printf("%s\n", opts[i]);
  fflush(stdout);

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
#ifdef _HAS_S3
  if (branch_factor > 1 || depth > 0) {
    FAIL("Cannot specify branch factor or depth when using S3 interface");
  }
  if (dirs_only && files_only) {
    FAIL("Must specify files (objects)  only (-F) or dirs (buckets) only (-D) when using S3 interface");
  }
  if (s3_host_ip == NULL || s3_host_id == NULL) {
    FAIL("Must specify s3 host ip (-A) and s3 host userid (-a)");
  }
#endif
}


void show_file_system_size(char *file_system) {
  char          real_path[MAX_LEN];
  char          file_system_unit_str[MAX_LEN] = "GiB";
  char          inode_unit_str[MAX_LEN]       = "Mi";
  long long int file_system_unit_val          = 1024 * 1024 * 1024;
  long long int inode_unit_val                = 1024 * 1024;
  long long int total_file_system_size = 0;
  long long int free_file_system_size = 0;
  long long int total_inodes = 0;
  long long int free_inodes = 0;
  double        total_file_system_size_hr,
    used_file_system_percentage,
    used_inode_percentage;
    __attribute__ ((unused)) struct statfs status_buffer;
#ifdef _HAS_PLFS
  struct statvfs stbuf;
  plfs_error_t plfs_ret;
#endif


  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering show_file_system_size...\n" );
    fflush( stdout );
  }

#ifdef _HAS_PLFS
  if ( using_plfs_path ) {
    /*
      printf( "Detected that file system, \"%s\" is a PLFS file system.\n", file_system );
    */

    plfs_ret = plfs_statvfs( file_system, &stbuf );
    if ( plfs_ret != PLFS_SUCCESS ) {
      FAIL( "unable to plfs_statvfs() file system" );
    }
  } else {
    /*
      printf( "Detected that file system, \"%s\" is a regular file system.\n", file_system );
    */
    if ( statfs( file_system, &status_buffer ) != 0 ) {
      FAIL("unable to statfs() file system");
    }
  }
#elif _HAS_HDFS
  /* Do Nothing */
#elif _HAS_S3
  /* Do Nothing */
#else
  if (statfs(file_system, &status_buffer) != 0) {
    FAIL("unable to statfs() file system");
  }
#endif

  /* data blocks */
#ifdef _HAS_PLFS
  if ( using_plfs_path ) {
    total_file_system_size = stbuf.f_blocks * stbuf.f_bsize;
    free_file_system_size = stbuf.f_bfree * stbuf.f_bsize;
  } else {
    total_file_system_size = status_buffer.f_blocks * status_buffer.f_bsize;
    free_file_system_size = status_buffer.f_bfree * status_buffer.f_bsize;
  }

#elif _HAS_HDFS
  /* Do Nothing */
#elif _HAS_S3
  /* Do Nothing */
#else
  total_file_system_size = status_buffer.f_blocks * status_buffer.f_bsize;
  free_file_system_size = status_buffer.f_bfree * status_buffer.f_bsize;
#endif
  used_file_system_percentage = (1 - ((double)free_file_system_size
				      / (double)total_file_system_size)) * 100;
  total_file_system_size_hr = (double)total_file_system_size
    / (double)file_system_unit_val;
  if (total_file_system_size_hr > 1024) {
    total_file_system_size_hr = total_file_system_size_hr / 1024;
    strcpy(file_system_unit_str, "TiB");
  }

  /* inodes */
#ifdef _HAS_PLFS
  if ( using_plfs_path ) {
    total_inodes = stbuf.f_files;
    free_inodes = stbuf.f_ffree;
  } else {
    total_inodes = status_buffer.f_files;
    free_inodes = status_buffer.f_ffree;
  }

#elif _HAS_HDFS
  /* Do Nothing */
#elif _HAS_S3
  /* Do Nothing */
#else
  total_inodes = status_buffer.f_files;
  free_inodes = status_buffer.f_ffree;
#endif
  used_inode_percentage = (1 - ((double)free_inodes/(double)total_inodes))
    * 100;

  /* show results */
#ifdef _HAS_PLFS
  if ( using_plfs_path ) {
    strcpy( real_path, file_system );
  } else {
    if (realpath(file_system, real_path) == NULL) {
      FAIL("unable to use realpath()");
    }
  }

#elif _HAS_HDFS
  /* Do Nothing */
#elif _HAS_S3
  /* Do Nothing */

#else
  if (realpath(file_system, real_path) == NULL) {
    FAIL("unable to use realpath()");
  }
#endif
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

#ifdef _HAS_PLFS
  plfs_error_t plfs_ret;
#endif

#ifdef _HAS_S3
  int rv;
#endif

  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "V-1: Entering create_remove_directory_tree, currDepth = %d...\n", currDepth );
    fflush( stdout );
  }

  //  fprintf( stdout, "Rank: %d  Current Depth: %d\n", rank, currDepth);

  if (currDepth == 0) {
#ifdef _HAS_S3
    sprintf(dir, "%s%s%s%s%d", path, dir_slash, base_tree_name, file_dot, dirNum);
#else
    sprintf(dir, "%s%s%s%s%d%s", path, dir_slash, base_tree_name, file_dot, dirNum, dir_slash);
#endif

    if (create) {
      if (rank == 0 && verbose >= 2) {
        printf("V-2: Making directory \"%s\"\n", dir);
        fflush(stdout);
      }

#ifdef _HAS_HDFS
      /* Do Nothing */
#elif _HAS_S3
      /* Do Nothing */
#else
      MDTS_stripe_on = 1;	/* Only stripe the top level directory */
      if(mdtest_mkdir(dir, DIRMODE) != MDTEST_SUCCESS) {
	FAIL("Unable to make directory");
      }
      MDTS_stripe_on = 0; 	/* Stop so we only stripe the top level */
#endif
// NOTE: IT APPEARS BLAIR MOVED THIS HERE where it use to be after the if create
// block  NOT SURE WHY BUT I WILL TEST MDT stuff to see if that is why A.T. 6/25
//      create_remove_directory_tree(create, ++currDepth, dir, ++dirNum);
    }
    create_remove_directory_tree(create, ++currDepth, dir, ++dirNum);
    if (!create) {
      if (rank == 0 && verbose >= 2) {
        printf("V-2: Remove directory \"%s\"\n", dir);
        fflush(stdout);
      }
#ifdef _HAS_PLFS
      if ( using_plfs_path ) {
        plfs_ret = plfs_rmdir( dir );
        if ( plfs_ret != PLFS_SUCCESS ) {
          FAIL("Unable to plfs_rmdir directory");
        }
      } else {
        if (rmdir(dir) == -1) {
          FAIL("Unable to remove directory");
        }
      }
#elif _HAS_HDFS
      /* Do nothing */
#elif _HAS_S3
      if (files_only) {  // check this because S3 does not have
                         // levels of directories (buckets)
        s3_set_bucket(NULL);
        rv = s3_delete_bucket(bf, dir);
        aws_iobuf_reset(bf);
      }

#else
      if (rmdir(dir) == -1) {
        FAIL("Unable to remove directory");
      }
#endif
    }
  } else if (currDepth <= depth) {

    char temp_path[MAX_LEN];
    strcpy(temp_path, path);
    int currDir = dirNum;

    for (i=0; i<branch_factor; i++) {
#ifdef _HAS_S3
      sprintf(dir, "%s%s%d", base_tree_name, file_dot, currDir);
#else
      sprintf(dir, "%s%s%d%s", base_tree_name, file_dot, currDir, dir_slash);
#endif
      sprintf(dir, "%s%s%d%s", base_tree_name, file_dot, currDir, dir_slash);
      strcat(temp_path, dir);

      if (create) {
        if (rank == 0 && verbose >= 2) {
          printf("V-2: Making directory \"%s\"\n", temp_path);
          fflush(stdout);
        }
#ifdef _HAS_HDFS
	/* Do Nothing */
#elif _HAS_S3
	/* Do Nothing */
#else
	if(mdtest_mkdir(temp_path, DIRMODE) != MDTEST_SUCCESS) {
	  FAIL("Unable to make directory");
	}

#endif
      }

      create_remove_directory_tree(create, ++currDepth,
                                   temp_path, (branch_factor*currDir)+1);
      currDepth--;

      if (!create) {
        if (rank == 0 && verbose >= 2) {
          printf("V-2: Remove directory \"%s\"\n", temp_path);
          fflush(stdout);
        }
#ifdef _HAS_PLFS
        if ( using_plfs_path ) {
          plfs_ret = plfs_rmdir( temp_path );
          if ( plfs_ret != PLFS_SUCCESS ) {
            FAIL("Unable to plfs_rmdir directory");
          }
        } else {
          if (rmdir(temp_path) == -1) {
            FAIL("Unable to remove directory");
          }
        }
#elif _HAS_HDFS
	/* Do Nothing */
#elif _HAS_S3
        rv = s3_delete_bucket(bf, temp_path);
        check_S3_error(rv, bf, S3_DELETE);
        aws_iobuf_reset(bf);
#else
        if (rmdir(temp_path) == -1) {
          FAIL("Unable to remove directory");
        }
#endif
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

  /* --- initialize a connection-builder, holding parameters for hdfsBuilderConnect() */

#ifdef _HAS_HDFS
  struct hdfsBuilder* builder = hdfsNewBuilder();
  if ( ! builder ) {
    fprintf(stderr, "couldn't create an hdsfsBuilder");
    exit (1);
  }



  /* see dfs.ha.namenodes.glome in /etc/hdfs-site.xml */

  // hdfsBuilderSetNameNode    ( builder, "gl-io02-ib0" );
  hdfsBuilderSetNameNode    ( builder, "default" );
  //
  // hdfsBuilderSetNameNodePort( builder, 50070 );
  // hdfsBuilderSetNameNodePort( builder, 9000 );
  // hdfsBuilderSetNameNodePort( builder, 8020 );
  //
  hdfsBuilderSetUserName    ( builder, "hadoop" );  // "jti" also works
  //
#endif
#ifdef _HAS_S3
  aws_init();
  aws_set_debug( 0 );
#endif
  /**********
	     int rc = aws_read_config ("atorrez");
	     if ( rc ) {
	     fprintf(stderr, "Unable to read aws config file\n");
	     exit (1);
	     }
	     s3_set_host ( "10.140.0.17:9020");
	     //s3_set_host ( "10.143.0.1:80");
	     aws_reuse_connections(1);
	     bf = aws_iobuf_new();
	     #endif
  *********/

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

#ifdef _HAS_PLFS
  pid = getpid();
  uid = getuid();
  plfs_error_t plfs_ret;
#endif


#ifdef _HAS_HDFS
  hd_fs = hdfsBuilderConnect(builder);
  if ( !hd_fs ) {
    FAIL("Unable to to peform HDFS connect");
    exit(1);
  }
#endif

  nodeCount = size / count_tasks_per_node();

  if (rank == 0) {
    printf("-- started at %s --\n\n", timestamp());
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

#ifdef _HAS_S3
    c = getopt(argc, argv, "a:A:b:BcCd:De:Ef:Fg:hi:I:l:Ln:N:p:rR::s:StTuvV:w:yz:");
#else
    c = getopt(argc, argv, "b:BcCd:De:Ef:Fhi:I:l:LMn:N:p:rR::s:StTuvV:w:yz:");
#endif
    if (c == -1) {
      break;
    }

    switch (c) {
#ifdef _HAS_S3
    case 'a':
      s3_host_id = strdup(optarg);
      break;
    case 'A':
      s3_host_ip = strdup(optarg);
      break;
    case 'g':
      ident = atoi(optarg);
      break;
#endif
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
      items_per_dir = ( unsigned long long )strtoul( optarg, ( char ** )NULL, 10 );   break;
      //items_per_dir = atoi(optarg); break;
    case 'l':
      last = atoi(optarg);          break;
    case 'L':
      leaf_only = 1;                break;
    case 'M': {         /* Auto fill the meta data target indexes */

      /* Need all of the nodes to succeed at this one otherwise fail */
      mdts = malloc(sizeof(struct MDTS));
      if(!mdts) {
	FAIL("No memory for MDTS struct ");
      }
      mdts->indexes = NULL;
      mdts->num = 0;
      mdts->max = 0;

      /* Have rank 0 figure out what MDTS are availible */
      if(rank == 0) {
	char buf[1024];
	fflush(stdout);
	FILE* mdsList = popen("lfs mdts | grep ACTIVE |cut -d : -f 1", "r");
	if(mdsList == NULL) {
	  fprintf(stderr,"lfs mdts failed, ignoring -M flag");

	  /* MPI BCAST NULL RESULT */
	  mdts->num = 0;
	  MPI_Bcast((void*) &mdts->num, 1 , MPI_INTEGER, 0, MPI_COMM_WORLD);
	  break;
	}
	/* some starting space.  Assumes small number of MDTS  */
	mdts->indexes = malloc(sizeof(unsigned int) * 10); /* Generic starting size of 10 */
	if(!mdts->indexes) {
	  free(mdts);
	  mdts = NULL;
	  fprintf(stderr,"Out of memory for MetaData target indexes, ignoring -M flag");

	  /* MPI BCAST NULL RESULT */
	  mdts->num = 0;
	  MPI_Bcast((void*) &mdts->num, 1 , MPI_INTEGER, 0, MPI_COMM_WORLD);
	  break;
	}
	mdts->max = 10;
	unsigned int* temp = NULL;
	while(fgets(buf, sizeof(buf),mdsList)) {
	  if(mdts->max == mdts->num) {
	    temp = realloc(mdts->indexes, mdts->max * 2);
	    if(!temp) {
	      fprintf(stderr,"Ran out of memory for MetaData targets, ignoring -M flag");
	      /* realloc failure leaves the block, so we need to free it */
	      free(mdts->indexes);
	      free(mdts);
	      mdts = NULL;
	      /* MPI BCAST NULL RESULT */
	      mdts->num = 0;
	      MPI_Bcast((void*) &mdts->num, 1 , MPI_INTEGER, 0, MPI_COMM_WORLD);
	    }
	    /* Realloc Moved the block */
	    if(temp != mdts->indexes) {
	      /* Realloc will free the old memory, but we have to change the pointer */
	      mdts->indexes = temp;
	    }
	    mdts->max = mdts->max * 2;
	  }
	  sscanf(buf, "%d", (mdts->indexes + mdts->num));

	  /* Because of the weirdness of buffers with popen, the output of the command
	   * is actually read twice. I guess due to the buffering change of stdout when it is
	   * directed to a file.(totally a guess!), but the output is in acending order of index
	   * so we just need to check if the MDTS index we just read is < the number of indexes
	   */
	  if(mdts->indexes[mdts->num] < mdts->num)
	    break;

	  ++mdts->num;
	}

	pclose(mdsList);


	/* MPI BCAST NUMBER OF MDTS RESULT */
	MPI_Bcast((void*) &mdts->num, 1 , MPI_INTEGER, 0, MPI_COMM_WORLD);

	/* Check and see if we actually sent anything */
	if(mdts->num == 0) {
	  fprintf(stderr,"No Meta data Targets found, ignoring -M flag\n");
	  free(mdts->indexes);
	  free(mdts);
	  mdts = NULL;
	  break;	/* Exit before we broadcast again, no one is listening. */
	}
	else {
	  /* We have results to share, so lets share them. */
	  MPI_Bcast((void*) mdts->indexes, mdts->num, MPI_INT, 0, MPI_COMM_WORLD);
	}
      }

      /* The not rank zero nodes */
      else {
	/* See if there are any records to get */
	MPI_Bcast((void*) &mdts->num, 1 , MPI_INT , 0, MPI_COMM_WORLD);

	if(mdts->num == 0) { /* Failure case, but Ignore the flag, don't FAIL */
	  free(mdts);
	  mdts = NULL;
	  break;
	}
	else {
	  mdts->max = mdts->num;
	  mdts->indexes = malloc(sizeof(int) * mdts->num);
	  if(!mdts->indexes) { /* FAIL because all nodes need to succeed at this */
	    FAIL("Unable to allocate memory for MDTS indexes ");
	  }

	  /* Collect the indexes of the availible MDTS */
	  MPI_Bcast((void*) mdts->indexes, mdts->num, MPI_INT, 0, MPI_COMM_WORLD);
	}
      }
      unique_dir_per_task = 1; /* Unique dirs so we don't bottle neck on one MDTS */
      break;
    }
    case 'n':
      items = ( unsigned long long )strtoul( optarg, ( char ** )NULL, 10 );   break;
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
      depth = atoi(optarg);		  break;
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

#ifdef _HAS_S3
  //    aws_init();
  //    aws_set_debug( 0 );
  int rc = aws_read_config (s3_host_id);
  if ( rc ) {
    fprintf(stderr, "Unable to read aws config file\n");
    exit (1);
  }
  s3_set_host ( s3_host_ip );
  aws_reuse_connections(1);
  bf = aws_iobuf_new();
#endif

  if (( rank == 0 ) && ( verbose >= 1 )) {
    fprintf( stdout, "barriers                : %s\n", ( barriers ? "True" : "False" ));
    fprintf( stdout, "collective_creates      : %s\n", ( collective_creates ? "True" : "False" ));
    fprintf( stdout, "create_only             : %s\n", ( create_only ? "True" : "False" ));
    fprintf( stdout, "dirpath(s):\n" );
    for ( i = 0; i < path_count; i++ ) {
      fprintf( stdout, "\t%s\n", filenames[i] );
    }
    fprintf( stdout, "dirs_only               : %s\n", ( dirs_only ? "True" : "False" ));
    fprintf( stdout, "read_bytes              : %zu\n", read_bytes );
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
    fprintf( stdout, "write_bytes             : %zu\n", write_bytes );
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

    unsigned long long stop = 0;
    unsigned long long s;

    if (leaf_only) {
      stop = items_per_dir * ( unsigned long long )pow(branch_factor, depth);
    } else {
      stop = items;
    }
    rand_array = (unsigned long long *) malloc( stop * sizeof( unsigned long long ));

    for (s=0; s<stop; s++) {
      rand_array[s] = s;
    }

    /* shuffle list randomly */
    unsigned long long n = stop;
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

      unsigned long long k =
	( unsigned long long ) ((( double )rand() / ( double )RAND_MAX ) * ( double )n );

      /*
       * Now move the nth element to the kth (randomly chosen)
       * element, and the kth element to the nth element.
       */

      unsigned long long tmp = rand_array[k];
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

#ifdef _HAS_S3
  // fixed name for now  -  bucket will be comprised of this + testdir
  // Check if user specified identifier (-g) in arguments so that bucket can
  // be uniquely named
  if (ident == -1) {
    sprintf(testdirpath, "%s", "mdtest-S3");
  }
  else {
    sprintf(testdirpath, "%s%d", "mdtest-S3",ident);
  }
#else

  /* setup directory path to work in */
  if (path_count == 0) { /* special case where no directory path provided with '-d' option */
    getcwd(testdirpath, MAX_LEN);
    path_count = 1;
  } else {
    strcpy(testdirpath, filenames[rank%path_count]);
  }
#endif


#ifdef _HAS_PLFS
  using_plfs_path = is_plfs_path( testdirpath );
#endif

  /*   if directory does not exist, create it */
  if(rank < path_count) {

#ifdef _HAS_HDFS
    if ( hdfsExists(hd_fs, testdirpath) == -1 ){
      if ( hdfsCreateDirectory(hd_fs, testdirpath) == -1 ) {
	FAIL("Unable to create test directory path");
      }
    }
#elif _HAS_S3
    /* Do Nothing */
#else
    if( mdtest_access(testdirpath, F_OK) != MDTEST_SUCCESS) {
      if(mdtest_mkdir(testdirpath, DIRMODE) != MDTEST_SUCCESS) {
	FAIL("Unable to make test directory path");
      }
    }
#endif
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

    sprintf(base_tree_name, "mdtest_tree%s%d", file_dot, rank);

  } else {
    sprintf(base_tree_name, "mdtest_tree");
  }

  /* start and end times of directory tree create/remove */
  double startCreate, endCreate;

  /* default use shared directory */
#ifdef _HAS_S3
  strcpy(mk_name, "mdtest-shared-");
  strcpy(stat_name, "mdtest-shared-");
  strcpy(read_name, "mdtest-shared-");
  strcpy(rm_name, "mdtest-shared-");
#else
  strcpy(mk_name, "mdtest.shared.");
  strcpy(stat_name, "mdtest.shared.");
  strcpy(read_name, "mdtest.shared.");
  strcpy(rm_name, "mdtest.shared.");
#endif

  MPI_Comm_group(MPI_COMM_WORLD, &worldgroup);
  /* Run the tests */
  for (i = first; i <= last && i <= size; i += stride) {
    range.last = i - 1;
    MPI_Group_range_incl(worldgroup, 1, (void *)&range, &testgroup);
    MPI_Comm_create(MPI_COMM_WORLD, testgroup, &testcomm);
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
	printf("V-1: main: * iteration %d %s *\n", j+1, timestamp());
	fflush(stdout);
      }

      strcpy(testdir, testdirpath);

      if ( testdir[strlen( testdir ) - 1] != '/' ) {
	strcat(testdir, dir_slash);     // S3 does not allow "/" in bucket names
      }
      strcat(testdir, TEST_DIR);
      sprintf(testdir, "%s%s%d", testdir, file_dot, j); // S3 does not allow "." in bucket names

      if (verbose >= 2 && rank == 0) {
	printf( "V-2: main (for j loop): making testdir, \"%s\"\n", testdir );
	fflush( stdout );
      }

      if(rank < path_count) {

#ifdef _HAS_HDFS
	if ( hdfsExists(hd_fs, testdir) == -1 ){
	  if ( hdfsCreateDirectory(hd_fs, testdir) == -1 ) {
	    FAIL("Unable to create test directory path");
	  }
	}

#elif _HAS_S3 // Do nothing if S3 because bucket will be created at lower level

#else
	if(mdtest_access(testdir, F_OK) != MDTEST_SUCCESS) {
	  if(mdtest_mkdir(testdir,DIRMODE) != MDTEST_SUCCESS) {
	    FAIL("Unable to make test directory");
	  }
	}

#endif
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

	      sprintf(base_tree_name, "mdtest_tree%s%d", file_dot,k);

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

      sprintf(unique_mk_dir, "%s%s%s%s0", testdir, dir_slash, base_tree_name, file_dot);
      sprintf(unique_chdir_dir, "%s%s%s%s0", testdir, dir_slash, base_tree_name, file_dot);
      sprintf(unique_stat_dir, "%s%s%s%s0", testdir, dir_slash, base_tree_name, file_dot);
      sprintf(unique_read_dir, "%s%s%s%s0", testdir, dir_slash, base_tree_name, file_dot);
      sprintf(unique_rm_dir, "%s%s%s%s0", testdir, dir_slash, base_tree_name, file_dot);
      sprintf(unique_rm_uni_dir, "%s", testdir);

      if (!unique_dir_per_task) {
	if (verbose >= 3 && rank == 0) {
	  printf( "V-3: main: Using unique_mk_dir, \"%s\"\n", unique_mk_dir );
	  fflush( stdout );
	}
      }

      if (rank < i) {
	if (!shared_file) {
	  sprintf(mk_name, "mdtest%s%d%s", file_dot, (rank+(0*nstride))%i, file_dot);
	  sprintf(stat_name, "mdtest%s%d%s", file_dot, (rank+(1*nstride))%i, file_dot);
	  sprintf(read_name, "mdtest%s%d%s", file_dot, (rank+(2*nstride))%i, file_dot);
	  sprintf(rm_name, "mdtest%s%d%s", file_dot, (rank+(3*nstride))%i, file_dot);
	}
	if (unique_dir_per_task) {
	  sprintf(unique_mk_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash,
		  file_dot, (rank+(0*nstride))%i, file_dot);
	  sprintf(unique_chdir_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash,
		  file_dot, (rank+(1*nstride))%i, file_dot);
	  sprintf(unique_stat_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash,
		  file_dot, (rank+(2*nstride))%i, file_dot);
	  sprintf(unique_read_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash,
		  file_dot, (rank+(3*nstride))%i, file_dot);
	  sprintf(unique_rm_dir, "%s%smdtest_tree%s%d%s0", testdir, dir_slash,
		  file_dot, (rank+(4*nstride))%i, file_dot);
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

	      sprintf(base_tree_name, "mdtest_tree%s%d", file_dot,k);

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

#ifdef _HAS_PLFS
	if ( using_plfs_path ) {
	  if ( rank < path_count ) {
	    plfs_ret = plfs_access( testdir, F_OK );
	    if ( plfs_ret == PLFS_SUCCESS ) {
	      plfs_ret = plfs_rmdir( testdir );
	      if ( plfs_ret != PLFS_SUCCESS ) {
		FAIL("Unable to plfs_rmdir directory");
	      }
	    }
	  }
	} else {
	  if ((rank < path_count) && access(testdir, F_OK) == 0) {
	    //if (( rank == 0 ) && access(testdir, F_OK) == 0) {
	    if (rmdir(testdir) == -1) {
	      FAIL("unable to remove directory");
	    }
	  }
	}
# elif defined _HAS_HDFS
	if (rank < path_count) {
	  if ( hdfsExists(hd_fs, testdir) == 0 ){
	    if ( hdfsDelete(hd_fs, testdir, 1) == -1 ) {
	      FAIL("Unable to remove directory path");
	    }
	  }
	}
#elif _HAS_S3
	/* Do Nothing */
#else
	if ((rank < path_count) && access(testdir, F_OK) == 0) {
	  //if (( rank == 0 ) && access(testdir, F_OK) == 0) {
	  if (rmdir(testdir) == -1) {
	    FAIL("unable to remove directory");
	  }
	}
#endif
      } else {
	summary_table[j].entry[9] = 0;
      }
#if _HAS_S3
      bucket_created = 0;
#endif
    }

    summarize_results(iterations);
    if (i == 1 && stride > 1) {
      i = 0;
    }
  }

  if (rank == 0) {
    printf("\n-- finished at %s --\n", timestamp());
    fflush(stdout);
  }
#ifdef _HAS_HDFS
  if (hdfsDisconnect(hd_fs) == -1 ) {
    FAIL("Unable to disconnect from hdfs");
  }
#endif

  if (random_seed > 0) {
    free(rand_array);
  }

#ifdef _HAS_S3
  aws_iobuf_free(bf);
#endif

  /* Free up the last of the memory used */
  if(mdts) {
    if(mdts->indexes) {
      free(mdts->indexes);
    }
    free(mdts);
  }

  MPI_Finalize();
  exit(0);
}
