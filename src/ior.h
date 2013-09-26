/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
\******************************************************************************/

#ifndef _IOR_H
#define _IOR_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iordef.h"

extern int numTasksWorld;
extern int rank;
extern int rankOffset;
extern int tasksPerNode;
extern int verbose;
extern MPI_Comm testComm;

/******************************************************************************/
/*
 * The parameter struct holds all of the "global" data to be passed,
 * as well as results to be parsed.
 *
 * NOTE: If IOR_Param_t is changed, also change:
 *         init_IOR_Param_t() [ior.c]
 *         DisplayUsage() [ior.c]
 *         ShowTest() [ior.c]
 *         DecodeDirective() [parse_options.c]
 *         ParseCommandLine() [parse_options.c]
 *         USER_GUIDE
 */

typedef struct
{
    char debug[MAX_STR];             /* debug info string */
    unsigned int mode;               /* file permissions */
    unsigned int openFlags;          /* open flags */
    int referenceNumber;             /* user supplied reference number */
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
    IOR_offset_t expectedAggFileSize; /* calculated aggregate file size */
    int preallocate;                 /* preallocate file size */
    int useFileView;                 /* use MPI_File_set_view */
    int useSharedFilePointer;        /* use shared file pointer */
    int useStridedDatatype;          /* put strided access into datatype */
    int useO_DIRECT;                 /* use O_DIRECT, bypassing I/O buffers */
    int showHints;                   /* show hints */
    int showHelp;                    /* show options and help */
    int summary_every_test;          /* flag to print summary every test, not just at end */
    int uniqueDir;                   /* use unique directory for each fpp */
    int useExistingTestFile;         /* do not delete test file before access */
    int storeFileOffset;             /* use file offset as stored signature */
    int deadlineForStonewalling; /* max time in seconds to run any test phase */
    int maxTimeDuration;             /* max time in minutes to run each test */
    int outlierThreshold;            /* warn on outlier N seconds from mean */
    int verbose;                     /* verbosity */
    int setTimeStampSignature;       /* set time stamp signature */
    unsigned int timeStampSignatureValue; /* value for time stamp signature */
    void * fd_fppReadCheck;          /* additional fd for fpp read check */
    int randomSeed;                  /* random seed for write/read check */
    int randomOffset;                /* access is to random offsets */
    MPI_Comm testComm;               /* MPI communicator */
    size_t memoryPerTask;            /* additional memory used per task */
    size_t memoryPerNode;            /* additional memory used per node */

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
    int lustre_set_striping;         /* flag that we need to set lustre striping */
    int lustre_ignore_locks;

    /* gpfs variables */
    int gpfs_hint_access;          /* use gpfs "access range" hint */
    int gpfs_release_token;        /* immediately release GPFS tokens after
                                      creating or opening a file */

    int id;                          /* test's unique ID */
    int intraTestBarriers;           /* barriers between open/op and op/close */
} IOR_param_t;

/* each pointer is to an array, each of length equal to the number of
   repetitions in the test */
typedef struct {
	double *writeTime;
	double *readTime;
	IOR_offset_t *aggFileSizeFromStat;
	IOR_offset_t *aggFileSizeFromXfer;
	IOR_offset_t *aggFileSizeForBW;
} IOR_results_t;

/* define the queuing structure for the test parameters */
typedef struct IOR_test_t {
        IOR_param_t params;
	IOR_results_t *results;
        struct IOR_test_t *next;
} IOR_test_t;

IOR_test_t *CreateTest(IOR_param_t *init_params, int test_num);
void AllocResults(IOR_test_t *test);
void GetPlatformName(char *);
void init_IOR_Param_t(IOR_param_t *p);

#endif /* !_IOR_H */
