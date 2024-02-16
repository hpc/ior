/******************************************************************************\
 *                                                                             *
 * Copyright (c) 2019 EPCC, The University of Edinburgh                        *
 * Written by Adrian Jackson a.jackson@epcc.ed.ac.uk                           *
 *                                                                             *
 *******************************************************************************
 *                                                                             *
 *                                                                             *
 * This file implements the abstract I/O interface for the low-level PMDK API  *
 *                                                                             *
\******************************************************************************/


#include "aiori.h"                                  /* abstract IOR interface */
#include <errno.h>                                  /* sys_errlist */
#include <stdio.h>                                  /* only for fprintf() */
#include <stdlib.h>
#include <sys/stat.h>
#include <libpmem.h>



static option_help options [] = {
      LAST_OPTION
};


/**************************** P R O T O T Y P E S *****************************/

static option_help * PMDK_options();
static aiori_fd_t *PMDK_Create(char *,int iorflags,  aiori_mod_opt_t *);
static aiori_fd_t *PMDK_Open(char *, int iorflags, aiori_mod_opt_t *);
static IOR_offset_t PMDK_Xfer(int, aiori_fd_t *, IOR_size_t *, IOR_offset_t, IOR_offset_t, aiori_mod_opt_t *);
static void PMDK_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static void PMDK_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void PMDK_Delete(char *, aiori_mod_opt_t *);
static IOR_offset_t PMDK_GetFileSize(aiori_mod_opt_t *, char *);

static aiori_xfer_hint_t * hints = NULL;

static void PMDK_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
}

/************************** D E C L A R A T I O N S ***************************/

extern int      errno;
extern int      rank;
extern int      rankOffset;
extern int      verbose;
extern MPI_Comm testComm;

ior_aiori_t pmdk_aiori = {
        .name = "PMDK",
        .name_legacy = NULL,
        .create = PMDK_Create,
        .open = PMDK_Open,
        .xfer = PMDK_Xfer,
        .close = PMDK_Close,
        .remove = PMDK_Delete,
        .get_version = aiori_get_version,
        .fsync = PMDK_Fsync,
        .xfer_hints = PMDK_xfer_hints,
        .get_file_size = PMDK_GetFileSize,
        .statfs = aiori_posix_statfs,
        .mkdir = aiori_posix_mkdir,
        .rmdir = aiori_posix_rmdir,
        .access = aiori_posix_access,
        .stat = aiori_posix_stat,
        .get_options = PMDK_options,
        .enable_mdtest = false,
};


/***************************** F U N C T I O N S ******************************/

/******************************************************************************/

static option_help * PMDK_options(){
	return options;
}


/*
 * Create and open a memory space through the PMDK interface.
 */
static aiori_fd_t *PMDK_Create(char * testFileName, int iorflags, aiori_mod_opt_t * param){
    char *pmemaddr = NULL;
    int is_pmem;
    size_t mapped_len;
    size_t open_length;

    if(! hints->filePerProc){
      fprintf(stdout, "\nPMDK functionality can only be used with filePerProc functionality\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    open_length = hints->blockSize * hints->segmentCount;

    if((pmemaddr = pmem_map_file(testFileName, open_length,
				  PMEM_FILE_CREATE|PMEM_FILE_EXCL,
				  0666, &mapped_len, &is_pmem)) == NULL) {
      fprintf(stdout, "\nFailed to pmem_map_file for filename: %s in IOR_Create_PMDK\n", testFileName);
      perror("pmem_map_file");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    if(!is_pmem){
      fprintf(stdout, "\n is_pmem is %d\n",is_pmem);
      fprintf(stdout, "\npmem_map_file thinks the hardware being used is not pmem\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }



    return((void *)pmemaddr);
} /* PMDK_Create() */


/******************************************************************************/
/*
 * Open a memory space through the PMDK interface.
 */
static aiori_fd_t *PMDK_Open(char * testFileName,int iorflags, aiori_mod_opt_t * param){

    char *pmemaddr = NULL;
    int is_pmem;
    size_t mapped_len;
    size_t open_length;

    if(!hints->filePerProc){
      fprintf(stdout, "\nPMDK functionality can only be used with filePerProc functionality\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    open_length = hints->blockSize * hints->segmentCount;

    if((pmemaddr = pmem_map_file(testFileName, 0,
				  PMEM_FILE_EXCL,
				  0666, &mapped_len, &is_pmem)) == NULL) {
      fprintf(stdout, "\nFailed to pmem_map_file for filename: %s\n in IOR_Open_PMDK", testFileName);
      perror("pmem_map_file");
      fprintf(stdout, "\n %ld %ld\n",open_length, mapped_len);
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    if(!is_pmem){
      fprintf(stdout, "pmem_map_file thinks the hardware being used is not pmem\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    return((void *)pmemaddr);
} /* PMDK_Open() */


/******************************************************************************/
/*
 * Write or read access to a memory space created with PMDK. Include drain/flush functionality.
 */

static IOR_offset_t PMDK_Xfer(int access, aiori_fd_t *file, IOR_size_t * buffer,
                       IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param){
    int            xferRetries = 0;
    long long      remaining  = (long long)length;
    char         * ptr = (char *)buffer;
    long long      rc;
    long long      i;
    long long      offset_size;

    offset_size = offset;

    if(access == WRITE){
      if(hints->fsyncPerWrite){
	      pmem_memcpy_nodrain(&file[offset_size], ptr, length);
      }else{
        pmem_memcpy_persist(&file[offset_size], ptr, length);
      }
    }else{
      memcpy(ptr, &file[offset_size], length);
    }

    return(length);
} /* PMDK_Xfer() */


/******************************************************************************/
/*
 * Perform fsync().
 */

static void PMDK_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
  pmem_drain();
} /* PMDK_Fsync() */


/******************************************************************************/
/*
 * Stub for close functionality that is not required for PMDK
 */

static void PMDK_Close(aiori_fd_t *fd, aiori_mod_opt_t * param){
  size_t open_length;
  open_length = hints->transferSize;
  pmem_unmap(fd, open_length);
} /* PMDK_Close() */


/******************************************************************************/
/*
 * Delete the file backing a memory space through PMDK
 */

static void PMDK_Delete(char *testFileName, aiori_mod_opt_t * param)
{
    char errmsg[256];
    sprintf(errmsg,"[RANK %03d]:cannot delete file %s\n",rank,testFileName);
    if (unlink(testFileName) != 0) WARN(errmsg);
} /* PMDK_Delete() */

/******************************************************************************/
/*
 * Use POSIX stat() to return aggregate file size.
 */

static IOR_offset_t PMDK_GetFileSize(aiori_mod_opt_t * test,
                      char        * testFileName)
{
    struct stat stat_buf;
    IOR_offset_t aggFileSizeFromStat,
                 tmpMin, tmpMax, tmpSum;
    if (hints->filePerProc == FALSE) {
      fprintf(stdout, "\nPMDK functionality can only be used with filePerProc functionality\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    if (stat(testFileName, &stat_buf) != 0) {
        ERR("cannot get status of written file");
    }
    aggFileSizeFromStat = stat_buf.st_size;

    return(aggFileSizeFromStat);
} /* PMDK_GetFileSize() */
