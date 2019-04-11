/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Implement abstract I/O interface for NetCDF-4 with parallel support (NC4).
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <netcdf.h>
#include <netcdf_par.h>

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"

#define NUM_DIMS 3              /* number of dimensions to data set */

/*
 * NC4_CHECK will display a custom error message and then exit the program
 */
#define NC4_CHECK(NC4_RETURN, MSG) do {                              \
                                                                         \
    if (NC4_RETURN < 0) {                                              \
        fprintf(stdout, "** error **\n");                                \
        fprintf(stdout, "ERROR in %s (line %d): %s.\n",                  \
                __FILE__, __LINE__, MSG);                                \
        fprintf(stdout, "ERROR: %s.\n", nc_strerror(NC4_RETURN));   \
        fprintf(stdout, "** exiting **\n");                              \
        exit(-1);                                                        \
    }                                                                    \
} while(0)

/**************************** P R O T O T Y P E S *****************************/

static int GetFileMode(IOR_param_t *);

static char *GetNamedMessage(char * message, char * name);

static void *NC4_Create(char *, IOR_param_t *);

static void *NC4_Open(char *, IOR_param_t *);

static IOR_offset_t NC4_Xfer(int, void *, IOR_size_t *,
                             IOR_offset_t, IOR_param_t *);

static void NC4_Close(void *, IOR_param_t *);

static void NC4_Delete(char *, IOR_param_t *);

static char *NC4_GetVersion();

static void NC4_Fsync(void *, IOR_param_t *);

static IOR_offset_t NC4_GetFileSize(IOR_param_t *, MPI_Comm, char *);

static int NC4_Access(const char *, int, IOR_param_t *);

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t nc4_aiori = {
        .name = "NC4",
        .name_legacy = NULL,
        .create = NC4_Create,
        .open = NC4_Open,
        .xfer = NC4_Xfer,
        .close = NC4_Close,
        .delete = NC4_Delete,
        .get_version = NC4_GetVersion,
        .fsync = NC4_Fsync,
        .get_file_size = NC4_GetFileSize,
        .statfs = aiori_posix_statfs,
        .mkdir = aiori_posix_mkdir,
        .rmdir = aiori_posix_rmdir,
        .access = NC4_Access,
        .stat = aiori_posix_stat,
};

/***************************** F U N C T I O N S ******************************/

/*
 * Create and open a file through the NC4 interface.
 */
static void *NC4_Create(char *testFileName, IOR_param_t *param) {
    int *fd;
    int fd_mode;
    MPI_Info mpiHints = MPI_INFO_NULL;

    SetHints(&mpiHints, param->hintsFileName);
    if (rank == 0 && param->showHints) {
        fprintf(stdout, "\nhints passed to nc_create_par() {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }

    fd = (int *) malloc(sizeof(int));
    if (fd == NULL)
        ERR("malloc() failed");

    fd_mode = GetFileMode(param);
    NC4_CHECK(nc_create_par(testFileName, fd_mode, testComm, mpiHints, fd),
              "cannot create file");

    return (fd);
}

/*
 * Open a file through the NC4 interface.
 */
static void *NC4_Open(char *testFileName, IOR_param_t *param) {
    int *fd;
    int fd_mode;
    MPI_Info mpiHints = MPI_INFO_NULL;

    SetHints(&mpiHints, param->hintsFileName);
    if (rank == 0 && param->showHints) {
        fprintf(stdout, "\nhints passed to nc_open_par() {\n");
        ShowHints(&mpiHints);
        fprintf(stdout, "}\n");
    }

    fd = (int *) malloc(sizeof(int));
    if (fd == NULL)
        ERR("malloc() failed");

    fd_mode = GetFileMode(param);
    NC4_CHECK(nc_open_par(testFileName, fd_mode, testComm, mpiHints, fd),
              "cannot open file");

    return (fd);
}

/*
 * Write or read access to file using the NC4 interface.
 */
static IOR_offset_t NC4_Xfer(int access, void *fd, IOR_size_t *buffer,
                             IOR_offset_t length, IOR_param_t *param) {
    signed char *bufferPtr = (signed char *) buffer;
    static int firstReadCheck = FALSE, startDataSet;
    int var_id, dim_id[NUM_DIMS];
    size_t bufSize[NUM_DIMS], offset[NUM_DIMS], chunk_sizes[NUM_DIMS];
    IOR_offset_t segmentPosition;
    int segmentNum, transferNum;

    if (length != param->transferSize) {
        char errMsg[256];
        sprintf(errMsg, "length(%lld) != param->transferSize(%lld)\n",
                length, param->transferSize);
        NC4_CHECK(-1, errMsg);
    }

    if (param->filePerProc == TRUE) {
        segmentPosition = (IOR_offset_t) 0;
    } else {
        segmentPosition =
                (IOR_offset_t) ((rank + rankOffset) % param->numTasks)
                * param->blockSize;
    }
    if ((int) (param->offset - segmentPosition) == 0) {
        startDataSet = TRUE;
        if (access == READCHECK) {
            if (firstReadCheck == TRUE) {
                firstReadCheck = FALSE;
            } else {
                firstReadCheck = TRUE;
            }
        }
    }

    if (startDataSet == TRUE && (access != READCHECK || firstReadCheck == TRUE)) {
        char * message;
        char * name;

        if (access == WRITE) {
            int numSegmentsTimesN = param->segmentCount * param->numTasks;

            message = "cannot define dataset dimension";
            name = "segments_times_np";
            NC4_CHECK(nc_def_dim(*(int *) fd, name, numSegmentsTimesN, &dim_id[0]),
                      GetNamedMessage(message, name));

            int numTransfers = param->blockSize / param->transferSize;

            name = "number_of_transfers";
            NC4_CHECK(nc_def_dim(*(int *) fd, name, numTransfers, &dim_id[1]),
                      GetNamedMessage(message, name));

            name = "transfer_size";
            NC4_CHECK(nc_def_dim(*(int *) fd, name, param->transferSize, &dim_id[2]),
                      GetNamedMessage(message, name));

            message = "cannot define dataset variable";
            name = "data_var";
            NC4_CHECK(nc_def_var(*(int *) fd, name, NC_BYTE, NUM_DIMS, dim_id, &var_id),
                      GetNamedMessage(message, name));

            message = "cannot define chunksizes for dataset variable";
            name = "data_var";
            chunk_sizes[0] = 1;
            chunk_sizes[1] = 1;
            chunk_sizes[2] = param->transferSize;
            NC4_CHECK(nc_def_var_chunking(*(int *) fd, var_id, NC_CHUNKED, &chunk_sizes),
                      GetNamedMessage(message, name));

            NC4_CHECK(nc_enddef(*(int *) fd), "cannot close dataset define mode");

        } else {
            message = "cannot retrieve dataset variable";
            name = "data_var";
            NC4_CHECK(nc_inq_varid(*(int *) fd, name, &var_id),
                      GetNamedMessage(message, name));
        }

        if (param->collective) {
            NC4_CHECK(nc_var_par_access(*(int *) fd, var_id, NC_COLLECTIVE),
                      "cannot enable collective data mode");
        }

        param->var_id = var_id;
        startDataSet = FALSE;
    }

    var_id = param->var_id;

    segmentNum = param->offset / (param->numTasks * param->blockSize);

    transferNum = param->offset % param->blockSize / param->transferSize;

    bufSize[0] = 1;
    bufSize[1] = 1;
    bufSize[2] = param->transferSize;

    offset[0] = segmentNum * numTasksWorld + rank;
    offset[1] = transferNum;
    offset[2] = 0;

    if (access == WRITE) {
        NC4_CHECK(nc_put_vara_schar(*(int *) fd, var_id, offset, bufSize, bufferPtr),
                  "cannot write to dataset");
    } else {
        NC4_CHECK(nc_get_vara_schar(*(int *) fd, var_id, offset, bufSize, bufferPtr),
                  "cannot read from dataset");
    }

    return (length);
}

/*
 * Perform sync().
 */
static void NC4_Fsync(void *fd, IOR_param_t *param) {
    NC4_CHECK(nc_sync(*(int *) fd), "cannot sync dataset to disk");
}

/*
 * Close a file through the NC4 interface.
 */
static void NC4_Close(void *fd, IOR_param_t *param) {
//    if (param->collective) {
//        NC4_CHECK(nc_var_par_access(*(int *) fd, var_id, NC_INDEPENDENT),
//                  "cannot disable collective data mode");
//    }
    NC4_CHECK(nc_close(*(int *) fd), "cannot close file");
    free(fd);
}

/*
 * Delete a file through the NC4 interface.
 */
static void NC4_Delete(char *testFileName, IOR_param_t *param) {
    return (MPIIO_Delete(testFileName, param));
}

/*
 * Determine api version.
 */
static char *NC4_GetVersion() {
    return (char *) nc_inq_libvers();
}

/*
 * Use MPIIO call to get file size.
 */
static IOR_offset_t NC4_GetFileSize(IOR_param_t *test, MPI_Comm testComm, char *testFileName) {
    return (MPIIO_GetFileSize(test, testComm, testFileName));
}

/*
 * Use MPIIO call to check for access.
 */
static int NC4_Access(const char *path, int mode, IOR_param_t *param) {
    return (MPIIO_Access(path, mode, param));
}

/*
 * Return the correct file mode for NC4.
 */
static int GetFileMode(IOR_param_t *param) {
    int fd_mode = 0;

    /* set IOR file flags to NC4 flags */
    if (param->openFlags & IOR_RDONLY) {
        fd_mode |= NC_NOWRITE;
    }
    if (param->openFlags & IOR_WRONLY) {
        fprintf(stdout, "File write only not implemented in NC4\n");
    }
    if (param->openFlags & IOR_RDWR) {
        fd_mode |= NC_WRITE;
    }
    if (param->openFlags & IOR_APPEND) {
        fprintf(stdout, "File append not implemented in NC4\n");
    }
    if (param->openFlags & IOR_CREAT) {
        fd_mode |= NC_CLOBBER;
    }
    if (param->openFlags & IOR_EXCL) {
        fprintf(stdout, "Exclusive access not implemented in NC4\n");
    }
    if (param->openFlags & IOR_TRUNC) {
        fprintf(stdout, "File truncation not implemented in NC4\n");
    }
    if (param->openFlags & IOR_DIRECT) {
        fprintf(stdout, "O_DIRECT not implemented in NC4\n");
    }

    fd_mode |= NC_NETCDF4;

    return (fd_mode);
}

/* Construct a message referencing a specific name */
static char * GetNamedMessage(char * message, char * name) {
    char * msg;
    strcpy(msg, message);
    strcat(msg, " (");
    strcat(msg, name);
    strcat(msg, ")");

    return (msg);
}