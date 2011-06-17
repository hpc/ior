/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: aiori-MPIIO.c,v $
*   $Revision: 1.2 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       Implementation of abstract I/O interface for MPIIO.
*
\******************************************************************************/

#include "aiori.h"                    /* abstract IOR interface */
#include <errno.h>                    /* sys_errlist */
#include <stdio.h>                    /* only for fprintf() */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef MPIAPI
#   define MPIAPI                     /* defined as __stdcall on Windows */
#endif


/**************************** P R O T O T Y P E S *****************************/

static IOR_offset_t SeekOffset_MPIIO    (MPI_File, IOR_offset_t,
                                         IOR_param_t *);
void                SetHints            (MPI_Info *, char *);
void                ShowHints           (MPI_Info *);

/************************** D E C L A R A T I O N S ***************************/

extern int      errno;
extern int      rank;
extern int      rankOffset;
extern int      verbose;
extern MPI_Comm testComm;

/***************************** F U N C T I O N S ******************************/

/******************************************************************************/
/*
 * Create and open a file through the MPIIO interface.
 */

void *
IOR_Create_MPIIO(char        * testFileName,
                 IOR_param_t * param)
{
    return IOR_Open_MPIIO(testFileName, param);
} /* IOR_Create_MPIIO() */


/******************************************************************************/
/*
 * Open a file through the MPIIO interface.  Setup file view.
 */

void *
IOR_Open_MPIIO(char        * testFileName,
               IOR_param_t * param)
{
    int          fd_mode                 = (int)0,
                 offsetFactor,
                 tasksPerFile,
                 transfersPerBlock       = param->blockSize
                                           / param->transferSize;
    struct       fileTypeStruct {
                     int globalSizes[2],
                         localSizes[2],
                         startIndices[2];
    }            fileTypeStruct;
    MPI_File   * fd;
    MPI_Comm     comm;
    MPI_Info     mpiHints                = MPI_INFO_NULL;

    fd = (MPI_File *)malloc(sizeof(MPI_File));
    if (fd == NULL) ERR("Unable to malloc MPI_File");

    *fd = 0;

    /* set IOR file flags to MPIIO flags */
    /* -- file open flags -- */
    if (param->openFlags & IOR_RDONLY) {fd_mode |= MPI_MODE_RDONLY;}
    if (param->openFlags & IOR_WRONLY) {fd_mode |= MPI_MODE_WRONLY;}
    if (param->openFlags & IOR_RDWR)   {fd_mode |= MPI_MODE_RDWR;}
    if (param->openFlags & IOR_APPEND) {fd_mode |= MPI_MODE_APPEND;}
    if (param->openFlags & IOR_CREAT)  {fd_mode |= MPI_MODE_CREATE;}
    if (param->openFlags & IOR_EXCL)   {fd_mode |= MPI_MODE_EXCL;}
    if (param->openFlags & IOR_TRUNC) {
        fprintf(stdout, "File truncation not implemented in MPIIO\n");
    }
    if (param->openFlags & IOR_DIRECT) {
        fprintf(stdout, "O_DIRECT not implemented in MPIIO\n");
    }

    /*
     * MPI_MODE_UNIQUE_OPEN mode optimization eliminates the overhead of file
     * locking.  Only open a file in this mode when the file will not be con-
     * currently opened elsewhere, either inside or outside the MPI environment.
     */
    fd_mode |= MPI_MODE_UNIQUE_OPEN;

    if (param->filePerProc) {
        comm = MPI_COMM_SELF;
    } else {
        comm = testComm;
    }

    SetHints(&mpiHints, param->hintsFileName);
    /*
     * note that with MP_HINTS_FILTERED=no, all key/value pairs will
     * be in the info object.  The info object that is attached to
     * the file during MPI_File_open() will only contain those pairs
     * deemed valid by the implementation.
     */
    /* show hints passed to file */
    if (rank == 0 && param->showHints) {
        fprintf(stdout, "\nhints passed to MPI_File_open() {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }
    MPI_CHECK(MPI_File_open(comm, testFileName, fd_mode, mpiHints, fd),
              "cannot open file");

    /* show hints actually attached to file handle */
    if (rank == 0 && param->showHints) {
        MPI_CHECK(MPI_File_get_info(*fd, &mpiHints),
                  "cannot get file info");
        fprintf(stdout, "\nhints returned from opened file {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }

    /* preallocate space for file */
    if (param->preallocate && param->open == WRITE) {
        MPI_CHECK(MPI_File_preallocate(*fd,
            (MPI_Offset)(param->segmentCount*param->blockSize*param->numTasks)),
            "cannot preallocate file");
    }
    /* create file view */
    if (param->useFileView) {
        /* create contiguous transfer datatype */
        MPI_CHECK(MPI_Type_contiguous(param->transferSize / sizeof(IOR_size_t),
                                      MPI_LONG_LONG_INT, &param->transferType),
                  "cannot create contiguous datatype");
        MPI_CHECK(MPI_Type_commit(&param->transferType),
                  "cannot commit datatype");
        if (param->filePerProc) {
            offsetFactor = 0;
            tasksPerFile = 1;
        } else {
            offsetFactor = (rank + rankOffset) % param->numTasks;
            tasksPerFile = param->numTasks;
        }

        /*
         * create file type using subarray
         */
        fileTypeStruct.globalSizes[0]  = 1;
        fileTypeStruct.globalSizes[1]  = transfersPerBlock * tasksPerFile;
        fileTypeStruct.localSizes[0]   = 1;
        fileTypeStruct.localSizes[1]   = transfersPerBlock;
        fileTypeStruct.startIndices[0] = 0;
        fileTypeStruct.startIndices[1] = transfersPerBlock * offsetFactor;

        MPI_CHECK(MPI_Type_create_subarray(2, fileTypeStruct.globalSizes,
                                           fileTypeStruct.localSizes,
                                           fileTypeStruct.startIndices,
                                           MPI_ORDER_C, param->transferType,
                                           &param->fileType),
                                           "cannot create subarray");
        MPI_CHECK(MPI_Type_commit(&param->fileType), "cannot commit datatype");

        MPI_CHECK(MPI_File_set_view(*fd, (MPI_Offset)0,
                                    param->transferType, param->fileType,
                                    "native", (MPI_Info)MPI_INFO_NULL),
                                    "cannot set file view");
    }
    return((void *)fd);
} /* IOR_Open_MPIIO() */


/******************************************************************************/
/*
 * Write or read access to file using the MPIIO interface.
 */

IOR_offset_t
IOR_Xfer_MPIIO(int            access,
               void         * fd,
               IOR_size_t   * buffer,
               IOR_offset_t   length,
               IOR_param_t  * param)
{
    int (MPIAPI *Access)        (MPI_File, void *, int,
                                 MPI_Datatype, MPI_Status *);
    int (MPIAPI *Access_at)     (MPI_File, MPI_Offset, void *, int,
                                 MPI_Datatype,MPI_Status *);
    int (MPIAPI *Access_all)    (MPI_File, void *, int,
                                 MPI_Datatype, MPI_Status *);
    int (MPIAPI *Access_at_all) (MPI_File, MPI_Offset, void *, int,
                                 MPI_Datatype, MPI_Status *);
    /*
     * this needs to be properly implemented:
     *
     *   int        (*Access_ordered)(MPI_File, void *, int,
     *                                MPI_Datatype, MPI_Status *);
     */
    MPI_Status   status;

    /* point functions to appropriate MPIIO calls */
    if (access == WRITE) { /* WRITE */
        Access         = MPI_File_write;
        Access_at      = MPI_File_write_at;
        Access_all     = MPI_File_write_all;
        Access_at_all  = MPI_File_write_at_all;
        /*
         * this needs to be properly implemented:
         *
         *   Access_ordered = MPI_File_write_ordered;
         */
    } else {               /* READ or CHECK */
        Access         = MPI_File_read;
        Access_at      = MPI_File_read_at;
        Access_all     = MPI_File_read_all;
        Access_at_all  = MPI_File_read_at_all;
        /*
         * this needs to be properly implemented:
         *
         *   Access_ordered = MPI_File_read_ordered;
         */
    }

    /*
     * 'useFileView' uses derived datatypes and individual file pointers
     */
    if (param->useFileView) {
        /* find offset in file */
        if (SeekOffset_MPIIO(*(MPI_File *)fd, param->offset, param) < 0) {
            /* if unsuccessful */
            length = -1;
        } else {
            /*
             * 'useStridedDatatype' fits multi-strided pattern into a datatype;
             * must use 'length' to determine repetitions (fix this for
             * multi-segments someday, WEL):
             * e.g.,  'IOR -s 2 -b 32K -t 32K -a MPIIO -S'
             */
            if (param->useStridedDatatype) {
                length = param->segmentCount;
            } else {
                length = 1;
            }
            if (param->collective) {
                /* individual, collective call */
                MPI_CHECK(Access_all(*(MPI_File *)fd, buffer, length,
                                     param->transferType, &status),
                          "cannot access collective");
            } else {
                /* individual, noncollective call */
                MPI_CHECK(Access(*(MPI_File *)fd, buffer, length,
                                 param->transferType, &status),
                          "cannot access noncollective");
            }
            length *= param->transferSize;       /* for return value in bytes */
        }
    } else {
        /*
         * !useFileView does not use derived datatypes, but it uses either
         * shared or explicit file pointers
         */
        if (param->useSharedFilePointer) {
            /* find offset in file */
            if (SeekOffset_MPIIO(*(MPI_File *)fd, param->offset, param) < 0) {
                /* if unsuccessful */
                length = -1;
            } else {
                /* shared, collective call */
                /*
                 * this needs to be properly implemented:
                 *
                 *   MPI_CHECK(Access_ordered(fd.MPIIO, buffer, length,
                 *                            MPI_BYTE, &status),
                 *             "cannot access shared, collective");
                 */
                fprintf(stdout, "useSharedFilePointer not implemented\n");
            }
        } else {
            if (param->collective) {
                /* explicit, collective call */
                MPI_CHECK(Access_at_all(*(MPI_File *)fd, param->offset,
                                        buffer, length, MPI_BYTE, &status),
                          "cannot access explicit, collective");
            } else {
                /* explicit, noncollective call */
                MPI_CHECK(Access_at(*(MPI_File *)fd, param->offset, buffer,
                                    length, MPI_BYTE, &status),
                          "cannot access explicit, noncollective");
            }
        }
    }
    return(length);
} /* IOR_Xfer_MPIIO() */
    

/******************************************************************************/
/*
 * Perform fsync().
 */

void
IOR_Fsync_MPIIO(void * fd, IOR_param_t * param)
{
    ;
} /* IOR_Fsync_MPIIO() */

    
/******************************************************************************/
/*
 * Close a file through the MPIIO interface.
 */

void
IOR_Close_MPIIO(void        *  fd,
                IOR_param_t * param)
{
    MPI_CHECK(MPI_File_close((MPI_File *)fd), "cannot close file");
    if ((param->useFileView == TRUE) && (param->fd_fppReadCheck == NULL)) {
        /*
         * need to free the datatype, so done in the close process
         */
        MPI_CHECK(MPI_Type_free(&param->fileType),
                  "cannot free MPI file datatype");
        MPI_CHECK(MPI_Type_free(&param->transferType),
                  "cannot free MPI transfer datatype");
    }
    free(fd);
} /* IOR_Close_MPIIO() */


/******************************************************************************/
/*
 * Delete a file through the MPIIO interface.
 */

void
IOR_Delete_MPIIO(char * testFileName, IOR_param_t * param)
{
    MPI_CHECK(MPI_File_delete(testFileName, (MPI_Info)MPI_INFO_NULL),
              "cannot delete file");
} /* IOR_Delete_MPIIO() */


/******************************************************************************/
/*
 * Determine api version.
 */

void
IOR_SetVersion_MPIIO(IOR_param_t *test)
{
    int version, subversion;
    MPI_CHECK(MPI_Get_version(&version, &subversion),
              "cannot get MPI version");
    sprintf(test->apiVersion, "%s (version=%d, subversion=%d)",
            test->api, version, subversion);
} /* IOR_SetVersion_MPIIO() */


/************************ L O C A L   F U N C T I O N S ***********************/

/******************************************************************************/
/*
 * Seek to offset in file using the MPIIO interface.
 */

static IOR_offset_t
SeekOffset_MPIIO(MPI_File       fd,
                 IOR_offset_t   offset,
                 IOR_param_t  * param)
{
    int          offsetFactor,
                 tasksPerFile;
    IOR_offset_t tempOffset;

    tempOffset = offset;

    if (param->filePerProc) {
        offsetFactor = 0;
        tasksPerFile = 1;
    } else {
        offsetFactor = (rank + rankOffset) % param->numTasks;
        tasksPerFile = param->numTasks;
    }
    if (param->useFileView) {
        /* recall that offsets in a file view are
           counted in units of transfer size */
        if (param->filePerProc) {
            tempOffset = tempOffset / param->transferSize;
        } else {
            /* 
             * this formula finds a file view offset for a task
             * from an absolute offset
             */
            tempOffset = ((param->blockSize / param->transferSize)
                          * (tempOffset / (param->blockSize * tasksPerFile)))
                         + (((tempOffset % (param->blockSize * tasksPerFile))
                          - (offsetFactor * param->blockSize))
                           / param->transferSize);
        }
    }
    MPI_CHECK(MPI_File_seek(fd, tempOffset, MPI_SEEK_SET),
              "cannot seek offset");
    return(offset);
} /* SeekOffset_MPIIO() */


/******************************************************************************/
/*
 * Use MPI_File_get_size() to return aggregate file size.
 */

IOR_offset_t
IOR_GetFileSize_MPIIO(IOR_param_t * test,
                      MPI_Comm      testComm,
                      char        * testFileName)
{
    IOR_offset_t aggFileSizeFromStat,
                 tmpMin, tmpMax, tmpSum;
    MPI_File     fd;

    MPI_CHECK(MPI_File_open(testComm, testFileName, MPI_MODE_RDONLY,
                            MPI_INFO_NULL, &fd),
              "cannot open file to get file size");
    MPI_CHECK(MPI_File_get_size(fd, &aggFileSizeFromStat),
              "cannot get file size");
    MPI_CHECK(MPI_File_close(&fd), "cannot close file");

    if (test->filePerProc == TRUE) {
        MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpSum, 1,
                                MPI_LONG_LONG_INT, MPI_SUM, testComm),
                  "cannot total data moved");
        aggFileSizeFromStat = tmpSum;
    } else {
        MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMin, 1,
                                MPI_LONG_LONG_INT, MPI_MIN, testComm),
                  "cannot total data moved");
        MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMax, 1,
                  MPI_LONG_LONG_INT, MPI_MAX, testComm),
                  "cannot total data moved");
        if (tmpMin != tmpMax) {
            if (rank == 0) {
                WARN("inconsistent file size by different tasks");
            }
            /* incorrect, but now consistent across tasks */
            aggFileSizeFromStat = tmpMin;
        }
    }

    return(aggFileSizeFromStat);

} /* IOR_GetFileSize_MPIIO() */
