/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Definitions and prototypes of abstract I/O interface
*
\******************************************************************************/

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


/**************************** P R O T O T Y P E S *****************************/

typedef struct ior_aiori {
        char *name;
        void *(*create)(char *, IOR_param_t *);
        void *(*open)(char *, IOR_param_t *);
        IOR_offset_t (*xfer)(int, void *, IOR_size_t *,
                             IOR_offset_t, IOR_param_t *);
        void (*close)(void *, IOR_param_t *);
        void (*delete)(char *, IOR_param_t *);
        void (*set_version)(IOR_param_t *);
        void (*fsync)(void *, IOR_param_t *);
        IOR_offset_t (*get_file_size)(IOR_param_t *, MPI_Comm, char *);
} ior_aiori_t;

void init_IOR_Param_t(IOR_param_t *p);
void SetHints (MPI_Info *, char *);
void ShowHints (MPI_Info *);

#endif /* not _AIORI_H */
