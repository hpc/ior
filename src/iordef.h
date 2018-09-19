/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
\******************************************************************************/

#ifndef _IORDEF_H
#define _IORDEF_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mpi.h>

#ifdef _WIN32
#   define _CRT_SECURE_NO_WARNINGS
#   define _CRT_RAND_S
#   pragma warning(4 : 4996)    /* Don't complain about POSIX names */
#   pragma warning(4 : 4267)    /* '=' : conversion from 'size_t' to 'int' */
#   pragma warning(4 : 4244)    /* 'function' : conversion from 'IOR_offset_t' to 'int' */

#   include <Windows.h>
#   include <io.h>
#   include <direct.h>

#   define F_OK 00
#   define W_OK 02
#   define R_OK 04
#   define X_OK 04

#   define lseek _lseeki64
#   define fsync _commit
#   define mkdir(dir, mode) _mkdir(dir)
#   define strcasecmp _stricmp
#   define strncasecmp _strnicmp
#   define srandom srand
#   define random() (rand() * (RAND_MAX+1) + rand())    /* Note: only 30 bits */
#   define sleep(X) Sleep((X)*1000)
#   define getpagesize() 4096
#else
#   include <sys/param.h>                               /* MAXPATHLEN */
#   include <unistd.h>
#   include <limits.h>
#endif

/************************** D E C L A R A T I O N S ***************************/

extern int numTasks;                           /* MPI variables */
extern int rank;
extern int rankOffset;
extern int verbose;                            /* verbose output */

/*************************** D E F I N I T I O N S ****************************/

enum OutputFormat_t{
  OUTPUT_DEFAULT,
  OUTPUT_CSV,
  OUTPUT_JSON
};

#ifndef FALSE
#   define FALSE           0
#endif /* not FALSE */

#ifndef TRUE
#   define TRUE            1
#endif /* not TRUE */

#ifndef NULL
#   define NULL            ((void *)0)
#endif /* not NULL */

#define KILOBYTE           1000
#define MEGABYTE           1000000
#define GIGABYTE           1000000000

#define KIBIBYTE           (1 << 10)
#define MEBIBYTE           (1 << 20)
#define GIBIBYTE           (1 << 30)

/* for displaying MiB or MB */
#define BASE_TWO           0
#define BASE_TEN           1

/* any write/read access in code */
#define WRITE              0
#define WRITECHECK         1
#define READ               2
#define READCHECK          3

/* verbosity settings */
#define VERBOSE_0          0
#define VERBOSE_1          1
#define VERBOSE_2          2
#define VERBOSE_3          3
#define VERBOSE_4          4
#define VERBOSE_5          5

#define MAX_STR            1024                /* max string length */
#define MAX_HINTS          16                  /* max number of hints */
#define MAX_RETRY          10000               /* max retries for POSIX xfer */
#ifndef PATH_MAX
#define PATH_MAX           4096
#endif

#define DELIMITERS         " \t\r\n="          /* ReadScript() */
#define FILENAME_DELIMITER '@'                 /* ParseFileName() */

/* MACROs for debugging */
#define HERE               fprintf(stdout, "** LINE %d (TASK=%d) **\n", \
                                   __LINE__, rank);

typedef long long int      IOR_offset_t;
typedef long long int      IOR_size_t;

#define                    IOR_format "%016llx"


/******************************** M A C R O S *********************************/

/******************************************************************************/
/*
 * WARN_RESET will display a custom error message and set value to default
 */
#define WARN_RESET(MSG, TO_STRUCT_PTR, FROM_STRUCT_PTR, MEMBER) do {     \
        (TO_STRUCT_PTR)->MEMBER = (FROM_STRUCT_PTR)->MEMBER;             \
        if (rank == 0) {                                                 \
            fprintf(stdout, "ior WARNING: %s.  Using value of %d.\n",    \
                    MSG, (TO_STRUCT_PTR)->MEMBER);                       \
        }                                                                \
        fflush(stdout);                                                  \
} while (0)


#define WARN(MSG) do {                                                   \
        if (verbose > VERBOSE_2) {                                       \
            fprintf(stdout, "ior WARNING: %s, (%s:%d).\n",               \
                    MSG, __FILE__, __LINE__);                            \
        } else {                                                         \
            fprintf(stdout, "ior WARNING: %s.\n", MSG);                  \
        }                                                                \
        fflush(stdout);                                                  \
} while (0)

/* warning with errno printed */
#define EWARN(MSG) do {                                                  \
        if (verbose > VERBOSE_2) {                                       \
            fprintf(stdout, "ior WARNING: %s, errno %d, %s (%s:%d).\n",  \
                    MSG, errno, strerror(errno), __FILE__, __LINE__);    \
        } else {                                                         \
            fprintf(stdout, "ior WARNING: %s, errno %d, %s \n",          \
                    MSG, errno, strerror(errno));                        \
        }                                                                \
        fflush(stdout);                                                  \
} while (0)


/* display error message and terminate execution */
#define ERR(MSG) do {                                                    \
        fprintf(stdout, "ior ERROR: %s, errno %d, %s (%s:%d)\n",         \
                MSG, errno, strerror(errno), __FILE__, __LINE__);        \
        fflush(stdout);                                                  \
        MPI_Abort(MPI_COMM_WORLD, -1);                                   \
} while (0)


/* display a simple error message (i.e. errno is not set) and terminate execution */
#define ERR_SIMPLE(MSG) do {                                            \
        fprintf(stdout, "ior ERROR: %s, (%s:%d)\n",                     \
                MSG, __FILE__, __LINE__);                               \
        fflush(stdout);                                                 \
        MPI_Abort(MPI_COMM_WORLD, -1);                                  \
} while (0)


/******************************************************************************/
/*
 * MPI_CHECK will display a custom error message as well as an error string
 * from the MPI_STATUS and then exit the program
 */

#define MPI_CHECK(MPI_STATUS, MSG) do {                                  \
    char resultString[MPI_MAX_ERROR_STRING];                             \
    int resultLength;                                                    \
                                                                         \
    if (MPI_STATUS != MPI_SUCCESS) {                                     \
        MPI_Error_string(MPI_STATUS, resultString, &resultLength);       \
        fprintf(stdout, "ior ERROR: %s, MPI %s, (%s:%d)\n",              \
                MSG, resultString, __FILE__, __LINE__);                  \
        fflush(stdout);                                                  \
        MPI_Abort(MPI_COMM_WORLD, -1);                                   \
    }                                                                    \
} while(0)


/******************************************************************************/
/*
 * System info for Windows.
 */

#ifdef _WIN32

struct utsname {
    char sysname [16];
    char nodename[257];
    char release [16];
    char version [16];
    char machine [16];
};

extern int uname(struct utsname *name);

#endif /* _WIN32 */

#endif /* not _IORDEF_H */
