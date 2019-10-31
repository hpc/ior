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
static void *PMDK_Create(char *, IOR_param_t *);
static void *PMDK_Open(char *, IOR_param_t *);
static IOR_offset_t PMDK_Xfer(int, void *, IOR_size_t *, IOR_offset_t, IOR_param_t *);
static void PMDK_Fsync(void *, IOR_param_t *);
static void PMDK_Close(void *, IOR_param_t *);
static void PMDK_Delete(char *, IOR_param_t *);
static IOR_offset_t PMDK_GetFileSize(IOR_param_t *, MPI_Comm, char *);


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
        .delete = PMDK_Delete,
        .get_version = aiori_get_version,
        .fsync = PMDK_Fsync,
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
static void *PMDK_Create(char * testFileName, IOR_param_t * param){
    char *pmemaddr = NULL;
    int is_pmem;
    size_t mapped_len;
    size_t open_length;

    if(!param->filePerProc){
      fprintf(stdout, "\nPMDK functionality can only be used with filePerProc functionality\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    open_length = param->blockSize * param->segmentCount;

    if((pmemaddr = pmem_map_file(testFileName, open_length,
				  PMEM_FILE_CREATE|PMEM_FILE_EXCL,
				  0666, &mapped_len, &is_pmem)) == NULL) {
      fprintf(stdout, "\nFailed to pmem_map_file for filename: %s in IOR_Create_PMDK\n", testFileName);
      perror("pmem_map_file");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    if(!is_pmem){
      fprintf(stdout, "\npmem_map_file thinks the hardware being used is not pmem\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    
    return((void *)pmemaddr);
} /* PMDK_Create() */


/******************************************************************************/
/*
 * Open a memory space through the PMDK interface.
 */

static void *PMDK_Open(char * testFileName, IOR_param_t * param){

    char *pmemaddr = NULL;
    int is_pmem;
    size_t mapped_len;
    size_t open_length;

    if(!param->filePerProc){
      fprintf(stdout, "\nPMDK functionality can only be used with filePerProc functionality\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    open_length = param->blockSize * param->segmentCount;

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

static IOR_offset_t PMDK_Xfer(int access, void *file, IOR_size_t * buffer,
                       IOR_offset_t length, IOR_param_t * param){
    int            xferRetries = 0;
    long long      remaining  = (long long)length;
    char         * ptr = (char *)buffer;
    long long      rc;
    long long      i;
    long long      offset_size;

    offset_size = param->offset;

    if(access == WRITE){
      if(param->fsync){
	pmem_memcpy_nodrain(&file[offset_size], ptr, length);
      }else{
        pmem_memcpy_persist(&file[offset_size], ptr, length);
      }
        /*    for(i=0; i<length; i++){
	((char *)file)[offset_size+i] = ((char *)ptr)[i];
	}
      pmem_persist(&file[offset_size],length*sizeof(char));*/
    }else{
      memcpy(ptr, &file[offset_size], length);
      /*for(i=0; i<length; i++){
	((char *)ptr)[i] = ((char *)file)[offset_size+i];
	}*/
    }

    return(length);
} /* PMDK_Xfer() */


/******************************************************************************/
/*
 * Perform fsync().
 */

static void PMDK_Fsync(void *fd, IOR_param_t * param)
{
  size_t open_length;
  open_length = param->transferSize;
  pmem_persist(&fd, open_length);
} /* PMDK_Fsync() */


/******************************************************************************/
/*
 * Stub for close functionality that is not required for PMDK
 */

static void PMDK_Close(void *fd, IOR_param_t * param){
  size_t open_length;
  open_length = param->transferSize;
  pmem_unmap(fd, open_length);

} /* PMDK_Close() */


/******************************************************************************/
/*
 * Delete the file backing a memory space through PMDK
 */

static void PMDK_Delete(char *testFileName, IOR_param_t * param)
{
    char errmsg[256];
    sprintf(errmsg,"[RANK %03d]:cannot delete file %s\n",rank,testFileName);
    if (unlink(testFileName) != 0) WARN(errmsg);
} /* PMDK_Delete() */


/******************************************************************************/
/*
 * Determine api version.
 */

static void PMDK_SetVersion(IOR_param_t *test)
{
    strcpy(test->apiVersion, test->api);
} /* PMDK_SetVersion() */


/******************************************************************************/
/*
 * Use POSIX stat() to return aggregate file size.
 */

static IOR_offset_t PMDK_GetFileSize(IOR_param_t * test,
                      MPI_Comm      testComm,
                      char        * testFileName)
{
    struct stat stat_buf;
    IOR_offset_t aggFileSizeFromStat,
                 tmpMin, tmpMax, tmpSum;
    if (test->filePerProc == FALSE) {
      fprintf(stdout, "\nPMDK functionality can only be used with filePerProc functionality\n");
      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
    }

    if (stat(testFileName, &stat_buf) != 0) {
        ERR("cannot get status of written file");
    }
    aggFileSizeFromStat = stat_buf.st_size;

    MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpSum, 1,
			    MPI_LONG_LONG_INT, MPI_SUM, testComm),
	      "cannot total data moved");
    aggFileSizeFromStat = tmpSum;

    return(aggFileSizeFromStat);
} /* PMDK_GetFileSize() */
