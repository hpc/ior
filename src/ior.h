/* -*- mode: c; indent-tabs-mode: nil; -*-
 * vim:expandtab:
 *
 * NOTE: Someone was setting indent-sizes in the mode-line.  Don't do that.
 *       8-chars of indenting is ridiculous.  If you really want 8-spaces,
 *       then change the mode-line to use tabs, and configure your personal
 *       editor environment to use 8-space tab-stops.
 *
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
#  include "config.h"
#endif

#ifdef USE_HDFS_AIORI
#  include <hdfs.h>             /* hdfsFS */
#else
#  include <stdint.h>
   typedef uint16_t tPort;       /* unused, but needs a type */
   typedef void*    hdfsFS;      /* unused, but needs a type */
#endif

#include "option.h"
#include "iordef.h"
#include "aiori.h"

#include <mpi.h>

#ifndef MPI_FILE_NULL
#   include <mpio.h>
#endif /* not MPI_FILE_NULL */

#define ISPOWEROFTWO(x) ((x != 0) && !(x & (x - 1)))


/***************** IOR_BUFFERS *************************************************/
/* A struct to hold the buffers so we can pass 1 pointer around instead of 3
 */

typedef struct IO_BUFFERS
{
    void* buffer;
    void* checkBuffer;
    void* readCheckBuffer;

} IOR_io_buffers;

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
    const struct ior_aiori * backend;
    char * debug;             /* debug info string */
    int referenceNumber;             /* user supplied reference number */
    char * api;               /* API for I/O */
    char * apiVersion;        /* API version */
    char * platform;          /* platform type */
    char * testFileName;   /* full name for test */
    char * options;        /* options string */
    // intermediate options
    int collective;                  /* collective I/O */
    MPI_Comm     testComm;           /* Current MPI communicator */
    MPI_Comm     mpi_comm_world;           /* The global MPI communicator */
    int dryRun;                      /* do not perform any I/Os just run evtl. inputs print dummy output */
    int dualMount;                   /* dual mount points */
    ior_memory_flags gpuMemoryFlags;  /* use the GPU to store the data */
    int gpuDirect;                /* use gpuDirect, this influences gpuMemoryFlags as well */
    int gpuID;                       /* the GPU to use for gpuDirect or memory options */
    int numTasks;                    /* number of tasks for test */
    int numNodes;                    /* number of nodes for test */
    int numTasksOnNode0;             /* number of tasks on node 0 (usually all the same, but don't have to be, use with caution) */
    int tasksBlockMapping;           /* are the tasks in contiguous blocks across nodes or round-robin */
    int repetitions;                 /* number of repetitions of test */
    int repCounter;                  /* rep counter */
    int multiFile;                   /* multiple files */
    int interTestDelay;              /* delay between reps in seconds */
    int interIODelay;                /* delay after each I/O in us */
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
    IOR_offset_t segmentCount;       /* number of segments (or HDF5 datasets) */
    IOR_offset_t blockSize;          /* contiguous bytes to write per task */
    IOR_offset_t transferSize;       /* size of transfer in bytes */
    IOR_offset_t expectedAggFileSize; /* calculated aggregate file size */
    IOR_offset_t fileSizeForRead;
    IOR_offset_t randomPrefillBlocksize;   /* prefill option for random IO, the amount of data used for prefill */

    char * savePerOpDataCSV;            /* save details about each I/O operation into this file */
    char * saveRankDetailsCSV;       /* save the details about the performance to a file */
    int summary_every_test;          /* flag to print summary every test, not just at end */
    int uniqueDir;                   /* use unique directory for each fpp */
    int useExistingTestFile;         /* do not delete test file before access */
    int deadlineForStonewalling;     /* max time in seconds to run any test phase */
    int stoneWallingWearOut;         /* wear out the stonewalling, once the timeout is over, each process has to write the same amount */
    int minTimeDuration;             /* minimum runtime */
    uint64_t stoneWallingWearOutIterations; /* the number of iterations for the stonewallingWearOut, needed for readBack */
    char * stoneWallingStatusFile;

    int maxTimeDuration;             /* max time in minutes to run each test */
    int outlierThreshold;            /* warn on outlier N seconds from mean */
    int verbose;                     /* verbosity */
    int setTimeStampSignature;       /* set time stamp signature */
    unsigned int timeStampSignatureValue; /* value for time stamp signature */
    int randomSeed;                  /* random seed for write/read check */
    unsigned int incompressibleSeed; /* random seed for incompressible file creation */
    int randomOffset;                /* access is to random offsets */
    size_t memoryPerTask;            /* additional memory used per task */
    size_t memoryPerNode;            /* additional memory used per node */
    char * memoryPerNodeStr;         /* for parsing */
    char * testscripts;              /* for parsing */
    char * buffer_type;              /* for parsing */
    ior_dataPacketType_e dataPacketType; /* The type of data packet.  */

    void * backend_options;          /* Backend-specific options */

    /* POSIX variables */
    int singleXferAttempt;           /* do not retry transfer if incomplete */
    int fsyncPerWrite;               /* fsync() after each write */
    int fsync;                       /* fsync() after write */

    char*       URI;                 /* "path" to target object */

    int id;                          /* test's unique ID */
    int intraTestBarriers;           /* barriers between open/op and op/close */
    int warningAsErrors;             /* treat any warning as an error */

    aiori_xfer_hint_t hints;
} IOR_param_t;

/* each pointer for a single test */
typedef struct {
   double time;
   size_t pairs_accessed; // number of I/Os done, useful for deadlineForStonewalling

   double     stonewall_time;
   long long  stonewall_min_data_accessed; // of all processes
   long long  stonewall_avg_data_accessed; // across all processes
   long long  stonewall_total_data_accessed; // sum accross all processes

   IOR_offset_t aggFileSizeFromStat;
   IOR_offset_t aggFileSizeFromXfer;
   IOR_offset_t aggFileSizeForBW;
} IOR_point_t;

typedef struct {
   int          errors;
   IOR_point_t  write;
   IOR_point_t  read;
} IOR_results_t;

/* define the queuing structure for the test parameters */
typedef struct IOR_test_t {
   IOR_param_t        params;
   IOR_results_t     *results;
   struct IOR_test_t *next;
} IOR_test_t;

IOR_test_t *CreateTest(IOR_param_t *init_params, int test_num);
void AllocResults(IOR_test_t *test);

char * GetPlatformName(void);
void init_IOR_Param_t(IOR_param_t *p, MPI_Comm global_com);

/*
 * This function runs IOR given by command line, useful for testing
 */
IOR_test_t * ior_run(int argc, char **argv, MPI_Comm world_com, FILE * out_logfile);

/* Actual IOR Main function, renamed to allow library usage */
int ior_main(int argc, char **argv);

#endif /* !_IOR_H */
