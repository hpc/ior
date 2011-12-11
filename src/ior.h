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
    double *writeTime;               /* array of write time results for each rep */
    double *readTime;                /* array to read time results for each rep */
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
    int lustre_set_striping;         /* flag that we need to set lustre striping */
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

/* define the queuing structure for the test parameters */
typedef struct IOR_queue_t {
        IOR_param_t testParameters;
        struct IOR_queue_t *nextTest;
} IOR_queue_t;

IOR_queue_t *CreateNewTest(int);
void GetPlatformName(char *);
void init_IOR_Param_t(IOR_param_t *p);

#endif /* !_IOR_H */
