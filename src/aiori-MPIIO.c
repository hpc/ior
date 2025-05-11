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
* Implement abstract I/O interface for MPIIO.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ior.h"
#include "iordef.h"
#include "aiori.h"
#include "utilities.h"

#ifndef MPIAPI
#define MPIAPI                  /* defined as __stdcall on Windows */
#endif

/**************************** P R O T O T Y P E S *****************************/

static IOR_offset_t SeekOffset(MPI_File, IOR_offset_t, aiori_mod_opt_t *);

static aiori_fd_t *MPIIO_Create(char *, int iorflags, aiori_mod_opt_t *);
static aiori_fd_t *MPIIO_Open(char *, int flags, aiori_mod_opt_t *);
static IOR_offset_t MPIIO_Xfer(int, aiori_fd_t *, IOR_size_t *,
                                   IOR_offset_t, IOR_offset_t, aiori_mod_opt_t *);
static void MPIIO_Close(aiori_fd_t *, aiori_mod_opt_t *);
static char* MPIIO_GetVersion();
static void MPIIO_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static int MPIIO_check_params(aiori_mod_opt_t * options);

/************************** D E C L A R A T I O N S ***************************/

typedef struct{
  MPI_File     fd;
  MPI_Datatype transferType;       /* datatype for transfer */
  MPI_Datatype contigType;         /* elem datatype */
  MPI_Datatype fileType;           /* filetype for file view */
} mpiio_fd_t;

static option_help * MPIIO_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  mpiio_options_t * o = malloc(sizeof(mpiio_options_t));
  if (init_values != NULL){
    memcpy(o, init_values, sizeof(mpiio_options_t));
  }else{
    memset(o, 0, sizeof(mpiio_options_t));
  }
  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = { 
    {0, "mpiio.hintsFileName","Full name for hints file", OPTION_OPTIONAL_ARGUMENT, 's', & o->hintsFileName},
    {0, "mpiio.showHints",    "Show MPI hints", OPTION_FLAG, 'd', & o->showHints},
    {0, "mpiio.preallocate",   "Preallocate file size", OPTION_FLAG, 'd', & o->preallocate},
    {0, "mpiio.useStridedDatatype", "put strided access into datatype", OPTION_FLAG, 'd', & o->useStridedDatatype},
    //{'P', NULL,        "useSharedFilePointer -- use shared file pointer [not working]", OPTION_FLAG, 'd', & params->useSharedFilePointer},
    {0, "mpiio.useFileView",  "Use MPI_File_set_view", OPTION_FLAG, 'd', & o->useFileView},
      LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}


ior_aiori_t mpiio_aiori = {
        .name = "MPIIO",
        .name_legacy = NULL,
        .create = MPIIO_Create,
        .get_options = MPIIO_options,
        .xfer_hints = MPIIO_xfer_hints,
        .open = MPIIO_Open,
        .xfer = MPIIO_Xfer,
        .close = MPIIO_Close,
        .remove = MPIIO_Delete,
        .get_version = MPIIO_GetVersion,
        .fsync = MPIIO_Fsync,
        .get_file_size = MPIIO_GetFileSize,
        .statfs = aiori_posix_statfs,
        .mkdir = aiori_posix_mkdir,
        .rmdir = aiori_posix_rmdir,
        .access = MPIIO_Access,
        .stat = aiori_posix_stat,
        .check_params = MPIIO_check_params
};

/***************************** F U N C T I O N S ******************************/
static aiori_xfer_hint_t * hints = NULL;

void MPIIO_xfer_hints(aiori_xfer_hint_t * params){
  hints = params;
}

static int MPIIO_check_params(aiori_mod_opt_t * module_options){
  mpiio_options_t * param = (mpiio_options_t*) module_options;
  if ((param->useFileView == TRUE)
    && (sizeof(MPI_Aint) < 8)   /* used for 64-bit datatypes */
    &&((hints->numTasks * hints->blockSize) >
       (2 * (IOR_offset_t) GIBIBYTE)))
        ERR("segment size must be < 2GiB");
  if (param->useSharedFilePointer)
        ERR("shared file pointer not implemented");
  if (param->useStridedDatatype && (hints->blockSize < sizeof(IOR_size_t)
                                      || hints->transferSize <
                                      sizeof(IOR_size_t)))
          ERR("need larger file size for strided datatype in MPIIO");
  if (hints->randomOffset && hints->collective)
          ERR("random offset not available with collective MPIIO");
  if (hints->randomOffset && param->useFileView)
          ERR("random offset not available with MPIIO fileviews");

  return 0;
}

/*
 * Try to access a file through the MPIIO interface.
 */
int MPIIO_Access(const char *path, int mode, aiori_mod_opt_t *module_options)
{
    if(hints->dryRun){
      return MPI_SUCCESS;
    }
    mpiio_options_t * param = (mpiio_options_t*) module_options;
    MPI_File fd;
    int mpi_mode = MPI_MODE_UNIQUE_OPEN;
    MPI_Info mpiHints = MPI_INFO_NULL;

    if ((mode & W_OK) && (mode & R_OK))
        mpi_mode |= MPI_MODE_RDWR;
    else if (mode & W_OK)
        mpi_mode |= MPI_MODE_WRONLY;
    else
        mpi_mode |= MPI_MODE_RDONLY;

    SetHints(&mpiHints, param->hintsFileName);

    // first perform a stat to check whether this is a directory
    struct stat st;
    int ret = stat(path, &st);

    if (ret == 0)
    {
        // if this is a directory, redirect to aiori_posix_access
        if (S_ISDIR(st.st_mode))
            return aiori_posix_access(path, mode, module_options);
    }

    // otherwise check if we can access the file with MPI (we also verify MPI
    // handles a file that doesn't exist or otherwise fails in stat above)
    ret = MPI_File_open(MPI_COMM_SELF, path, mpi_mode, mpiHints, &fd);

    if (!ret)
        MPI_File_close(&fd);

    if (mpiHints != MPI_INFO_NULL)
        MPI_CHECK(MPI_Info_free(&mpiHints), "MPI_Info_free failed");
    return ret;
}

/*
 * Create and open a file through the MPIIO interface.
 */
static aiori_fd_t *MPIIO_Create(char *testFileName, int iorflags, aiori_mod_opt_t * module_options)
{
  return MPIIO_Open(testFileName, iorflags, module_options);
}

/*
 * Open a file through the MPIIO interface.  Setup file view.
 */
static aiori_fd_t *MPIIO_Open(char *testFileName, int flags, aiori_mod_opt_t * module_options)
{
        mpiio_options_t * param = (mpiio_options_t*) module_options;
        int fd_mode = (int)0,
            offsetFactor,
            tasksPerFile,
            transfersPerBlock = hints->blockSize / hints->transferSize;


        mpiio_fd_t * mfd = malloc(sizeof(mpiio_fd_t));
        memset(mfd, 0, sizeof(mpiio_fd_t));
        MPI_Comm comm;
        MPI_Info mpiHints = MPI_INFO_NULL;

        /* set IOR file flags to MPIIO flags */
        /* -- file open flags -- */
        if (flags & IOR_RDONLY) {
                fd_mode |= MPI_MODE_RDONLY;
        }
        if (flags & IOR_WRONLY) {
                fd_mode |= MPI_MODE_WRONLY;
        }
        if (flags & IOR_RDWR) {
                fd_mode |= MPI_MODE_RDWR;
        }
        if (flags & IOR_APPEND) {
                fd_mode |= MPI_MODE_APPEND;
        }
        if (flags & IOR_CREAT) {
                fd_mode |= MPI_MODE_CREATE;
        }
        if (flags & IOR_EXCL) {
                fd_mode |= MPI_MODE_EXCL;
        }
        if (flags & IOR_DIRECT) {
                fprintf(stdout, "O_DIRECT not implemented in MPIIO\n");
        }

        /*
         * MPI_MODE_UNIQUE_OPEN mode optimization eliminates the overhead of file
         * locking.  Only open a file in this mode when the file will not be con-
         * currently opened elsewhere, either inside or outside the MPI environment.
         */
        fd_mode |= MPI_MODE_UNIQUE_OPEN;

        if (hints->filePerProc) {
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
        if(! hints->dryRun){
            MPI_CHECKF(MPI_File_open(comm, testFileName, fd_mode, mpiHints, & mfd->fd),
                       "cannot open file: %s", testFileName);
            if (flags & IOR_TRUNC) {
                MPI_CHECKF(MPI_File_set_size(mfd->fd, 0), "cannot truncate file: %s", testFileName);
            }
        }

        /* show hints actually attached to file handle */
        if (rank == 0 && param->showHints && ! hints->dryRun) {
                if (mpiHints != MPI_INFO_NULL)
                        MPI_CHECK(MPI_Info_free(&mpiHints), "MPI_Info_free failed");
                MPI_CHECK(MPI_File_get_info(mfd->fd, &mpiHints),
                          "cannot get file info");
                fprintf(stdout, "\nhints returned from opened file {\n");
                ShowHints(&mpiHints);
                fprintf(stdout, "}\n");
        }

        /* preallocate space for file */
        if (param->preallocate && flags & IOR_CREAT && ! hints->dryRun) {
                MPI_CHECK(MPI_File_preallocate(mfd->fd,
                                               (MPI_Offset) (hints->segmentCount
                                                             *
                                                             hints->blockSize *
                                                             hints->numTasks)),
                          "cannot preallocate file");
        }


        /* create file view */
        if (param->useFileView) {
                /* Create in-memory datatype */
                MPI_CHECK(MPI_Type_contiguous (hints->transferSize / sizeof(IOR_size_t), MPI_LONG_LONG_INT, & mfd->contigType), "cannot create contiguous datatype");
                MPI_CHECK(MPI_Type_create_resized( mfd->contigType, 0, 0, & mfd->transferType), "cannot create resized type");
                MPI_CHECK(MPI_Type_commit(& mfd->contigType), "cannot commit datatype");
                MPI_CHECK(MPI_Type_commit(& mfd->transferType), "cannot commit datatype");

                /* create contiguous transfer datatype */

                if (hints->filePerProc) {
                        offsetFactor = 0;
                        tasksPerFile = 1;
                } else {
                        offsetFactor = (rank + rankOffset) % hints->numTasks;
                        tasksPerFile = hints->numTasks;
                }

                if(! hints->dryRun) {
                  if(! param->useStridedDatatype){
                    struct fileTypeStruct {
                        int globalSizes[2], localSizes[2], startIndices[2];
                    } fileTypeStruct;

                    /*
                    * create file type using subarray
                    */
                    fileTypeStruct.globalSizes[0] = 1;
                    fileTypeStruct.globalSizes[1] = transfersPerBlock * tasksPerFile;
                    fileTypeStruct.localSizes[0] = 1;
                    fileTypeStruct.localSizes[1] = transfersPerBlock;
                    fileTypeStruct.startIndices[0] = 0;
                    fileTypeStruct.startIndices[1] = transfersPerBlock * offsetFactor;
                    
                    MPI_CHECK(MPI_Type_create_subarray
                            (2, fileTypeStruct.globalSizes,
                              fileTypeStruct.localSizes,
                              fileTypeStruct.startIndices, MPI_ORDER_C,
                              mfd->contigType, & mfd->fileType),
                              "cannot create subarray");
                    MPI_CHECK(MPI_Type_commit(& mfd->fileType), "cannot commit datatype");
                    MPI_CHECK(MPI_File_set_view(mfd->fd, 0,
                                            mfd->contigType, 
                                            mfd->fileType,
                                            "native",
                                            (MPI_Info) MPI_INFO_NULL),
                          "cannot set file view");
                  }else{
                    MPI_CHECK(MPI_Type_create_resized(mfd->contigType, 0, tasksPerFile * hints->blockSize, & mfd->fileType), "cannot create MPI_Type_create_hvector");
                    MPI_CHECK(MPI_Type_commit(& mfd->fileType), "cannot commit datatype");
                  }
                }
        }
        if (mpiHints != MPI_INFO_NULL)
                MPI_CHECK(MPI_Info_free(&mpiHints), "MPI_Info_free failed");
        return ((void *) mfd);
}

/*
 * Write or read access to file using the MPIIO interface.
 */

#ifndef HAVE_MPI_FILE_READ_C
static int MPI_File_read_c(MPI_File f, void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_read(f, buf, c, t, s);
}

static int MPI_File_read_at_c(MPI_File f, MPI_Offset off, void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_read_at(f, off, buf, c, t, s);
}

static int MPI_File_read_all_c(MPI_File f, void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_read_all(f, buf, c, t, s);
}

static int MPI_File_read_at_all_c(MPI_File f, MPI_Offset off, void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_read_at_all(f, off, buf, c, t, s);
}

static int MPI_File_write_c(MPI_File f, const void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_write(f, buf, c, t, s);
}

static int MPI_File_write_at_c(MPI_File f, MPI_Offset off, const void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_write_at(f, off, buf, c, t, s);
}

static int MPI_File_write_all_c(MPI_File f, const void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_write_all(f, buf, c, t, s);
}

static int MPI_File_write_at_all_c(MPI_File f, MPI_Offset off, const void * buf, MPI_Count c,
                MPI_Datatype t, MPI_Status *s)
{
        if (c > INT_MAX)
                ERR("count too large for this MPI implementation");
        return MPI_File_write_at_all(f, off, buf, c, t, s);
}
#endif

static IOR_offset_t MPIIO_Xfer(int access, aiori_fd_t * fdp, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * module_options)
{
        /* NOTE: The second arg is (void *) for reads, and (const void *)
           for writes.  Therefore, one of the two sets of assignments below
           will get "assignment from incompatible pointer-type" warnings,
           if we only use this one set of signatures. */
        mpiio_options_t * param = (mpiio_options_t*) module_options;
        if(hints->dryRun)
          return length;
        mpiio_fd_t * mfd = (mpiio_fd_t*) fdp;

        int (MPIAPI * Access) (MPI_File, void *, MPI_Count,
                               MPI_Datatype, MPI_Status *);
        int (MPIAPI * Access_at) (MPI_File, MPI_Offset, void *, MPI_Count,
                                  MPI_Datatype, MPI_Status *);
        int (MPIAPI * Access_all) (MPI_File, void *, MPI_Count,
                                   MPI_Datatype, MPI_Status *);
        int (MPIAPI * Access_at_all) (MPI_File, MPI_Offset, void *, MPI_Count,
                                      MPI_Datatype, MPI_Status *);
        /*
         * this needs to be properly implemented:
         *
         *   int        (*Access_ordered)(MPI_File, void *, int,
         *                                MPI_Datatype, MPI_Status *);
         */
        MPI_Status status;
        MPI_Count elementsAccessed;
        MPI_Count elementSize;
        IOR_offset_t xferBytes, expectedBytes;
        int retryAccess = 1, xferRetries = 0;

        /* point functions to appropriate MPIIO calls */
        if (access == WRITE) {  /* WRITE */
                Access = (int (MPIAPI *)(MPI_File, void *, MPI_Count,
                          MPI_Datatype, MPI_Status *)) MPI_File_write_c;
                Access_at = (int (MPIAPI *)(MPI_File, MPI_Offset, void *, MPI_Count,
                             MPI_Datatype, MPI_Status *))  MPI_File_write_at_c;
                Access_all = (int (MPIAPI *) (MPI_File, void *, MPI_Count,
                              MPI_Datatype, MPI_Status *)) MPI_File_write_all_c;
                Access_at_all = (int (MPIAPI *) (MPI_File, MPI_Offset, void *, MPI_Count,
                                 MPI_Datatype, MPI_Status *)) MPI_File_write_at_all_c;
                /*
                 * this needs to be properly implemented:
                 *
                 *   Access_ordered = MPI_File_write_ordered;
                 */
        } else {                /* READ or CHECK */
                Access = MPI_File_read_c;
                Access_at = MPI_File_read_at_c;
                Access_all = MPI_File_read_all_c;
                Access_at_all = MPI_File_read_at_all_c;
                /*
                 * this needs to be properly implemented:
                 *
                 *   Access_ordered = MPI_File_read_ordered;
                 */
        }
        while( retryAccess ) {
                /*
                 * 'useFileView' uses derived datatypes and individual file pointers
                 */
                if (param->useFileView) {
                        /* find offset in file */
                        if (SeekOffset(mfd->fd, offset, module_options) <
                            0) {
                                /* if unsuccessful */
                                length = -1;
                                retryAccess = 0; // don't retry
                        } else {
        
                                /*
                                * 'useStridedDatatype' fits multi-strided pattern into a datatype;
                                * must use 'length' to determine repetitions (fix this for
                                * multi-segments someday, WEL):
                                * e.g.,  'IOR -s 2 -b 32K -t 32K -a MPIIO --mpiio.useStridedDatatype --mpiio.useFileView'
                                */
                                MPI_CHECK(MPI_Type_size_x(mfd->transferType, &elementSize), "couldn't get size of type");
                                if (param->useStridedDatatype) {
                                  if(offset >= (rank+1) * hints->blockSize){
                                    /* we shall write only once per transferSize */
                                    /* printf("FAKE access %d %lld\n", rank, offset); */
                                    return hints->transferSize;
                                  }
                                  length = hints->segmentCount;
                                  MPI_CHECK(MPI_File_set_view(mfd->fd, offset,
                                                          mfd->contigType, 
                                                          mfd->fileType,
                                                          "native",
                                                          (MPI_Info) MPI_INFO_NULL), "cannot set file view");                  
                                  expectedBytes = hints->segmentCount * elementSize;
                                  /* printf("ACCESS %d %lld -> %lld\n", rank, offset, length); */
                                  /* get the size of transferType; length is overloaded and used
                                   * set to number of segments */
                                }else{
                                  length = 1;
                                  expectedBytes = elementSize;
                                }
                                if (hints->collective) {
                                        /* individual, collective call */
                                        MPI_CHECK(Access_all
                                                  (mfd->fd, buffer, length,
                                                   mfd->transferType, &status),
                                                  "cannot access collective");
                                } else {
                                        /* individual, noncollective call */
                                        MPI_CHECK(Access
                                                  (mfd->fd, buffer, length,
                                                   mfd->transferType, &status),
                                                  "cannot access noncollective");
                                }
                                /* MPI-IO driver does "nontcontiguous" by transfering
                                 * 'segment' regions of 'transfersize' bytes, but
                                 * our caller WriteOrReadSingle does not know how to
                                 * deal with us reporting that we wrote N times more
                                 * data than requested. */
                                length = hints->transferSize;
                                MPI_CHECK(MPI_Get_elements_x(&status, MPI_BYTE, &elementsAccessed),
                                          "can't get elements accessed" );
                                xferBytes = elementsAccessed;
                        }
                } else {
                        /*
                         * !useFileView does not use derived datatypes, but it uses either
                         * shared or explicit file pointers
                         */
                        if (param->useSharedFilePointer) {
                                /* find offset in file */
                                if (SeekOffset
                                    (mfd->fd, offset, module_options) < 0) {
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
                                        fprintf(stdout,
                                                "useSharedFilePointer not implemented\n");
                                }
                        } else {
                                if (hints->collective) {
                                        /* explicit, collective call */
                                        MPI_CHECK(Access_at_all
                                                  (mfd->fd, offset,
                                                   buffer, length, MPI_BYTE, &status),
                                                  "cannot access explicit, collective");
                                } else {
                                        /* explicit, noncollective call */
                                        MPI_CHECK(Access_at
                                                  (mfd->fd, offset,
                                                   buffer, length, MPI_BYTE, &status),
                                                  "cannot access explicit, noncollective");
                                }
                        }
                        MPI_CHECK(MPI_Get_elements_x(&status, MPI_BYTE, &elementsAccessed),
                                   "can't get elements accessed" );

                        expectedBytes = length;
                        xferBytes = elementsAccessed;
                }

                /* Retrying collective xfers would require syncing after every IO to check if any
                 * IOs were short and all ranks retrying. Instead, report the short access which
                 * will lead to an abort after returning. Otherwise, retry the IO similarly to
                 * what's done for POSIX except retrying the full request. expectedBytes is
                 * transferSize/length unless --mpiio-useStridedDatatype and it will then be
                 * segmentCount * transferSize */
                if (xferBytes != expectedBytes ){
                        WARNF("task %d, partial %s, %lld of %lld bytes at offset %lld, xferRetries: %d\n",
                              rank,
                              access == WRITE ? "write()" : "read()",
                              xferBytes, expectedBytes,
                              offset, xferRetries);
                        /* don't allow retry if collective, return the short xfer amount now */
                        if (hints->collective) {
                                  WARNF("task %d, not retrying collective %s\n", rank,
                                       access == WRITE ? "write()" : "read()");
                                  return xferBytes;
                        }
                        if (xferRetries++ > MAX_RETRY || hints->singleXferAttempt){
                                  WARN("too many retries -- aborting");
                                  return xferBytes;
                        }
                } else {
                        retryAccess = 0; // expected data transferred, return normally
                }
        }
        return hints->transferSize; // short xfers already returned in the expectedBytes/retry check
}

/*
 * Perform fsync().
 */
static void MPIIO_Fsync(aiori_fd_t *fdp, aiori_mod_opt_t * module_options)
{
  mpiio_options_t * param = (mpiio_options_t*) module_options;
  if(hints->dryRun)
    return;
  mpiio_fd_t * mfd = (mpiio_fd_t*) fdp;
  if (MPI_File_sync(mfd->fd) != MPI_SUCCESS)
      WARN("fsync() failed");
}

/*
 * Close a file through the MPIIO interface.
 */
static void MPIIO_Close(aiori_fd_t *fdp, aiori_mod_opt_t * module_options)
{
        mpiio_options_t * param = (mpiio_options_t*) module_options;
        mpiio_fd_t * mfd = (mpiio_fd_t*) fdp;
        if(! hints->dryRun){
              MPI_CHECK(MPI_File_close(& mfd->fd), "cannot close file");
        }
        if (param->useFileView == TRUE) {
          /*
           * need to free the datatype, so done in the close process
           */
          MPI_CHECK(MPI_Type_free(& mfd->fileType), "cannot free MPI file datatype");
          MPI_CHECK(MPI_Type_free(& mfd->transferType), "cannot free MPI transfer datatype");
          MPI_CHECK(MPI_Type_free(& mfd->contigType), "cannot free type");
        }
        free(fdp);
}

/*
 * Delete a file through the MPIIO interface.
 */
void MPIIO_Delete(char *testFileName, aiori_mod_opt_t * module_options)
{
  mpiio_options_t * param = (mpiio_options_t*) module_options;
  if(hints->dryRun)
    return;
  MPI_CHECKF(MPI_File_delete(testFileName, (MPI_Info) MPI_INFO_NULL),
             "cannot delete file: %s", testFileName);
}

/*
 * Determine api version.
 */
static char* MPIIO_GetVersion()
{
  static char ver[1024] = {};
  int version, subversion;
  MPI_CHECK(MPI_Get_version(&version, &subversion), "cannot get MPI version");
  sprintf(ver, "(%d.%d)", version, subversion);
  return ver;
}

/*
 * Seek to offset in file using the MPIIO interface.
 */
static IOR_offset_t SeekOffset(MPI_File fd, IOR_offset_t offset,
                               aiori_mod_opt_t * module_options)
{
        mpiio_options_t * param = (mpiio_options_t*) module_options;
        int offsetFactor, tasksPerFile;
        IOR_offset_t tempOffset;

        tempOffset = offset;

        if (hints->filePerProc) {
                offsetFactor = 0;
                tasksPerFile = 1;
        } else {
                offsetFactor = (rank + rankOffset) % hints->numTasks;
                tasksPerFile = hints->numTasks;
        }
        if (param->useFileView) {
                /* recall that offsets in a file view are
                   counted in units of transfer size */
                if (hints->filePerProc) {
                        tempOffset = tempOffset / hints->transferSize;
                } else {
                        /*
                         * this formula finds a file view offset for a task
                         * from an absolute offset
                         */
                        tempOffset = ((hints->blockSize / hints->transferSize)
                                      * (tempOffset /
                                         (hints->blockSize * tasksPerFile)))
                            + (((tempOffset % (hints->blockSize * tasksPerFile))
                                - (offsetFactor * hints->blockSize))
                               / hints->transferSize);
                }
        }
        MPI_CHECK(MPI_File_seek(fd, tempOffset, MPI_SEEK_SET),
                  "cannot seek offset");
        return (offset);
}

/*
 * Use MPI_File_get_size() to return aggregate file size.
 * NOTE: This function is used by the HDF5 and NCMPI backends.
 */
IOR_offset_t MPIIO_GetFileSize(aiori_mod_opt_t * module_options, char *testFileName)
{
        mpiio_options_t * test = (mpiio_options_t*) module_options;
        if(hints->dryRun)
          return 0;
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;
        MPI_File fd;
        MPI_Info mpiHints = MPI_INFO_NULL;

        if (hints->filePerProc || rank == 0) {
                if(test)
                        SetHints(&mpiHints, test->hintsFileName);
                MPI_CHECK(MPI_File_open(MPI_COMM_SELF, testFileName, MPI_MODE_RDONLY,
                                        mpiHints, &fd),
                          "cannot open file to get file size");
                MPI_CHECK(MPI_File_get_size(fd, (MPI_Offset *) & aggFileSizeFromStat),
                          "cannot get file size");
                MPI_CHECK(MPI_File_close(&fd), "cannot close file");
                if (mpiHints != MPI_INFO_NULL)
                        MPI_CHECK(MPI_Info_free(&mpiHints), "MPI_Info_free failed");
        }
        if (!hints->filePerProc) {
                MPI_CHECK(MPI_Bcast(&aggFileSizeFromStat, 1, MPI_INT64_T, 0, testComm),
                          "cannot broadcast file_size");
        }
        return (aggFileSizeFromStat);
}
