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
* Additional utilities
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef __linux__
#  define _GNU_SOURCE            /* Needed for O_DIRECT in fcntl */
#endif                           /* __linux__ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>               /* pow() */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifndef _WIN32
#  include <regex.h>
#  ifdef __sun                    /* SunOS does not support statfs(), instead uses statvfs() */
#    include <sys/statvfs.h>
#  elif (defined __APPLE__)
#    include <sys/param.h>
#    include <sys/mount.h>
#  else                           /* ! __sun  or __APPLE__ */
#    include <sys/statfs.h>
#  endif                          /* __sun */
#  include <sys/time.h>           /* gettimeofday() */
#endif

#include "utilities.h"
#include "aiori.h"
#include "ior.h"

/************************** D E C L A R A T I O N S ***************************/

extern int errno;
extern int numTasks;

/* globals used by other files, also defined "extern" in ior.h */
int      numTasksWorld = 0;
int      rank = 0;
int      rankOffset = 0;
int      tasksPerNode = 0;           /* tasks per node */
int      verbose = VERBOSE_0;        /* verbose output */
MPI_Comm testComm;
MPI_Comm mpi_comm_world;
FILE * out_logfile;
FILE * out_resultfile;
enum OutputFormat_t outputFormat;

/***************************** F U N C T I O N S ******************************/


/* Used in aiori-POSIX.c and aiori-PLFS.c
 */

void set_o_direct_flag(int *fd)
{
/* note that TRU64 needs O_DIRECTIO, SunOS uses directio(),
   and everyone else needs O_DIRECT */
#ifndef O_DIRECT
#  ifndef O_DIRECTIO
     WARN("cannot use O_DIRECT");
#    define O_DIRECT 000000
#  else                           /* O_DIRECTIO */
#    define O_DIRECT O_DIRECTIO
#  endif                          /* not O_DIRECTIO */
#endif                            /* not O_DIRECT */

        *fd |= O_DIRECT;
}


/*
 * Returns string containing the current time.
 *
 * NOTE: On some systems, MPI jobs hang while ctime() waits for a lock.
 * This is true even though CurrentTimeString() is only called for rank==0.
 * ctime_r() fixes this.
 */
char *CurrentTimeString(void)
{
        static time_t currentTime;
        char*         currentTimePtr;

        if ((currentTime = time(NULL)) == -1)
                ERR("cannot get current time");

#if     (_POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || _POSIX_SOURCE)
        static char   threadSafeBuff[32]; /* "must be at least 26 characters long" */
        if ((currentTimePtr = ctime_r(&currentTime, threadSafeBuff)) == NULL) {
                ERR("cannot read current time");
        }
#else
        if ((currentTimePtr = ctime(&currentTime)) == NULL) {
                ERR("cannot read current time");
        }
#endif
        /* ctime string ends in \n */
        return (currentTimePtr);
}

/*
 * Dump transfer buffer.
 */
void DumpBuffer(void *buffer,
                size_t size)    /* <size> in bytes */
{
        size_t i, j;
        IOR_size_t *dumpBuf = (IOR_size_t *)buffer;

        /* Turns out, IOR_size_t is unsigned long long, but we don't want
           to assume that it must always be */
        for (i = 0; i < ((size / sizeof(IOR_size_t)) / 4); i++) {
                for (j = 0; j < 4; j++) {
                        fprintf(out_logfile, IOR_format" ", dumpBuf[4 * i + j]);
                }
                fprintf(out_logfile, "\n");
        }
        return;
}                               /* DumpBuffer() */

#if MPI_VERSION >= 3
int CountTasksPerNode(MPI_Comm comm) {
    /* modern MPI provides a simple way to get the local process count */
    MPI_Comm shared_comm;
    int count;

    MPI_Comm_split_type (comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shared_comm);
    MPI_Comm_size (shared_comm, &count);
    MPI_Comm_free (&shared_comm);

    return count;
}
#else
/*
 * Count the number of tasks that share a host.
 *
 * This function employees the gethostname() call, rather than using
 * MPI_Get_processor_name().  We are interested in knowing the number
 * of tasks that share a file system client (I/O node, compute node,
 * whatever that may be).  However on machines like BlueGene/Q,
 * MPI_Get_processor_name() uniquely identifies a cpu in a compute node,
 * not the node where the I/O is function shipped to.  gethostname()
 * is assumed to identify the shared filesystem client in more situations.
 *
 * NOTE: This also assumes that the task count on all nodes is equal
 * to the task count on the host running MPI task 0.
 */
int CountTasksPerNode(MPI_Comm comm) {
    int size;
    MPI_Comm_size(comm, & size);
    /* for debugging and testing */
    if (getenv("IOR_FAKE_TASK_PER_NODES")){
      int tasksPerNode = atoi(getenv("IOR_FAKE_TASK_PER_NODES"));
      int rank;
      MPI_Comm_rank(comm, & rank);
      if(rank == 0){
        printf("Fake tasks per node: using %d\n", tasksPerNode);
      }
      return tasksPerNode;
    }
    char       localhost[MAX_PATHLEN],
        hostname[MAX_PATHLEN];
    int        count               = 1,
        i;
    MPI_Status status;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering count_tasks_per_node...\n" );
        fflush( out_logfile );
    }

    if (gethostname(localhost, MAX_PATHLEN) != 0) {
        FAIL("gethostname()");
    }
    if (rank == 0) {
        /* MPI_receive all hostnames, and compares them to the local hostname */
        for (i = 0; i < size-1; i++) {
            MPI_Recv(hostname, MAX_PATHLEN, MPI_CHAR, MPI_ANY_SOURCE,
                     MPI_ANY_TAG, comm, &status);
            if (strcmp(hostname, localhost) == 0) {
                count++;
            }
        }
    } else {
        /* MPI_send hostname to root node */
        MPI_Send(localhost, MAX_PATHLEN, MPI_CHAR, 0, 0, comm);
    }
    MPI_Bcast(&count, 1, MPI_INT, 0, comm);

    return(count);
}
#endif


/*
 * Extract key/value pair from hint string.
 */
void ExtractHint(char *settingVal, char *valueVal, char *hintString)
{
        char *settingPtr, *valuePtr, *tmpPtr1, *tmpPtr2;

        settingPtr = (char *)strtok(hintString, " =");
        valuePtr = (char *)strtok(NULL, " =\t\r\n");
        tmpPtr1 = settingPtr;
        tmpPtr2 = (char *)strstr(settingPtr, "IOR_HINT__MPI__");
        if (tmpPtr1 == tmpPtr2) {
                settingPtr += strlen("IOR_HINT__MPI__");

        } else {
                tmpPtr2 = (char *)strstr(settingPtr, "IOR_HINT__GPFS__");
                if (tmpPtr1 == tmpPtr2) {
                        settingPtr += strlen("IOR_HINT__GPFS__");
                        fprintf(out_logfile,
                                "WARNING: Unable to set GPFS hints (not implemented.)\n");
                }
        }
        strcpy(settingVal, settingPtr);
        strcpy(valueVal, valuePtr);
}

/*
 * Set hints for MPIIO, HDF5, or NCMPI.
 */
void SetHints(MPI_Info * mpiHints, char *hintsFileName)
{
        char hintString[MAX_STR];
        char settingVal[MAX_STR];
        char valueVal[MAX_STR];
        extern char **environ;
        int i;
        FILE *fd;

        /*
         * This routine checks for hints from the environment and/or from the
         * hints files.  The hints are of the form:
         * 'IOR_HINT__<layer>__<hint>=<value>', where <layer> is either 'MPI'
         * or 'GPFS', <hint> is the full name of the hint to be set, and <value>
         * is the hint value.  E.g., 'setenv IOR_HINT__MPI__IBM_largeblock_io true'
         * or 'IOR_HINT__GPFS__hint=value' in the hints file.
         */
        MPI_CHECK(MPI_Info_create(mpiHints), "cannot create info object");

        /* get hints from environment */
        for (i = 0; environ[i] != NULL; i++) {
                /* if this is an IOR_HINT, pass the hint to the info object */
                if (strncmp(environ[i], "IOR_HINT", strlen("IOR_HINT")) == 0) {
                        strcpy(hintString, environ[i]);
                        ExtractHint(settingVal, valueVal, hintString);
                        MPI_CHECK(MPI_Info_set(*mpiHints, settingVal, valueVal),
                                  "cannot set info object");
                }
        }

        /* get hints from hints file */
        if (hintsFileName != NULL && strcmp(hintsFileName, "") != 0) {

                /* open the hint file */
                fd = fopen(hintsFileName, "r");
                if (fd == NULL) {
                        WARN("cannot open hints file");
                } else {
                        /* iterate over hints file */
                        while (fgets(hintString, MAX_STR, fd) != NULL) {
                                if (strncmp
                                    (hintString, "IOR_HINT",
                                     strlen("IOR_HINT")) == 0) {
                                        ExtractHint(settingVal, valueVal,
                                                    hintString);
                                        MPI_CHECK(MPI_Info_set
                                                  (*mpiHints, settingVal,
                                                   valueVal),
                                                  "cannot set info object");
                                }
                        }
                        /* close the hints files */
                        if (fclose(fd) != 0)
                                ERR("cannot close hints file");
                }
        }
}

/*
 * Show all hints (key/value pairs) in an MPI_Info object.
 */
void ShowHints(MPI_Info * mpiHints)
{
        char key[MPI_MAX_INFO_VAL];
        char value[MPI_MAX_INFO_VAL];
        int flag, i, nkeys;

        MPI_CHECK(MPI_Info_get_nkeys(*mpiHints, &nkeys),
                  "cannot get info object keys");

        for (i = 0; i < nkeys; i++) {
                MPI_CHECK(MPI_Info_get_nthkey(*mpiHints, i, key),
                          "cannot get info object key");
                MPI_CHECK(MPI_Info_get(*mpiHints, key, MPI_MAX_INFO_VAL - 1,
                                       value, &flag),
                          "cannot get info object value");
                fprintf(out_logfile, "\t%s = %s\n", key, value);
        }
}

/*
 * Takes a string of the form 64, 8m, 128k, 4g, etc. and converts to bytes.
 */
IOR_offset_t StringToBytes(char *size_str)
{
        IOR_offset_t size = 0;
        char range;
        int rc;

        rc = sscanf(size_str, "%lld%c", &size, &range);
        if (rc == 2) {
                switch ((int)range) {
                case 'k':
                case 'K':
                        size <<= 10;
                        break;
                case 'm':
                case 'M':
                        size <<= 20;
                        break;
                case 'g':
                case 'G':
                        size <<= 30;
                        break;
                }
        } else if (rc == 0) {
                size = -1;
        }
        return (size);
}

/*
 * Displays size of file system and percent of data blocks and inodes used.
 */
void ShowFileSystemSize(char *fileSystem)
{
#ifndef _WIN32                  /* FIXME */
        char realPath[PATH_MAX];
        char *fileSystemUnitStr;
        long long int totalFileSystemSize;
        long long int freeFileSystemSize;
        long long int totalInodes;
        long long int freeInodes;
        double totalFileSystemSizeHR;
        double usedFileSystemPercentage;
        double usedInodePercentage;
#ifdef __sun                    /* SunOS does not support statfs(), instead uses statvfs() */
        struct statvfs statusBuffer;
#else                           /* !__sun */
        struct statfs statusBuffer;
#endif                          /* __sun */

#ifdef __sun
        if (statvfs(fileSystem, &statusBuffer) != 0) {
                ERR("unable to statvfs() file system");
        }
#else                           /* !__sun */
        if (statfs(fileSystem, &statusBuffer) != 0) {
                ERR("unable to statfs() file system");
        }
#endif                          /* __sun */

        /* data blocks */
#ifdef __sun
        totalFileSystemSize = statusBuffer.f_blocks * statusBuffer.f_frsize;
        freeFileSystemSize = statusBuffer.f_bfree * statusBuffer.f_frsize;
#else                           /* !__sun */
        totalFileSystemSize = statusBuffer.f_blocks * statusBuffer.f_bsize;
        freeFileSystemSize = statusBuffer.f_bfree * statusBuffer.f_bsize;
#endif                          /* __sun */

        usedFileSystemPercentage = (1 - ((double)freeFileSystemSize
                                         / (double)totalFileSystemSize)) * 100;
        totalFileSystemSizeHR =
                (double)totalFileSystemSize / (double)(1<<30);
        fileSystemUnitStr = "GiB";
        if (totalFileSystemSizeHR > 1024) {
                totalFileSystemSizeHR = (double)totalFileSystemSize / (double)((long long)1<<40);
                fileSystemUnitStr = "TiB";
        }

        /* inodes */
        totalInodes = statusBuffer.f_files;
        freeInodes = statusBuffer.f_ffree;
        usedInodePercentage =
            (1 - ((double)freeInodes / (double)totalInodes)) * 100;

        /* show results */
        if (realpath(fileSystem, realPath) == NULL) {
                ERR("unable to use realpath()");
        }

        if(outputFormat == OUTPUT_DEFAULT){
          fprintf(out_resultfile, "%-20s: %s\n", "Path", realPath);
          fprintf(out_resultfile, "%-20s: %.1f %s   Used FS: %2.1f%%   ",
                  "FS", totalFileSystemSizeHR, fileSystemUnitStr,
                  usedFileSystemPercentage);
          fprintf(out_resultfile, "Inodes: %.1f Mi   Used Inodes: %2.1f%%\n",
                  (double)totalInodes / (double)(1<<20),
                  usedInodePercentage);
          fflush(out_logfile);
        }else if(outputFormat == OUTPUT_JSON){
          fprintf(out_resultfile, "    , \"Path\": \"%s\",", realPath);
          fprintf(out_resultfile, "\"Capacity\": \"%.1f %s\", \"Used Capacity\": \"%2.1f%%\",",
                  totalFileSystemSizeHR, fileSystemUnitStr,
                  usedFileSystemPercentage);
          fprintf(out_resultfile, "\"Inodes\": \"%.1f Mi\", \"Used Inodes\" : \"%2.1f%%\"\n",
                  (double)totalInodes / (double)(1<<20),
                  usedInodePercentage);
        }else if(outputFormat == OUTPUT_CSV){

        }

#endif /* !_WIN32 */

        return;
}

/*
 * Return match of regular expression -- 0 is failure, 1 is success.
 */
int Regex(char *string, char *pattern)
{
        int retValue = 0;
#ifndef _WIN32                  /* Okay to always not match */
        regex_t regEx;
        regmatch_t regMatch;

        regcomp(&regEx, pattern, REG_EXTENDED);
        if (regexec(&regEx, string, 1, &regMatch, 0) == 0) {
                retValue = 1;
        }
        regfree(&regEx);
#endif

        return (retValue);
}

/*
 * Seed random generator.
 */
void SeedRandGen(MPI_Comm testComm)
{
        unsigned int randomSeed;

        if (rank == 0) {
#ifdef _WIN32
                rand_s(&randomSeed);
#else
                struct timeval randGenTimer;
                gettimeofday(&randGenTimer, (struct timezone *)NULL);
                randomSeed = randGenTimer.tv_usec;
#endif
        }
        MPI_CHECK(MPI_Bcast(&randomSeed, 1, MPI_INT, 0,
                            testComm), "cannot broadcast random seed value");
        srandom(randomSeed);
}

/*
 * System info for Windows.
 */
#ifdef _WIN32
int uname(struct utsname *name)
{
        DWORD nodeNameSize = sizeof(name->nodename) - 1;

        memset(name, 0, sizeof(struct utsname));
        if (!GetComputerNameEx
            (ComputerNameDnsFullyQualified, name->nodename, &nodeNameSize))
                ERR("GetComputerNameEx failed");

        strncpy(name->sysname, "Windows", sizeof(name->sysname) - 1);
        /* FIXME - these should be easy to fetch */
        strncpy(name->release, "-", sizeof(name->release) - 1);
        strncpy(name->version, "-", sizeof(name->version) - 1);
        strncpy(name->machine, "-", sizeof(name->machine) - 1);
        return 0;
}
#endif /* _WIN32 */


double wall_clock_deviation;
double wall_clock_delta = 0;

/*
 * Get time stamp.  Use MPI_Timer() unless _NO_MPI_TIMER is defined,
 * in which case use gettimeofday().
 */
double GetTimeStamp(void)
{
        double timeVal;
#ifdef _NO_MPI_TIMER
        struct timeval timer;

        if (gettimeofday(&timer, (struct timezone *)NULL) != 0)
                ERR("cannot use gettimeofday()");
        timeVal = (double)timer.tv_sec + ((double)timer.tv_usec / 1000000);
#else                           /* not _NO_MPI_TIMER */
        timeVal = MPI_Wtime();  /* no MPI_CHECK(), just check return value */
        if (timeVal < 0)
                ERR("cannot use MPI_Wtime()");
#endif                          /* _NO_MPI_TIMER */

        /* wall_clock_delta is difference from root node's time */
        timeVal -= wall_clock_delta;

        return (timeVal);
}

/*
 * Determine any spread (range) between node times.
 */
static double TimeDeviation(void)
{
        double timestamp;
        double min = 0;
        double max = 0;
        double roottimestamp;

        MPI_CHECK(MPI_Barrier(mpi_comm_world), "barrier error");
        timestamp = GetTimeStamp();
        MPI_CHECK(MPI_Reduce(&timestamp, &min, 1, MPI_DOUBLE,
                             MPI_MIN, 0, mpi_comm_world),
                  "cannot reduce tasks' times");
        MPI_CHECK(MPI_Reduce(&timestamp, &max, 1, MPI_DOUBLE,
                             MPI_MAX, 0, mpi_comm_world),
                  "cannot reduce tasks' times");

        /* delta between individual nodes' time and root node's time */
        roottimestamp = timestamp;
        MPI_CHECK(MPI_Bcast(&roottimestamp, 1, MPI_DOUBLE, 0, mpi_comm_world),
                  "cannot broadcast root's time");
        wall_clock_delta = timestamp - roottimestamp;

        return max - min;
}

void init_clock(){
  /* check for skew between tasks' start times */
  wall_clock_deviation = TimeDeviation();
}

char * PrintTimestamp() {
    static char datestring[80];
    time_t cur_timestamp;

    if (( rank == 0 ) && ( verbose >= 1 )) {
        fprintf( out_logfile, "V-1: Entering PrintTimestamp...\n" );
    }

    fflush(out_logfile);
    cur_timestamp = time(NULL);
    strftime(datestring, 80, "%m/%d/%Y %T", localtime(&cur_timestamp));

    return datestring;
}

int64_t ReadStoneWallingIterations(char * const filename){
  long long data;
  if(rank != 0){
    MPI_Bcast( & data, 1, MPI_LONG_LONG_INT, 0, mpi_comm_world);
    return data;
  }else{
    FILE * out = fopen(filename, "r");
    if (out == NULL){
      data = -1;
      MPI_Bcast( & data, 1, MPI_LONG_LONG_INT, 0, mpi_comm_world);
      return data;
    }
    int ret = fscanf(out, "%lld", & data);
    if (ret != 1){
      return -1;
    }
    fclose(out);
    MPI_Bcast( & data, 1, MPI_LONG_LONG_INT, 0, mpi_comm_world);
    return data;
  }
}

void StoreStoneWallingIterations(char * const filename, int64_t count){
  if(rank != 0){
    return;
  }
  FILE * out = fopen(filename, "w");
  if (out == NULL){
    FAIL("Cannot write to the stonewalling file!");
  }
  fprintf(out, "%lld", (long long) count);
  fclose(out);
}

/*
 * Sleep for 'delay' seconds.
 */
void DelaySecs(int delay){
  if (rank == 0 && delay > 0) {
    if (verbose >= VERBOSE_1)
            fprintf(out_logfile, "delaying %d seconds . . .\n", delay);
    sleep(delay);
  }
}


/*
 * Convert IOR_offset_t value to human readable string.  This routine uses a
 * statically-allocated buffer internally and so is not re-entrant.
 */
char *HumanReadable(IOR_offset_t value, int base)
{
        static char valueStr[MAX_STR];
        IOR_offset_t m = 0, g = 0, t = 0;
        char m_str[8], g_str[8], t_str[8];

        if (base == BASE_TWO) {
                m = MEBIBYTE;
                g = GIBIBYTE;
                t = GIBIBYTE * 1024llu;
                strcpy(m_str, "MiB");
                strcpy(g_str, "GiB");
                strcpy(t_str, "TiB");
        } else if (base == BASE_TEN) {
                m = MEGABYTE;
                g = GIGABYTE;
                t = GIGABYTE * 1000llu;
                strcpy(m_str, "MB");
                strcpy(g_str, "GB");
                strcpy(t_str, "TB");
        }

        if (value >= t) {
                if (value % t) {
                        snprintf(valueStr, MAX_STR-1, "%.2f %s",
                                (double)((double)value / t), t_str);
                } else {
                        snprintf(valueStr, MAX_STR-1, "%d %s", (int)(value / t), t_str);
                }
        }else if (value >= g) {
                if (value % g) {
                        snprintf(valueStr, MAX_STR-1, "%.2f %s",
                                (double)((double)value / g), g_str);
                } else {
                        snprintf(valueStr, MAX_STR-1, "%d %s", (int)(value / g), g_str);
                }
        } else if (value >= m) {
                if (value % m) {
                        snprintf(valueStr, MAX_STR-1, "%.2f %s",
                                (double)((double)value / m), m_str);
                } else {
                        snprintf(valueStr, MAX_STR-1, "%d %s", (int)(value / m), m_str);
                }
        } else if (value >= 0) {
                snprintf(valueStr, MAX_STR-1, "%d bytes", (int)value);
        } else {
                snprintf(valueStr, MAX_STR-1, "-");
        }
        return valueStr;
}
