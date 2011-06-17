/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* CVS info:
*   $RCSfile: aiori.h,v $
*   $Revision: 1.3 $
*   $Date: 2008/12/02 17:12:14 $
*   $Author: rklundt $
*
* Purpose:
*       This is a header file that contains the definitions and prototypes
*       needed for the abstract I/O interfaces necessary for IOR.
*
\******************************************************************************/
/********************* Modifications to IOR-2.10.1 *****************************
* hodson - 8/18/2008: Added option flags for the following new options         *
*    int TestNum;                    * test reference number                   *
*    double * writeVal;              * array to write results - IOPS added     *
*    double * readVal;               * array to read results  - IOPS added     *
*    int taskPerNodeOffset;          * task node offset for reading files      *
*    int reorderTasksRandom;         * reorder tasks for random file read back *
*    int reorderTasksRandomSeed;     * reorder tasks for random file read seed *
*    int fsyncPerWrite;              * fsync() after each write                *
*******************************************************************************/

#ifndef _AIORI_H
#define _AIORI_H

#include "iordef.h"                                     /* IOR Definitions */
#include <mpi.h>

#ifndef MPI_FILE_NULL
#   include <mpio.h>
#endif /* not MPI_FILE_NULL */

#include <string.h>

/*************************** D E F I N I T I O N S ****************************/

                                /* -- file open flags -- */
#define IOR_RDONLY        1     /* read only */
#define IOR_WRONLY        2     /* write only */
#define IOR_RDWR          4     /* read/write */
#define IOR_APPEND        8     /* append */
#define IOR_CREAT         16    /* create */
#define IOR_TRUNC         32    /* truncate */
#define IOR_EXCL          64    /* exclusive */
#define IOR_DIRECT        128   /* bypass I/O buffers */
                                /* -- file mode flags -- */
#define IOR_IRWXU         1     /* read, write, execute perm: owner */
#define IOR_IRUSR         2     /* read permission: owner */
#define IOR_IWUSR         4     /* write permission: owner */
#define IOR_IXUSR         8     /* execute permission: owner */
#define IOR_IRWXG         16    /* read, write, execute perm: group */
#define IOR_IRGRP         32    /* read permission: group */
#define IOR_IWGRP         64    /* write permission: group */
#define IOR_IXGRP         128   /* execute permission: group */
#define IOR_IRWXO         256   /* read, write, execute perm: other */
#define IOR_IROTH         512   /* read permission: other */
#define IOR_IWOTH         1024  /* write permission: other */
#define IOR_IXOTH         2048  /* execute permission: other */

/******************************************************************************/
/*
 * The parameter struct holds all of the "global" data to be passed,
 * as well as results to be parsed.
 *
 * NOTE: If this is changed, also change:
 *         defaultParameters [defaults.h]
 *         DisplayUsage() [IOR.c]
 *         ShowTest() [IOR.c]
 *         DecodeDirective() [parse_options.c]
 *         ParseCommandLine() [parse_options.c]
 *         USER_GUIDE
 */

typedef struct
{
    char debug[MAX_STR];             /* debug info string */
    unsigned int mode;               /* file permissions */
    unsigned int openFlags;          /* open flags */
    int TestNum;                     /* test reference number */
    char api[MAX_STR];               /* API for I/O */
    char apiVersion[MAX_STR];        /* API version */
    char platform[MAX_STR];          /* platform type */
    char testFileName[MAXPATHLEN];   /* full name for test */
    char testFileName_fppReadCheck[MAXPATHLEN];/* filename for fpp read check */
    char hintsFileName[MAXPATHLEN];  /* full name for hints file */
    char options[MAXPATHLEN];        /* options string */
    int numTasks;                    /* number of tasks for test */
    int nodes;                       /* number of nodes for test */
    int tasksPerNode;                /* number of tasks per node */
    int repetitions;                 /* number of repetitions of test */
    int repCounter;                  /* rep counter */
    int multiFile;                   /* multiple files */
    int interTestDelay;              /* delay between reps in seconds */
    double * writeVal[2];            /* array to write results */
    double * readVal[2];             /* array to read results */
    int open;                        /* flag for writing or reading */
    int readFile;                    /* read of existing file */
    int writeFile;                   /* write of file */
    int filePerProc;                 /* single file or file-per-process */
    int reorderTasks;                /* reorder tasks for read back and check */
    int taskPerNodeOffset;           /* task node offset for reading files   */
    int reorderTasksRandom;          /* reorder tasks for random file read back */
    int reorderTasksRandomSeed;      /* reorder tasks for random file read seed */
    int checkWrite;                  /* check read after write */
    int checkRead;                   /* check read after read */
    int keepFile;                    /* don't delete the testfile on exit */
    int keepFileWithError;           /* don't delete the testfile with errors */
    int errorFound;                  /* error found in data check */
    int quitOnError;                 /* quit code when error in check */
    int collective;                  /* collective I/O */
    IOR_offset_t segmentCount;       /* number of segments (or HDF5 datasets) */
    IOR_offset_t blockSize;          /* contiguous bytes to write per task */
    IOR_offset_t transferSize;       /* size of transfer in bytes */
    IOR_offset_t offset;             /* offset for read/write */
    IOR_offset_t * aggFileSizeFromCalc; /* calculated aggregate file size */
    IOR_offset_t * aggFileSizeFromStat; /* stat() aggregate file size */
    IOR_offset_t * aggFileSizeFromXfer; /* transfered aggregate file size */
    IOR_offset_t * aggFileSizeForBW; /* aggregate file size used for b/w */
    int preallocate;                 /* preallocate file size */
    int useFileView;                 /* use MPI_File_set_view */
    int useSharedFilePointer;        /* use shared file pointer */
    int useStridedDatatype;          /* put strided access into datatype */
    int useO_DIRECT;                 /* use O_DIRECT, bypassing I/O buffers */
    int showHints;                   /* show hints */
    int showHelp;                    /* show options and help */
    int uniqueDir;                   /* use unique directory for each fpp */
    int useExistingTestFile;         /* do not delete test file before access */
    int storeFileOffset;             /* use file offset as stored signature */
    int deadlineForStonewalling; /* max time in seconds to run any test phase */
    int maxTimeDuration;             /* max time in minutes to run tests */
    int outlierThreshold;            /* warn on outlier N seconds from mean */
    int verbose;                     /* verbosity */
    int setTimeStampSignature;       /* set time stamp signature */
    unsigned int timeStampSignatureValue; /* value for time stamp signature */
    void * fd_fppReadCheck;          /* additional fd for fpp read check */
    int randomSeed;                  /* random seed for write/read check */
    int randomOffset;                /* access is to random offsets */
    MPI_Comm testComm;               /* MPI communicator */

    /* POSIX variables */
    int singleXferAttempt;           /* do not retry transfer if incomplete */
    int fsyncPerWrite;               /* fsync() after each write */
    int fsync;                       /* fsync() after write */

    /* MPI variables */
    MPI_Datatype transferType;       /* datatype for transfer */
    MPI_Datatype fileType;           /* filetype for file view */

    /* HDF5 variables */
    int individualDataSets;          /* datasets not shared by all procs */
    int noFill;                      /* no fill in file creation */
    IOR_offset_t setAlignment;       /* alignment in bytes */

    /* NCMPI variables */
    int var_id;                      /* variable id handle for data set */

    /* Lustre variables */
    int lustre_stripe_count;
    int lustre_stripe_size;
    int lustre_start_ost;
    int lustre_ignore_locks;

#if USE_UNDOC_OPT
    int corruptFile;
    int fillTheFileSystem;
    int includeDeleteTime;
    int multiReRead;

    /* NFS variables */
    char NFS_rootPath[MAXPATHLEN];   /* absolute path to NFS servers */
    char NFS_serverName[MAXPATHLEN]; /* prefix for NFS server name */
    int NFS_serverCount;             /* number of NFS servers to be used */
#endif /* USE_UNDOC_OPT */

    int id;                          /* test's unique ID */
    int intraTestBarriers;           /* barriers between open/op and op/close */
} IOR_param_t;


/**************************** P R O T O T Y P E S *****************************/

/* functions for aiori-*.c */
/* POSIX-specific functions */
void *       IOR_Create_POSIX      (char *, IOR_param_t *);
void *       IOR_Open_POSIX        (char *, IOR_param_t *);
IOR_offset_t IOR_Xfer_POSIX        (int, void *, IOR_size_t *,
                                    IOR_offset_t, IOR_param_t *);
void         IOR_Close_POSIX       (void *, IOR_param_t *);
void         IOR_Delete_POSIX      (char *, IOR_param_t *);
void         IOR_SetVersion_POSIX  (IOR_param_t *);
void         IOR_Fsync_POSIX       (void *, IOR_param_t *);
IOR_offset_t IOR_GetFileSize_POSIX (IOR_param_t *, MPI_Comm, char *);

/* MPIIO-specific functions */
void *       IOR_Create_MPIIO      (char *, IOR_param_t *);
void *       IOR_Open_MPIIO        (char *, IOR_param_t *);
IOR_offset_t IOR_Xfer_MPIIO        (int, void *, IOR_size_t *,
                                    IOR_offset_t, IOR_param_t *);
void         IOR_Close_MPIIO       (void *, IOR_param_t *);
void         IOR_Delete_MPIIO      (char *, IOR_param_t *);
void         IOR_SetVersion_MPIIO  (IOR_param_t *);
void         IOR_Fsync_MPIIO       (void *, IOR_param_t *);
IOR_offset_t IOR_GetFileSize_MPIIO (IOR_param_t *, MPI_Comm, char *);

/* HDF5-specific functions */
void *       IOR_Create_HDF5       (char *, IOR_param_t *);
void *       IOR_Open_HDF5         (char *, IOR_param_t *);
IOR_offset_t IOR_Xfer_HDF5         (int, void *, IOR_size_t *,
                                    IOR_offset_t, IOR_param_t *);
void         IOR_Close_HDF5        (void *, IOR_param_t *);
void         IOR_Delete_HDF5       (char *, IOR_param_t *);
void         IOR_SetVersion_HDF5   (IOR_param_t *);
void         IOR_Fsync_HDF5        (void *, IOR_param_t *);
IOR_offset_t IOR_GetFileSize_HDF5  (IOR_param_t *, MPI_Comm, char *);

/* NCMPI-specific functions */
void *       IOR_Create_NCMPI      (char *, IOR_param_t *);
void *       IOR_Open_NCMPI        (char *, IOR_param_t *);
IOR_offset_t IOR_Xfer_NCMPI        (int, void *, IOR_size_t *,
                                    IOR_offset_t, IOR_param_t *);
void         IOR_Close_NCMPI       (void *, IOR_param_t *);
void         IOR_Delete_NCMPI      (char *, IOR_param_t *);
void         IOR_SetVersion_NCMPI  (IOR_param_t *);
void         IOR_Fsync_NCMPI       (void *, IOR_param_t *);
IOR_offset_t IOR_GetFileSize_NCMPI (IOR_param_t *, MPI_Comm, char *);

#endif /* not _AIORI_H */
