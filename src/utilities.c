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
#include "config.h"
#endif

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
#include <regex.h>
#ifdef __sun                    /* SunOS does not support statfs(), instead uses statvfs() */
#include <sys/statvfs.h>
#else                           /* !__sun */
#include <sys/statfs.h>
#endif                          /* __sun */
#include <sys/time.h>           /* gettimeofday() */
#endif

#include "utilities.h"
#include "aiori.h"
#include "ior.h"

/************************** D E C L A R A T I O N S ***************************/

extern int errno;
extern int numTasks;
extern int rank;
extern int rankOffset;
extern int verbose;

/***************************** F U N C T I O N S ******************************/

/*
 * Returns string containing the current time.
 */
char *CurrentTimeString(void)
{
        static time_t currentTime;
        char *currentTimePtr;

        if ((currentTime = time(NULL)) == -1)
                ERR("cannot get current time");
        if ((currentTimePtr = ctime(&currentTime)) == NULL) {
                ERR("cannot read current time");
        }
        /* ctime string ends in \n */
        return (currentTimePtr);
}

/*
 * Dump transfer buffer.
 */
void DumpBuffer(void *buffer, size_t size)
{
        size_t i, j;
        unsigned long long *dumpBuf = (unsigned long long *)buffer;

        for (i = 0; i < ((size / sizeof(IOR_size_t)) / 4); i++) {
                for (j = 0; j < 4; j++) {
                        fprintf(stdout, "%016llx ", dumpBuf[4 * i + j]);
                }
                fprintf(stdout, "\n");
        }
        return;
}                               /* DumpBuffer() */

/*
 * Sends all strings to root nodes and displays.
 */
void OutputToRoot(int numTasks, MPI_Comm comm, char *stringToDisplay)
{
        int i;
        int swapNeeded = TRUE;
        int pairsToSwap;
        char **stringArray;
        char tmpString[MAX_STR];
        MPI_Status status;

        /* malloc string array */
        stringArray = (char **)malloc(sizeof(char *) * numTasks);
        if (stringArray == NULL)
                ERR("out of memory");
        for (i = 0; i < numTasks; i++) {
                stringArray[i] = (char *)malloc(sizeof(char) * MAX_STR);
                if (stringArray[i] == NULL)
                        ERR("out of memory");
        }

        strcpy(stringArray[rank], stringToDisplay);

        if (rank == 0) {
                /* MPI_receive all strings */
                for (i = 1; i < numTasks; i++) {
                        MPI_CHECK(MPI_Recv(stringArray[i], MAX_STR, MPI_CHAR,
                                           MPI_ANY_SOURCE, MPI_ANY_TAG, comm,
                                           &status), "MPI_Recv() error");
                }
        } else {
                /* MPI_send string to root node */
                MPI_CHECK(MPI_Send
                          (stringArray[rank], MAX_STR, MPI_CHAR, 0, 0, comm),
                          "MPI_Send() error");
        }
        MPI_CHECK(MPI_Barrier(comm), "barrier error");

        /* sort strings using bubblesort */
        if (rank == 0) {
                pairsToSwap = numTasks - 1;
                while (swapNeeded) {
                        swapNeeded = FALSE;
                        for (i = 0; i < pairsToSwap; i++) {
                                if (strcmp(stringArray[i], stringArray[i + 1]) >
                                    0) {
                                        strcpy(tmpString, stringArray[i]);
                                        strcpy(stringArray[i],
                                               stringArray[i + 1]);
                                        strcpy(stringArray[i + 1], tmpString);
                                        swapNeeded = TRUE;
                                }
                        }
                        pairsToSwap--;
                }
        }

        /* display strings */
        if (rank == 0) {
                for (i = 0; i < numTasks; i++) {
                        fprintf(stdout, "%s\n", stringArray[i]);
                }
        }

        /* free strings */
        for (i = 0; i < numTasks; i++) {
                free(stringArray[i]);
        }
        free(stringArray);
}

/*
 * Extract key/value pair from hint string.
 */
void ExtractHint(char *settingVal, char *valueVal, char *hintString)
{
        char *settingPtr, *valuePtr, *tmpPtr1, *tmpPtr2;

        settingPtr = (char *)strtok(hintString, "=");
        valuePtr = (char *)strtok(NULL, " \t\r\n");
        tmpPtr1 = settingPtr;
        tmpPtr2 = (char *)strstr(settingPtr, "IOR_HINT__MPI__");
        if (tmpPtr1 == tmpPtr2) {
                settingPtr += strlen("IOR_HINT__MPI__");

        } else {
                tmpPtr2 = (char *)strstr(settingPtr, "IOR_HINT__GPFS__");
                if (tmpPtr1 == tmpPtr2) {
                        settingPtr += strlen("IOR_HINT__GPFS__");
                        fprintf(stdout,
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
        if (strcmp(hintsFileName, "") != 0) {

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
                fprintf(stdout, "\t%s = %s\n", key, value);
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
        fprintf(stdout, "Path: %s\n", realPath);
        fprintf(stdout, "FS: %.1f %s   Used FS: %2.1f%%   ",
                totalFileSystemSizeHR, fileSystemUnitStr,
                usedFileSystemPercentage);
        fprintf(stdout, "Inodes: %.1f Mi   Used Inodes: %2.1f%%\n",
                (double)totalInodes / (double)(1<<20),
                usedInodePercentage);
        fflush(stdout);
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
