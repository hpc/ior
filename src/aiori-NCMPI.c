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
* Implement abstract I/O interface for Parallel NetCDF (NCMPI).
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pnetcdf.h>

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"

#define NUM_DIMS 3              /* number of dimensions to data set */

/*
 * NCMPI_CHECK will display a custom error message and then exit the program
 */
#define NCMPI_CHECK(NCMPI_RETURN, MSG) do {                              \
    int _NCMPI_RETURN = (NCMPI_RETURN);                                  \
                                                                         \
    if (_NCMPI_RETURN != NC_NOERR) {                                     \
        fprintf(stdout, "** error **\n");                                \
        fprintf(stdout, "ERROR in %s (line %d): %s.\n",                  \
                __FILE__, __LINE__, MSG);                                \
        fprintf(stdout, "ERROR: %s.\n", ncmpi_strerror(_NCMPI_RETURN));  \
        fprintf(stdout, "** exiting **\n");                              \
        exit(EXIT_FAILURE);                                              \
    }                                                                    \
} while(0)

/**************************** P R O T O T Y P E S *****************************/

static int GetFileMode(int flags);

static aiori_fd_t *NCMPI_Create(char *, int iorflags, aiori_mod_opt_t *);
static aiori_fd_t *NCMPI_Open(char *, int iorflags, aiori_mod_opt_t *);
static IOR_offset_t NCMPI_Xfer(int, aiori_fd_t *, IOR_size_t *,
                               IOR_offset_t, IOR_offset_t, aiori_mod_opt_t *);
static void NCMPI_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void NCMPI_Delete(char *, aiori_mod_opt_t *);
static char *NCMPI_GetVersion();
static void NCMPI_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static IOR_offset_t NCMPI_GetFileSize(aiori_mod_opt_t *, char *);
static int NCMPI_Access(const char *, int, aiori_mod_opt_t *);

/************************** D E C L A R A T I O N S ***************************/
static aiori_xfer_hint_t * hints = NULL;

static void NCMPI_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;

  MPIIO_xfer_hints(params);
}

typedef struct {
  mpiio_options_t mpio;

  /* runtime variables */
  int var_id;                      /* variable id handle for data set */
  int firstReadCheck;
  int startDataSet;
} ncmpi_options_t;


static option_help * NCMPI_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  ncmpi_options_t * o = malloc(sizeof(ncmpi_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(ncmpi_options_t));
  }else{
    memset(o, 0, sizeof(ncmpi_options_t));
  }
  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
    {0, "ncmpi.hintsFileName","Full name for hints file", OPTION_OPTIONAL_ARGUMENT, 's', & o->mpio.hintsFileName},
    {0, "ncmpi.showHints",    "Show MPI hints", OPTION_FLAG, 'd', & o->mpio.showHints},
    {0, "ncmpi.preallocate",   "Preallocate file size", OPTION_FLAG, 'd', & o->mpio.preallocate},
    {0, "ncmpi.useStridedDatatype", "put strided access into datatype", OPTION_FLAG, 'd', & o->mpio.useStridedDatatype},
    {0, "ncmpi.useFileView",  "Use MPI_File_set_view", OPTION_FLAG, 'd', & o->mpio.useFileView},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

ior_aiori_t ncmpi_aiori = {
        .name = "NCMPI",
        .name_legacy = NULL,
        .create = NCMPI_Create,
        .open = NCMPI_Open,
        .xfer = NCMPI_Xfer,
        .close = NCMPI_Close,
        .remove = NCMPI_Delete,
        .get_version = NCMPI_GetVersion,
        .fsync = NCMPI_Fsync,
        .get_file_size = NCMPI_GetFileSize,
        .statfs = aiori_posix_statfs,
        .mkdir = aiori_posix_mkdir,
        .rmdir = aiori_posix_rmdir,
        .access = NCMPI_Access,
        .stat = aiori_posix_stat,
        .get_options = NCMPI_options,
        .xfer_hints = NCMPI_xfer_hints,
};

/***************************** F U N C T I O N S ******************************/

/*
 * Create and open a file through the NCMPI interface.
 */
static aiori_fd_t *NCMPI_Create(char *testFileName, int iorflags, aiori_mod_opt_t * param)
{
        int *fd;
        int fd_mode;
        MPI_Info mpiHints = MPI_INFO_NULL;
        ncmpi_options_t * o = (ncmpi_options_t*) param;

        /* read and set MPI file hints from hintsFile */
        SetHints(&mpiHints, o->mpio.hintsFileName);
        if (rank == 0 && o->mpio.showHints) {
                fprintf(stdout, "\nhints passed to MPI_File_open() {\n");
                ShowHints(&mpiHints);
                fprintf(stdout, "}\n");
        }

        fd = (int *)malloc(sizeof(int));
        if (fd == NULL)
                ERR("malloc() failed");

        fd_mode = GetFileMode(iorflags);
        NCMPI_CHECK(ncmpi_create(testComm, testFileName, fd_mode,
                                 mpiHints, fd), "cannot create file");

        /* free up the mpiHints object */
        if (mpiHints != MPI_INFO_NULL)
            MPI_CHECK(MPI_Info_free(&mpiHints), "cannot free file info");

#if defined(PNETCDF_VERSION_MAJOR) && (PNETCDF_VERSION_MAJOR > 1 || PNETCDF_VERSION_MINOR >= 2)
        /* ncmpi_get_file_info is first available in 1.2.0 */
        if (rank == 0 && o->mpio.showHints) {
            MPI_Info info_used;
            NCMPI_CHECK(ncmpi_get_file_info(*fd, &info_used), "cannot inquire file info");
            /* print the MPI file hints currently used */
            fprintf(stdout, "\nhints returned from opened file {\n");
            ShowHints(&info_used);
            fprintf(stdout, "}\n");
            MPI_CHECK(MPI_Info_free(&info_used), "cannot free file info");
        }
#endif

        return (aiori_fd_t*)(fd);
}

/*
 * Open a file through the NCMPI interface.
 */
static aiori_fd_t *NCMPI_Open(char *testFileName, int iorflags, aiori_mod_opt_t * param)
{
        int *fd;
        int fd_mode;
        MPI_Info mpiHints = MPI_INFO_NULL;
        ncmpi_options_t * o = (ncmpi_options_t*) param;

        /* read and set MPI file hints from hintsFile */
        SetHints(&mpiHints, o->mpio.hintsFileName);
        if (rank == 0 && o->mpio.showHints) {
                fprintf(stdout, "\nhints passed to MPI_File_open() {\n");
                ShowHints(&mpiHints);
                fprintf(stdout, "}\n");
        }

        fd = (int *)malloc(sizeof(int));
        if (fd == NULL)
                ERR("malloc() failed");

        fd_mode = GetFileMode(iorflags);
        NCMPI_CHECK(ncmpi_open(testComm, testFileName, fd_mode,
                               mpiHints, fd), "cannot open file");

        /* free up the mpiHints object */
        if (mpiHints != MPI_INFO_NULL)
            MPI_CHECK(MPI_Info_free(&mpiHints), "cannot free file info");

#if defined(PNETCDF_VERSION_MAJOR) && (PNETCDF_VERSION_MAJOR > 1 || PNETCDF_VERSION_MINOR >= 2)
        /* ncmpi_get_file_info is first available in 1.2.0 */
        if (rank == 0 && o->mpio.showHints) {
            MPI_Info info_used;
            MPI_CHECK(ncmpi_get_file_info(*fd, &info_used),
                      "cannot inquire file info");
            /* print the MPI file hints currently used */
            fprintf(stdout, "\nhints returned from opened file {\n");
            ShowHints(&info_used);
            fprintf(stdout, "}\n");
            MPI_CHECK(MPI_Info_free(&info_used), "cannot free file info");
        }
#endif

        return (aiori_fd_t*)(fd);
}

/*
 * Write or read access to file using the NCMPI interface.
 */
static IOR_offset_t NCMPI_Xfer(int access, aiori_fd_t *fd, IOR_size_t * buffer, IOR_offset_t transferSize, IOR_offset_t offset, aiori_mod_opt_t * param)
{
        signed char *bufferPtr = (signed char *)buffer;
        ncmpi_options_t * o = (ncmpi_options_t*) param;
        int var_id, dim_id[NUM_DIMS];
        MPI_Offset bufSize[NUM_DIMS], offsets[NUM_DIMS];
        IOR_offset_t segmentPosition;
        int segmentNum, transferNum;

        /* determine by offset if need to start data set */
        if (hints->filePerProc == TRUE) {
                segmentPosition = (IOR_offset_t) 0;
        } else {
                segmentPosition = (IOR_offset_t) ((rank + rankOffset) % hints->numTasks) * hints->blockSize;
        }
        if ((int)(offset - segmentPosition) == 0) {
                o->startDataSet = TRUE;
                /*
                 * this toggle is for the read check operation, which passes through
                 * this function twice; note that this function will open a data set
                 * only on the first read check and close only on the second
                 */
                if (access == READCHECK) {
                  o->firstReadCheck = ! o->firstReadCheck;
                }
        }

        if (o->startDataSet == TRUE &&
            (access != READCHECK || o->firstReadCheck == TRUE)) {
                if (access == WRITE) {
                        int numTransfers = hints->blockSize / hints->transferSize;

                        /* reshape 1D array to 3D array:
                           [segmentCount*numTasks][numTransfers][transferSize]
                           Requirement: none of these dimensions should be > 4G,
                         */
                        NCMPI_CHECK(ncmpi_def_dim
                                    (*(int *)fd, "segments_times_np",
                                     NC_UNLIMITED, &dim_id[0]),
                                    "cannot define data set dimensions");
                        NCMPI_CHECK(ncmpi_def_dim
                                    (*(int *)fd, "number_of_transfers",
                                     numTransfers, &dim_id[1]),
                                    "cannot define data set dimensions");
                        NCMPI_CHECK(ncmpi_def_dim
                                    (*(int *)fd, "transfer_size",
                                     hints->transferSize, &dim_id[2]),
                                    "cannot define data set dimensions");
                        NCMPI_CHECK(ncmpi_def_var
                                    (*(int *)fd, "data_var", NC_BYTE, NUM_DIMS,
                                     dim_id, &var_id),
                                    "cannot define data set variables");
                        NCMPI_CHECK(ncmpi_enddef(*(int *)fd),
                                    "cannot close data set define mode");

                } else {
                        NCMPI_CHECK(ncmpi_inq_varid
                                    (*(int *)fd, "data_var", &var_id),
                                    "cannot retrieve data set variable");
                }

                if (hints->collective == FALSE) {
                        NCMPI_CHECK(ncmpi_begin_indep_data(*(int *)fd),
                                    "cannot enable independent data mode");
                }

                o->var_id = var_id;
                o->startDataSet = FALSE;
        }

        var_id = o->var_id;

        /* calculate the segment number */
        segmentNum = offset / (hints->numTasks * hints->blockSize);

        /* calculate the transfer number in each block */
        transferNum = offset % hints->blockSize / hints->transferSize;

        /* read/write the 3rd dim of the dataset, each is of
           amount param->transferSize */
        bufSize[0] = 1;
        bufSize[1] = 1;
        bufSize[2] = transferSize;

        offsets[0] = segmentNum * hints->numTasks + rank;
        offsets[1] = transferNum;
        offsets[2] = 0;

        /* access the file */
        if (access == WRITE) {  /* WRITE */
                if (hints->collective) {
                        NCMPI_CHECK(ncmpi_put_vara_schar_all
                                    (*(int *)fd, var_id, offsets, bufSize, bufferPtr),
                                    "cannot write to data set");
                } else {
                        NCMPI_CHECK(ncmpi_put_vara_schar
                                    (*(int *)fd, var_id, offsets, bufSize, bufferPtr),
                                    "cannot write to data set");
                }
        } else {                /* READ or CHECK */
                if (hints->collective == TRUE) {
                        NCMPI_CHECK(ncmpi_get_vara_schar_all
                                    (*(int *)fd, var_id, offsets, bufSize, bufferPtr),
                                    "cannot read from data set");
                } else {
                        NCMPI_CHECK(ncmpi_get_vara_schar
                                    (*(int *)fd, var_id, offsets, bufSize, bufferPtr),
                                    "cannot read from data set");
                }
        }

        return (transferSize);
}

/*
 * Perform fsync().
 */
static void NCMPI_Fsync(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        NCMPI_CHECK(ncmpi_sync(*(int *)fd), "cannot sync file");
}

/*
 * Close a file through the NCMPI interface.
 */
static void NCMPI_Close(aiori_fd_t *fd, aiori_mod_opt_t * param)
{
        NCMPI_CHECK(ncmpi_close(*(int *)fd), "cannot close file");
        free(fd);
}

/*
 * Delete a file through the NCMPI interface.
 */
static void NCMPI_Delete(char *testFileName, aiori_mod_opt_t * param)
{
        NCMPI_CHECK(ncmpi_delete(testFileName, MPI_INFO_NULL), "cannot delete file");
}

/*
 * Determine api version.
 */
static char* NCMPI_GetVersion()
{
  return (char *)ncmpi_inq_libvers();
}

/*
 * Return the correct file mode for NCMPI.
 */
static int GetFileMode(int flags)
{
        int fd_mode = 0;

        /* set IOR file flags to NCMPI flags */
        /* -- file open flags -- */
        if (flags & IOR_RDONLY) {
                fd_mode |= NC_NOWRITE;
        }
        if (flags & IOR_WRONLY) {
            WARN("File write only not implemented in NCMPI");
        }
        if (flags & IOR_RDWR) {
                fd_mode |= NC_WRITE;
        }
        if (flags & IOR_APPEND) {
            WARN("File append not implemented in NCMPI");
        }
        if (flags & IOR_CREAT) {
                fd_mode |= NC_CLOBBER;
        }
        if (flags & IOR_EXCL) {
            WARN("Exclusive access not implemented in NCMPI");
        }
        if (flags & IOR_TRUNC) {
                fd_mode |= NC_CLOBBER;
        }
        if (flags & IOR_DIRECT) {
            WARN("O_DIRECT not implemented in NCMPI");
        }

        /* to enable > 4GB variable size */
        fd_mode |= NC_64BIT_DATA;

        return (fd_mode);
}

/*
 * Use MPIIO call to get file size.
 */
static IOR_offset_t NCMPI_GetFileSize(aiori_mod_opt_t * opt,
                                      char *testFileName)
{
        return(MPIIO_GetFileSize(opt, testFileName));
}

/*
 * Use MPIIO call to check for access.
 */
static int NCMPI_Access(const char *path, int mode, aiori_mod_opt_t *param)
{
        return(MPIIO_Access(path, mode, param));
}
