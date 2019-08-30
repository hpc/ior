/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
\******************************************************************************/

#ifndef _UTILITIES_H
#define _UTILITIES_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <mpi.h>
#include "ior.h"

extern int rank;
extern int rankOffset;
extern int verbose;
extern MPI_Comm testComm;
extern MPI_Comm mpi_comm_world;
extern FILE * out_logfile;
extern FILE * out_resultfile;
extern enum OutputFormat_t outputFormat;  /* format of the output */

/*
 * Try using the system's PATH_MAX, which is what realpath and such use.
 */
#define MAX_PATHLEN PATH_MAX


#ifdef __linux__
#define ERROR_LOCATION __func__
#else
#define ERROR_LOCATION __LINE__
#endif

#define FAIL(...) FailMessage(rank, ERROR_LOCATION, __VA_ARGS__)
void FailMessage(int rank, const char *location, char *format, ...);

void* safeMalloc(uint64_t size);
void set_o_direct_flag(int *fd);

char *CurrentTimeString(void);
int Regex(char *, char *);
void ShowFileSystemSize(char *);
void DumpBuffer(void *, size_t);
void SeedRandGen(MPI_Comm);
void SetHints (MPI_Info *, char *);
void ShowHints (MPI_Info *);
char *HumanReadable(IOR_offset_t value, int base);
int QueryNodeMapping(MPI_Comm comm, int print_nodemap);
int GetNumNodes(MPI_Comm);
int GetNumTasks(MPI_Comm);
int GetNumTasksOnNode0(MPI_Comm);
void DelaySecs(int delay);
void updateParsedOptions(IOR_param_t * options, options_all_t * global_options);
size_t NodeMemoryStringToBytes(char *size_str);

/* Returns -1, if cannot be read  */
int64_t ReadStoneWallingIterations(char * const filename);
void StoreStoneWallingIterations(char * const filename, int64_t count);

void init_clock(void);
double GetTimeStamp(void);
char * PrintTimestamp(); // TODO remove this function

extern double wall_clock_deviation;
extern double wall_clock_delta;
#endif  /* !_UTILITIES_H */
