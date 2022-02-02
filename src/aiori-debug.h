#ifndef _AIORI_UTIL_H
#define _AIORI_UTIL_H

/* This file contains only debug relevant helpers */

#include <stdio.h>
#include <mpi.h>

/* output log file */
extern FILE * out_logfile;
/* verbosity level */
extern int verbose;
/* treat warnings as errors */
extern int aiori_warning_as_errors;

#define FAIL(...) FailMessage(rank, ERROR_LOCATION, __VA_ARGS__)
void FailMessage(int rank, const char *location, char *format, ...);

/* display simple warning message and reset member value to default */
#define WARN_RESET(MSG, TO_STRUCT_PTR, FROM_STRUCT_PTR, MEMBER) do {    \
    (TO_STRUCT_PTR)->MEMBER = (FROM_STRUCT_PTR)->MEMBER;                \
    if (rank == 0) {                                                    \
        fprintf(out_logfile, "WARNING: %s. Using value of %d.\n",       \
                MSG, (TO_STRUCT_PTR)->MEMBER);                          \
    }                                                                   \
    fflush(out_logfile);                                                \
} while (0)

/* display warning message with format string */
#define WARNF(FORMAT, ...) do {                                         \
    if(aiori_warning_as_errors){                                        \
        ERRF(FORMAT, __VA_ARGS__);                                      \
    }                                                                   \
    if (verbose > VERBOSE_2) {                                          \
        fprintf(out_logfile, "WARNING: " FORMAT ", (%s:%d).\n",         \
                __VA_ARGS__,  __FILE__, __LINE__);                      \
    } else {                                                            \
        fprintf(out_logfile, "WARNING: " FORMAT "\n",                   \
                __VA_ARGS__);                                           \
    }                                                                   \
    fflush(out_logfile);                                                \
} while (0)

/* display simple warning message */
#define WARN(MSG) do {                                                  \
    WARNF("%s", MSG);                                                   \
} while (0)

/* display info message with format string */
#define INFOF(FORMAT, ...) do {                                         \
    if (verbose > VERBOSE_2) {                                          \
        fprintf(out_logfile, "INFO: " FORMAT ", (%s:%d).\n",            \
                __VA_ARGS__,  __FILE__, __LINE__);                      \
    } else {                                                            \
        fprintf(out_logfile, "INFO: " FORMAT "\n",                      \
                __VA_ARGS__);                                           \
    }                                                                   \
    fflush(out_logfile);                                                \
} while (0)

/* display simple info message */
#define INFO(MSG) do {                                                  \
    INFOF("%s", MSG);                                                   \
} while (0)

/* display error message with format string and terminate execution */
#define ERRF(FORMAT, ...) do {                                          \
    fprintf(out_logfile, "ERROR: " FORMAT ", (%s:%d)\n",                \
            __VA_ARGS__, __FILE__, __LINE__);                           \
    fflush(out_logfile);                                                \
    MPI_Abort(MPI_COMM_WORLD, -1);                                      \
} while (0)

/* display simple error message and terminate execution */
#define ERR(MSG) do {                                                   \
    ERRF("%s", MSG);                                                    \
} while (0)

/* if MPI_STATUS indicates error, display error message with format */
/* string and error string from MPI_STATUS and terminate execution */
#define MPI_CHECKF(MPI_STATUS, FORMAT, ...) do {                        \
    char resultString[MPI_MAX_ERROR_STRING];                            \
    int resultLength;                                                   \
    int _MPI_STATUS = (MPI_STATUS);                                     \
                                                                        \
    if (_MPI_STATUS != MPI_SUCCESS) {                                   \
        MPI_Error_string(_MPI_STATUS, resultString, &resultLength);     \
        fprintf(out_logfile, "ERROR: " FORMAT ", MPI %s, (%s:%d)\n",    \
                __VA_ARGS__, resultString, __FILE__, __LINE__);         \
        fflush(out_logfile);                                            \
        MPI_Abort(MPI_COMM_WORLD, -1);                                  \
    }                                                                   \
} while(0)

/* if MPI_STATUS indicates error, display simple error message with */
/* error string from MPI_STATUS and terminate execution */
#define MPI_CHECK(MPI_STATUS, MSG) do {                                 \
    MPI_CHECKF(MPI_STATUS, "%s", MSG);                                  \
} while(0)

#endif
