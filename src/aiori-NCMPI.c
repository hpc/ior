/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: aiori-NCMPI.c,v $
*   $Revision: 1.2 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       Implementation of abstract I/O interface for Parallel NetCDF (NCMPI).
*
\******************************************************************************/

#include "aiori.h"                        /* abstract IOR interface */
#include <errno.h>                        /* sys_errlist */
#include <stdio.h>                        /* only for fprintf() */
#include <stdlib.h>
#include <sys/stat.h>
#include <pnetcdf.h>

#define NUM_DIMS 3                        /* number of dimensions to data set */

/******************************************************************************/
/*
 * NCMPI_CHECK will display a custom error message and then exit the program
 */

#define NCMPI_CHECK(NCMPI_RETURN, MSG) do {                              \
    char resultString[1024];                                             \
                                                                         \
    if (NCMPI_RETURN < 0) {                                              \
        fprintf(stdout, "** error **\n");                                \
        fprintf(stdout, "ERROR in %s (line %d): %s.\n",                  \
                __FILE__, __LINE__, MSG);                                \
        fprintf(stdout, "ERROR: %s.\n", ncmpi_strerror(NCMPI_RETURN));   \
        fprintf(stdout, "** exiting **\n");                              \
        exit(-1);                                                        \
    }                                                                    \
} while(0)

/**************************** P R O T O T Y P E S *****************************/

int  GetFileMode(IOR_param_t *);
void SetHints   (MPI_Info *, char *);
void ShowHints  (MPI_Info *);

/************************** D E C L A R A T I O N S ***************************/

extern int      errno,                                /* error number */
                numTasksWorld,
                rank,
                rankOffset,
                verbose;                              /* verbose output */
extern MPI_Comm testComm;

/***************************** F U N C T I O N S ******************************/
/******************************************************************************/
/*
 * Create and open a file through the NCMPI interface.
 */

void *
IOR_Create_NCMPI(char        * testFileName,
                 IOR_param_t * param)
{
    int * fd;
    int   fd_mode;
    MPI_Info mpiHints = MPI_INFO_NULL;

    /* Wei-keng Liao: read and set MPI file hints from hintsFile */
    SetHints(&mpiHints, param->hintsFileName);
    if (rank == 0 && param->showHints) {
        fprintf(stdout, "\nhints passed to MPI_File_open() {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }

    fd = (int *)malloc(sizeof(int));
    if (fd == NULL) ERR("Unable to malloc file descriptor");

    fd_mode = GetFileMode(param);
    NCMPI_CHECK(ncmpi_create(testComm, testFileName, fd_mode,
                             mpiHints, fd), "cannot create file");

    /* Wei-keng Liao: print the MPI file hints currently used */
/* WEL - add when ncmpi_get_file_info() is in current parallel-netcdf release
    if (rank == 0 && param->showHints) {
        MPI_CHECK(ncmpi_get_file_info(*fd, &mpiHints),
                  "cannot get file info");
        fprintf(stdout, "\nhints returned from opened file {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }
*/

    /* Wei-keng Liao: free up the mpiHints object */
/* WEL - this needs future fix from next release of PnetCDF
    if (mpiHints != MPI_INFO_NULL)
        MPI_CHECK(MPI_Info_free(&mpiHints), "cannot free file info");
*/

    return(fd);
} /* IOR_Create_NCMPI() */


/******************************************************************************/
/*
 * Open a file through the NCMPI interface.
 */

void *
IOR_Open_NCMPI(char        * testFileName,
               IOR_param_t * param)
{
    int * fd;
    int   fd_mode;
    MPI_Info mpiHints = MPI_INFO_NULL;

    /* Wei-keng Liao: read and set MPI file hints from hintsFile */
    SetHints(&mpiHints, param->hintsFileName);
    if (rank == 0 && param->showHints) {
        fprintf(stdout, "\nhints passed to MPI_File_open() {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }

    fd = (int *)malloc(sizeof(int));
    if (fd == NULL) ERR("Unable to malloc file descriptor");

    fd_mode = GetFileMode(param);
    NCMPI_CHECK(ncmpi_open(testComm, testFileName, fd_mode,
                           mpiHints, fd), "cannot open file");

    /* Wei-keng Liao: print the MPI file hints currently used */
/* WEL - add when ncmpi_get_file_info() is in current parallel-netcdf release
    if (rank == 0 && param->showHints) {
        MPI_CHECK(ncmpi_get_file_info(*fd, &mpiHints),
                  "cannot get file info");
        fprintf(stdout, "\nhints returned from opened file {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }
*/

    /* Wei-keng Liao: free up the mpiHints object */
/* WEL - this needs future fix from next release of PnetCDF
    if (mpiHints != MPI_INFO_NULL)
        MPI_CHECK(MPI_Info_free(&mpiHints), "cannot free file info");
*/

    return(fd);
} /* IOR_Open_NCMPI() */


/******************************************************************************/
/*
 * Write or read access to file using the NCMPI interface.
 */

IOR_offset_t
IOR_Xfer_NCMPI(int            access,
               void         * fd,
               IOR_size_t   * buffer,
               IOR_offset_t   length,
               IOR_param_t  * param)
{
    char         * bufferPtr          = (char *)buffer;
    static int     firstReadCheck     = FALSE,
                   startDataSet;
    int            var_id,
                   dim_id[NUM_DIMS];
    MPI_Offset     bufSize[NUM_DIMS],
                   offset[NUM_DIMS];
    IOR_offset_t   segmentPosition;
    int            segmentNum,
                   transferNum;

    /* Wei-keng Liao: In IOR.c line 1979 says "block size must be a multiple
       of transfer size."  Hence, length should always == param->transferSize
       below.  I leave it here to double check.
    */
    if (length != param->transferSize) {
        char errMsg[256];
        sprintf(errMsg,"length(%lld) != param->transferSize(%lld)\n",
                length, param->transferSize);
        NCMPI_CHECK(-1, errMsg);
    }

    /* determine by offset if need to start data set */
    if (param->filePerProc == TRUE) {
        segmentPosition = (IOR_offset_t)0;
    } else {
        segmentPosition = (IOR_offset_t)((rank + rankOffset) % param->numTasks)
                                        * param->blockSize;
    }
    if ((int)(param->offset - segmentPosition) == 0) {
        startDataSet = TRUE;
        /*
         * this toggle is for the read check operation, which passes through
         * this function twice; note that this function will open a data set
         * only on the first read check and close only on the second
         */
        if (access == READCHECK) {
            if (firstReadCheck == TRUE) {
                firstReadCheck = FALSE;
            } else {
                firstReadCheck = TRUE;
            }
        }
    }

    if (startDataSet == TRUE &&
        (access != READCHECK || firstReadCheck == TRUE)) {
        if (access == WRITE) {
            int numTransfers = param->blockSize / param->transferSize;

            /* Wei-keng Liao: change 1D array to 3D array of dimensions:
               [segmentCount*numTasksWorld][numTransfers][transferSize]
               Requirement: none of these dimensions should be > 4G,
            */
            NCMPI_CHECK(ncmpi_def_dim(*(int *)fd, "segments_times_np",
                        NC_UNLIMITED, &dim_id[0]),
                        "cannot define data set dimensions");
            NCMPI_CHECK(ncmpi_def_dim(*(int *)fd, "number_of_transfers",
                        numTransfers, &dim_id[1]),
                        "cannot define data set dimensions");
            NCMPI_CHECK(ncmpi_def_dim(*(int *)fd, "transfer_size",
                        param->transferSize, &dim_id[2]),
                        "cannot define data set dimensions");
            NCMPI_CHECK(ncmpi_def_var(*(int *)fd, "data_var", NC_BYTE,
                                      NUM_DIMS, dim_id, &var_id),
                        "cannot define data set variables");
            NCMPI_CHECK(ncmpi_enddef(*(int *)fd),
                        "cannot close data set define mode");
        
        } else {
            NCMPI_CHECK(ncmpi_inq_varid(*(int *)fd, "data_var", &var_id),
                        "cannot retrieve data set variable");
        }

        if (param->collective == FALSE) {
            NCMPI_CHECK(ncmpi_begin_indep_data(*(int *)fd),
                        "cannot enable independent data mode");
        }

        param->var_id = var_id;
        startDataSet = FALSE;
    }

    var_id = param->var_id;

    /* Wei-keng Liao: calculate the segment number */
    segmentNum  = param->offset / (param->numTasks * param->blockSize);

    /* Wei-keng Liao: calculate the transfer number in each block */
    transferNum = param->offset % param->blockSize / param->transferSize;

    /* Wei-keng Liao: read/write the 3rd dim of the dataset, each is of
       amount param->transferSize */
    bufSize[0] = 1;
    bufSize[1] = 1;
    bufSize[2] = param->transferSize;

    offset[0] = segmentNum * numTasksWorld + rank;
    offset[1] = transferNum;
    offset[2] = 0;

    /* access the file */
    if (access == WRITE) { /* WRITE */
        if (param->collective) {
            NCMPI_CHECK(ncmpi_put_vara_all(*(int *)fd, var_id, offset, bufSize,
                                           bufferPtr, length, MPI_BYTE),
                        "cannot write to data set");
        } else {
            NCMPI_CHECK(ncmpi_put_vara(*(int *)fd, var_id, offset, bufSize,
                                       bufferPtr, length, MPI_BYTE),
                        "cannot write to data set");
        }
    } else {               /* READ or CHECK */
        if (param->collective == TRUE) {
            NCMPI_CHECK(ncmpi_get_vara_all(*(int *)fd, var_id, offset, bufSize,
                                           bufferPtr, length, MPI_BYTE),
                        "cannot read from data set");
        } else {
            NCMPI_CHECK(ncmpi_get_vara(*(int *)fd, var_id, offset, bufSize,
                                       bufferPtr, length, MPI_BYTE),
                        "cannot read from data set");
        }
    }

    return(length);
} /* IOR_Xfer_NCMPI() */


/******************************************************************************/
/*
 * Perform fsync().
 */

void
IOR_Fsync_NCMPI(void * fd, IOR_param_t * param)
{
    ;
} /* IOR_Fsync_NCMPI() */


/******************************************************************************/
/*
 * Close a file through the NCMPI interface.
 */

void
IOR_Close_NCMPI(void       * fd,
               IOR_param_t * param)
{
    if (param->collective == FALSE) {
        NCMPI_CHECK(ncmpi_end_indep_data(*(int *)fd),
                    "cannot disable independent data mode");
    }
    NCMPI_CHECK(ncmpi_close(*(int *)fd), "cannot close file");
    free(fd);
} /* IOR_Close_NCMPI() */


/******************************************************************************/
/*
 * Delete a file through the NCMPI interface.
 */

void
IOR_Delete_NCMPI(char * testFileName, IOR_param_t * param)
{
    if (unlink(testFileName) != 0) WARN("cannot delete file");
} /* IOR_Delete_NCMPI() */


/******************************************************************************/
/*
 * Determine api version.
 */

void
IOR_SetVersion_NCMPI(IOR_param_t * test)
{
    sprintf(test->apiVersion, "%s (%s)",
            test->api, ncmpi_inq_libvers());
} /* IOR_SetVersion_NCMPI() */


/************************ L O C A L   F U N C T I O N S ***********************/

/******************************************************************************/
/*
 * Return the correct file mode for NCMPI.
 */

int
GetFileMode(IOR_param_t * param)
{
    int fd_mode = 0;

    /* set IOR file flags to NCMPI flags */
    /* -- file open flags -- */
    if (param->openFlags & IOR_RDONLY) {fd_mode |= NC_NOWRITE;}
    if (param->openFlags & IOR_WRONLY) {
        fprintf(stdout, "File write only not implemented in NCMPI\n");
    }
    if (param->openFlags & IOR_RDWR)   {fd_mode |= NC_WRITE;}
    if (param->openFlags & IOR_APPEND) {
        fprintf(stdout, "File append not implemented in NCMPI\n");
    }
    if (param->openFlags & IOR_CREAT)  {fd_mode |= NC_CLOBBER;}
    if (param->openFlags & IOR_EXCL)   {
        fprintf(stdout, "Exclusive access not implemented in NCMPI\n");
    }
    if (param->openFlags & IOR_TRUNC)  {
        fprintf(stdout, "File truncation not implemented in NCMPI\n");
    }
    if (param->openFlags & IOR_DIRECT) {
        fprintf(stdout, "O_DIRECT not implemented in NCMPI\n");
    }

    /* Wei-keng Liao: to enable > 4GB file size */
    fd_mode |= NC_64BIT_OFFSET;

    return(fd_mode);
} /* GetFileMode() */


/******************************************************************************/
/*
 * Use MPIIO call to get file size.
 */

IOR_offset_t
IOR_GetFileSize_NCMPI(IOR_param_t * test,
                      MPI_Comm      testComm,
                      char        * testFileName)
{
    return(IOR_GetFileSize_MPIIO(test, testComm, testFileName));
} /* IOR_GetFileSize_NCMPI() */
