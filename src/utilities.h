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
#include "config.h"
#endif

#include <mpi.h>

#include "ior.h"

char *CurrentTimeString(void);
void OutputToRoot(int, MPI_Comm, char *);
int Regex(char *, char *);
void ShowFileSystemSize(char *);
void DumpBuffer(void *, size_t);
void SeedRandGen(MPI_Comm);
void SetHints (MPI_Info *, char *);
void ShowHints (MPI_Info *);

#endif  /* !_UTILITIES_H */
