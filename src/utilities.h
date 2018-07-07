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

extern int numTasksWorld;
extern int rank;
extern int rankOffset;
extern int tasksPerNode;
extern int verbose;
extern MPI_Comm testComm;
extern MPI_Comm mpi_comm_world;
extern FILE * out_logfile;

void set_o_direct_flag(int *fd);

char *CurrentTimeString(void);
void OutputToRoot(int, MPI_Comm, char *);
int Regex(char *, char *);
void ShowFileSystemSize(char *);
void DumpBuffer(void *, size_t);
void SeedRandGen(MPI_Comm);
void SetHints (MPI_Info *, char *);
void ShowHints (MPI_Info *);

void init_clock(void);
double GetTimeStamp(void);

extern double wall_clock_deviation;
extern double wall_clock_delta;
#endif  /* !_UTILITIES_H */
