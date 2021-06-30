#ifndef _AIORI_UTIL_H
#define _AIORI_UTIL_H

/* This file contains only debug relevant helpers */

#include <stdio.h>
#include <mpi.h>

extern FILE * out_logfile;
extern int verbose;                            /* verbose output */

#define FAIL(...) FailMessage(rank, ERROR_LOCATION, __VA_ARGS__)
void FailMessage(int rank, const char *location, char *format, ...);

/******************************** M A C R O S *********************************/

/******************************************************************************/
/*
 * WARN_RESET will display a custom error message and set value to default
 */
#define WARN_RESET(MSG, TO_STRUCT_PTR, FROM_STRUCT_PTR, MEMBER) do {     \
        (TO_STRUCT_PTR)->MEMBER = (FROM_STRUCT_PTR)->MEMBER;             \
        if (rank == 0) {                                                 \
            fprintf(out_logfile, "WARNING: %s.  Using value of %d.\n",    \
                    MSG, (TO_STRUCT_PTR)->MEMBER);                       \
        }                                                                \
        fflush(out_logfile);                                                  \
} while (0)

extern int aiori_warning_as_errors;

#define WARN(MSG) do {                                                   \
        if(aiori_warning_as_errors){ ERR(MSG); }                           \
        if (verbose > VERBOSE_2) {                                       \
            fprintf(out_logfile, "WARNING: %s, (%s:%d).\n",               \
                    MSG, __FILE__, __LINE__);                            \
        } else {                                                         \
            fprintf(out_logfile, "WARNING: %s.\n", MSG);                  \
        }                                                                \
        fflush(out_logfile);                                                  \
} while (0)


/* warning with format string and errno printed */
#define EWARNF(FORMAT, ...) do {                                         \
        if(aiori_warning_as_errors){ ERRF(FORMAT, __VA_ARGS__); }          \
        if (verbose > VERBOSE_2) {                                       \
            fprintf(out_logfile, "WARNING: " FORMAT ", (%s:%d).\n", \
                    __VA_ARGS__,  __FILE__, __LINE__); \
        } else {                                                         \
            fprintf(out_logfile, "WARNING: " FORMAT "\n",  \
                    __VA_ARGS__);                \
        }                                                                \
        fflush(out_logfile);                                                  \
} while (0)


/* warning with errno printed */
#define EWARN(MSG) do {                                                  \
        EWARNF("%s", MSG);                                               \
} while (0)


/* warning with format string and errno printed */
#define EINFO(FORMAT, ...) do {                                         \
        if (verbose > VERBOSE_2) {                                       \
            fprintf(out_logfile, "INFO: " FORMAT ", (%s:%d).\n", \
                    __VA_ARGS__,  __FILE__, __LINE__); \
        } else {                                                         \
            fprintf(out_logfile, "INFO: " FORMAT "\n",  \
                    __VA_ARGS__);                \
        }                                                                \
        fflush(out_logfile);                                                  \
} while (0)

/* display error message with format string and terminate execution */
#define ERRF(FORMAT, ...) do {                                           \
        fprintf(out_logfile, "ERROR: " FORMAT ", (%s:%d)\n", \
                __VA_ARGS__, __FILE__, __LINE__); \
        fflush(out_logfile);                                                  \
        MPI_Abort(MPI_COMM_WORLD, -1);                                   \
} while (0)


/* display error message and terminate execution */
#define ERR_ERRNO(MSG) do {                                                    \
        ERRF("%s", MSG);                                                 \
} while (0)


/* display a simple error message (i.e. errno is not set) and terminate execution */
#define ERR(MSG) do {                                            \
        fprintf(out_logfile, "ERROR: %s, (%s:%d)\n",                     \
                MSG, __FILE__, __LINE__);                               \
        fflush(out_logfile);                                                 \
        MPI_Abort(MPI_COMM_WORLD, -1);                                  \
} while (0)


/******************************************************************************/
/*
 * MPI_CHECKF will display a custom format string as well as an error string
 * from the MPI_STATUS and then exit the program
 */

#define MPI_CHECKF(MPI_STATUS, FORMAT, ...) do {                         \
    char resultString[MPI_MAX_ERROR_STRING];                             \
    int resultLength;                                                    \
    int checkf_mpi_status = MPI_STATUS;                                  \
                                                                         \
    if (checkf_mpi_status != MPI_SUCCESS) {                              \
        MPI_Error_string(checkf_mpi_status, resultString, &resultLength);\
        fprintf(out_logfile, "ERROR: " FORMAT ", MPI %s, (%s:%d)\n",     \
                __VA_ARGS__, resultString, __FILE__, __LINE__);          \
        fflush(out_logfile);                                             \
        MPI_Abort(MPI_COMM_WORLD, -1);                                   \
    }                                                                    \
} while(0)


/******************************************************************************/
/*
 * MPI_CHECK will display a custom error message as well as an error string
 * from the MPI_STATUS and then exit the program
 */

#define MPI_CHECK(MPI_STATUS, MSG) do {                                  \
    MPI_CHECKF(MPI_STATUS, "%s", MSG);                                   \
} while(0)

#endif
