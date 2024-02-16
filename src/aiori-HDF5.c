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
*  Implement abstract I/O interface for HDF5.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>              /* only for fprintf() */
#include <stdlib.h>
#include <sys/stat.h>
/* HDF5 routines here still use the old 1.6 style.  Nothing wrong with that but
 * save users the trouble of  passing this flag through configure */
#define H5_USE_16_API
#include <hdf5.h>
#include <mpi.h>

#include "aiori.h"              /* abstract IOR interface */
#include "utilities.h"
#include "iordef.h"

#define NUM_DIMS 1              /* number of dimensions to data set */

/******************************************************************************/
/*
 * HDF5_CHECK will display a custom error message and then exit the program
 */

/*
 * should use MPI_Abort(), not exit(), in this macro; some versions of
 * MPI, however, hang with HDF5 property lists et al. left unclosed
 */

/*
 * for versions later than hdf5-1.6, the H5Eget_[major|minor]() functions
 * have been deprecated and replaced with H5Eget_msg()
 */
#if H5_VERS_MAJOR > 1 && H5_VERS_MINOR > 6
#define HDF5_CHECK(HDF5_RETURN, MSG) do {                                \
    char   resultString[1024];                                           \
    herr_t _HDF5_RETURN = (HDF5_RETURN);                                 \
                                                                         \
    if (_HDF5_RETURN < 0) {                                              \
        fprintf(stdout, "** error **\n");                                \
        fprintf(stdout, "ERROR in %s (line %d): %s.\n",                  \
                __FILE__, __LINE__, MSG);                                \
        strcpy(resultString, H5Eget_major((H5E_major_t)_HDF5_RETURN));   \
        if (strcmp(resultString, "Invalid major error number") != 0)     \
            fprintf(stdout, "HDF5 %s\n", resultString);                  \
        strcpy(resultString, H5Eget_minor((H5E_minor_t)_HDF5_RETURN));   \
        if (strcmp(resultString, "Invalid minor error number") != 0)     \
            fprintf(stdout, "%s\n", resultString);                       \
        fprintf(stdout, "** exiting **\n");                              \
        exit(EXIT_FAILURE);                                                        \
    }                                                                    \
} while(0)
#else                           /* ! (H5_VERS_MAJOR > 1 && H5_VERS_MINOR > 6) */
#define HDF5_CHECK(HDF5_RETURN, MSG) do {                                \
                                                                         \
    if (HDF5_RETURN < 0) {                                               \
        fprintf(stdout, "** error **\n");                                \
        fprintf(stdout, "ERROR in %s (line %d): %s.\n",                  \
                __FILE__, __LINE__, MSG);                                \
        /*                                                               \
         * H5Eget_msg(hid_t mesg_id, H5E_type_t* mesg_type,              \
         *            char* mesg, size_t size)                           \
         */                                                              \
        fprintf(stdout, "** exiting **\n");                              \
        exit(EXIT_FAILURE);                                                        \
    }                                                                    \
} while(0)
#endif                          /* H5_VERS_MAJOR > 1 && H5_VERS_MINOR > 6 */

/**************************** P R O T O T Y P E S *****************************/

static IOR_offset_t SeekOffset(void *, IOR_offset_t, aiori_mod_opt_t *);
static aiori_fd_t *HDF5_Create(char *, int flags, aiori_mod_opt_t *);
static aiori_fd_t *HDF5_Open(char *, int flags, aiori_mod_opt_t *);
static IOR_offset_t HDF5_Xfer(int, aiori_fd_t *, IOR_size_t *,
                              IOR_offset_t, IOR_offset_t, aiori_mod_opt_t *);
static void HDF5_Close(aiori_fd_t *, aiori_mod_opt_t *);
static void HDF5_Delete(char *, aiori_mod_opt_t *);
static char* HDF5_GetVersion();
static void HDF5_Fsync(aiori_fd_t *, aiori_mod_opt_t *);
static IOR_offset_t HDF5_GetFileSize(aiori_mod_opt_t *, char *);
static int HDF5_StatFS(const char *, ior_aiori_statfs_t *, aiori_mod_opt_t *);
static int HDF5_MkDir(const char *, mode_t, aiori_mod_opt_t *);
static int HDF5_RmDir(const char *, aiori_mod_opt_t *);
static int HDF5_Access(const char *, int, aiori_mod_opt_t *);
static int HDF5_Stat(const char *, struct stat *, aiori_mod_opt_t *);
static void HDF5_Finalize(aiori_mod_opt_t *);
static void HDF5_init_xfer_options(aiori_xfer_hint_t * params);
static int HDF5_check_params(aiori_mod_opt_t * options);

/************************** O P T I O N S *****************************/
typedef struct{
  mpiio_options_t mpio;
  
  int collective_md;
  int individualDataSets;          /* datasets not shared by all procs */
  int noFill;                      /* no fill in file creation */
  IOR_offset_t setAlignment;       /* alignment in bytes */
  int chunk_size;
} HDF5_options_t;
/***************************** F U N C T I O N S ******************************/

static option_help * HDF5_options(aiori_mod_opt_t ** init_backend_options, aiori_mod_opt_t * init_values){
  HDF5_options_t * o = malloc(sizeof(HDF5_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(HDF5_options_t));
  }else{
    memset(o, 0, sizeof(HDF5_options_t));
    /* initialize the options properly */
    o->collective_md = 0;
    o->setAlignment = 1;
    o->chunk_size = 0;
    o->mpio.hintsFileName = NULL;
  }

  *init_backend_options = (aiori_mod_opt_t*) o;

  option_help h [] = {
    /* options imported from MPIIO */
    {0, "hdf5.hintsFileName","Full name for hints file", OPTION_OPTIONAL_ARGUMENT, 's', & o->mpio.hintsFileName},
    {0, "hdf5.showHints",    "Show MPI hints", OPTION_FLAG, 'd', & o->mpio.showHints},    
    /* generic options */
    {0, "hdf5.collectiveMetadata", "Use collectiveMetadata (available since HDF5-1.10.0)", OPTION_FLAG, 'd', & o->collective_md},
    {0, "hdf5.individualDataSets",        "Datasets not shared by all procs [not working]", OPTION_FLAG, 'd', & o->individualDataSets},
    {0, "hdf5.setAlignment",        "HDF5 alignment in bytes (e.g.: 8, 4k, 2m, 1g)", OPTION_OPTIONAL_ARGUMENT, 'd', & o->setAlignment},
    {0, "hdf5.noFill", "No fill in HDF5 file creation", OPTION_FLAG, 'd', & o->noFill},
    {0, "hdf5.chunkSize", "Chunk size (in terms of dataset elements) to use for I/O", OPTION_FLAG, 'd', & o->chunk_size},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}


/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t hdf5_aiori = {
        .name = "HDF5",
        .name_legacy = NULL,
        .create = HDF5_Create,
        .open = HDF5_Open,
        .xfer = HDF5_Xfer,
        .close = HDF5_Close,
        .remove = HDF5_Delete,
        .get_version = HDF5_GetVersion,
        .xfer_hints = HDF5_init_xfer_options,
        .fsync = HDF5_Fsync,
        .get_file_size = HDF5_GetFileSize,
        .statfs = HDF5_StatFS,
        .mkdir = HDF5_MkDir,
        .rmdir = HDF5_RmDir,
        .access = HDF5_Access,
        .stat = HDF5_Stat,
        .finalize = HDF5_Finalize,
        .get_options = HDF5_options,
        .check_params = HDF5_check_params
};

typedef struct{
  hid_t fd;
  hid_t xferPropList;             /* xfer property list */
  hid_t dataSet;                  /* data set id */
  hid_t dataSpace;                /* data space id */
  hid_t fileDataSpace;            /* file data space id */
  hid_t memDataSpace;             /* memory data space id */
  int newlyOpenedFile;            /* newly opened file */  
  int firstReadCheck;
  int startNewDataSet;
} aiori_h5fd_t;

static void SetupDataSet(aiori_h5fd_t *, int flags, aiori_mod_opt_t *);

/***************************** F U N C T I O N S ******************************/
static aiori_xfer_hint_t * hints = NULL;

static void HDF5_init_xfer_options(aiori_xfer_hint_t * params){
  hints = params;
  /** HDF5 utilizes the MPIIO backend too, so init hints there */
  MPIIO_xfer_hints(params);
}

static int HDF5_check_params(aiori_mod_opt_t * options){
  HDF5_options_t *o = (HDF5_options_t*) options;
  if (o->setAlignment < 0)
      ERR("alignment must be non-negative integer");
  if (o->individualDataSets)
      ERR("individual data sets not implemented");
  return 0;
}

/*
 * Create and open a file through the HDF5 interface.
 */
static aiori_fd_t *HDF5_Create(char *testFileName, int flags, aiori_mod_opt_t * param)
{
        return HDF5_Open(testFileName, flags, param);
}

/*
 * Open a file through the HDF5 interface.
 */
static aiori_fd_t *HDF5_Open(char *testFileName, int flags, aiori_mod_opt_t * param)
{
        HDF5_options_t *o = (HDF5_options_t*) param;
        hid_t accessPropList, createPropList;
        hsize_t memStart[NUM_DIMS],
            dataSetDims[NUM_DIMS],
            memStride[NUM_DIMS],
            memCount[NUM_DIMS], memBlock[NUM_DIMS], memDataSpaceDims[NUM_DIMS];
        int tasksPerDataSet;
        unsigned fd_mode = (unsigned)0;
        aiori_h5fd_t * fd = safeMalloc(sizeof(aiori_h5fd_t));
        MPI_Comm comm;
        MPI_Info mpiHints = MPI_INFO_NULL;

        /*
         * HDF5 uses different flags than those for POSIX/MPIIO
         */
        /* set IOR file flags to HDF5 flags */
        /* -- file open flags -- */
        if (flags & IOR_RDONLY) {
                fd_mode |= H5F_ACC_RDONLY;
        }
        if (flags & IOR_WRONLY || flags & IOR_RDWR) {
                fd_mode |= H5F_ACC_RDWR;
        }
        if (flags & IOR_APPEND) {
                fprintf(stdout, "File append not implemented in HDF5\n");
        }
        if (flags & IOR_CREAT) {
                fd_mode |= H5F_ACC_CREAT;
        }
        if (flags & IOR_EXCL) {
                fd_mode |= H5F_ACC_EXCL;
        }
        if (flags & IOR_TRUNC) {
                fd_mode |= H5F_ACC_TRUNC;
        }
        if (flags & IOR_DIRECT) {
                fprintf(stdout, "O_DIRECT not implemented in HDF5\n");
        }

        /* set up file creation property list */
        createPropList = H5Pcreate(H5P_FILE_CREATE);
        HDF5_CHECK(createPropList, "cannot create file creation property list");
        /* set size of offset and length used to address HDF5 objects */
        HDF5_CHECK(H5Pset_sizes
                   (createPropList, sizeof(hsize_t), sizeof(hsize_t)),
                   "cannot set property list properly");

        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");

        /*
         * someday HDF5 implementation will allow subsets of MPI_COMM_WORLD
         */
        /* store MPI communicator info for the file access property list */
        if (hints->filePerProc) {
                comm = MPI_COMM_SELF;
        } else {
                comm = testComm;
        }

        /*
         * note that with MP_HINTS_FILTERED=no, all key/value pairs will
         * be in the info object.  The info object that is attached to
         * the file during MPI_File_open() will only contain those pairs
         * deemed valid by the implementation.
         */
        /* show hints passed to file */
        SetHints(&mpiHints, o->mpio.hintsFileName);
        if (rank == 0 && o->mpio.showHints) {
                fprintf(stdout, "\nhints passed to access property list {\n");
                ShowHints(&mpiHints);
                fprintf(stdout, "}\n");
        }

        HDF5_CHECK(H5Pset_fapl_mpio(accessPropList, comm, mpiHints),
                   "cannot set file access property list");

        /* set alignment */
        HDF5_CHECK(H5Pset_alignment(accessPropList, o->setAlignment, o->setAlignment),
                   "cannot set alignment");

#ifdef HAVE_H5PSET_ALL_COLL_METADATA_OPS
        if (o->collective_md) {
                /* more scalable metadata */

                HDF5_CHECK(H5Pset_all_coll_metadata_ops(accessPropList, 1),
                        "cannot set collective md read");
                HDF5_CHECK(H5Pset_coll_metadata_write(accessPropList, 1),
                        "cannot set collective md write");
        }
#endif

        /* open file */
        if(! hints->dryRun){
          if (flags & IOR_CREAT) {     /* WRITE */
                  fd->fd = H5Fcreate(testFileName, H5F_ACC_TRUNC, createPropList, accessPropList);
                  HDF5_CHECK(fd->fd, "cannot create file");
          } else {                /* READ or CHECK */
                  fd->fd = H5Fopen(testFileName, fd_mode, accessPropList);
                  HDF5_CHECK(fd->fd, "cannot open file");
          }
        }

        /* show hints actually attached to file handle */
        if (o->mpio.showHints) {
                MPI_File *fd_mpiio;
                HDF5_CHECK(H5Fget_vfd_handle(fd->fd, accessPropList, (void **) &fd_mpiio), "cannot get file handle");
                MPI_Info info_used;
                MPI_CHECK(MPI_File_get_info(*fd_mpiio, &info_used), "cannot get file info");
                if (rank == 0) {
                        /* print the MPI file hints currently used */
                        fprintf(stdout, "\nhints returned from opened file {\n");
                        ShowHints(&info_used);
                                fprintf(stdout, "}\n");
                        }
                MPI_CHECK(MPI_Info_free(&info_used), "cannot free file info");
        }

        /* this is necessary for resetting various parameters
           needed for reopening and checking the file */
        fd->newlyOpenedFile = TRUE;

        HDF5_CHECK(H5Pclose(createPropList),
                   "cannot close creation property list");
        HDF5_CHECK(H5Pclose(accessPropList),
                   "cannot close access property list");

        /* create property list for serial/parallel access */
        fd->xferPropList = H5Pcreate(H5P_DATASET_XFER);
        HDF5_CHECK(fd->xferPropList, "cannot create transfer property list");

        /* set data transfer mode */
        if (hints->collective) {
                HDF5_CHECK(H5Pset_dxpl_mpio(fd->xferPropList, H5FD_MPIO_COLLECTIVE),
                           "cannot set collective data transfer mode");
        } else {
                HDF5_CHECK(H5Pset_dxpl_mpio
                           (fd->xferPropList, H5FD_MPIO_INDEPENDENT),
                           "cannot set independent data transfer mode");
        }

        /* set up memory data space for transfer */
        memStart[0] = (hsize_t) 0;
        memCount[0] = (hsize_t) 1;
        memStride[0] = (hsize_t) (hints->transferSize / sizeof(IOR_size_t));
        memBlock[0] = (hsize_t) (hints->transferSize / sizeof(IOR_size_t));
        memDataSpaceDims[0] = (hsize_t) hints->transferSize;
        fd->memDataSpace = H5Screate_simple(NUM_DIMS, memDataSpaceDims, NULL);
        HDF5_CHECK(fd->memDataSpace, "cannot create simple memory data space");

        /* define hyperslab for memory data space */
        HDF5_CHECK(H5Sselect_hyperslab(fd->memDataSpace, H5S_SELECT_SET,
                                       memStart, memStride, memCount,
                                       memBlock), "cannot create hyperslab");

        /* set up parameters for fpp or different dataset count */
        if (hints->filePerProc) {
                tasksPerDataSet = 1;
        } else {
                if (o->individualDataSets) {
                        /* each task in segment has single data set */
                        tasksPerDataSet = 1;
                } else {
                        /* share single data set across all tasks in segment */
                        tasksPerDataSet = hints->numTasks;
                }
        }
        dataSetDims[0] = (hsize_t) ((hints->blockSize / sizeof(IOR_size_t))
                                    * tasksPerDataSet);

        /* create a simple data space containing information on size
           and shape of data set, and open it for access */
        fd->dataSpace = H5Screate_simple(NUM_DIMS, dataSetDims, NULL);
        HDF5_CHECK(fd->dataSpace, "cannot create simple data space");
        if (mpiHints != MPI_INFO_NULL)
                MPI_Info_free(&mpiHints);

        return (aiori_fd_t*)(fd);
}

/*
 * Write or read access to file using the HDF5 interface.
 */
static IOR_offset_t HDF5_Xfer(int access, aiori_fd_t *afd, IOR_size_t * buffer,
                              IOR_offset_t length, IOR_offset_t offset, aiori_mod_opt_t * param)
{
        IOR_offset_t segmentPosition, segmentSize;
        aiori_h5fd_t * fd = (aiori_h5fd_t *) afd;

        /*
         * this toggle is for the read check operation, which passes through
         * this function twice; note that this function will open a data set
         * only on the first read check and close only on the second
         */
        if (access == READCHECK) {
                if (fd->firstReadCheck == TRUE) {
                        fd->firstReadCheck = FALSE;
                } else {
                        fd->firstReadCheck = TRUE;
                }
        }

        /* determine by offset if need to start new data set */
        if (hints->filePerProc == TRUE) {
                segmentPosition = (IOR_offset_t) 0;
                segmentSize = hints->blockSize;
        } else {
                segmentPosition =
                    (IOR_offset_t) ((rank + rankOffset) % hints->numTasks)
                    * hints->blockSize;
                segmentSize = (IOR_offset_t) (hints->numTasks) * hints->blockSize;
        }
        if ((IOR_offset_t) ((offset - segmentPosition) % segmentSize) ==
            0) {
                /*
                 * ordinarily start a new data set, unless this is the
                 * second pass through during a read check
                 */
                fd->startNewDataSet = TRUE;
                if (access == READCHECK && fd->firstReadCheck != TRUE) {
                        fd->startNewDataSet = FALSE;
                }
        }

        if(hints->dryRun)
          return length;

        /* create new data set */
        if (fd->startNewDataSet == TRUE) {
                /* if just opened this file, no data set to close yet */
                if (fd->newlyOpenedFile != TRUE) {
                        HDF5_CHECK(H5Dclose(fd->dataSet), "cannot close data set");
                        HDF5_CHECK(H5Sclose(fd->fileDataSpace),
                                   "cannot close file data space");
                }
                SetupDataSet(fd, access == WRITE ? IOR_CREAT : IOR_RDWR, param);
        }

        SeekOffset(fd, offset, param);

        /* this is necessary to reset variables for reaccessing file */
        fd->startNewDataSet = FALSE;
        fd->newlyOpenedFile = FALSE;

        /* access the file */
        if (access == WRITE) {  /* WRITE */
                HDF5_CHECK(H5Dwrite(fd->dataSet, H5T_NATIVE_LLONG,
                                    fd->memDataSpace, fd->fileDataSpace,
                                    fd->xferPropList, buffer),
                           "cannot write to data set");
        } else {                /* READ or CHECK */
                HDF5_CHECK(H5Dread(fd->dataSet, H5T_NATIVE_LLONG,
                                   fd->memDataSpace, fd->fileDataSpace,
                                   fd->xferPropList, buffer),
                           "cannot read from data set");
        }
        return (length);
}

/*
 * Perform fsync().
 */
static void HDF5_Fsync(aiori_fd_t *afd, aiori_mod_opt_t * param)
{
  aiori_h5fd_t * fd = (aiori_h5fd_t *) afd;
  HDF5_CHECK(H5Fflush(fd->fd, H5F_SCOPE_LOCAL), "cannot flush file to disk");
}

/*
 * Close a file through the HDF5 interface.
 */
static void HDF5_Close(aiori_fd_t *afd, aiori_mod_opt_t * param)
{
    aiori_h5fd_t * fd = (aiori_h5fd_t *) afd;
    if(hints->dryRun)
      return;
    //if (hints->fd_fppReadCheck == NULL) {
            HDF5_CHECK(H5Dclose(fd->dataSet), "cannot close data set");
            HDF5_CHECK(H5Sclose(fd->dataSpace), "cannot close data space");
            HDF5_CHECK(H5Sclose(fd->fileDataSpace),
                       "cannot close file data space");
            HDF5_CHECK(H5Sclose(fd->memDataSpace),
                       "cannot close memory data space");
            HDF5_CHECK(H5Pclose(fd->xferPropList),
                       " cannot close transfer property list");
    //}
    HDF5_CHECK(H5Fclose(fd->fd), "cannot close file");
    free(fd);
}

/*
 * Delete a file through the HDF5 interface.
 */
static void HDF5_Delete(char *testFileName, aiori_mod_opt_t * param)
{
#ifdef HAVE_H5FDELETE
        hid_t accessPropList;
        MPI_Comm comm = MPI_COMM_SELF; /* Ony one rank accesses the file */
        MPI_Info mpiHints = MPI_INFO_NULL;
#endif

        if(hints->dryRun)
                return;

#ifdef HAVE_H5FDELETE
        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");

        HDF5_CHECK(H5Pset_fapl_mpio(accessPropList, comm, mpiHints),
                   "cannot set file access property list");

        HDF5_CHECK(H5Fdelete(testFileName, accessPropList),
                   "cannot delete file");

        HDF5_CHECK(H5Pclose(accessPropList),
                   "cannot close access property list");
#else
        MPIIO_Delete(testFileName, NULL);
#endif

        return;
}

/*
 * Determine api version.
 */
static char * HDF5_GetVersion()
{
        static char version[1024] = {0};
        if(version[0]) return version;

        unsigned major, minor, release;
        if (H5get_libversion(&major, &minor, &release) < 0) {
                WARN("cannot get HDF5 library version");
        } else {
                sprintf(version, "%u.%u.%u", major, minor, release);
        }
#ifndef H5_HAVE_PARALLEL
        strcat(version, " (Serial)");
#else                           /* H5_HAVE_PARALLEL */
        strcat(version, " (Parallel)");
#endif                          /* not H5_HAVE_PARALLEL */
  return version;
}

/*
 * Seek to offset in file using the HDF5 interface and set up hyperslab.
 */
static IOR_offset_t SeekOffset(void *afd, IOR_offset_t offset,
                                            aiori_mod_opt_t * param)
{
    aiori_h5fd_t * fd = (aiori_h5fd_t *) afd;
    HDF5_options_t *o = (HDF5_options_t*) param;
    IOR_offset_t segmentSize;
    hsize_t hsStride[NUM_DIMS], hsCount[NUM_DIMS], hsBlock[NUM_DIMS];
    hsize_t hsStart[NUM_DIMS];

    if (hints->filePerProc == TRUE) {
            segmentSize = (IOR_offset_t) hints->blockSize;
    } else {
            segmentSize = (IOR_offset_t) (hints->numTasks) * hints->blockSize;
    }

    /* create a hyperslab representing the file data space */
    if (o->individualDataSets) {
            /* start at zero offset if not */
            hsStart[0] = (hsize_t) ((offset % hints->blockSize) / sizeof(IOR_size_t));
    } else {
            /* start at a unique offset if shared */
            hsStart[0] = (hsize_t) ((offset % segmentSize) / sizeof(IOR_size_t));
    }
    hsCount[0] = (hsize_t) 1;
    hsStride[0] = (hsize_t) (hints->transferSize / sizeof(IOR_size_t));
    hsBlock[0] = (hsize_t) (hints->transferSize / sizeof(IOR_size_t));

    /* select hyperslab in file data space */
    HDF5_CHECK(H5Sselect_hyperslab(fd->fileDataSpace, H5S_SELECT_SET, hsStart, hsStride, hsCount, hsBlock),
               "cannot select hyperslab");
    return (offset);
}

/*
 * Create HDF5 data set.
 */
static void SetupDataSet(aiori_h5fd_t *fd, int flags, aiori_mod_opt_t * param)
{
  
        HDF5_options_t *o = (HDF5_options_t*) param;
        char dataSetName[MAX_STR];
        hid_t dataSetPropList;
        int dataSetID;
        static int dataSetSuffix = 0;

        /* may want to use an extendable dataset (H5S_UNLIMITED) someday */

        /* need to reset suffix counter if newly-opened file */
        if (fd->newlyOpenedFile)
                dataSetSuffix = 0;

        /* may want to use individual access to each data set someday */
        if (o->individualDataSets) {
                dataSetID = (rank + rankOffset) % hints->numTasks;
        } else {
                dataSetID = 0;
        }

        sprintf(dataSetName, "%s-%04d.%04d", "Dataset", dataSetID,
                dataSetSuffix++);

        if (flags & IOR_CREAT) {     /* WRITE */
                hsize_t chunk_dims[NUM_DIMS];

                /* create data set */
                dataSetPropList = H5Pcreate(H5P_DATASET_CREATE);

                /* Set chunk size */
                if (o->chunk_size > 0) {
                    int i;

                    for (i = 0; i < NUM_DIMS; i++)
                            chunk_dims[i] = o->chunk_size;

                    HDF5_CHECK(H5Pset_chunk(dataSetPropList, NUM_DIMS, chunk_dims),
                        "cannot set chunk size");
                }

                if (o->noFill == TRUE) {
                        if (rank == 0 && verbose >= VERBOSE_1) {
                                fprintf(stdout, "\nusing 'no fill' option\n");
                        }
                        HDF5_CHECK(H5Pset_fill_time(dataSetPropList,
                                                    H5D_FILL_TIME_NEVER),
                                   "cannot set fill time for property list");
                }
                fd->dataSet = H5Dcreate(fd->fd, dataSetName, H5T_NATIVE_LLONG, fd->dataSpace, dataSetPropList);
                HDF5_CHECK(fd->dataSet, "cannot create data set");
        } else {                /* READ or CHECK */
                fd->dataSet = H5Dopen(*(hid_t *) fd, dataSetName);
                HDF5_CHECK(fd->dataSet, "cannot open data set");
        }

        /* retrieve data space from data set for hyperslab */
        fd->fileDataSpace = H5Dget_space(fd->dataSet);
        HDF5_CHECK(fd->fileDataSpace, "cannot get data space from data set");
}

static IOR_offset_t HDF5_GetFileSize(aiori_mod_opt_t * test, char *testFileName)
{
        /* Ensure that non-native VOLs do not use MPIIO_GetFileSize() */
#ifdef HAVE_H5PGET_VOL_ID
        hid_t vol_id, accessPropList;
#endif
        IOR_offset_t ret;

        if(hints->dryRun)
                return 0;

#ifdef HAVE_H5PGET_VOL_ID
        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");
        HDF5_CHECK(H5Pget_vol_id(accessPropList, &vol_id), "cannot get vol id");
        HDF5_CHECK(H5Pclose(accessPropList), "cannot close access property list");

        if(vol_id == H5VL_NATIVE)
#endif
                ret = MPIIO_GetFileSize(test, testFileName);
#ifdef HAVE_H5PGET_VOL_ID
        else {
                if(rank == 0)
                    WARN("getfilesize not supported with current VOL connector!");

                ret = -1;
        }

        HDF5_CHECK(H5VLclose(vol_id), "cannot close VOL ID");
#endif

        return ret;
}

static int HDF5_StatFS(const char * oid, ior_aiori_statfs_t * stat_buf,
                       aiori_mod_opt_t * param)
{
        int ret;
#ifdef HAVE_H5PGET_VOL_ID
        hid_t vol_id, accessPropList;

        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");
        HDF5_CHECK(H5Pget_vol_id(accessPropList, &vol_id), "cannot get vol id");
        HDF5_CHECK(H5Pclose(accessPropList), "cannot close access property list");

        if(vol_id == H5VL_NATIVE)
#endif
                ret = aiori_posix_statfs(oid, stat_buf, param);
#ifdef HAVE_H5PGET_VOL_ID
        else {
                if(rank == 0)
                    WARN("statfs not supported by current VOL connector!");

                ret = -1;
        }

        HDF5_CHECK(H5VLclose(vol_id), "cannot close VOL ID");
#endif

        return ret;
}

static int HDF5_MkDir(const char * oid, mode_t mode, aiori_mod_opt_t * param)
{
        int ret;
#ifdef HAVE_H5PGET_VOL_ID
        hid_t vol_id, accessPropList;

        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");
        HDF5_CHECK(H5Pget_vol_id(accessPropList, &vol_id), "cannot get vol id");
        HDF5_CHECK(H5Pclose(accessPropList), "cannot close access property list");

        if(vol_id == H5VL_NATIVE)
#endif
                ret = aiori_posix_mkdir(oid, mode, param);
#ifdef HAVE_H5PGET_VOL_ID
        else {
                if(rank == 0)
                    WARN("mkdir not supported by current VOL connector!");

                ret = -1;
        }

        HDF5_CHECK(H5VLclose(vol_id), "cannot close VOL ID");
#endif

        return ret;
}

static int HDF5_RmDir(const char * oid, aiori_mod_opt_t * param)
{
        int ret;
#ifdef HAVE_H5PGET_VOL_ID
        hid_t vol_id, accessPropList;

        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");
        HDF5_CHECK(H5Pget_vol_id(accessPropList, &vol_id), "cannot get vol id");
        HDF5_CHECK(H5Pclose(accessPropList), "cannot close access property list");

        if(vol_id == H5VL_NATIVE)
#endif
                ret = aiori_posix_rmdir(oid, param);
#ifdef HAVE_H5PGET_VOL_ID
        else {
                if(rank == 0)
                    WARN("rmdir not supported by current VOL connector!");

                ret = -1;
        }

        HDF5_CHECK(H5VLclose(vol_id), "cannot close VOL ID");
#endif

        return ret;
}

static int HDF5_Access(const char * path, int mode, aiori_mod_opt_t * param)
{
        htri_t accessible = -1;
        hid_t accessPropList;
        MPI_Comm comm = MPI_COMM_SELF; /* Ony one rank accesses the file */
        MPI_Info mpiHints = MPI_INFO_NULL;
        int ret = -1;

        if(hints->dryRun)
                return 0;

#ifdef HAVE_H5FIS_ACCESSIBLE
        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");

        HDF5_CHECK(H5Pset_fapl_mpio(accessPropList, comm, mpiHints),
                   "cannot set file access property list");

        H5E_BEGIN_TRY {
            accessible = H5Fis_accessible(path, accessPropList);
        } H5E_END_TRY;
        if (accessible > 0)
            ret = 0;

        HDF5_CHECK(H5Pclose(accessPropList),
                   "cannot close access property list");
#else
        ret = MPIIO_Access(path, mode, param);
#endif

        return ret;
}

static int HDF5_Stat(const char * oid, struct stat * buf, aiori_mod_opt_t * param)
{
        int ret;
#ifdef HAVE_H5PGET_VOL_ID
        hid_t vol_id, accessPropList;

        /* set up file access property list */
        accessPropList = H5Pcreate(H5P_FILE_ACCESS);
        HDF5_CHECK(accessPropList, "cannot create file access property list");
        HDF5_CHECK(H5Pget_vol_id(accessPropList, &vol_id), "cannot get vol id");
        HDF5_CHECK(H5Pclose(accessPropList), "cannot close access property list");

        if(vol_id == H5VL_NATIVE)
#endif
                ret = aiori_posix_stat(oid, buf, param);
#ifdef HAVE_H5PGET_VOL_ID
        else {
                if(rank == 0)
                    WARN("stat not supported by current VOL connector!");

                ret = -1;
        }

        HDF5_CHECK(H5VLclose(vol_id), "cannot close VOL ID");
#endif

        return ret;
}

/*
 * Call H5close to ensure library is
 * shutdown before MPI_Finalize is called
 */
static void
HDF5_Finalize(aiori_mod_opt_t * options)
{
    H5close();
}
